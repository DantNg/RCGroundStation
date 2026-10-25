#include "bridge/rc_view_model.h"

#include "domain/telemetry.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

#include <algorithm>

namespace gcs::bridge {

namespace {
constexpr int64_t kDebounceMs = 60;   // công tắc phải đứng yên ngần này mới tính là đổi
constexpr int kCalMinSpan = 1000;     // STM32 từ chối kênh quét < 1000 count (§6.3)
constexpr int kCalPollMs = 200;       // CAL_STATUS ~5 Hz khi phiên mở
constexpr int kAckMs = 500;
constexpr int kFlashMs = 2000;        // lệnh có ghi flash
const char *const kPosNames[3] = {"LOW", "MID", "HIGH"};

QString switchesPath()
{
    return QDir::homePath() + QStringLiteral("/.lite_gcs/rc_switches.json");
}

void putU16le(QByteArray &b, int v)
{
    b.append(static_cast<char>(v & 0xFF));
    b.append(static_cast<char>((v >> 8) & 0xFF));
}

int u16le(const QByteArray &d, int off)
{
    return static_cast<quint8>(d[off]) | (static_cast<quint8>(d[off + 1]) << 8);
}
} // namespace

RcViewModel::RcViewModel(app::GcsController *controller, const QString &portName, QObject *parent)
    : QObject(parent), m_controller(controller)
{
    for (int i = 0; i < 8; ++i) {
        m_controls.append(1500);
        m_channels.append(1500);
        m_channelSources.append(QStringLiteral("—"));
    }
    for (int i = 0; i < 6; ++i)
        m_rawAdc.append(0);

    loadSwitchModes();

    m_link = new rc::RcLink(this);
    m_link->start(portName);

    m_timer = new QTimer(this);
    m_timer->setInterval(33); // ~30 Hz, đủ cho hiển thị (dữ liệu vào 50 Hz)
    connect(m_timer, &QTimer::timeout, this, &RcViewModel::poll);
    m_timer->start();
    m_fpsT0 = domain::nowMs();

    m_calTimer = new QTimer(this);
    m_calTimer->setInterval(kCalPollMs);
    connect(m_calTimer, &QTimer::timeout, this, &RcViewModel::pollCalStatus);
}

QStringList RcViewModel::controlNames() const
{
    QStringList out;
    for (const char *n : rc::kControls)
        out << QLatin1String(n);
    return out;
}

QVariantList RcViewModel::switchModes() const
{
    QVariantList out;
    for (const auto &sw : m_modes) {
        QVariantList row;
        for (const QString &m : sw)
            row.append(m);
        out.append(QVariant(row));
    }
    return out;
}

QStringList RcViewModel::availableModes() const
{
    return {QStringLiteral("STABILIZE"), QStringLiteral("ALT_HOLD"), QStringLiteral("LOITER"),
            QStringLiteral("POSHOLD"),   QStringLiteral("RTL"),      QStringLiteral("SMART_RTL"),
            QStringLiteral("LAND"),      QStringLiteral("AUTO"),     QStringLiteral("GUIDED"),
            QStringLiteral("BRAKE"),     QStringLiteral("ACRO"),     QStringLiteral("SPORT"),
            QStringLiteral("DRIFT"),     QStringLiteral("CIRCLE")};
}

QVariantList RcViewModel::storedCal() const
{
    QVariantList out;
    for (int i = 0; i < 6; ++i) {
        QVariantList row;
        if (m_storedValid[i])
            row << m_stored[i][0] << m_stored[i][1] << m_stored[i][2];
        out.append(QVariant(row));
    }
    return out;
}

int RcViewModel::calMinSpan() const
{
    return kCalMinSpan;
}

bool RcViewModel::calReady() const
{
    // ÁP DỤNG chỉ bật khi mọi kênh đã chọn quét đủ ≥ 1000 count
    if (!m_flags.calActive || m_calMask == 0)
        return false;
    for (int ch = 0; ch < 6; ++ch) {
        if (!(m_calMask & (1 << ch)))
            continue;
        if (!(m_calRange & (1 << ch)) || m_calValues[ch * 3 + 2] - m_calValues[ch * 3] < kCalMinSpan)
            return false;
    }
    return true;
}

QString RcViewModel::maskNames(int mask)
{
    QStringList out;
    for (int ch = 0; ch < 6; ++ch)
        if (mask & (1 << ch))
            out << QLatin1String(rc::kAdcChannels[ch]);
    return out.isEmpty() ? QStringLiteral("—") : out.join(QStringLiteral(", "));
}

// ── chu kỳ cập nhật ───────────────────────────────────────────────────────────

void RcViewModel::poll()
{
    const int64_t now = domain::nowMs();

    const bool open = m_link->isOpen();
    const bool conn = m_link->connected();
    if (open != m_portOpen || conn != m_connected) {
        const bool cameUp = conn && !m_connected;
        m_portOpen = open;
        m_connected = conn;
        if (!conn) {
            // mất link (hoặc STM32 tạm dừng khi ghi flash): lần frame tới chỉ làm mốc
            m_stablePos.fill(-1);
            m_candPos.fill(-1);
        }
        emit linkChanged();
        if (cameUp)
            refreshConfig();
    }

    if (now - m_fpsT0 >= 1000) {
        const quint64 frames = m_link->frames();
        m_fps = static_cast<int>((frames - m_fpsFrames) * 1000 / (now - m_fpsT0));
        m_fpsFrames = frames;
        m_fpsT0 = now;
        emit healthChanged();
    }

    const auto &st = m_link->state();
    if (!st || !conn)
        return;

    if (st->seq != m_lastSeq) {
        m_lastSeq = st->seq;
        for (int i = 0; i < 8; ++i) {
            m_controls[i] = st->controlsUs[i];
            m_channels[i] = st->channelsUs[i];
        }
        for (int i = 0; i < 6; ++i)
            m_rawAdc[i] = st->rawAdc[i];
        emit stateChanged();

        if (st->calibLoaded != m_flags.calibLoaded || st->elrsOutput != m_flags.elrsOutput
            || st->stickCrsf != m_flags.stickCrsf || st->calActive != m_flags.calActive
            || st->dirty != m_flags.dirty) {
            const bool calOpened = !m_flags.calActive && st->calActive;
            const bool calClosed = m_flags.calActive && !st->calActive;
            m_flags = *st;
            emit flagsChanged();
            if (calOpened) {
                m_calTimer->start();
                pollCalStatus();
            } else if (calClosed) {
                m_calTimer->stop();
                m_calRange = m_calCenter = 0;
                refreshConfig(); // đọc lại hiệu chỉnh đang dùng
            }
            emit calChanged(); // calReady phụ thuộc calActive
        }
    }

    handleSwitches(*st);
}

void RcViewModel::handleSwitches(const rc::RcState &st)
{
    const int64_t now = domain::nowMs();
    bool displayChanged = false;

    for (int i = 0; i < 4; ++i) {
        const int p = st.switchPos[i];
        if (p != m_switchPos[i]) {
            m_switchPos[i] = p;
            displayChanged = true;
        }
        if (p != m_candPos[i]) {
            m_candPos[i] = p;
            m_candSince[i] = now;
        }
        if (now - m_candSince[i] < kDebounceMs || p == m_stablePos[i] || p < 0 || p > 2)
            continue;

        const bool baseline = m_stablePos[i] < 0;
        m_stablePos[i] = p;
        if (baseline || !m_modeSwitchEnabled)
            continue;

        const QString &mode = m_modes[i][p];
        if (mode.isEmpty())
            continue;
        const QString what = QStringLiteral("%1 → %2 ⇒ %3")
                                 .arg(QLatin1String(rc::kSwitches[i]), QLatin1String(kPosNames[p]), mode);
        if (m_controller->isConnected()) {
            m_controller->commands().setModeByName(mode);
            m_lastAction = what;
        } else {
            m_lastAction = what + QStringLiteral(" (chưa kết nối MAVLink)");
        }
        emit lastActionChanged();
    }

    if (displayChanged)
        emit switchesChanged();
}

// ── gán chế độ / hiển thị ─────────────────────────────────────────────────────

void RcViewModel::setModeSwitchEnabled(bool on)
{
    if (on == m_modeSwitchEnabled)
        return;
    m_modeSwitchEnabled = on;
    // bật lại: vị trí hiện tại là mốc mới, không nhảy mode ngay lập tức
    m_stablePos = m_switchPos;
    saveSwitchModes();
    emit switchModesChanged();
}

void RcViewModel::setGimbalSwap(bool on)
{
    if (on == m_gimbalSwap)
        return;
    m_gimbalSwap = on;
    saveSwitchModes();
    emit gimbalSwapChanged();
}

void RcViewModel::setSwitchMode(int sw, int pos, const QString &mode)
{
    if (sw < 0 || sw > 3 || pos < 0 || pos > 2)
        return;
    m_modes[sw][pos] = mode.trimmed().toUpper();
    saveSwitchModes();
    emit switchModesChanged();
}

void RcViewModel::loadSwitchModes()
{
    m_modeSwitchEnabled = true;
    QFile f(switchesPath());
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    m_modeSwitchEnabled = o.value("enabled").toBool(true);
    m_gimbalSwap = o.value("gimbal_swap").toBool(false);
    const QJsonObject modes = o.value("modes").toObject();
    for (int i = 0; i < 4; ++i) {
        const QJsonArray arr = modes.value(QLatin1String(rc::kSwitches[i])).toArray();
        for (int p = 0; p < 3 && p < arr.size(); ++p)
            m_modes[i][p] = arr.at(p).toString();
    }
}

void RcViewModel::saveSwitchModes() const
{
    QJsonObject modes;
    for (int i = 0; i < 4; ++i) {
        QJsonArray arr;
        for (const QString &m : m_modes[i])
            arr.append(m);
        modes[QLatin1String(rc::kSwitches[i])] = arr;
    }
    QJsonObject o;
    o["enabled"] = m_modeSwitchEnabled;
    o["modes"] = modes;
    o["gimbal_swap"] = m_gimbalSwap;
    QDir().mkpath(QFileInfo(switchesPath()).absolutePath());
    QFile f(switchesPath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
}

// ── lệnh STM32 ────────────────────────────────────────────────────────────────

void RcViewModel::setMessage(const QString &msg, bool error)
{
    m_message = msg;
    m_messageError = error;
    emit messageChanged();
}

void RcViewModel::exec(quint8 cmd, const QByteArray &data, int timeoutMs, Done done)
{
    ++m_pending;
    emit busyChanged();
    m_link->command(cmd, data, timeoutMs, [this, done = std::move(done)](int status, const QByteArray &v) {
        --m_pending;
        emit busyChanged();
        if (done)
            done(status, v);
    });
}

void RcViewModel::writeConfig(quint8 sub, const QByteArray &payload, const QString &what, Done onOk)
{
    if (!m_connected)
        return setMessage(QStringLiteral("Chưa có tín hiệu từ tay điều khiển."), true);
    exec(rc::kCmdConfigWrite, QByteArray(1, static_cast<char>(sub)) + payload, kAckMs,
         [this, what, onOk = std::move(onOk)](int status, const QByteArray &v) {
             if (status != rc::kStatusOk)
                 return setMessage(QStringLiteral("%1 lỗi: %2").arg(what, rc::statusName(status)), true);
             setMessage(QStringLiteral("%1 — có hiệu lực ngay (RAM). Bấm LƯU để giữ sau khi tắt nguồn.").arg(what));
             if (onOk)
                 onOk(status, v);
         });
}

void RcViewModel::refreshConfig()
{
    for (quint8 ch = 0; ch < 8; ++ch) {
        m_link->configRead(rc::kSubMap, ch, [this, ch](int status, const QByteArray &v) {
            QString name = QStringLiteral("?");
            if (status == rc::kStatusOk && !v.isEmpty()) {
                const quint8 src = static_cast<quint8>(v[0]);
                name = src < rc::kControls.size() ? QLatin1String(rc::kControls[src]) : QStringLiteral("—");
            }
            m_channelSources[ch] = name;
            emit configChanged();
        });
    }
    for (quint8 src = 0; src < 8; ++src) {
        m_link->configRead(rc::kSubReverse, src, [this, src](int status, const QByteArray &v) {
            m_reverse[src] = (status == rc::kStatusOk && !v.isEmpty()) ? (v[0] ? 1 : 0) : -1;
            emit configChanged();
        });
    }
    for (quint8 src = 0; src < 4; ++src) {
        m_link->configRead(rc::kSubStickHold, src, [this, src](int status, const QByteArray &v) {
            m_stickHold[src] = (status == rc::kStatusOk && v.size() >= 2) ? u16le(v, 0) : -1;
            emit configChanged();
        });
    }
    m_link->configRead(rc::kSubStickTrim, std::nullopt, [this](int status, const QByteArray &v) {
        m_stickTrim = (status == rc::kStatusOk && !v.isEmpty()) ? (v[0] ? 1 : 0) : -1;
        emit configChanged();
    });
    for (quint8 ch = 0; ch < 6; ++ch) {
        m_link->configRead(rc::kSubCal, ch, [this, ch](int status, const QByteArray &v) {
            m_storedValid[ch] = status == rc::kStatusOk && v.size() >= 6;
            if (m_storedValid[ch])
                m_stored[ch] = {u16le(v, 0), u16le(v, 2), u16le(v, 4)};
            emit configChanged();
        });
    }
}

void RcViewModel::setReverse(int src, bool on)
{
    if (src < 0 || src > 7)
        return;
    QByteArray payload(1, static_cast<char>(src));
    payload.append(static_cast<char>(on ? 1 : 0));
    writeConfig(rc::kSubReverse, payload,
                QStringLiteral("%1: %2").arg(QLatin1String(rc::kControls[src]),
                                             on ? QStringLiteral("ĐẢO chiều") : QStringLiteral("chiều thường")),
                [this, src, on](int, const QByteArray &) {
                    m_reverse[src] = on ? 1 : 0;
                    emit configChanged();
                });
}

void RcViewModel::setStickCrsf(bool viaRadio)
{
    // trạng thái thật lấy từ cờ RC_STATE (stick_crsf), không giữ bản sao riêng
    writeConfig(rc::kSubStickCrsf, QByteArray(1, static_cast<char>(viaRadio ? 1 : 0)),
                viaRadio ? QStringLiteral("Cần lái → drone qua RADIO (mLRS)")
                         : QStringLiteral("Cần lái → PI (drone nhận giá trị giữ)"));
}

void RcViewModel::setStickHold(int src, int us)
{
    if (src < 0 || src > 3)
        return;
    if (us < 1000 || us > 2000)
        return setMessage(QStringLiteral("Giá trị giữ phải trong 1000..2000 µs."), true);
    QByteArray payload(1, static_cast<char>(src));
    putU16le(payload, us);
    writeConfig(rc::kSubStickHold, payload,
                QStringLiteral("Giá trị giữ %1 = %2 µs").arg(QLatin1String(rc::kControls[src])).arg(us),
                [this, src, us](int, const QByteArray &) {
                    m_stickHold[src] = us;
                    emit configChanged();
                });
}

void RcViewModel::setElrsOutput(bool on)
{
    writeConfig(rc::kSubElrsOut, QByteArray(1, static_cast<char>(on ? 1 : 0)),
                on ? QStringLiteral("BẬT xuất RC ra radio") : QStringLiteral("TẮT xuất RC ra radio (drone failsafe)"));
}

void RcViewModel::saveConfig()
{
    if (!m_connected)
        return setMessage(QStringLiteral("Chưa có tín hiệu từ tay điều khiển."), true);
    setMessage(QStringLiteral("Đang lưu cấu hình xuống flash STM32…"));
    exec(rc::kCmdConfigSave, {}, kFlashMs, [this](int status, const QByteArray &) {
        if (status == rc::kStatusOk)
            setMessage(QStringLiteral("Đã lưu cấu hình xuống flash."));
        else
            setMessage(QStringLiteral("Lưu flash thất bại: %1").arg(rc::statusName(status)), true);
    });
}

void RcViewModel::reloadConfig()
{
    if (!m_connected)
        return setMessage(QStringLiteral("Chưa có tín hiệu từ tay điều khiển."), true);
    exec(rc::kCmdConfigReload, {}, kAckMs, [this](int status, const QByteArray &) {
        if (status == rc::kStatusOk)
            setMessage(QStringLiteral("Đã bỏ thay đổi chưa lưu (nạp lại từ flash)."));
        else
            setMessage(QStringLiteral("Nạp lại thất bại: %1").arg(rc::statusName(status)), true);
        refreshConfig();
    });
}

void RcViewModel::resetDefaults(bool keepCal)
{
    if (!m_connected)
        return setMessage(QStringLiteral("Chưa có tín hiệu từ tay điều khiển."), true);
    exec(rc::kCmdConfigDefaults, QByteArray(1, static_cast<char>(keepCal ? 1 : 0)), kAckMs,
         [this, keepCal](int status, const QByteArray &) {
             if (status == rc::kStatusOk)
                 setMessage(keepCal ? QStringLiteral("Đã về mặc định (giữ hiệu chỉnh) trong RAM. Bấm LƯU để giữ.")
                                    : QStringLiteral("Đã về mặc định (xoá hiệu chỉnh) trong RAM. Bấm LƯU để giữ."));
             else
                 setMessage(QStringLiteral("Về mặc định thất bại: %1").arg(rc::statusName(status)), true);
             refreshConfig();
         });
}

// ── hiệu chỉnh (phiên STM32) ──────────────────────────────────────────────────

void RcViewModel::toggleCalChannel(int ch)
{
    if (ch < 0 || ch > 5 || m_flags.calActive)
        return;
    m_calMask ^= (1 << ch);
    emit calChanged();
}

void RcViewModel::setCalMask(int mask)
{
    if (m_flags.calActive)
        return;
    m_calMask = mask & rc::kCalMaskAll;
    emit calChanged();
}

void RcViewModel::startCalibration()
{
    if (!m_connected)
        return setMessage(QStringLiteral("Chưa có tín hiệu từ tay điều khiển."), true);
    if (m_calMask == 0)
        return setMessage(QStringLiteral("Chưa chọn kênh nào để hiệu chỉnh."), true);
    const int mask = m_calMask;
    exec(rc::kCmdCalStart, QByteArray(1, static_cast<char>(mask)), kAckMs, [this, mask](int status, const QByteArray &) {
        if (status != rc::kStatusOk)
            return setMessage(QStringLiteral("Bắt đầu hiệu chỉnh lỗi: %1").arg(rc::statusName(status)), true);
        QString msg = QStringLiteral("Đang hiệu chỉnh %1: ").arg(maskNames(mask));
        if (mask & rc::kCalMaskSticks)
            msg += QStringLiteral("xoay cần hết vòng vài lần, thả về giữa rồi bấm CHỤP TÂM. ");
        if (mask & rc::kCalMaskSwitches)
            msg += QStringLiteral("gạt SB/SC qua cả 3 vị trí. ");
        msg += QStringLiteral("ÁP DỤNG sáng khi mọi kênh quét ≥ %1.").arg(kCalMinSpan);
        setMessage(msg);
        m_calTimer->start();
        pollCalStatus();
    });
}

void RcViewModel::captureCenter()
{
    // chỉ chụp tâm cho cần lái — không dùng cho công tắc SB/SC (§6.3)
    const int mask = m_calMask & rc::kCalMaskSticks;
    if (!m_connected || mask == 0)
        return;
    exec(rc::kCmdCalCenter, QByteArray(1, static_cast<char>(mask)), kAckMs, [this, mask](int status, const QByteArray &) {
        if (status != rc::kStatusOk)
            return setMessage(QStringLiteral("Chụp tâm lỗi: %1").arg(rc::statusName(status)), true);
        setMessage(QStringLiteral("Đã chụp tâm %1 (trung bình 16 mẫu).").arg(maskNames(mask)));
        pollCalStatus();
    });
}

void RcViewModel::applyCalibration()
{
    if (!m_connected)
        return;
    // save = 0: áp dụng vào RAM, người dùng tự bấm LƯU (lưu flash làm STM32 dừng thoáng chốc)
    exec(rc::kCmdCalApply, QByteArray(1, '\0'), kFlashMs, [this](int status, const QByteArray &v) {
        const int mask = v.isEmpty() ? 0 : static_cast<quint8>(v[0]);
        if (status == rc::kStatusOk) {
            setMessage(QStringLiteral("Đã áp dụng hiệu chỉnh %1 (RAM). Bấm LƯU để giữ sau khi tắt nguồn.")
                           .arg(maskNames(mask)));
            refreshConfig();
        } else if (status == rc::kStatusRange) {
            setMessage(QStringLiteral("Chưa đạt: %1 — quét lại hết hành trình (≥ %2) rồi ÁP DỤNG, hoặc HUỶ. "
                                      "Chưa có gì được áp dụng.")
                           .arg(maskNames(mask))
                           .arg(kCalMinSpan),
                       true);
        } else {
            setMessage(QStringLiteral("Áp dụng lỗi: %1").arg(rc::statusName(status)), true);
        }
    });
}

void RcViewModel::cancelCalibration()
{
    if (!m_connected)
        return;
    exec(rc::kCmdCalCancel, {}, kAckMs, [this](int status, const QByteArray &) {
        if (status == rc::kStatusOk)
            setMessage(QStringLiteral("Đã huỷ hiệu chỉnh — STM32 giữ nguyên hiệu chỉnh cũ."));
        else
            setMessage(QStringLiteral("Huỷ lỗi: %1").arg(rc::statusName(status)), true);
    });
}

void RcViewModel::pollCalStatus()
{
    // chỉ đọc (không ghi) — một yêu cầu một lúc để không dồn hàng đợi
    if (m_calStatusPending || !m_link->isOpen())
        return;
    m_calStatusPending = true;
    m_link->command(rc::kCmdCalStatus, {}, kAckMs, [this](int status, const QByteArray &v) {
        m_calStatusPending = false;
        // [active][range_mask][center_mask][6 × (min,mid,max) u16 LE]
        if (status != rc::kStatusOk || v.size() < 3 + 36)
            return;
        m_calRange = static_cast<quint8>(v[1]);
        m_calCenter = static_cast<quint8>(v[2]);
        for (int i = 0; i < 18; ++i)
            m_calValues[i] = u16le(v, 3 + 2 * i);
        // phiên mở từ trước (vd app vừa khởi động lại): chọn luôn các kênh đang quét
        if (v[0])
            m_calMask |= m_calRange;
        emit calChanged();
    });
}

} // namespace gcs::bridge
