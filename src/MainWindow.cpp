#include "MainWindow.h"
#include "ui_MainWindow.h"
#include <QGridLayout>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    m_libraryModel = new RecordingLibraryModel(this);
    ui->libraryTable->setModel(m_libraryModel);
    connect(&m_masterTimer, &QTimer::timeout, this, &MainWindow::updateMasterTimer);
    m_masterTimer.start(1000);
    connect(ui->openButton, &QPushButton::clicked, this, &MainWindow::openRecording);
    connect(ui->revealButton, &QPushButton::clicked, this, &MainWindow::revealRecording);
    rebuildSources(1);
}

MainWindow::~MainWindow()
{
    for (auto rec : m_recorders)
    {
        rec->stop();
        delete rec;
    }
    delete ui;
}

void MainWindow::rebuildSources(int count)
{
    QLayoutItem *child;
    while ((child = ui->gridLayout->takeAt(0)) != nullptr)
    {
        delete child->widget();
        delete child;
    }
    m_tiles.clear();
    m_recorders.clear();

    for (int i = 0; i < count; ++i)
    {
        SourceRecorder *rec = new SourceRecorder(this);
        SourceTile *tile = new SourceTile(this);
        tile->setRecorder(rec);
        connect(tile, &SourceTile::settingsRequested, this, &MainWindow::handleSettings);
        connect(rec, &SourceRecorder::recordingStarted, this, [this, rec](const QString &file) {
            RecordingEntry e;
            e.fullPath = file;
            QFileInfo info(file);
            e.filename = info.fileName();
            e.sourceLabel = rec->settings().label;
            e.timestamp = info.lastModified();
            e.size = info.size();
            m_libraryModel->addEntry(e);
        });
        int row = i / 2;
        int col = i % 2;
        ui->gridLayout->addWidget(tile, row, col);
        m_recorders.append(rec);
        m_tiles.append(tile);
    }
}

void MainWindow::on_sourceCountSpin_valueChanged(int value)
{
    rebuildSources(value);
}

void MainWindow::on_startAllButton_clicked()
{
    for (auto rec : m_recorders)
        rec->start();
}

void MainWindow::on_stopAllButton_clicked()
{
    for (auto rec : m_recorders)
        rec->stop();
}

void MainWindow::on_pauseAllButton_clicked()
{
    for (auto rec : m_recorders)
    {
        if (rec->status() == "Paused")
            rec->resume();
        else
            rec->pause();
    }
}

void MainWindow::handleSettings(SourceRecorder *recorder)
{
    SourceSettingsDialog dlg(this);
    dlg.setSettings(recorder->settings());
    if (dlg.exec() == QDialog::Accepted)
    {
        recorder->applySettings(dlg.settings());
    }
}

void MainWindow::updateMasterTimer()
{
    int total = 0;
    for (auto rec : m_recorders)
    {
        if (rec->status() == "Recording" || rec->status() == "Paused")
            ++total;
    }
    ui->masterStatusLabel->setText(QString("Active sources: %1").arg(total));
}

void MainWindow::openRecording()
{
    QModelIndex idx = ui->libraryTable->currentIndex();
    if (!idx.isValid())
        return;
    QString path = m_libraryModel->data(m_libraryModel->index(idx.row(), 2), Qt::DisplayRole).toString();
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void MainWindow::revealRecording()
{
    QModelIndex idx = ui->libraryTable->currentIndex();
    if (!idx.isValid())
        return;
    QString path = m_libraryModel->data(m_libraryModel->index(idx.row(), 2), Qt::DisplayRole).toString();
    QString cmd = QString("explorer.exe /select,\"%1\"").arg(path);
    system(cmd.toUtf8().constData());
}
