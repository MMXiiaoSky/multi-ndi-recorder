#include "Logging.h"
#include <QDir>
#include <QMutexLocker>

Logger &Logger::instance()
{
    static Logger inst;
    return inst;
}

Logger::Logger()
    : m_file(), m_stream(&m_file)
{
    QDir().mkpath("logs");
    m_file.setFileName("logs/app.log");
    m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
}

void Logger::log(const QString &message)
{
    QMutexLocker locker(&m_mutex);
    if (!m_file.isOpen())
    {
        return;
    }
    QString line = QString("[%1] %2\n").arg(QDateTime::currentDateTime().toString(Qt::ISODate), message);
    m_stream << line;
    m_stream.flush();
}

QString Logger::logFilePath() const
{
    return m_file.fileName();
}
