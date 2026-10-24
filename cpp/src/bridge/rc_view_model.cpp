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
#include <memory>

namespace gcs::bridge {

namespace {
constexpr int64_t kDebounceMs = 60;  // công tắc phải đứng yên ngần này mới tính là đổi
constexpr int kMinSpan = 300;         // hành trình min→max tối thiểu để một kênh được ghi
constexpr int kAutoSelectSpan = 1000; // hành trình min→max để kênh tự được chọn ghi
// Tâm chụp được nằm sát một đầu (< 15% hành trình) ⇒ cần không tự về giữa (ga):
// dùng điểm giữa min/max làm tâm thay vì báo lỗi.
constexpr double kEdgeCenterFrac = 0.15;
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
}

QVariantList RcViewModel::toList(const std::array<int, 6> &a)
{
    QVariantList out;
    for (int v : a)
        out.append(v);
    return out;
}

QVariantList RcViewModel::switchPos() const
{
    QVariantList out;
    for (int p : m_switchPos)
        out.append(p);
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

QVariantList RcViewModel::calUse() const
{
    QVariantList out;
    for (bool u : m_calUse)
        out.append(u);
    return out;
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
            // mất link: lần frame tới chỉ làm mốc, không coi là "gạt công tắc"
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
        for (int i = 0; i < 6; ++i) {
            m_rawAdc[i] = st->rawAdc[i];
            m_adcHist[i][m_histPos] = st->rawAdc[i];
        }
        m_histPos = (m_histPos + 1) % kHistLen;
        m_histCount = std::min(m_histCount + 1, kHistLen);
        emit stateChanged();

        if (st->calibLoaded != m_calibLoaded || st->elrsOutput != m_elrsOutput) {
            m_calibLoaded = st->calibLoaded;
            m_elrsOutput = st->elrsOutput;
            emit healthChanged();
        }

        if (m_calStep == 1) {
            for (int i = 0; i < 6; ++i) {
                m_calMin[i] = std::min(m_calMin[i], st->rawAdc[i]);
                m_calMax[i] = std::max(m_calMax[i], st->rawAdc[i]);
                // kênh được đẩy đủ xa ⇒ tự chọn để ghi (trừ khi người dùng đã tự chọn)
                if (!m_calManual[i] && m_calMax[i] - m_calMin[i] >= kAutoSelectSpan)
                    m_calUse[i] = true;
            }
            emit calChanged();
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

// ── gán chế độ ────────────────────────────────────────────────────────────────

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

// ── cấu hình STM32 (MAP + CAL + REVERSE) ──────────────────────────────────────

void RcViewModel::refreshConfig()
{
    for (quint8 ch = 0; ch < 8; ++ch) {
        m_link->configRead(rc::kSubMap, ch, [this, ch](int status, const QByteArray &v) {
            QString name = QStringLiteral("?");
            if (status == rc::kStatusOk && !v.isEmpty()) {
                const quint8 src = static_cast<quint8>(v[0]);
                name = src < rc::kControls.size() ? QLatin1String(rc::kControls[src]) : QStringLiteral("—");
            }
            if (m_channelSources[ch] != name) {
                m_channelSources[ch] = name;
                emit channelSourcesChanged();
            }
        });
    }
    for (quint8 src = 0; src < 8; ++src) {
        m_link->configRead(rc::kSubReverse, src, [this, src](int status, const QByteArray &v) {
            const int r = (status == rc::kStatusOk && !v.isEmpty()) ? (v[0] ? 1 : 0) : -1;
            if (m_reverse[src] != r) {
                m_reverse[src] = r;
                emit reverseChanged();
            }
        });
    }
    for (quint8 ch = 0; ch < 6; ++ch) {
        m_link->configRead(rc::kSubCal, ch, [this, ch](int status, const QByteArray &v) {
            m_storedValid[ch] = status == rc::kStatusOk && v.size() >= 6;
            if (m_storedValid[ch])
                m_stored[ch] = {u16le(v, 0), u16le(v, 2), u16le(v, 4)};
            emit calChanged();
        });
    }
}

// ── hiệu chỉnh ────────────────────────────────────────────────────────────────

void RcViewModel::setMessage(const QString &msg, bool error)
{
    m_calMessage = msg;
    m_calMessageError = error;
    emit calChanged();
}

bool RcViewModel::calChannelValid(int ch) const
{
    return m_calMax[ch] - m_calMin[ch] >= kMinSpan && m_calMin[ch] < m_calMid[ch] && m_calMid[ch] < m_calMax[ch];
}

QString RcViewModel::calDetail(int ch) const
{
    return QStringLiteral("%1 (min %2 · tâm %3 · max %4)")
        .arg(QLatin1String(rc::kAdcChannels[ch]))
        .arg(m_calMin[ch])
        .arg(m_calMid[ch])
        .arg(m_calMax[ch]);
}

QStringList RcViewModel::selectedCalNames() const
{
    QStringList out;
    for (int i = 0; i < 6; ++i)
        if (m_calUse[i])
            out << QLatin1String(rc::kAdcChannels[i]);
    return out;
}

void RcViewModel::startCalibration()
{
    if (!m_connected || m_busy) {
        setMessage(QStringLiteral("Chưa có tín hiệu từ tay điều khiển."), true);
        return;
    }
    for (int i = 0; i < 6; ++i) {
        const int v = m_rawAdc[i].toInt();
        m_calMin[i] = m_calMax[i] = m_calMid[i] = v;
    }
    m_calUse.fill(false);
    m_calManual.fill(false);
    m_calStep = 1;
    setMessage(QStringLiteral("Chỉ đẩy hết hành trình những cần/công tắc cần hiệu chỉnh — kênh nào đủ hành trình "
                              "tự được chọn (chạm hàng để bật/tắt). Xong đưa về TÂM / MID rồi bấm ĐẶT TÂM."),
               false);
}

void RcViewModel::toggleCalChannel(int ch)
{
    if (ch < 0 || ch > 5 || m_calStep == 0 || m_busy)
        return;
    if (m_calStep == 2 && !m_calUse[ch] && !calChannelValid(ch)) {
        setMessage(QStringLiteral("Hành trình quá ngắn: %1 — không thể chọn. HUỶ rồi làm lại nếu cần.")
                       .arg(calDetail(ch)),
                   true);
        return;
    }
    m_calUse[ch] = !m_calUse[ch];
    m_calManual[ch] = true;
    emit calChanged();
}

void RcViewModel::captureCenter()
{
    if (m_calStep != 1)
        return;
    if (selectedCalNames().isEmpty()) {
        setMessage(QStringLiteral("Chưa chọn kênh nào — đẩy cần cần hiệu chỉnh hoặc chạm hàng để chọn."), true);
        return;
    }

    // tâm = trung bình vài frame gần nhất (lọc nhiễu / cần còn rung khi vừa thả)
    QStringList midpointUsed;
    for (int i = 0; i < 6; ++i) {
        int sum = 0;
        for (int k = 0; k < m_histCount; ++k)
            sum += m_adcHist[i][k];
        const int raw = m_histCount > 0 ? sum / m_histCount : m_rawAdc[i].toInt();
        // trung bình có thể lệch khỏi min/max vừa ghi một chút — kẹp lại
        m_calMin[i] = std::min(m_calMin[i], raw);
        m_calMax[i] = std::max(m_calMax[i], raw);
        m_calMid[i] = raw;

        const int span = m_calMax[i] - m_calMin[i];
        const int edge = static_cast<int>(span * kEdgeCenterFrac);
        if (span >= kMinSpan && (raw - m_calMin[i] < edge || m_calMax[i] - raw < edge)) {
            m_calMid[i] = (m_calMin[i] + m_calMax[i]) / 2;
            if (m_calUse[i])
                midpointUsed << QLatin1String(rc::kAdcChannels[i]);
        }
    }

    QStringList ok, bad;
    for (int i = 0; i < 6; ++i) {
        if (!m_calUse[i])
            continue;
        (calChannelValid(i) ? ok : bad) << calDetail(i);
        qInfo("[rc] calib %s valid=%d", qPrintable(calDetail(i)), int(calChannelValid(i)));
    }
    if (ok.isEmpty()) {
        setMessage(QStringLiteral("Hành trình quá ngắn (cần ≥ %1): %2 — đẩy tiếp hết hai đầu rồi bấm ĐẶT TÂM lại.")
                       .arg(kMinSpan)
                       .arg(bad.join(QStringLiteral("; "))),
                   true);
        return; // vẫn ở bước 1, tiếp tục ghi min/max
    }
    // kênh hỏng bị bỏ qua thay vì chặn cả lượt
    for (int i = 0; i < 6; ++i)
        if (m_calUse[i] && !calChannelValid(i))
            m_calUse[i] = false;

    m_calStep = 2;
    QString msg = QStringLiteral("Đã chụp tâm. Sẽ ghi: %1 (kênh khác giữ nguyên).")
                      .arg(selectedCalNames().join(QStringLiteral(", ")));
    if (!midpointUsed.isEmpty())
        msg += QStringLiteral(" Tâm %1 nằm sát đầu → dùng điểm giữa hành trình.")
                   .arg(midpointUsed.join(QStringLiteral(", ")));
    if (!bad.isEmpty())
        msg += QStringLiteral(" Bỏ qua (hành trình ngắn): %1.").arg(bad.join(QStringLiteral("; ")));
    msg += QStringLiteral(" Bấm GHI để áp dụng.");
    setMessage(msg, !bad.isEmpty());
}

void RcViewModel::cancelCalibration()
{
    m_calStep = 0;
    m_calUse.fill(false);
    setMessage(QStringLiteral("Đã huỷ hiệu chỉnh — không ghi gì xuống STM32."), false);
}

void RcViewModel::applyCalibration()
{
    if (m_calStep != 2 || m_busy)
        return;
    const QStringList names = selectedCalNames();
    if (names.isEmpty()) {
        setMessage(QStringLiteral("Chưa chọn kênh nào để ghi."), true);
        return;
    }
    m_busy = true;
    setMessage(QStringLiteral("Đang ghi hiệu chỉnh %1…").arg(names.join(QStringLiteral(", "))), false);

    auto pending = std::make_shared<int>(names.size());
    auto failed = std::make_shared<QStringList>();
    for (quint8 ch = 0; ch < 6; ++ch) {
        if (!m_calUse[ch])
            continue;
        QByteArray payload(1, static_cast<char>(ch));
        putU16le(payload, m_calMin[ch]);
        putU16le(payload, m_calMid[ch]);
        putU16le(payload, m_calMax[ch]);
        m_link->configWrite(rc::kSubCal, payload, [this, ch, pending, failed, names](int status, const QByteArray &) {
            if (status != rc::kStatusOk)
                *failed << QStringLiteral("%1:%2").arg(QLatin1String(rc::kAdcChannels[ch]), rc::statusName(status));
            if (--*pending > 0)
                return;
            m_busy = false;
            if (failed->isEmpty()) {
                m_calStep = 0;
                m_calUse.fill(false);
                setMessage(QStringLiteral("Đã ghi %1 vào RAM STM32. Bấm LƯU FLASH để giữ sau khi tắt nguồn.")
                               .arg(names.join(QStringLiteral(", "))),
                           false);
            } else {
                setMessage(QStringLiteral("Ghi lỗi: %1").arg(failed->join(QStringLiteral(", "))), true);
            }
            refreshConfig();
        });
    }
}

QVariantList RcViewModel::reversed() const
{
    QVariantList out;
    for (int r : m_reverse)
        out.append(r);
    return out;
}

void RcViewModel::setReverse(int src, bool on)
{
    if (src < 0 || src > 7 || m_busy || !m_connected)
        return;
    m_busy = true;
    const QString name = QLatin1String(rc::kControls[src]);
    QByteArray payload(1, static_cast<char>(src));
    payload.append(static_cast<char>(on ? 1 : 0));
    m_link->configWrite(rc::kSubReverse, payload, [this, src, on, name](int status, const QByteArray &) {
        m_busy = false;
        if (status == rc::kStatusOk) {
            m_reverse[src] = on ? 1 : 0;
            emit reverseChanged();
            setMessage(QStringLiteral("%1: %2 (RAM, có hiệu lực ngay). Bấm LƯU FLASH để giữ.")
                           .arg(name, on ? QStringLiteral("ĐẢO chiều") : QStringLiteral("chiều thường")),
                       false);
        } else {
            setMessage(QStringLiteral("Đảo chiều %1 lỗi: %2").arg(name, rc::statusName(status)), true);
        }
    });
}

void RcViewModel::saveConfig()
{
    if (m_busy)
        return;
    m_busy = true;
    setMessage(QStringLiteral("Đang lưu cấu hình xuống flash STM32…"), false);
    m_link->configSave([this](int status, const QByteArray &) {
        m_busy = false;
        if (status == rc::kStatusOk)
            setMessage(QStringLiteral("Đã lưu cấu hình xuống flash."), false);
        else
            setMessage(QStringLiteral("Lưu flash thất bại: %1").arg(rc::statusName(status)), true);
    });
}

} // namespace gcs::bridge
