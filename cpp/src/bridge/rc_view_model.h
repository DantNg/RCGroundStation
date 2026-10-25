// Lớp cầu nối tay điều khiển RC (STM32 qua /dev/ttyAMA2) cho QML.
//
//  1. Giám sát: cần gạt, công tắc SA..SD, kênh CH1..CH8, raw ADC, sức khoẻ link
//     (fps / dropped) và cờ RC_STATE (calib, ELRS, cần lái → mLRS, phiên hiệu
//     chỉnh, chưa lưu) — chụp ~30 Hz từ ``rc::RcLink``.
//  2. Gán chế độ bay theo công tắc: mỗi vị trí công tắc ↔ một mode (hoặc trống).
//     Khi công tắc đổi vị trí (ổn định ≥ 60 ms) và tính năng đang BẬT, gửi
//     SET_MODE qua ``controller.commands()``. Bảng gán lưu ở
//     ~/.lite_gcs/rc_switches.json. Lần đọc đầu chỉ làm mốc — không tự đổi mode.
//  3. Hiệu chỉnh dùng phiên trên STM32 (PI_UI_AGENT_GUIDE.md §6.3): chọn kênh →
//     CAL_START → quét → (CAL_CENTER) → CAL_APPLY; trạng thái phiên đọc bằng
//     CAL_STATUS ~5 Hz. STM32 giữ hiệu chỉnh cũ cho tới khi áp dụng.
//  4. Cấu hình (§6.2, §6.4): giao cần lái cho Pi / radio, giá trị giữ, ELRS out,
//     đảo chiều, lưu / bỏ thay đổi / mặc định.
//
// Mọi lệnh STM32 chạy bất đồng bộ (không chặn luồng UI) và chỉ khi người dùng
// bấm — không bao giờ tự ghi theo timer. Mọi thay đổi có hiệu lực ngay trong RAM
// STM32; cờ ``dirty`` báo còn thay đổi chưa lưu flash.
#pragma once

#include "app/controller.h"
#include "rc/rc_link.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

#include <array>
#include <functional>

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

    // ── cờ RC_STATE ─────────────────────────────────────────────────────────
    Q_PROPERTY(bool calibLoaded READ calibLoaded NOTIFY flagsChanged)
    Q_PROPERTY(bool elrsOutput READ elrsOutput NOTIFY flagsChanged)
    Q_PROPERTY(bool stickCrsf READ stickCrsf NOTIFY flagsChanged)   // false = cần lái giao cho Pi
    Q_PROPERTY(bool calActive READ calActive NOTIFY flagsChanged)   // phiên hiệu chỉnh đang mở
    Q_PROPERTY(bool dirty READ dirty NOTIFY flagsChanged)           // RAM STM32 chưa lưu flash

    // ── trạng thái điều khiển ───────────────────────────────────────────────
    Q_PROPERTY(QVariantList controls READ controls NOTIFY stateChanged)   // 8 × µs (LH..SD)
    Q_PROPERTY(QVariantList channels READ channels NOTIFY stateChanged)   // 8 × µs (CH1..CH8)
    Q_PROPERTY(QVariantList rawAdc READ rawAdc NOTIFY stateChanged)       // 6 × raw (LH,LV,RH,RV,SB,SC)
    Q_PROPERTY(QVariantList switchPos READ switchPos NOTIFY switchesChanged) // 4 × 0/1/2
    Q_PROPERTY(QStringList channelSources READ channelSources NOTIFY configChanged) // CH1..8 → "RV"…
    Q_PROPERTY(QStringList controlNames READ controlNames CONSTANT) // "LH".."SD" = chỉ số nguồn

    // ── cấu hình đọc từ STM32 (-1 = chưa đọc được) ──────────────────────────
    Q_PROPERTY(QVariantList reversed READ reversed NOTIFY configChanged)  // 8 × 0/1
    Q_PROPERTY(QVariantList stickHold READ stickHold NOTIFY configChanged) // 4 × µs (LH,LV,RH,RV)
    Q_PROPERTY(int stickTrim READ stickTrim NOTIFY configChanged)         // 1 = trim cũ, 0 = 3 điểm

    // ── gán chế độ bay ──────────────────────────────────────────────────────
    Q_PROPERTY(bool modeSwitchEnabled READ modeSwitchEnabled WRITE setModeSwitchEnabled NOTIFY switchModesChanged)
    Q_PROPERTY(QVariantList switchModes READ switchModes NOTIFY switchModesChanged) // 4 × [3 × mode|""]
    Q_PROPERTY(QStringList availableModes READ availableModes CONSTANT)
    Q_PROPERTY(QString lastAction READ lastAction NOTIFY lastActionChanged)

    // hiển thị: đổi chỗ gimbal trái/phải
    Q_PROPERTY(bool gimbalSwap READ gimbalSwap WRITE setGimbalSwap NOTIFY gimbalSwapChanged)

    // ── hiệu chỉnh (phiên trên STM32) ───────────────────────────────────────
    Q_PROPERTY(int calMask READ calMask NOTIFY calChanged)             // kênh chọn để hiệu chỉnh
    Q_PROPERTY(int calRangeMask READ calRangeMask NOTIFY calChanged)   // kênh đang quét min/max
    Q_PROPERTY(int calCenterMask READ calCenterMask NOTIFY calChanged) // kênh đã chụp tâm
    Q_PROPERTY(QVariantList calValues READ calValues NOTIFY calChanged) // 18 phẳng: ch*3 + (0 min,1 mid,2 max) (CAL_STATUS)
    Q_PROPERTY(QVariantList storedCal READ storedCal NOTIFY configChanged) // 6 × [min,mid,max] | []
    Q_PROPERTY(bool calReady READ calReady NOTIFY calChanged)           // mọi kênh chọn quét ≥ 1000
    Q_PROPERTY(int calMinSpan READ calMinSpan CONSTANT)

    // ── thao tác ────────────────────────────────────────────────────────────
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)
    Q_PROPERTY(bool messageError READ messageError NOTIFY messageChanged)

public:
    RcViewModel(app::GcsController *controller, const QString &portName, QObject *parent = nullptr);

    QString portName() const { return m_link->portName(); }
    bool portOpen() const { return m_portOpen; }
    bool connected() const { return m_connected; }
    QString linkError() const { return m_link->lastError(); }
    int fps() const { return m_fps; }
    int dropped() const { return static_cast<int>(m_link->dropped()); }
    int badFrames() const { return static_cast<int>(m_link->badFrames()); }

    bool calibLoaded() const { return m_flags.calibLoaded; }
    bool elrsOutput() const { return m_flags.elrsOutput; }
    bool stickCrsf() const { return m_flags.stickCrsf; }
    bool calActive() const { return m_flags.calActive; }
    bool dirty() const { return m_flags.dirty; }

    QVariantList controls() const { return m_controls; }
    QVariantList channels() const { return m_channels; }
    QVariantList rawAdc() const { return m_rawAdc; }
    QVariantList switchPos() const { return toList(m_switchPos); }
    QStringList channelSources() const { return m_channelSources; }
    QStringList controlNames() const;

    QVariantList reversed() const { return toList(m_reverse); }
    QVariantList stickHold() const { return toList(m_stickHold); }
    int stickTrim() const { return m_stickTrim; }

    bool modeSwitchEnabled() const { return m_modeSwitchEnabled; }
    void setModeSwitchEnabled(bool on);
    QVariantList switchModes() const;
    QStringList availableModes() const;
    QString lastAction() const { return m_lastAction; }

    bool gimbalSwap() const { return m_gimbalSwap; }
    void setGimbalSwap(bool on);

    int calMask() const { return m_calMask; }
    int calRangeMask() const { return m_calRange; }
    int calCenterMask() const { return m_calCenter; }
    QVariantList calValues() const { return toList(m_calValues); }
    QVariantList storedCal() const;
    bool calReady() const;
    int calMinSpan() const;

    bool busy() const { return m_pending > 0; }
    QString message() const { return m_message; }
    bool messageError() const { return m_messageError; }

    // ── hành động từ QML ────────────────────────────────────────────────────
    Q_INVOKABLE void setSwitchMode(int sw, int pos, const QString &mode);

    Q_INVOKABLE void toggleCalChannel(int ch); // chọn/bỏ kênh (chỉ khi chưa mở phiên)
    Q_INVOKABLE void setCalMask(int mask);     // preset: 0x0F cần lái, 0x30 SB/SC, 0x3F tất cả
    Q_INVOKABLE void startCalibration();       // CAL_START(mask)
    Q_INVOKABLE void captureCenter();          // CAL_CENTER(mask ∩ cần lái)
    Q_INVOKABLE void applyCalibration();       // CAL_APPLY(save = 0) — RAM, cần LƯU
    Q_INVOKABLE void cancelCalibration();      // CAL_CANCEL

    Q_INVOKABLE void setReverse(int src, bool on);
    Q_INVOKABLE void setStickCrsf(bool viaRadio);
    Q_INVOKABLE void setStickHold(int src, int us);
    Q_INVOKABLE void setElrsOutput(bool on);
    Q_INVOKABLE void saveConfig();             // CONFIG_SAVE (flash)
    Q_INVOKABLE void reloadConfig();           // CONFIG_RELOAD (bỏ thay đổi chưa lưu)
    Q_INVOKABLE void resetDefaults(bool keepCal); // CONFIG_DEFAULTS (RAM, cần LƯU)
    Q_INVOKABLE void refreshConfig();          // đọc lại MAP / REVERSE / HOLD / TRIM / CAL

signals:
    void linkChanged();
    void healthChanged();
    void flagsChanged();
    void stateChanged();
    void switchesChanged();
    void configChanged();
    void switchModesChanged();
    void lastActionChanged();
    void gimbalSwapChanged();
    void calChanged();
    void busyChanged();
    void messageChanged();

private:
    using Done = std::function<void(int status, const QByteArray &value)>;

    template <size_t N>
    static QVariantList toList(const std::array<int, N> &a)
    {
        QVariantList out;
        for (int v : a)
            out.append(v);
        return out;
    }

    void poll();
    void pollCalStatus();
    void handleSwitches(const rc::RcState &st);
    void setMessage(const QString &msg, bool error = false);
    void exec(quint8 cmd, const QByteArray &data, int timeoutMs, Done done);
    void writeConfig(quint8 sub, const QByteArray &payload, const QString &what, Done onOk = {});
    static QString maskNames(int mask);
    void loadSwitchModes();
    void saveSwitchModes() const;

    app::GcsController *m_controller;
    rc::RcLink *m_link;
    QTimer *m_timer;
    QTimer *m_calTimer;

    bool m_portOpen = false;
    bool m_connected = false;
    int m_fps = 0;
    quint64 m_fpsFrames = 0;
    qint64 m_fpsT0 = 0;
    int m_lastSeq = -1;
    rc::RcState m_flags; // chỉ dùng các cờ

    QVariantList m_controls, m_channels, m_rawAdc;
    std::array<int, 4> m_switchPos{{-1, -1, -1, -1}};
    QStringList m_channelSources;
    std::array<int, 8> m_reverse{{-1, -1, -1, -1, -1, -1, -1, -1}};
    std::array<int, 4> m_stickHold{{-1, -1, -1, -1}};
    int m_stickTrim = -1;

    // công tắc → mode: m_modes[sw][pos]; vị trí "đã xác nhận" + ứng viên debounce
    std::array<std::array<QString, 3>, 4> m_modes;
    bool m_modeSwitchEnabled = false;
    std::array<int, 4> m_stablePos{{-1, -1, -1, -1}};
    std::array<int, 4> m_candPos{{-1, -1, -1, -1}};
    std::array<qint64, 4> m_candSince{};
    QString m_lastAction;
    bool m_gimbalSwap = false;

    int m_calMask = rc::kCalMaskSticks;
    int m_calRange = 0;
    int m_calCenter = 0;
    std::array<int, 18> m_calValues{}; // 6 × (min, mid, max)
    bool m_calStatusPending = false;
    std::array<std::array<int, 3>, 6> m_stored{};
    std::array<bool, 6> m_storedValid{};

    int m_pending = 0;
    QString m_message;
    bool m_messageError = false;
};

} // namespace gcs::bridge
