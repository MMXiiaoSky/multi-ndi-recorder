#pragma once
#include <QString>
#include <QMutex>
#include <QDateTime>
#include <functional>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

struct RecordingConfig
{
    QString outputFolder;
    QString sourceLabel;
    bool segmented = false;
    int segmentMinutes = 20;
    int width = 1920;
    int height = 1080;
    int fps = 30;
    AVPixelFormat pixFmt = AV_PIX_FMT_YUV420P;
    int audioSampleRate = 48000;
    AVSampleFormat audioSampleFmt = AV_SAMPLE_FMT_FLTP;
    int audioChannels = 2;
};

class FfmpegWriter
{
public:
    FfmpegWriter();
    ~FfmpegWriter();

    bool start(const RecordingConfig &cfg);
    void stop();
    bool writeVideoFrame(AVFrame *frame);
    bool writeAudioFrame(AVFrame *frame);
    bool needsRollover();
    void rollover();

    QString currentFile() const { return m_currentFile; }

private:
    bool openContext(const QString &path);
    void closeContext();
    QString nextFileName();

    RecordingConfig m_cfg;
    AVFormatContext *m_fmtCtx;
    AVStream *m_videoStream;
    AVStream *m_audioStream;
    AVCodecContext *m_videoCodecCtx;
    AVCodecContext *m_audioCodecCtx;
    SwsContext *m_sws;
    SwrContext *m_swr;
    qint64 m_startMs;
    QString m_currentFile;
    QMutex m_mutex;
    int m_segmentIndex;
};
