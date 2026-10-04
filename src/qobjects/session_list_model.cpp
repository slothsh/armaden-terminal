#include "session_list_model.hpp"

SessionListModel::SessionListModel(QObject* parent)
    : QAbstractListModel(parent) {}

int SessionListModel::rowCount(QModelIndex const& parent) const {
    if (parent.isValid())
        return 0;
    return static_cast<int>(m_entries.size());
}

QVariant SessionListModel::data(QModelIndex const& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};

    SessionEntry const& entry = m_entries.at(index.row());
    switch (role) {
        case SessionIdRole: return entry.id;
        case TitleRole: return entry.title;
        case UpdatedAtRole: return entry.updatedAt;
        default: return {};
    }
}

QHash<int, QByteArray> SessionListModel::roleNames() const {
    return {
        {SessionIdRole, "sessionId"},
        {TitleRole, "title"},
        {UpdatedAtRole, "updatedAt"},
    };
}

void SessionListModel::replace(QList<SessionEntry> const& entries) {
    beginResetModel();
    m_entries = entries;
    endResetModel();
}

void SessionListModel::upsert(SessionEntry const& entry) {
    for (int row = 0; row < m_entries.size(); ++row) {
        if (m_entries.at(row).id == entry.id) {
            m_entries[row] = entry;
            QModelIndex const changed = index(row);
            emit dataChanged(changed, changed);
            return;
        }
    }

    beginInsertRows(QModelIndex(), m_entries.size(), m_entries.size());
    m_entries.append(entry);
    endInsertRows();
}

int SessionListModel::indexOf(QString const& id) const {
    for (int row = 0; row < m_entries.size(); ++row) {
        if (m_entries.at(row).id == id)
            return row;
    }
    return -1;
}
