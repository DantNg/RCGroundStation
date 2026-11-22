#include "net/wifi_manager.h"

#include <QProcess>
#include <QStandardPaths>
#include <QTimer>
#include <QVariantMap>

#include <algorithm>
#include <memory>

namespace gcs::net {

namespace {
constexpr int kPollMs = 5000;
constexpr int kScanWaitMs = 4000;     // wpa_cli: chờ kết quả quét
constexpr int kConnectTimeoutS = 30;  // nmcli --wait
constexpr int kWpaAssocTries = 20;    // × 1 s chờ wpa_state=COMPLETED

// nmcli -t: trường cách nhau bởi ':', trong trường thì '\:' và '\\'
QStringList splitTerse(const QString &line)
{
    QStringList out;
    QString cur;
    for (int i = 0; i < line.size(); ++i) {
        const QChar c = line[i];
        if (c == QLatin1Char('\\') && i + 1 < line.size()) {
            cur += line[++i];
        } else if (c == QLatin1Char(':')) {
            out << cur;
            cur.clear();
        } else {
            cur += c;
        }
    }
    out << cur;
    return out;
}

QString findTool(const QString &name)
{
    QString p = QStandardPaths::findExecutable(name);
    if (p.isEmpty()) // PATH của user desktop thường thiếu sbin
        p = QStandardPaths::findExecutable(name, {QStringLiteral("/usr/sbin"), QStringLiteral("/sbin"),
                                                  QStringLiteral("/usr/bin"), QStringLiteral("/bin")});
    return p;
}

QString firstLine(const QString &s)
{
    return s.trimmed().section(QLatin1Char('\n'), 0, 0).trimmed();
}

// wpa_supplicant báo dBm — quy đổi thô sang 0..100 như nmcli
int dbmToPercent(int dbm)
{
    return std::clamp(2 * (dbm + 100), 0, 100);
}
} // namespace

WifiManager::WifiManager(QObject *parent) : QObject(parent)
{
#ifdef Q_OS_LINUX
    if (!(m_program = findTool(QStringLiteral("nmcli"))).isEmpty()) {
        m_backend = Backend::Nmcli;
    } else if (!(m_program = findTool(QStringLiteral("wpa_cli"))).isEmpty()) {
        m_backend = Backend::WpaCli;
        m_iface = qEnvironmentVariable("LITEGCS_WIFI_IF", QStringLiteral("wlan0"));
    }
#endif
    m_poll = new QTimer(this);
    m_poll->setInterval(kPollMs);
    connect(m_poll, &QTimer::timeout, this, &WifiManager::refreshStatus);
}

QString WifiManager::backendName() const
{
    switch (m_backend) {
    case Backend::Nmcli: return QStringLiteral("nmcli");
    case Backend::WpaCli: return QStringLiteral("wpa_cli");
    default: return {};
    }
}

void WifiManager::setActive(bool on)
{
    if (on == m_active)
        return;
    m_active = on;
    emit activeChanged();
    if (!available())
        return;
    if (on) {
        refreshStatus();
        scan();
        m_poll->start();
    } else {
        m_poll->stop();
    }
}

void WifiManager::run(const QString &program, const QStringList &args, int timeoutMs, Done done)
{
    auto *proc = new QProcess(this);
    auto fired = std::make_shared<bool>(false);
    auto finish = [proc, fired, done](int code, const QString &err) {
        if (*fired)
            return;
        *fired = true;
        const QString out = QString::fromUtf8(proc->readAllStandardOutput());
        const QString e = err.isEmpty() ? QString::fromUtf8(proc->readAllStandardError()) : err;
        proc->deleteLater();
        if (done)
            done(code, out, e);
    };
    connect(proc, &QProcess::finished, this, [finish](int code, QProcess::ExitStatus st) {
        finish(st == QProcess::NormalExit ? code : -1, {});
    });
    connect(proc, &QProcess::errorOccurred, this, [proc, finish](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart)
            finish(-1, proc->errorString());
    });
    QTimer::singleShot(timeoutMs, proc, [proc] { proc->kill(); });
    proc->start(program, args);
}

void WifiManager::setMessage(const QString &text, bool error)
{
    m_message = text;
    m_messageError = error;
    emit messageChanged();
}

void WifiManager::setStatus(const QString &ssid, const QString &ip, int signal)
{
    if (ssid == m_currentSsid && ip == m_ip && signal == m_signal)
        return;
    m_currentSsid = ssid;
    m_ip = ip;
    m_signal = signal;
    emit statusChanged();
}

// gộp theo SSID (nhiều AP cùng tên), đánh dấu đang dùng / đã lưu, xếp:
// đang dùng → mạnh → yếu
void WifiManager::publishNetworks(QVariantList list)
{
    QVariantList merged;
    for (const QVariant &v : list) {
        QVariantMap n = v.toMap();
        const QString ssid = n.value(QStringLiteral("ssid")).toString();
        if (ssid.isEmpty())
            continue; // mạng ẩn
        n.insert(QStringLiteral("known"), m_known.contains(ssid));
        auto it = std::find_if(merged.begin(), merged.end(), [&](const QVariant &m) {
            return m.toMap().value(QStringLiteral("ssid")).toString() == ssid;
        });
        if (it == merged.end()) {
            merged.append(n);
            continue;
        }
        QVariantMap m = it->toMap();
        m.insert(QStringLiteral("signal"), std::max(m.value(QStringLiteral("signal")).toInt(),
                                                    n.value(QStringLiteral("signal")).toInt()));
        m.insert(QStringLiteral("active"), m.value(QStringLiteral("active")).toBool()
                                               || n.value(QStringLiteral("active")).toBool());
        *it = m;
    }
    std::sort(merged.begin(), merged.end(), [](const QVariant &a, const QVariant &b) {
        const QVariantMap x = a.toMap(), y = b.toMap();
        const bool ax = x.value(QStringLiteral("active")).toBool(), ay = y.value(QStringLiteral("active")).toBool();
        if (ax != ay)
            return ax;
        return x.value(QStringLiteral("signal")).toInt() > y.value(QStringLiteral("signal")).toInt();
    });
    m_networks = merged;
    emit networksChanged();
}

void WifiManager::scan()
{
    if (!available() || m_scanning)
        return;
    m_scanning = true;
    emit busyChanged();
    if (m_backend == Backend::Nmcli)
        nmScan();
    else
        wpaScan();
}

void WifiManager::refreshStatus()
{
    if (!available())
        return;
    if (m_backend == Backend::Nmcli)
        nmStatus();
    else
        wpaStatus();
}

void WifiManager::connectTo(const QString &ssid, const QString &password)
{
    if (!available() || ssid.isEmpty() || !m_connectingSsid.isEmpty())
        return;
    m_connectingSsid = ssid;
    emit busyChanged();
    setMessage(QStringLiteral("Đang kết nối \"%1\"…").arg(ssid));
    if (m_backend == Backend::Nmcli)
        nmConnect(ssid, password);
    else
        wpaConnect(ssid, password);
}

void WifiManager::finishConnect(bool ok, const QString &text)
{
    m_connectingSsid.clear();
    emit busyChanged();
    setMessage(text, !ok);
    refreshStatus();
    // danh sách cập nhật cờ đang dùng / đã lưu
    m_scanning = false;
    scan();
}

// ── nmcli ───────────────────────────────────────────────────────────────────

void WifiManager::nmScan()
{
    // mạng đã lưu trước, rồi quét (rescan mất vài giây)
    run(m_program, {QStringLiteral("-t"), QStringLiteral("-f"), QStringLiteral("NAME,TYPE"),
                    QStringLiteral("connection"), QStringLiteral("show")},
        5000, [this](int code, const QString &out, const QString &) {
            if (code == 0) {
                m_known.clear();
                for (const QString &line : out.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
                    const QStringList f = splitTerse(line);
                    if (f.size() >= 2 && f[1] == QLatin1String("802-11-wireless"))
                        m_known << f[0];
                }
            }
            run(m_program, {QStringLiteral("-t"), QStringLiteral("-f"), QStringLiteral("IN-USE,SSID,SIGNAL,SECURITY"),
                            QStringLiteral("device"), QStringLiteral("wifi"), QStringLiteral("list"),
                            QStringLiteral("--rescan"), QStringLiteral("yes")},
                20000, [this](int code, const QString &out, const QString &err) {
                    m_scanning = false;
                    emit busyChanged();
                    if (code != 0)
                        return setMessage(QStringLiteral("Quét Wi-Fi lỗi: %1").arg(firstLine(err)), true);
                    QVariantList list;
                    for (const QString &line : out.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
                        const QStringList f = splitTerse(line);
                        if (f.size() < 4)
                            continue;
                        QVariantMap n;
                        n.insert(QStringLiteral("active"), f[0] == QLatin1String("*"));
                        n.insert(QStringLiteral("ssid"), f[1]);
                        n.insert(QStringLiteral("signal"), f[2].toInt());
                        n.insert(QStringLiteral("secure"), !f[3].isEmpty() && f[3] != QLatin1String("--"));
                        list.append(n);
                    }
                    publishNetworks(list);
                });
        });
}

void WifiManager::nmStatus()
{
    // mạng đang dùng + tín hiệu (không quét lại), rồi IP của thiết bị wifi
    run(m_program, {QStringLiteral("-t"), QStringLiteral("-f"), QStringLiteral("IN-USE,SSID,SIGNAL"),
                    QStringLiteral("device"), QStringLiteral("wifi"), QStringLiteral("list"),
                    QStringLiteral("--rescan"), QStringLiteral("no")},
        5000, [this](int code, const QString &out, const QString &) {
            QString ssid;
            int sig = -1;
            if (code == 0) {
                for (const QString &line : out.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
                    const QStringList f = splitTerse(line);
                    if (f.size() >= 3 && f[0] == QLatin1String("*")) {
                        ssid = f[1];
                        sig = f[2].toInt();
                        break;
                    }
                }
            }
            run(m_program, {QStringLiteral("-t"), QStringLiteral("-f"), QStringLiteral("DEVICE,TYPE,STATE"),
                            QStringLiteral("device")},
                5000, [this, ssid, sig](int code, const QString &out, const QString &) {
                    QString dev;
                    if (code == 0) {
                        for (const QString &line : out.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
                            const QStringList f = splitTerse(line);
                            if (f.size() >= 2 && f[1] == QLatin1String("wifi")) {
                                dev = f[0];
                                break;
                            }
                        }
                    }
                    if (dev.isEmpty())
                        return setStatus(ssid, {}, sig);
                    m_iface = dev;
                    run(m_program, {QStringLiteral("-t"), QStringLiteral("-f"), QStringLiteral("IP4.ADDRESS"),
                                    QStringLiteral("device"), QStringLiteral("show"), dev},
                        5000, [this, ssid, sig](int code, const QString &out, const QString &) {
                            // "IP4.ADDRESS[1]:192.168.3.60/24"
                            const QString ip = code == 0
                                ? firstLine(out).section(QLatin1Char(':'), 1).section(QLatin1Char('/'), 0, 0)
                                : QString();
                            setStatus(ssid, ip, sig);
                        });
                });
        });
}

void WifiManager::nmConnect(const QString &ssid, const QString &password)
{
    const QString wait = QString::number(kConnectTimeoutS);
    auto done = [this, ssid](int code, const QString &, const QString &err) {
        if (code == 0)
            finishConnect(true, QStringLiteral("Đã kết nối \"%1\".").arg(ssid));
        else
            finishConnect(false, QStringLiteral("Không kết nối được \"%1\": %2")
                                     .arg(ssid, firstLine(err).remove(QStringLiteral("Error: "))));
    };
    const int timeout = (kConnectTimeoutS + 5) * 1000;

    if (m_known.contains(ssid) && password.isEmpty()) {
        run(m_program, {QStringLiteral("--wait"), wait, QStringLiteral("connection"), QStringLiteral("up"),
                        QStringLiteral("id"), ssid},
            timeout, done);
    } else if (m_known.contains(ssid)) {
        // đã lưu nhưng nhập mật khẩu mới: sửa profile rồi bật
        run(m_program, {QStringLiteral("connection"), QStringLiteral("modify"), QStringLiteral("id"), ssid,
                        QStringLiteral("wifi-sec.key-mgmt"), QStringLiteral("wpa-psk"),
                        QStringLiteral("wifi-sec.psk"), password},
            5000, [this, ssid, wait, timeout, done](int code, const QString &, const QString &err) {
                if (code != 0)
                    return done(code, {}, err);
                run(m_program, {QStringLiteral("--wait"), wait, QStringLiteral("connection"), QStringLiteral("up"),
                                QStringLiteral("id"), ssid},
                    timeout, done);
            });
    } else {
        QStringList args{QStringLiteral("--wait"), wait, QStringLiteral("device"), QStringLiteral("wifi"),
                         QStringLiteral("connect"), ssid};
        if (!password.isEmpty())
            args << QStringLiteral("password") << password;
        run(m_program, args, timeout, done);
    }
}

// ── wpa_cli ─────────────────────────────────────────────────────────────────

void WifiManager::wpa(const QStringList &args, Done done, int timeoutMs)
{
    run(m_program, QStringList{QStringLiteral("-i"), m_iface} + args, timeoutMs, std::move(done));
}

void WifiManager::wpaScan()
{
    wpa({QStringLiteral("list_networks")}, [this](int code, const QString &out, const QString &) {
        if (code == 0) {
            m_known.clear();
            // network id / ssid / bssid / flags — dòng đầu là tiêu đề
            for (const QString &line : out.split(QLatin1Char('\n'), Qt::SkipEmptyParts).mid(1)) {
                const QStringList f = line.split(QLatin1Char('\t'));
                if (f.size() >= 2)
                    m_known << f[1];
            }
        }
        wpa({QStringLiteral("scan")}, [this](int code, const QString &, const QString &err) {
            if (code != 0) {
                m_scanning = false;
                emit busyChanged();
                return setMessage(QStringLiteral("Quét Wi-Fi lỗi: %1").arg(firstLine(err)), true);
            }
            QTimer::singleShot(kScanWaitMs, this, [this] {
                wpa({QStringLiteral("scan_results")}, [this](int code, const QString &out, const QString &) {
                    m_scanning = false;
                    emit busyChanged();
                    if (code != 0)
                        return setMessage(QStringLiteral("Không đọc được kết quả quét."), true);
                    // bssid / frequency / signal level / flags / ssid
                    QVariantList list;
                    for (const QString &line : out.split(QLatin1Char('\n'), Qt::SkipEmptyParts).mid(1)) {
                        const QStringList f = line.split(QLatin1Char('\t'));
                        if (f.size() < 5)
                            continue;
                        QVariantMap n;
                        n.insert(QStringLiteral("ssid"), f[4]);
                        n.insert(QStringLiteral("signal"), dbmToPercent(f[2].toInt()));
                        n.insert(QStringLiteral("secure"), f[3].contains(QLatin1String("WPA"))
                                                               || f[3].contains(QLatin1String("RSN"))
                                                               || f[3].contains(QLatin1String("WEP"))
                                                               || f[3].contains(QLatin1String("SAE")));
                        n.insert(QStringLiteral("active"), f[4] == m_currentSsid);
                        list.append(n);
                    }
                    publishNetworks(list);
                });
            });
        });
    });
}

void WifiManager::wpaStatus()
{
    wpa({QStringLiteral("status")}, [this](int code, const QString &out, const QString &) {
        if (code != 0)
            return setStatus({}, {}, -1);
        QString ssid, ip, state;
        for (const QString &line : out.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
            const QString key = line.section(QLatin1Char('='), 0, 0);
            const QString val = line.section(QLatin1Char('='), 1);
            if (key == QLatin1String("ssid"))
                ssid = val;
            else if (key == QLatin1String("ip_address"))
                ip = val;
            else if (key == QLatin1String("wpa_state"))
                state = val;
        }
        if (state != QLatin1String("COMPLETED"))
            return setStatus({}, {}, -1);
        wpa({QStringLiteral("signal_poll")}, [this, ssid, ip](int code, const QString &out, const QString &) {
            int sig = -1;
            if (code == 0) {
                for (const QString &line : out.split(QLatin1Char('\n'), Qt::SkipEmptyParts))
                    if (line.startsWith(QLatin1String("RSSI=")))
                        sig = dbmToPercent(line.mid(5).toInt());
            }
            setStatus(ssid, ip, sig);
        });
    });
}

void WifiManager::wpaConnect(const QString &ssid, const QString &password)
{
    // mạng đã lưu + không nhập mật khẩu mới ⇒ chỉ chọn lại
    wpa({QStringLiteral("list_networks")}, [this, ssid, password](int code, const QString &out, const QString &) {
        QString id;
        if (code == 0) {
            for (const QString &line : out.split(QLatin1Char('\n'), Qt::SkipEmptyParts).mid(1)) {
                const QStringList f = line.split(QLatin1Char('\t'));
                if (f.size() >= 2 && f[1] == ssid) {
                    id = f[0];
                    break;
                }
            }
        }
        auto select = [this, ssid](const QString &netId) {
            wpa({QStringLiteral("select_network"), netId}, [this, ssid](int code, const QString &, const QString &) {
                if (code != 0)
                    return finishConnect(false, QStringLiteral("wpa_supplicant từ chối chọn mạng."));
                wpa({QStringLiteral("save_config")}, {});
                wpaWaitAssociated(ssid, kWpaAssocTries);
            });
        };
        if (!id.isEmpty() && password.isEmpty())
            return select(id);

        auto configure = [this, ssid, password, select](const QString &netId) {
            // tham số chuỗi của wpa_cli phải nằm trong ngoặc kép
            wpa({QStringLiteral("set_network"), netId, QStringLiteral("ssid"), QLatin1Char('"') + ssid + QLatin1Char('"')},
                [this, netId, password, select](int code, const QString &, const QString &) {
                    if (code != 0)
                        return finishConnect(false, QStringLiteral("Không đặt được SSID."));
                    const QStringList sec = password.isEmpty()
                        ? QStringList{QStringLiteral("set_network"), netId, QStringLiteral("key_mgmt"), QStringLiteral("NONE")}
                        : QStringList{QStringLiteral("set_network"), netId, QStringLiteral("psk"),
                                      QLatin1Char('"') + password + QLatin1Char('"')};
                    wpa(sec, [this, netId, select](int code, const QString &out, const QString &) {
                        if (code != 0 || out.contains(QLatin1String("FAIL")))
                            return finishConnect(false, QStringLiteral("Mật khẩu không hợp lệ (8..63 ký tự)."));
                        select(netId);
                    });
                });
        };
        if (!id.isEmpty())
            return configure(id);
        wpa({QStringLiteral("add_network")}, [this, configure](int code, const QString &out, const QString &) {
            const QString netId = out.trimmed().section(QLatin1Char('\n'), -1).trimmed();
            bool isNum = false;
            netId.toInt(&isNum);
            if (code != 0 || !isNum)
                return finishConnect(false, QStringLiteral("wpa_supplicant không thêm được mạng."));
            configure(netId);
        });
    });
}

void WifiManager::wpaWaitAssociated(const QString &ssid, int triesLeft)
{
    wpa({QStringLiteral("status")}, [this, ssid, triesLeft](int code, const QString &out, const QString &) {
        if (code == 0 && out.contains(QLatin1String("wpa_state=COMPLETED"))
            && out.contains(QStringLiteral("\nssid=%1\n").arg(ssid))) {
            // xin lại IP: udhcpc của ifup đang chạy thì gửi SIGUSR1, không thì chạy mới
            const QString renew = QStringLiteral(
                "kill -USR1 $(cat /var/run/udhcpc.%1.pid) 2>/dev/null || udhcpc -b -R -i %1 -p /var/run/udhcpc.%1.pid")
                .arg(m_iface);
            run(QStringLiteral("/bin/sh"), {QStringLiteral("-c"), renew}, 10000, {});
            QTimer::singleShot(3000, this, [this, ssid] {
                finishConnect(true, QStringLiteral("Đã kết nối \"%1\".").arg(ssid));
            });
            return;
        }
        if (triesLeft <= 0)
            return finishConnect(false, QStringLiteral("Không vào được \"%1\" — sai mật khẩu hoặc sóng yếu.").arg(ssid));
        QTimer::singleShot(1000, this, [this, ssid, triesLeft] { wpaWaitAssociated(ssid, triesLeft - 1); });
    });
}

} // namespace gcs::net
