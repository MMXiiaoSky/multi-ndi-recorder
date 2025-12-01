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
    static NDIlib_find_instance_t s_finder;
    static QStringList s_sources;
    static QMutex s_mutex;
};
