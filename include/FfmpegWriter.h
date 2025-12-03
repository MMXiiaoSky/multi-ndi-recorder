#pragma once
#include <QString>
#include <QMutex>
#include <QDateTime>
#include <functional>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
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
    int fpsNum = 30;
    int fpsDen = 1;
    AVPixelFormat inputPixFmt = AV_PIX_FMT_RGBA;
    AVPixelFormat outputPixFmt = AV_PIX_FMT_YUV420P;
};

class FfmpegWriter
{
public:
    FfmpegWriter();
    ~FfmpegWriter();

    bool start(const RecordingConfig &cfg);
    void stop();
    bool writeVideoFrame(AVFrame *frame);
    bool needsRollover();
    void rollover();

    QString currentFile() const { return m_currentFile; }
    AVRational videoTimeBase() const;

private:
    bool openContext(const QString &path);
    void closeContext();
    QString nextFileName();
    bool ensureConvertedFrame();

    RecordingConfig m_cfg;
    AVFormatContext *m_fmtCtx;
    AVStream *m_videoStream;
    AVCodecContext *m_videoCodecCtx;
    SwsContext *m_sws;
    AVFrame *m_convertedFrame;
    qint64 m_startMs;
    QString m_currentFile;
    QMutex m_mutex;
    int m_segmentIndex;
    int m_inputWidth;
    int m_inputHeight;
    AVPixelFormat m_inputFormat;
};
