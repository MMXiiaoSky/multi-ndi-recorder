#include "NdiManager.h"
#include "Logging.h"
#include <QMutexLocker>

NdiManager::NdiManager(QObject *parent)
    : QObject(parent), m_finder(nullptr)
{
    ensureInitialized();
}

NdiManager::~NdiManager()
{
    if (m_finder)
    {
        NDIlib_find_destroy(m_finder);
        m_finder = nullptr;
    }
    NDIlib_destroy();
}

void NdiManager::ensureInitialized()
{
    if (!NDIlib_initialize())
    {
        Logger::instance().log("Failed to initialize NDI");
    }
    if (!m_finder)
    {
        m_finder = NDIlib_find_create_v2();
        if (!m_finder)
        {
            Logger::instance().log("Failed to create NDI finder");
        }
    }
}

QStringList NdiManager::availableSources()
{
    QMutexLocker locker(&m_mutex);
    refreshSources();
    return m_sources;
}

void NdiManager::refreshSources()
{
    if (!m_finder)
    {
        ensureInitialized();
    }
    uint32_t no_sources = 0;
    const NDIlib_source_t *sources = NDIlib_find_get_current_sources(m_finder, &no_sources);
    QStringList list;
    for (uint32_t i = 0; i < no_sources; ++i)
    {
        list << QString::fromUtf8(sources[i].p_ndi_name);
    }
    m_sources = list;
}
