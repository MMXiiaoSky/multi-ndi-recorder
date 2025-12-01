#include "FfmpegWriter.h"
#include "Logging.h"
#include <QDir>
#include <QDebug>

FfmpegWriter::FfmpegWriter()
    : m_fmtCtx(nullptr), m_videoStream(nullptr), m_audioStream(nullptr), m_videoCodecCtx(nullptr),
      m_audioCodecCtx(nullptr), m_sws(nullptr), m_swr(nullptr), m_startMs(0), m_segmentIndex(1)
{
    avformat_network_init();
}

FfmpegWriter::~FfmpegWriter()
{
    stop();
    avformat_network_deinit();
}

QString FfmpegWriter::nextFileName()
{
    QDateTime now = QDateTime::currentDateTime();
    QString ts = now.toString("yyyyMMdd_HHmmss");
    if (m_cfg.segmented)
    {
        return QString("%1/%2_%3_part%4.mkv").arg(m_cfg.outputFolder, m_cfg.sourceLabel, ts, QString::number(m_segmentIndex).rightJustified(2, '0'));
    }
    return QString("%1/%2_%3.mkv").arg(m_cfg.outputFolder, m_cfg.sourceLabel, ts);
}

bool FfmpegWriter::openContext(const QString &path)
{
    avformat_alloc_output_context2(&m_fmtCtx, nullptr, "matroska", path.toUtf8().constData());
    if (!m_fmtCtx)
    {
        Logger::instance().log("Failed to alloc output context");
        return false;
    }

    const AVCodec *videoCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
    const AVCodec *audioCodec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!videoCodec || !audioCodec)
    {
        Logger::instance().log("Missing codecs");
        return false;
    }

    m_videoStream = avformat_new_stream(m_fmtCtx, videoCodec);
    m_audioStream = avformat_new_stream(m_fmtCtx, audioCodec);
    if (!m_videoStream || !m_audioStream)
    {
        Logger::instance().log("Failed to create streams");
        return false;
    }

    m_videoCodecCtx = avcodec_alloc_context3(videoCodec);
    m_videoCodecCtx->codec_id = AV_CODEC_ID_H264;
    m_videoCodecCtx->width = m_cfg.width;
    m_videoCodecCtx->height = m_cfg.height;
    m_videoCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    m_videoCodecCtx->time_base = {1, m_cfg.fps};
    m_videoCodecCtx->framerate = {m_cfg.fps, 1};
    m_videoCodecCtx->gop_size = m_cfg.fps;

    if (m_fmtCtx->oformat->flags & AVFMT_GLOBALHEADER)
        m_videoCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (avcodec_open2(m_videoCodecCtx, videoCodec, nullptr) < 0)
    {
        Logger::instance().log("Failed to open video codec");
        return false;
    }

    if (avcodec_parameters_from_context(m_videoStream->codecpar, m_videoCodecCtx) < 0)
    {
        Logger::instance().log("Failed to copy video params");
        return false;
    }

    m_audioCodecCtx = avcodec_alloc_context3(audioCodec);
    m_audioCodecCtx->codec_id = AV_CODEC_ID_AAC;
    m_audioCodecCtx->sample_rate = m_cfg.audioSampleRate;
    m_audioCodecCtx->channel_layout = av_get_default_channel_layout(m_cfg.audioChannels);
    m_audioCodecCtx->channels = m_cfg.audioChannels;
    m_audioCodecCtx->sample_fmt = audioCodec->sample_fmts ? audioCodec->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
    m_audioCodecCtx->time_base = {1, m_cfg.audioSampleRate};
    if (m_fmtCtx->oformat->flags & AVFMT_GLOBALHEADER)
        m_audioCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (avcodec_open2(m_audioCodecCtx, audioCodec, nullptr) < 0)
    {
        Logger::instance().log("Failed to open audio codec");
        return false;
    }
    if (avcodec_parameters_from_context(m_audioStream->codecpar, m_audioCodecCtx) < 0)
    {
        Logger::instance().log("Failed to copy audio params");
        return false;
    }

    m_audioStream->time_base = m_audioCodecCtx->time_base;
    m_videoStream->time_base = m_videoCodecCtx->time_base;

    if (!(m_fmtCtx->oformat->flags & AVFMT_NOFILE))
    {
        if (avio_open(&m_fmtCtx->pb, path.toUtf8().constData(), AVIO_FLAG_WRITE) < 0)
        {
            Logger::instance().log("Failed to open output file");
            return false;
        }
    }

    if (avformat_write_header(m_fmtCtx, nullptr) < 0)
    {
        Logger::instance().log("Failed to write header");
        return false;
    }

    m_startMs = QDateTime::currentMSecsSinceEpoch();
    return true;
}

bool FfmpegWriter::start(const RecordingConfig &cfg)
{
    QMutexLocker locker(&m_mutex);
    m_cfg = cfg;
    m_segmentIndex = 1;
    QDir().mkpath(cfg.outputFolder);
    m_currentFile = nextFileName();
    return openContext(m_currentFile);
}

void FfmpegWriter::closeContext()
{
    if (m_fmtCtx)
    {
        av_write_trailer(m_fmtCtx);
        if (!(m_fmtCtx->oformat->flags & AVFMT_NOFILE))
        {
            avio_closep(&m_fmtCtx->pb);
        }
        avformat_free_context(m_fmtCtx);
    }
    m_fmtCtx = nullptr;
    if (m_videoCodecCtx)
    {
        avcodec_free_context(&m_videoCodecCtx);
    }
    if (m_audioCodecCtx)
    {
        avcodec_free_context(&m_audioCodecCtx);
    }
    if (m_sws)
    {
        sws_freeContext(m_sws);
        m_sws = nullptr;
    }
    if (m_swr)
    {
        swr_free(&m_swr);
    }
}

void FfmpegWriter::stop()
{
    QMutexLocker locker(&m_mutex);
    closeContext();
}

bool FfmpegWriter::writeVideoFrame(AVFrame *frame)
{
    QMutexLocker locker(&m_mutex);
    if (!m_fmtCtx)
        return false;
    frame->format = m_videoCodecCtx->pix_fmt;
    frame->width = m_videoCodecCtx->width;
    frame->height = m_videoCodecCtx->height;
    frame->pts = av_rescale_q(frame->pts, m_videoCodecCtx->time_base, m_videoStream->time_base);

    if (avcodec_send_frame(m_videoCodecCtx, frame) < 0)
        return false;
    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = nullptr;
    pkt.size = 0;
    while (avcodec_receive_packet(m_videoCodecCtx, &pkt) == 0)
    {
        pkt.stream_index = m_videoStream->index;
        av_packet_rescale_ts(&pkt, m_videoCodecCtx->time_base, m_videoStream->time_base);
        if (av_interleaved_write_frame(m_fmtCtx, &pkt) < 0)
        {
            av_packet_unref(&pkt);
            return false;
        }
        av_packet_unref(&pkt);
    }
    return true;
}

bool FfmpegWriter::writeAudioFrame(AVFrame *frame)
{
    QMutexLocker locker(&m_mutex);
    if (!m_fmtCtx)
        return false;
    frame->pts = av_rescale_q(frame->pts, m_audioCodecCtx->time_base, m_audioStream->time_base);
    if (avcodec_send_frame(m_audioCodecCtx, frame) < 0)
        return false;
    AVPacket pkt;
    av_init_packet(&pkt);
    while (avcodec_receive_packet(m_audioCodecCtx, &pkt) == 0)
    {
        pkt.stream_index = m_audioStream->index;
        av_packet_rescale_ts(&pkt, m_audioCodecCtx->time_base, m_audioStream->time_base);
        if (av_interleaved_write_frame(m_fmtCtx, &pkt) < 0)
        {
            av_packet_unref(&pkt);
            return false;
        }
        av_packet_unref(&pkt);
    }
    return true;
}

bool FfmpegWriter::needsRollover()
{
    if (!m_cfg.segmented)
        return false;
    qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - m_startMs;
    return elapsed >= (qint64)m_cfg.segmentMinutes * 60 * 1000;
}

void FfmpegWriter::rollover()
{
    QMutexLocker locker(&m_mutex);
    closeContext();
    ++m_segmentIndex;
    m_currentFile = nextFileName();
    openContext(m_currentFile);
}
