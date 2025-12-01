#pragma once
#include <QObject>
#include <QStringList>
#include <QMutex>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <atlbase.h>

class AudioDeviceManager : public QObject
{
    Q_OBJECT
public:
    explicit AudioDeviceManager(QObject *parent = nullptr);
    ~AudioDeviceManager();

    QStringList inputDevices();
    void refresh();
    IMMDevice *deviceByName(const QString &name);

private:
    void enumerate();
    QStringList m_devices;
    CComPtr<IMMDeviceEnumerator> m_enum;
    QMutex m_mutex;
};
