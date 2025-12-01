#pragma once
#include <QObject>
#include <QStringList>
#include <QMutex>
#include <Processing.NDI.Lib.h>

class NdiManager : public QObject
{
    Q_OBJECT
public:
    explicit NdiManager(QObject *parent = nullptr);
    ~NdiManager();

    QStringList availableSources();
    void refreshSources();

private:
    void ensureInitialized();
    NDIlib_find_instance_t m_finder;
    QStringList m_sources;
    QMutex m_mutex;
};
