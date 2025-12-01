#include "SourceSettingsDialog.h"
#include "ui_SourceSettingsDialog.h"
#include <QFileDialog>

SourceSettingsDialog::SourceSettingsDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::SourceSettingsDialog)
{
    ui->setupUi(this);
    refreshNdi();
    refreshAudio();
    connect(ui->refreshNdiButton, &QPushButton::clicked, this, &SourceSettingsDialog::refreshNdi);
    connect(ui->refreshAudioButton, &QPushButton::clicked, this, &SourceSettingsDialog::refreshAudio);
    connect(ui->chooseFolderButton, &QPushButton::clicked, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, tr("Output Folder"), ui->folderEdit->text());
        if (!dir.isEmpty())
            ui->folderEdit->setText(dir);
    });
}

SourceSettingsDialog::~SourceSettingsDialog()
{
    delete ui;
}

void SourceSettingsDialog::setSettings(const SourceSettings &settings)
{
    m_settings = settings;
    ui->ndiCombo->setCurrentText(settings.ndiSource);
    ui->audioCombo->setCurrentText(settings.audioDevice);
    ui->labelEdit->setText(settings.label);
    ui->folderEdit->setText(settings.outputFolder);
    ui->segmentSpin->setValue(settings.segmentMinutes);
    ui->modeSegmented->setChecked(settings.segmented);
    ui->modeContinuous->setChecked(!settings.segmented);
}

SourceSettings SourceSettingsDialog::settings() const
{
    SourceSettings s;
    s.ndiSource = ui->ndiCombo->currentText();
    s.audioDevice = ui->audioCombo->currentText();
    s.outputFolder = ui->folderEdit->text();
    s.label = ui->labelEdit->text();
    s.segmentMinutes = ui->segmentSpin->value();
    s.segmented = ui->modeSegmented->isChecked();
    return s;
}

void SourceSettingsDialog::refreshNdi()
{
    ui->ndiCombo->clear();
    ui->ndiCombo->addItems(m_ndi.availableSources());
}

void SourceSettingsDialog::refreshAudio()
{
    ui->audioCombo->clear();
    ui->audioCombo->addItems(m_audio.inputDevices());
}

void SourceSettingsDialog::on_buttonBox_accepted()
{
    m_settings = settings();
    accept();
}
