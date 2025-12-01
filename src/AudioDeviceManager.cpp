#include "AudioDeviceManager.h"
#include "Logging.h"
#include <atlbase.h>

AudioDeviceManager::AudioDeviceManager(QObject *parent)
    : QObject(parent)
{
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    m_enum.CoCreateInstance(__uuidof(MMDeviceEnumerator));
    enumerate();
}

AudioDeviceManager::~AudioDeviceManager()
{
    CoUninitialize();
}

void AudioDeviceManager::enumerate()
{
    QMutexLocker locker(&m_mutex);
    m_devices.clear();
    if (!m_enum)
        return;
    CComPtr<IMMDeviceCollection> collection;
    if (FAILED(m_enum->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &collection)))
    {
        Logger::instance().log("Failed to enumerate audio endpoints");
        return;
    }
    UINT count = 0;
    collection->GetCount(&count);
    for (UINT i = 0; i < count; ++i)
    {
        CComPtr<IMMDevice> device;
        if (SUCCEEDED(collection->Item(i, &device)))
        {
            CComPtr<IPropertyStore> props;
            device->OpenPropertyStore(STGM_READ, &props);
            PROPVARIANT varName;
            PropVariantInit(&varName);
            if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &varName)))
            {
                m_devices << QString::fromWCharArray(varName.pwszVal);
                PropVariantClear(&varName);
            }
        }
    }
}

QStringList AudioDeviceManager::inputDevices()
{
    enumerate();
    return m_devices;
}

void AudioDeviceManager::refresh()
{
    enumerate();
}

IMMDevice *AudioDeviceManager::deviceByName(const QString &name)
{
    if (!m_enum)
        return nullptr;
    CComPtr<IMMDeviceCollection> collection;
    if (FAILED(m_enum->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &collection)))
    {
        return nullptr;
    }
    UINT count = 0;
    collection->GetCount(&count);
    for (UINT i = 0; i < count; ++i)
    {
        CComPtr<IMMDevice> device;
        if (SUCCEEDED(collection->Item(i, &device)))
        {
            CComPtr<IPropertyStore> props;
            device->OpenPropertyStore(STGM_READ, &props);
            PROPVARIANT varName;
            PropVariantInit(&varName);
            if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &varName)))
            {
                QString devName = QString::fromWCharArray(varName.pwszVal);
                PropVariantClear(&varName);
                if (devName == name)
                {
                    device.p->AddRef();
                    return device;
                }
            }
        }
    }
    return nullptr;
}
