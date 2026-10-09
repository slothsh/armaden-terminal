#include "session_list_model.hpp"

SessionListModel::SessionListModel(QObject* parent)
    : QAbstractListModel(parent) {}

auto SessionListModel::rowCount(QModelIndex const& parent) const -> int {
    if (parent.isValid()) {
        return 0;
    }

    return static_cast<int>(m_entries.size());
}

auto SessionListModel::data(QModelIndex const& index, int role) const -> QVariant {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return {};
    }

    SessionEntry const& entry = m_entries.at(index.row());

    switch (role) {
        case SessionIdRole:
            return entry.id;

        case TitleRole:
            return entry.title;

        case UpdatedAtRole:
            return entry.updatedAt;

        case RunningRole:
            return entry.running;

        default:
            return {};
    }
}

auto SessionListModel::roleNames() const -> QHash<int, QByteArray> {
    return {
        {SessionIdRole, "sessionId"},
        {TitleRole, "title"},
        {UpdatedAtRole, "updatedAt"},
        {RunningRole, "running"},
    };
}

auto SessionListModel::replace(QList<SessionEntry> const& entries) -> void {
    beginResetModel();
    m_entries = entries;
    endResetModel();
}

auto SessionListModel::upsert(SessionEntry const& entry) -> void {
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

auto SessionListModel::setRunning(QString const& id, bool running) -> void {
    for (int row = 0; row < m_entries.size(); ++row) {
        if (m_entries.at(row).id != id) {
            continue;
        }

        if (m_entries.at(row).running == running) {
            return;
        }

        m_entries[row].running = running;

        QModelIndex const changed = index(row);
        emit dataChanged(changed, changed, {RunningRole});
        return;
    }
}

auto SessionListModel::indexOf(QString const& id) const -> int {
    for (int row = 0; row < m_entries.size(); ++row) {
        if (m_entries.at(row).id == id) {
            return row;
        }
    }

    return -1;
}
