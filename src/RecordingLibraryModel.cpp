#include "RecordingLibraryModel.h"
#include <QDirIterator>

RecordingLibraryModel::RecordingLibraryModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int RecordingLibraryModel::rowCount(const QModelIndex &) const
{
    return m_entries.size();
}

int RecordingLibraryModel::columnCount(const QModelIndex &) const
{
    return 5;
}

QVariant RecordingLibraryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || role != Qt::DisplayRole)
        return QVariant();
    const RecordingEntry &e = m_entries.at(index.row());
    switch (index.column())
    {
    case 0: return e.sourceLabel;
    case 1: return e.filename;
    case 2: return e.fullPath;
    case 3: return e.timestamp.toString(Qt::ISODate);
    case 4: return QString::number(e.size / (1024.0 * 1024.0), 'f', 2) + " MB";
    default: return QVariant();
    }
}

QVariant RecordingLibraryModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
        return QVariant();
    switch (section)
    {
    case 0: return "Source";
    case 1: return "Filename";
    case 2: return "Path";
    case 3: return "Date";
    case 4: return "Size";
    default: return QVariant();
    }
}

void RecordingLibraryModel::addEntry(const RecordingEntry &entry)
{
    beginInsertRows(QModelIndex(), m_entries.size(), m_entries.size());
    m_entries.append(entry);
    endInsertRows();
}

void RecordingLibraryModel::scanFolders(const QStringList &folders)
{
    beginResetModel();
    m_entries.clear();
    for (const QString &folder : folders)
    {
        QDirIterator it(folder, QStringList() << "*.mkv", QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext())
        {
            QFileInfo info(it.next());
            RecordingEntry e;
            e.fullPath = info.absoluteFilePath();
            e.filename = info.fileName();
            e.sourceLabel = info.baseName();
            e.timestamp = info.lastModified();
            e.size = info.size();
            m_entries.append(e);
        }
    }
    endResetModel();
}
