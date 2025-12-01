#include "SourceRecorder.h"
#include "Logging.h"
#include <QImage>
#include <QBuffer>
#include <QByteArray>
#include <QThread>
#include <QMutexLocker>
#include <algorithm>
#include <cmath>
#include <objbase.h>
extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/channel_layout.h>
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

    if (m_settings.ndiSource.isEmpty() || m_settings.audioDevice.isEmpty() || m_settings.outputFolder.isEmpty())
    {
        m_status = "Missing settings";
        emit errorOccurred("Configure NDI source, audio device, and output folder before starting.");
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
    connect(&m_audioThread, &QThread::started, this, &SourceRecorder::audioThreadFunc, Qt::DirectConnection);

    moveToThread(&m_videoThread);
    m_videoThread.start();
    m_audioThread.start();
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
    m_audioThread.quit();
    m_videoThread.wait();
    m_audioThread.wait();
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
                const bool hasFrameRate = videoFrame.frame_rate_N > 0 && videoFrame.frame_rate_D > 0;
                const double fpsValue = hasFrameRate
                                            ? static_cast<double>(videoFrame.frame_rate_N) / videoFrame.frame_rate_D
                                            : 0.0;
                const int fpsInt = fpsValue > 0.0 ? (std::max)(1, static_cast<int>(fpsValue + 0.5)) : 60;
                cfg.fps = fpsInt;
                cfg.fpsNum = hasFrameRate ? videoFrame.frame_rate_N : fpsInt;
                cfg.fpsDen = hasFrameRate ? videoFrame.frame_rate_D : 1;
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
                if (m_firstTimestamp == NDIlib_recv_timestamp_undefined)
                    m_firstTimestamp = videoFrame.timestamp;
                qint64 pausedDurationTicks = 0;
                {
                    QMutexLocker stateLocker(&m_stateMutex);
                    pausedDurationTicks = m_pausedDurationMs * 10000;
                }
                const int64_t delta = videoFrame.timestamp - m_firstTimestamp - pausedDurationTicks;
                ptsValue = av_rescale_q(std::max<int64_t>(0, delta), AVRational{1, 10000000}, timeBase);
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
            }
            break;
        }
        case NDIlib_frame_type_audio:
            // Audio handled in audio thread
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

void SourceRecorder::audioThreadFunc()
{
    HRESULT coinit = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool comInitialized = coinit == S_OK || coinit == S_FALSE;
    if (FAILED(coinit) && coinit != RPC_E_CHANGED_MODE)
    {
        emit errorOccurred("Failed to initialize COM for audio capture: 0x" + QString::number(coinit, 16));
        return;
    }

    // Capture from WASAPI input device
    IMMDevice *device = m_audioManager.deviceByName(m_settings.audioDevice);
    if (!device)
    {
        emit errorOccurred("Audio device not found");
        if (comInitialized)
            CoUninitialize();
        return;
    }
    CComPtr<IAudioClient> audioClient;
    HRESULT hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void **)&audioClient);
    if (FAILED(hr) || !audioClient)
    {
        emit errorOccurred("Failed to activate audio client: 0x" + QString::number(hr, 16));
        device->Release();
        if (comInitialized)
            CoUninitialize();
        return;
    }

    WAVEFORMATEX *mixFormat = nullptr;
    hr = audioClient->GetMixFormat(&mixFormat);
    if (FAILED(hr))
    {
        emit errorOccurred("Failed to get audio mix format: 0x" + QString::number(hr, 16));
        device->Release();
        if (comInitialized)
            CoUninitialize();
        return;
    }

    hr = audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 0, 0, mixFormat, nullptr);
    if (FAILED(hr))
    {
        emit errorOccurred("Failed to initialize audio client: 0x" + QString::number(hr, 16));
        CoTaskMemFree(mixFormat);
        device->Release();
        if (comInitialized)
            CoUninitialize();
        return;
    }
    CComPtr<IAudioCaptureClient> captureClient;
    hr = audioClient->GetService(__uuidof(IAudioCaptureClient), (void **)&captureClient);
    if (FAILED(hr) || !captureClient)
    {
        emit errorOccurred("Failed to get audio capture client: 0x" + QString::number(hr, 16));
        audioClient->Stop();
        CoTaskMemFree(mixFormat);
        device->Release();
        if (comInitialized)
            CoUninitialize();
        return;
    }
    audioClient->Start();

    int frameSize = mixFormat->nBlockAlign;
    UINT32 packetFrames = 0;
    qint64 pts = 0;

    while (m_running)
    {
        if (m_paused)
        {
            QThread::msleep(10);
            continue;
        }
        UINT32 numFrames = 0;
        BYTE *data = nullptr;
        DWORD flags = 0;
        if (SUCCEEDED(captureClient->GetBuffer(&data, &numFrames, &flags, nullptr, nullptr)))
        {
            if (numFrames == 0)
            {
                captureClient->ReleaseBuffer(0);
                continue;
            }

            AVFrame *frame = av_frame_alloc();
            frame->nb_samples = numFrames;
            av_channel_layout_default(&frame->ch_layout, 2);
            frame->format = AV_SAMPLE_FMT_FLTP;
            frame->sample_rate = mixFormat->nSamplesPerSec;
            if (av_frame_get_buffer(frame, 0) < 0)
            {
                av_frame_free(&frame);
                captureClient->ReleaseBuffer(numFrames);
                continue;
            }

            // Interleave planar float conversion
            const float *input = reinterpret_cast<const float *>(data);
            const int channels = frame->ch_layout.nb_channels;
            for (int ch = 0; ch < channels; ++ch)
            {
                for (uint32_t i = 0; i < numFrames; ++i)
                {
                    reinterpret_cast<float *>(frame->data[ch])[i] = input[i * 2 + ch];
                }
            }
            frame->pts = pts;
            pts += numFrames;
            if (!m_writer.currentFile().isEmpty())
                m_writer.writeAudioFrame(frame);
            av_frame_free(&frame);
            captureClient->ReleaseBuffer(numFrames);
        }
        else
        {
            QThread::msleep(5);
        }
    }
    audioClient->Stop();
    CoTaskMemFree(mixFormat);
    device->Release();
    if (comInitialized)
        CoUninitialize();
}
