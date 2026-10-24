// Lớp cầu nối tay điều khiển RC (STM32 qua /dev/ttyAMA2) cho QML.
//
// Ba việc:
//  1. Giám sát: cần gạt, công tắc SA..SD, kênh CH1..CH8, raw ADC, sức khoẻ link
//     (fps / dropped / calib / ELRS) — chụp ~30 Hz từ ``rc::RcLink``.
//  2. Gán chế độ bay theo công tắc: mỗi vị trí công tắc ↔ một mode (hoặc trống).
//     Khi công tắc đổi vị trí (ổn định ≥ 60 ms) và tính năng đang BẬT, gửi
//     SET_MODE qua ``controller.commands()``. Bảng gán lưu ở
//     ~/.lite_gcs/rc_switches.json. Lần đọc đầu chỉ làm mốc — không tự đổi mode.
//  3. Hiệu chỉnh cần gạt: ghi min/max khi người dùng đẩy hết hành trình, chụp
//     tâm, rồi GHI (RAM STM32, có hiệu lực ngay) và LƯU (flash) — luôn do người
//     dùng bấm, không bao giờ tự gửi (PI_UI_AGENT_GUIDE.md §6). Chỉ những kênh
//     được chọn mới ghi: kênh tự được chọn khi đẩy đủ hành trình; kênh không
//     đụng tới giữ nguyên hiệu chỉnh cũ.
#pragma once

#include "app/controller.h"
#include "rc/rc_link.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

#include <array>

class QTimer;

namespace gcs::bridge {

class RcViewModel : public QObject {
    Q_OBJECT

    // ── link ────────────────────────────────────────────────────────────────
    Q_PROPERTY(QString portName READ portName CONSTANT)
    Q_PROPERTY(bool portOpen READ portOpen NOTIFY linkChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY linkChanged)
    Q_PROPERTY(QString linkError READ linkError NOTIFY linkChanged)
    Q_PROPERTY(int fps READ fps NOTIFY healthChanged)
    Q_PROPERTY(int dropped READ dropped NOTIFY healthChanged)
    Q_PROPERTY(int badFrames READ badFrames NOTIFY healthChanged)
    Q_PROPERTY(bool calibLoaded READ calibLoaded NOTIFY healthChanged)
    Q_PROPERTY(bool elrsOutput READ elrsOutput NOTIFY healthChanged)

    // ── trạng thái điều khiển ───────────────────────────────────────────────
    Q_PROPERTY(QVariantList controls READ controls NOTIFY stateChanged)   // 8 × µs (LH..SD)
    Q_PROPERTY(QVariantList channels READ channels NOTIFY stateChanged)   // 8 × µs (CH1..CH8)
    Q_PROPERTY(QVariantList rawAdc READ rawAdc NOTIFY stateChanged)       // 6 × raw (LH,LV,RH,RV,SB,SC)
    Q_PROPERTY(QVariantList switchPos READ switchPos NOTIFY switchesChanged) // 4 × 0/1/2
    Q_PROPERTY(QStringList channelSources READ channelSources NOTIFY channelSourcesChanged) // CH1..8 → "RV"…
    // đảo chiều theo nguồn (LH..SD): 8 × 0/1, -1 = chưa đọc được
    Q_PROPERTY(QVariantList reversed READ reversed NOTIFY reverseChanged)
    Q_PROPERTY(QStringList controlNames READ controlNames CONSTANT) // "LH".."SD" = chỉ số nguồn

    // ── gán chế độ bay ──────────────────────────────────────────────────────
    Q_PROPERTY(bool modeSwitchEnabled READ modeSwitchEnabled WRITE setModeSwitchEnabled NOTIFY switchModesChanged)
    Q_PROPERTY(QVariantList switchModes READ switchModes NOTIFY switchModesChanged) // 4 × [3 × mode|""]
    Q_PROPERTY(QStringList availableModes READ availableModes CONSTANT)
    Q_PROPERTY(QString lastAction READ lastAction NOTIFY lastActionChanged)

    // hiển thị: đổi chỗ gimbal trái/phải (cần LH/LV nằm bên phải trên tay cầm)
    Q_PROPERTY(bool gimbalSwap READ gimbalSwap WRITE setGimbalSwap NOTIFY gimbalSwapChanged)

    // ── hiệu chỉnh ──────────────────────────────────────────────────────────
    // calStep: 0 = rảnh, 1 = đang ghi hành trình min/max, 2 = đã chụp tâm (chờ GHI)
    Q_PROPERTY(int calStep READ calStep NOTIFY calChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY calChanged)
    Q_PROPERTY(QVariantList calMin READ calMin NOTIFY calChanged)
    Q_PROPERTY(QVariantList calMid READ calMid NOTIFY calChanged)
    Q_PROPERTY(QVariantList calMax READ calMax NOTIFY calChanged)
    Q_PROPERTY(QVariantList storedCal READ storedCal NOTIFY calChanged) // 6 × [min,mid,max] | []
    // kênh nào sẽ được ghi: tự bật khi kênh được đẩy đủ hành trình, chạm để bật/tắt tay
    Q_PROPERTY(QVariantList calUse READ calUse NOTIFY calChanged)
    Q_PROPERTY(QString calMessage READ calMessage NOTIFY calChanged)
    Q_PROPERTY(bool calMessageError READ calMessageError NOTIFY calChanged)

public:
    RcViewModel(app::GcsController *controller, const QString &portName, QObject *parent = nullptr);

    QString portName() const { return m_link->portName(); }
    bool portOpen() const { return m_portOpen; }
    bool connected() const { return m_connected; }
    QString linkError() const { return m_link->lastError(); }
    int fps() const { return m_fps; }
    int dropped() const { return static_cast<int>(m_link->dropped()); }
    int badFrames() const { return static_cast<int>(m_link->badFrames()); }
    bool calibLoaded() const { return m_calibLoaded; }
    bool elrsOutput() const { return m_elrsOutput; }

    QVariantList controls() const { return m_controls; }
    QVariantList channels() const { return m_channels; }
    QVariantList rawAdc() const { return m_rawAdc; }
    QVariantList switchPos() const;
    QStringList channelSources() const { return m_channelSources; }
    QVariantList reversed() const;
    QStringList controlNames() const
    {
        QStringList out;
        for (const char *n : rc::kControls)
            out << QLatin1String(n);
        return out;
    }

    bool modeSwitchEnabled() const { return m_modeSwitchEnabled; }
    void setModeSwitchEnabled(bool on);
    QVariantList switchModes() const;
    QStringList availableModes() const;
    QString lastAction() const { return m_lastAction; }

    bool gimbalSwap() const { return m_gimbalSwap; }
    void setGimbalSwap(bool on);

    int calStep() const { return m_calStep; }
    bool busy() const { return m_busy; }
    QVariantList calMin() const { return toList(m_calMin); }
    QVariantList calMid() const { return toList(m_calMid); }
    QVariantList calMax() const { return toList(m_calMax); }
    QVariantList storedCal() const;
    QVariantList calUse() const;
    QString calMessage() const { return m_calMessage; }
    bool calMessageError() const { return m_calMessageError; }

    // ── hành động từ QML ────────────────────────────────────────────────────
    Q_INVOKABLE void setSwitchMode(int sw, int pos, const QString &mode);
    Q_INVOKABLE void startCalibration();
    Q_INVOKABLE void captureCenter();
    Q_INVOKABLE void cancelCalibration();
    Q_INVOKABLE void toggleCalChannel(int ch); // bật/tắt ghi kênh này (tắt chế độ tự chọn của kênh)
    Q_INVOKABLE void applyCalibration(); // CONFIG_WRITE CAL × 6 (RAM)
    Q_INVOKABLE void setReverse(int src, bool on); // CONFIG_WRITE REVERSE (RAM, hiệu lực ngay)
    Q_INVOKABLE void saveConfig();       // CONFIG_SAVE (flash)
    Q_INVOKABLE void refreshConfig();    // đọc lại MAP + CAL

signals:
    void linkChanged();
    void healthChanged();
    void stateChanged();
    void switchesChanged();
    void channelSourcesChanged();
    void reverseChanged();
    void switchModesChanged();
    void lastActionChanged();
    void gimbalSwapChanged();
    void calChanged();

private:
    static QVariantList toList(const std::array<int, 6> &a);

    void poll();
    void handleSwitches(const rc::RcState &st);
    void setMessage(const QString &msg, bool error);
    bool calChannelValid(int ch) const;
    QStringList selectedCalNames() const;
    QString calDetail(int ch) const;
    void loadSwitchModes();
    void saveSwitchModes() const;

    app::GcsController *m_controller;
    rc::RcLink *m_link;
    QTimer *m_timer;

    bool m_portOpen = false;
    bool m_connected = false;
    int m_fps = 0;
    quint64 m_fpsFrames = 0;
    qint64 m_fpsT0 = 0;
    bool m_calibLoaded = false;
    bool m_elrsOutput = false;
    int m_lastSeq = -1;

    QVariantList m_controls, m_channels, m_rawAdc;
    std::array<int, 4> m_switchPos{{-1, -1, -1, -1}};
    QStringList m_channelSources;
    std::array<int, 8> m_reverse{{-1, -1, -1, -1, -1, -1, -1, -1}};

    // vài frame raw ADC gần nhất — lấy trung bình khi chụp tâm
    static constexpr int kHistLen = 8;
    std::array<std::array<int, kHistLen>, 6> m_adcHist{};
    int m_histPos = 0;
    int m_histCount = 0;

    // công tắc → mode: m_modes[sw][pos]; vị trí "đã xác nhận" + ứng viên debounce
    std::array<std::array<QString, 3>, 4> m_modes;
    bool m_modeSwitchEnabled = false;
    std::array<int, 4> m_stablePos{{-1, -1, -1, -1}};
    std::array<int, 4> m_candPos{{-1, -1, -1, -1}};
    std::array<qint64, 4> m_candSince{};
    QString m_lastAction;
    bool m_gimbalSwap = false;

    int m_calStep = 0;
    bool m_busy = false;
    std::array<int, 6> m_calMin{}, m_calMid{}, m_calMax{};
    std::array<bool, 6> m_calUse{};    // kênh sẽ được ghi
    std::array<bool, 6> m_calManual{}; // người dùng đã tự chọn → không tự bật nữa
    std::array<std::array<int, 3>, 6> m_stored{};
    std::array<bool, 6> m_storedValid{};
    QString m_calMessage;
    bool m_calMessageError = false;
};

} // namespace gcs::bridge
