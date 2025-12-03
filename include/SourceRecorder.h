#pragma once
#include <QObject>
#include <QImage>
#include <QThread>
#include <QAtomicInteger>
#include <QElapsedTimer>
#include <QMutex>
#include "FfmpegWriter.h"
#include "NdiManager.h"

struct SourceSettings
{
    QString ndiSource;
    QString outputFolder;
    QString label;
    bool segmented = false;
    int segmentMinutes = 20;
};

class SourceRecorder : public QObject
{
    Q_OBJECT
public:
    explicit SourceRecorder(QObject *parent = nullptr);
    ~SourceRecorder();

    void applySettings(const SourceSettings &settings);
    SourceSettings settings() const { return m_settings; }

    void start();
    void stop();
    void pause();
    void resume();

    QImage lastFrame() const;
    QString status() const { return m_status; }
    qint64 elapsedMs() const;
    QString currentFile() const { return m_writer.currentFile(); }

signals:
    void previewUpdated();
    void errorOccurred(const QString &err);
    void recordingStarted(const QString &file);
    void recordingStopped();

private:
    void videoThreadFunc();
    void reconnect();

    mutable QMutex m_mutex;
    mutable QMutex m_stateMutex;
    SourceSettings m_settings;
    FfmpegWriter m_writer;
    QThread m_videoThread;
    QAtomicInteger<bool> m_running;
    QAtomicInteger<bool> m_paused;
    QAtomicInteger<bool> m_recordingStarted;
    qint64 m_videoPts = 0;
    qint64 m_firstTimestamp = NDIlib_recv_timestamp_undefined;
    qint64 m_prevTimestamp = NDIlib_recv_timestamp_undefined;
    qint64 m_timestampScale = 10000000; // ticks per second (10ns default)
    qint64 m_expectedFrameTicks10ns = 0;
    QImage m_preview;
    QString m_status;
    QElapsedTimer m_timer;
    QElapsedTimer m_previewThrottle;
    NDIlib_recv_instance_t m_recv;
    qint64 m_pausedDurationMs;
    qint64 m_pauseStartMs;
};
