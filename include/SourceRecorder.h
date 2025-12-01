#pragma once
#include <QObject>
#include <QImage>
#include <QThread>
#include <QAtomicInteger>
#include <QElapsedTimer>
#include "FfmpegWriter.h"
#include "NdiManager.h"
#include "AudioDeviceManager.h"

struct SourceSettings
{
    QString ndiSource;
    QString audioDevice;
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
    void audioThreadFunc();
    void reconnect();

    mutable QMutex m_mutex;
    SourceSettings m_settings;
    FfmpegWriter m_writer;
    QThread m_videoThread;
    QThread m_audioThread;
    QAtomicInteger<bool> m_running;
    QAtomicInteger<bool> m_paused;
    QImage m_preview;
    QString m_status;
    QElapsedTimer m_timer;
    NDIlib_recv_instance_t m_recv;
    AudioDeviceManager m_audioManager;
    qint64 m_pausedDurationMs;
    qint64 m_pauseStartMs;
};
