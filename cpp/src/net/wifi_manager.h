// Quản lý Wi-Fi cho trang cài đặt: quét mạng, xem mạng đang dùng, đổi mạng.
//
// Không tự nói chuyện với driver — gọi công cụ hệ thống qua QProcess (bất đồng
// bộ, không chặn UI):
//   * nmcli   — Raspberry Pi OS (NetworkManager), chạy được với user của phiên
//               desktop nhờ polkit;
//   * wpa_cli — image Buildroot (wpa_supplicant + udhcpc của busybox).
// Tự chọn công cụ có sẵn; không có cái nào (Windows…) thì available = false.
// Giao diện tên "wlan0" của wpa_cli đổi được bằng biến môi trường LITEGCS_WIFI_IF.
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

#include <functional>

class QTimer;

namespace gcs::net {

class WifiManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(QString backendName READ backendName CONSTANT)   // "nmcli" | "wpa_cli" | ""
    Q_PROPERTY(bool scanning READ scanning NOTIFY busyChanged)
    Q_PROPERTY(QString connectingSsid READ connectingSsid NOTIFY busyChanged)
    // mỗi phần tử: { ssid, signal 0..100, secure, active, known }
    Q_PROPERTY(QVariantList networks READ networks NOTIFY networksChanged)
    Q_PROPERTY(QString currentSsid READ currentSsid NOTIFY statusChanged)
    Q_PROPERTY(QString ipAddress READ ipAddress NOTIFY statusChanged)
    Q_PROPERTY(int signal READ signal NOTIFY statusChanged)              // 0..100, -1 = không rõ
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)
    Q_PROPERTY(bool messageError READ messageError NOTIFY messageChanged)
    // trang Wi-Fi đang mở ⇒ cập nhật trạng thái mỗi 5 s
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)

public:
    explicit WifiManager(QObject *parent = nullptr);

    bool available() const { return m_backend != Backend::None; }
    QString backendName() const;
    bool scanning() const { return m_scanning; }
    QString connectingSsid() const { return m_connectingSsid; }
    QVariantList networks() const { return m_networks; }
    QString currentSsid() const { return m_currentSsid; }
    QString ipAddress() const { return m_ip; }
    int signal() const { return m_signal; }
    QString message() const { return m_message; }
    bool messageError() const { return m_messageError; }
    bool active() const { return m_active; }
    void setActive(bool on);

    Q_INVOKABLE void scan();
    Q_INVOKABLE void refreshStatus();
    // password rỗng: mạng mở hoặc mạng đã lưu (dùng mật khẩu cũ)
    Q_INVOKABLE void connectTo(const QString &ssid, const QString &password);

signals:
    void busyChanged();
    void networksChanged();
    void statusChanged();
    void messageChanged();
    void activeChanged();

private:
    enum class Backend { None, Nmcli, WpaCli };
    using Done = std::function<void(int code, const QString &out, const QString &err)>;

    void run(const QString &program, const QStringList &args, int timeoutMs, Done done);
    void setMessage(const QString &text, bool error = false);
    void setStatus(const QString &ssid, const QString &ip, int signal);
    void publishNetworks(QVariantList list);

    // nmcli
    void nmScan();
    void nmStatus();
    void nmConnect(const QString &ssid, const QString &password);
    // wpa_cli
    void wpa(const QStringList &args, Done done, int timeoutMs = 5000);
    void wpaScan();
    void wpaStatus();
    void wpaConnect(const QString &ssid, const QString &password);
    void wpaWaitAssociated(const QString &ssid, int triesLeft);
    void finishConnect(bool ok, const QString &text);

    Backend m_backend = Backend::None;
    QString m_program;   // đường dẫn nmcli / wpa_cli
    QString m_iface;     // wlan0 (wpa_cli) / thiết bị wifi của NetworkManager
    QStringList m_known; // tên mạng đã lưu

    bool m_scanning = false;
    QString m_connectingSsid;
    QVariantList m_networks;
    QString m_currentSsid;
    QString m_ip;
    int m_signal = -1;
    QString m_message;
    bool m_messageError = false;
    bool m_active = false;
    QTimer *m_poll = nullptr;
};

} // namespace gcs::net
