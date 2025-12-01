#pragma once
#include <QWidget>
#include <QTimer>
#include "SourceRecorder.h"

namespace Ui { class SourceTile; }

class SourceTile : public QWidget
{
    Q_OBJECT
public:
    explicit SourceTile(QWidget *parent = nullptr);
    ~SourceTile();

    void setRecorder(SourceRecorder *recorder);
    SourceRecorder *recorder() const { return m_recorder; }

signals:
    void settingsRequested(SourceRecorder *recorder);

private slots:
    void updatePreview();
    void on_startButton_clicked();
    void on_stopButton_clicked();
    void on_pauseButton_clicked();
    void on_settingsButton_clicked();

private:
    Ui::SourceTile *ui;
    SourceRecorder *m_recorder;
    QTimer m_timer;
};
