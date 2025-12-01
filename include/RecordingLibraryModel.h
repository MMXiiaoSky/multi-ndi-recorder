#pragma once
#include <QAbstractTableModel>
#include <QVector>
#include <QFileInfo>

struct RecordingEntry
{
    QString sourceLabel;
    QString filename;
    QString fullPath;
    QDateTime timestamp;
    qint64 size;
};

class RecordingLibraryModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit RecordingLibraryModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void addEntry(const RecordingEntry &entry);
    void scanFolders(const QStringList &folders);

private:
    QVector<RecordingEntry> m_entries;
};
