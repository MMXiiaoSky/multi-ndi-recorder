#pragma once
#include <QDialog>
#include "NdiManager.h"
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
    void on_buttonBox_accepted();

private:
    Ui::SourceSettingsDialog *ui;
    NdiManager m_ndi;
    SourceSettings m_settings;
};
