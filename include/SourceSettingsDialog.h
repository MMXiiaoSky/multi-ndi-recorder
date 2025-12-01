#pragma once
#include <QDialog>
#include "NdiManager.h"
#include "AudioDeviceManager.h"
#include "SourceRecorder.h"

namespace Ui { class SourceSettingsDialog; }

class SourceSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SourceSettingsDialog(QWidget *parent = nullptr);
    ~SourceSettingsDialog();

    void setSettings(const SourceSettings &settings);
    SourceSettings settings() const;

private slots:
    void refreshNdi();
    void refreshAudio();
    void on_buttonBox_accepted();

private:
    Ui::SourceSettingsDialog *ui;
    NdiManager m_ndi;
    AudioDeviceManager m_audio;
    SourceSettings m_settings;
};
