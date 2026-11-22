// Tự cập nhật (Raspberry Pi OS): phía app.
//
// Việc tải + build do tools/pi/litegcs-update.sh làm ở nền (gọi bởi
// litegcs-session.sh lúc khởi động và mỗi 30 phút); script ghi trạng thái vào
// ~/.lite_gcs/update.json. Lớp này chỉ đọc file đó, so commit đã build với
// commit của chính binary (LITEGCS_VERSION) để biết có bản mới, và khi người
// dùng đồng ý thì thoát với mã 42 — session script chạy lại app = bản mới.
// Không bao giờ tự khởi động lại.
#pragma once

#include <QObject>
#include <QString>

class QTimer;

namespace gcs::app {

class UpdateManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString version READ version CONSTANT)          // commit của binary đang chạy
    Q_PROPERTY(bool supported READ supported CONSTANT)         // có script cập nhật
    Q_PROPERTY(bool canRestart READ canRestart CONSTANT)       // chạy qua litegcs-session.sh
    // idle | checking | building | ready | uptodate | offline | failed
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(QString newVersion READ newVersion NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(QString checkedAt READ checkedAt NOTIFY changed)
    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY changed)

public:
    static constexpr int kRestartCode = 42;

    explicit UpdateManager(QObject *parent = nullptr);

    QString version() const;
    bool supported() const { return !m_script.isEmpty(); }
    bool canRestart() const { return m_session; }
    QString state() const { return m_state; }
    QString newVersion() const { return m_commit; }
    QString message() const { return m_message; }
    QString checkedAt() const { return m_time; }
    bool updateAvailable() const;

    Q_INVOKABLE void checkNow();
    Q_INVOKABLE void restartToUpdate();

signals:
    void changed();

private:
    void readStatus();

    QString m_script;
    QString m_statusPath;
    bool m_session = false;
    QString m_state = QStringLiteral("idle");
    QString m_commit;
    QString m_message;
    QString m_time;
    QTimer *m_poll = nullptr;
};

} // namespace gcs::app
