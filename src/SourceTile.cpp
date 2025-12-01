#include "SourceTile.h"
#include "ui_SourceTile.h"
#include <QDateTime>
#include <QPixmap>

SourceTile::SourceTile(QWidget *parent)
    : QWidget(parent), ui(new Ui::SourceTile), m_recorder(nullptr)
{
    ui->setupUi(this);
    connect(&m_timer, &QTimer::timeout, this, &SourceTile::updatePreview);
    m_timer.start(500);
}

SourceTile::~SourceTile()
{
    delete ui;
}

void SourceTile::setRecorder(SourceRecorder *recorder)
{
    m_recorder = recorder;
    if (recorder)
    {
        connect(recorder, &SourceRecorder::previewUpdated, this, &SourceTile::updatePreview);
    }
}

void SourceTile::updatePreview()
{
    if (!m_recorder)
        return;
    QImage frame = m_recorder->lastFrame();
    if (!frame.isNull())
    {
        ui->previewLabel->setPixmap(QPixmap::fromImage(frame).scaled(ui->previewLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        ui->previewLabel->setText(QString());
    }
    else
    {
        ui->previewLabel->clear();
        ui->previewLabel->setText("No preview");
    }
    ui->statusLabel->setText(m_recorder->status());
    int secs = m_recorder->elapsedMs() / 1000;
    ui->timerLabel->setText(QString("%1:%2").arg(secs / 60, 2, 10, QChar('0')).arg(secs % 60, 2, 10, QChar('0')));
}

void SourceTile::on_startButton_clicked()
{
    if (m_recorder)
        m_recorder->start();
}

void SourceTile::on_stopButton_clicked()
{
    if (m_recorder)
        m_recorder->stop();
}

void SourceTile::on_pauseButton_clicked()
{
    if (!m_recorder)
        return;
    if (m_recorder->status() == "Paused")
        m_recorder->resume();
    else
        m_recorder->pause();
}

void SourceTile::on_settingsButton_clicked()
{
    emit settingsRequested(m_recorder);
}
