#include "app/update_manager.h"

#include "litegcs_version.h" // sinh lúc build (cmake/gen_version.cmake)

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTimer>

namespace gcs::app {

namespace {
constexpr int kPollMs = 3000;
}

UpdateManager::UpdateManager(QObject *parent) : QObject(parent)
{
#ifdef Q_OS_LINUX
    m_script = qEnvironmentVariable("LITEGCS_UPDATE_SCRIPT");
    if (m_script.isEmpty()) // binary ở <repo>/cpp/build/ ⇒ script ở <repo>/tools/pi/
        m_script = QDir(QCoreApplication::applicationDirPath())
                       .absoluteFilePath(QStringLiteral("../../tools/pi/litegcs-update.sh"));
    if (!QFileInfo::exists(m_script))
        m_script.clear();
#endif
    m_session = qEnvironmentVariable("LITEGCS_SESSION") == QLatin1String("1");
    m_statusPath = QDir::homePath() + QStringLiteral("/.lite_gcs/update.json");

    m_poll = new QTimer(this);
    m_poll->setInterval(kPollMs);
    connect(m_poll, &QTimer::timeout, this, &UpdateManager::readStatus);
    if (supported()) {
        m_poll->start();
        readStatus();
    }
}

QString UpdateManager::version() const
{
    return QStringLiteral(LITEGCS_VERSION);
}

bool UpdateManager::updateAvailable() const
{
    // build xong một commit khác với binary đang chạy
    return m_state == QLatin1String("ready") && !m_commit.isEmpty() && m_commit != version();
}

void UpdateManager::readStatus()
{
    QFile f(m_statusPath);
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    if (o.isEmpty())
        return;
    QString state = o.value(QStringLiteral("state")).toString();
    const QString commit = o.value(QStringLiteral("commit")).toString();
    // "ready" của chính bản đang chạy = đã mới nhất
    if (state == QLatin1String("ready") && commit == version())
        state = QStringLiteral("uptodate");
    const QString message = o.value(QStringLiteral("message")).toString();
    const QString time = o.value(QStringLiteral("time")).toString();
    if (state == m_state && commit == m_commit && message == m_message && time == m_time)
        return;
    m_state = state;
    m_commit = commit;
    m_message = message;
    m_time = time;
    emit changed();
}

void UpdateManager::checkNow()
{
    if (!supported() || m_state == QLatin1String("checking") || m_state == QLatin1String("building"))
        return;
    m_state = QStringLiteral("checking");
    m_message = QStringLiteral("Đang kiểm tra bản mới");
    emit changed();
    QProcess::startDetached(QStringLiteral("/bin/bash"), {m_script});
}

void UpdateManager::restartToUpdate()
{
    if (m_session)
        QCoreApplication::exit(kRestartCode);
}

} // namespace gcs::app
