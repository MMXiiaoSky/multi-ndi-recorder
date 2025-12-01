#pragma once
#include <QString>
#include <QFile>
#include <QTextStream>
#include <QMutex>
#include <QDateTime>

class Logger
{
public:
    static Logger &instance();
    void log(const QString &message);
    QString logFilePath() const;

private:
    Logger();
    QFile m_file;
    QTextStream m_stream;
    mutable QMutex m_mutex;
};
