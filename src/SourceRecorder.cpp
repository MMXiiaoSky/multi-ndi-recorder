#include "SourceRecorder.h"
#include "Logging.h"
#include <QImage>
#include <QBuffer>
#include <QThread>
#include <QMutexLocker>
#include <objbase.h>
extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/channel_layout.h>
}

SourceRecorder::SourceRecorder(QObject *parent)
    : QObject(parent), m_running(false), m_paused(false), m_recv(nullptr)
{
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
    m_running = true;
    m_paused = false;
    m_timer.start();

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
}

void SourceRecorder::pause()
{
    if (!m_running)
        return;
    m_paused = true;
    m_status = "Paused";
}

void SourceRecorder::resume()
{
    if (!m_running)
        return;
    m_paused = false;
    m_status = "Recording";
}

qint64 SourceRecorder::elapsedMs() const
{
    return m_timer.elapsed();
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
    NDIlib_recv_create_v3_t recvCreate;
    recvCreate.source_to_connect_to.p_ndi_name = m_settings.ndiSource.toUtf8().constData();
    recvCreate.color_format = NDIlib_recv_color_format_RGBX_RGBA;
    recvCreate.bandwidth = NDIlib_recv_bandwidth_highest;
    recvCreate.allow_video_fields = false;
    m_recv = NDIlib_recv_create_v3(&recvCreate);
    if (!m_recv)
    {
        Logger::instance().log("Failed to create NDI receiver for " + m_settings.ndiSource);
        emit errorOccurred("NDI receiver failed");
    }
}

void SourceRecorder::videoThreadFunc()
{
    reconnect();

    NDIlib_video_frame_v2_t videoFrame;
    NDIlib_audio_frame_v3_t audioFrame;

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
            QImage img((uchar *)videoFrame.p_data, videoFrame.xres, videoFrame.yres, QImage::Format_RGBA8888);
            {
                QMutexLocker locker(&m_mutex);
                m_preview = img.copy();
                m_status = "Recording";
            }
            emit previewUpdated();

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
                cfg.fps = videoFrame.frame_rate_N ? videoFrame.frame_rate_N / videoFrame.frame_rate_D : 30;
                m_writer.start(cfg);
                emit recordingStarted(m_writer.currentFile());
            }

            AVFrame *frame = av_frame_alloc();
            frame->format = AV_PIX_FMT_RGBA;
            frame->width = videoFrame.xres;
            frame->height = videoFrame.yres;
            av_image_fill_arrays(frame->data, frame->linesize, videoFrame.p_data, AV_PIX_FMT_RGBA, videoFrame.xres, videoFrame.yres, 1);
            frame->pts = videoFrame.timestamp;
            m_writer.writeVideoFrame(frame);
            av_frame_free(&frame);

            NDIlib_recv_free_video_v2(m_recv, &videoFrame);
            if (m_writer.needsRollover())
            {
                m_writer.rollover();
            }
            break;
        }
        case NDIlib_frame_type_audio:
            // Audio handled in audio thread
            NDIlib_recv_free_audio_v3(m_recv, &audioFrame);
            break;
        case NDIlib_frame_type_none:
            Logger::instance().log("NDI timeout for " + m_settings.label);
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
            AVFrame *frame = av_frame_alloc();
            frame->nb_samples = numFrames;
            av_channel_layout_default(&frame->ch_layout, 2);
            frame->format = AV_SAMPLE_FMT_FLTP;
            frame->sample_rate = mixFormat->nSamplesPerSec;
            av_frame_get_buffer(frame, 0);

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
