#include "SourceRecorder.h"
#include "Logging.h"
#include <QImage>
#include <QByteArray>
#include <QThread>
#include <QMutexLocker>
#include <algorithm>
#include <cmath>
extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/rational.h>
}

SourceRecorder::SourceRecorder(QObject *parent)
    : QObject(parent), m_running(false), m_paused(false), m_recordingStarted(false), m_recv(nullptr), m_pausedDurationMs(0), m_pauseStartMs(0)
{
    m_status = "Idle";
}

SourceRecorder::~SourceRecorder()
{
    stop();
}

void SourceRecorder::applySettings(const SourceSettings &settings)
{
    QMutexLocker locker(&m_mutex);
    m_settings = settings;
    if (m_settings.label.isEmpty())
        m_settings.label = m_settings.ndiSource;
}

void SourceRecorder::start()
{
    if (m_running)
        return;

    if (m_settings.ndiSource.isEmpty() || m_settings.outputFolder.isEmpty())
    {
        m_status = "Missing settings";
        emit errorOccurred("Configure NDI source and output folder before starting.");
        return;
    }

    // Validate that the configured NDI source is still available
    NdiManager ndi;
    if (!ndi.availableSources().contains(m_settings.ndiSource))
    {
        m_status = "Source unavailable";
        emit errorOccurred("NDI source not found: " + m_settings.ndiSource);
        return;
    }

    m_running = true;
    m_paused = false;
    m_recordingStarted = false;
    m_videoPts = 0;
    m_firstTimestamp = NDIlib_recv_timestamp_undefined;
    m_prevTimestamp = NDIlib_recv_timestamp_undefined;
    m_timestampScale = 10000000;
    m_expectedFrameTicks10ns = 0;
    {
        QMutexLocker stateLocker(&m_stateMutex);
        m_pausedDurationMs = 0;
        m_pauseStartMs = 0;
    }
    m_previewThrottle.invalidate();
    m_status = "Connecting";
    {
        QMutexLocker locker(&m_mutex);
        m_preview = QImage();
    }
    emit previewUpdated();

    connect(&m_videoThread, &QThread::started, this, &SourceRecorder::videoThreadFunc);
    moveToThread(&m_videoThread);
    m_videoThread.start();
}

void SourceRecorder::stop()
{
    m_running = false;
    m_paused = false;
    m_recordingStarted = false;
    {
        QMutexLocker stateLocker(&m_stateMutex);
        m_pausedDurationMs = 0;
        m_pauseStartMs = 0;
    }
    m_videoThread.quit();
    m_videoThread.wait();
    if (m_recv)
    {
        NDIlib_recv_destroy(m_recv);
        m_recv = nullptr;
    }
    m_writer.stop();
    emit recordingStopped();
    m_status = "Idle";
    {
        QMutexLocker locker(&m_mutex);
        m_preview = QImage();
    }
    emit previewUpdated();
}

void SourceRecorder::pause()
{
    if (!m_running || !m_recordingStarted)
        return;
    m_paused = true;
    {
        QMutexLocker stateLocker(&m_stateMutex);
        m_pauseStartMs = m_timer.elapsed();
    }
    m_status = "Paused";
}

void SourceRecorder::resume()
{
    if (!m_running || !m_recordingStarted)
        return;
    m_paused = false;
    {
        QMutexLocker stateLocker(&m_stateMutex);
        m_pausedDurationMs += m_timer.elapsed() - m_pauseStartMs;
        m_pauseStartMs = 0;
    }
    m_status = "Recording";
}

qint64 SourceRecorder::elapsedMs() const
{
    if ((!m_running && !m_paused) || !m_recordingStarted)
        return 0;
    QMutexLocker stateLocker(&m_stateMutex);
    if (m_paused)
        return m_pauseStartMs - m_pausedDurationMs;
    return m_timer.elapsed() - m_pausedDurationMs;
}

QImage SourceRecorder::lastFrame() const
{
    QMutexLocker locker(&m_mutex);
    return m_preview;
}

void SourceRecorder::reconnect()
{
    if (m_recv)
    {
        NDIlib_recv_destroy(m_recv);
        m_recv = nullptr;
    }

    QByteArray ndiNameUtf8 = m_settings.ndiSource.toUtf8();
    NDIlib_source_t source = {};
    source.p_ndi_name = ndiNameUtf8.constData();

    NDIlib_recv_create_v3_t recvCreate = {};
    recvCreate.source_to_connect_to = source;
    recvCreate.color_format = NDIlib_recv_color_format_RGBX_RGBA;
    recvCreate.bandwidth = NDIlib_recv_bandwidth_highest;
    recvCreate.allow_video_fields = false;

    m_recv = NDIlib_recv_create_v3(&recvCreate);
    if (!m_recv)
    {
        Logger::instance().log("Failed to create NDI receiver for " + m_settings.ndiSource);
        emit errorOccurred("NDI receiver failed");
        m_running = false;
        m_status = "Error";
    }
}

void SourceRecorder::videoThreadFunc()
{
    reconnect();

    NDIlib_video_frame_v2_t videoFrame;
    NDIlib_audio_frame_v3_t audioFrame;
    int timeoutStreak = 0;

    while (m_running)
    {
        if (m_paused)
        {
            QThread::msleep(20);
            continue;
        }
        switch (NDIlib_recv_capture_v3(m_recv, &videoFrame, &audioFrame, nullptr, 500))
        {
        case NDIlib_frame_type_video:
        {
            // Update preview
            const bool shouldUpdatePreview = !m_previewThrottle.isValid() || m_previewThrottle.elapsed() >= 200;
            if (shouldUpdatePreview)
            {
                QImage img((uchar *)videoFrame.p_data, videoFrame.xres, videoFrame.yres, QImage::Format_RGBA8888);
                {
                    QMutexLocker locker(&m_mutex);
                    m_preview = img.copy();
                    m_status = "Recording";
                }
                emit previewUpdated();
                m_previewThrottle.restart();
            }
            else
            {
                QMutexLocker locker(&m_mutex);
                m_status = "Recording";
            }

            if (!m_recordingStarted)
            {
                m_timer.restart();
                m_recordingStarted = true;
            }
            timeoutStreak = 0;

            // Prepare FFmpeg writer if needed
            if (m_writer.currentFile().isEmpty())
            {
                RecordingConfig cfg;
                cfg.outputFolder = m_settings.outputFolder;
                cfg.sourceLabel = m_settings.label;
                cfg.segmented = m_settings.segmented;
                cfg.segmentMinutes = m_settings.segmentMinutes;
                cfg.width = videoFrame.xres;
                cfg.height = videoFrame.yres;
                const int defaultFps = 60;
                auto validatedFrameRate = [&](int num, int den) {
                    struct
                    {
                        int fps{};
                        int num{};
                        int den{};
                        bool fromSource{};
                    } result;

                    if (num > 0 && den > 0)
                    {
                        const double fpsValue = static_cast<double>(num) / den;
                        if (fpsValue >= 1.0 && fpsValue <= 240.0)
                        {
                            result.fps = (std::max)(1, static_cast<int>(fpsValue + 0.5));
                            result.num = num;
                            result.den = den;
                            result.fromSource = true;
                            return result;
                        }
                        Logger::instance().log(QString("Ignoring unreasonable NDI frame rate %1/%2 for %3")
                                                   .arg(num)
                                                   .arg(den)
                                                   .arg(m_settings.label));
                    }

                    result.fps = defaultFps;
                    result.num = defaultFps;
                    result.den = 1;
                    result.fromSource = false;
                    return result;
                };

                const auto fpsInfo = validatedFrameRate(videoFrame.frame_rate_N, videoFrame.frame_rate_D);
                cfg.fps = fpsInfo.fps;
                cfg.fpsNum = fpsInfo.num;
                cfg.fpsDen = fpsInfo.den;
                m_expectedFrameTicks10ns = (static_cast<qint64>(10000000) * fpsInfo.den) / fpsInfo.num;
                cfg.inputPixFmt = AV_PIX_FMT_RGBA;
                cfg.outputPixFmt = AV_PIX_FMT_YUV420P;
                if (!m_writer.start(cfg))
                {
                    m_status = "Error";
                    emit errorOccurred("Failed to start writer for " + m_settings.label);
                    m_running = false;
                    NDIlib_recv_free_video_v2(m_recv, &videoFrame);
                    break;
                }
                m_videoPts = 0;
                emit recordingStarted(m_writer.currentFile());
            }

            AVFrame *frame = av_frame_alloc();
            frame->format = AV_PIX_FMT_RGBA;
            frame->width = videoFrame.xres;
            frame->height = videoFrame.yres;
            av_image_fill_arrays(frame->data, frame->linesize, videoFrame.p_data, AV_PIX_FMT_RGBA, videoFrame.xres, videoFrame.yres, 1);
            AVRational timeBase = m_writer.videoTimeBase();
            qint64 ptsValue = m_videoPts++;
            if (videoFrame.timestamp != NDIlib_recv_timestamp_undefined && timeBase.num > 0 && timeBase.den > 0)
            {
                if (m_prevTimestamp != NDIlib_recv_timestamp_undefined && m_expectedFrameTicks10ns > 0)
                {
                    const qint64 deltaTicks = videoFrame.timestamp - m_prevTimestamp;
                    // Some NDI sources report timestamps in microseconds instead of 100ns ticks.
                    // Detect this by comparing the observed delta with the expected frame spacing.
                    if (m_timestampScale == 10000000 && deltaTicks > 0 && deltaTicks < m_expectedFrameTicks10ns / 2)
                    {
                        m_timestampScale = 1000000;
                        Logger::instance().log(QString("NDI timestamps appear to be in microseconds for %1; adjusting scale")
                                                   .arg(m_settings.label));
                    }
                }
                m_prevTimestamp = videoFrame.timestamp;
                if (m_firstTimestamp == NDIlib_recv_timestamp_undefined)
                    m_firstTimestamp = videoFrame.timestamp;
                qint64 pausedDurationTicks = 0;
                {
                    QMutexLocker stateLocker(&m_stateMutex);
                    pausedDurationTicks = m_pausedDurationMs * (m_timestampScale / 1000);
                }
                const int64_t delta = videoFrame.timestamp - m_firstTimestamp - pausedDurationTicks;
                ptsValue = av_rescale_q(std::max<int64_t>(0, delta), AVRational{1, static_cast<int>(m_timestampScale)}, timeBase);
            }
            frame->pts = ptsValue;
            m_writer.writeVideoFrame(frame);
            av_frame_free(&frame);

            NDIlib_recv_free_video_v2(m_recv, &videoFrame);
            if (m_writer.needsRollover())
            {
                m_writer.rollover();
                m_videoPts = 0;
                m_firstTimestamp = NDIlib_recv_timestamp_undefined;
                m_prevTimestamp = NDIlib_recv_timestamp_undefined;
            }
            break;
        }
        case NDIlib_frame_type_audio:
            NDIlib_recv_free_audio_v3(m_recv, &audioFrame);
            break;
        case NDIlib_frame_type_none:
            Logger::instance().log("NDI timeout for " + m_settings.label);
            if (!m_recordingStarted && ++timeoutStreak >= 10)
            {
                m_status = "No signal";
                emit errorOccurred("No video received from " + m_settings.label);
            }
            break;
        default:
            break;
        }
    }
}

