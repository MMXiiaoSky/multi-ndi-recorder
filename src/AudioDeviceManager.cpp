#include "AudioDeviceManager.h"
#include "Logging.h"
#include <atlbase.h>
#include <Functiondiscoverykeys_devpkey.h>

AudioDeviceManager::AudioDeviceManager(QObject *parent)
    : QObject(parent)
{
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    HRESULT hr = m_enum.CoCreateInstance(__uuidof(MMDeviceEnumerator));
    if (FAILED(hr))
    {
        Logger::instance().log("Failed to create MMDeviceEnumerator: " + QString::number(hr, 16));
    }
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
    HRESULT hr = m_enum->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &collection);
    if (FAILED(hr))
    {
        Logger::instance().log("Failed to enumerate audio endpoints: " + QString::number(hr, 16));
        return;
    }
    UINT count = 0;
    hr = collection->GetCount(&count);
    if (FAILED(hr))
    {
        Logger::instance().log("Failed to get audio endpoint count: " + QString::number(hr, 16));
        return;
    }
    for (UINT i = 0; i < count; ++i)
    {
        CComPtr<IMMDevice> device;
        if (SUCCEEDED(collection->Item(i, &device)))
        {
            CComPtr<IPropertyStore> props;
            hr = device->OpenPropertyStore(STGM_READ, &props);
            if (FAILED(hr) || !props)
            {
                Logger::instance().log("Failed to open property store for device " + QString::number(i) + ": " + QString::number(hr, 16));
                continue;
            }
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
    HRESULT hr = m_enum->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &collection);
    if (FAILED(hr))
    {
        return nullptr;
    }
    UINT count = 0;
    if (FAILED(collection->GetCount(&count)))
    {
        return nullptr;
    }
    for (UINT i = 0; i < count; ++i)
    {
        CComPtr<IMMDevice> device;
        if (SUCCEEDED(collection->Item(i, &device)))
        {
            CComPtr<IPropertyStore> props;
            if (FAILED(device->OpenPropertyStore(STGM_READ, &props)) || !props)
            {
                continue;
            }
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
