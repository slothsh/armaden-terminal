#include "conversation_model.hpp"

ConversationModel::ConversationModel(QObject* parent)
    : QAbstractListModel(parent) {}

auto ConversationModel::rowCount(QModelIndex const& parent) const -> int {
    if (parent.isValid()) {
        return 0;
    }

    return static_cast<int>(m_blocks.size());
}

auto ConversationModel::data(QModelIndex const& index, int role) const -> QVariant {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_blocks.size()) {
        return {};
    }

    ConversationBlock const& block = m_blocks.at(index.row());

    switch (role) {
        case KindRole:
            return block.kind;

        case TextRole:
            return block.text;

        case PayloadRole:
            return block.payload;

        default:
            return {};
    }
}

auto ConversationModel::roleNames() const -> QHash<int, QByteArray> {
    return {
        {KindRole, "kind"},
        {TextRole, "text"},
        {PayloadRole, "payload"},
    };
}

auto ConversationModel::clear() -> void {
    bool const hadBlocks = !m_blocks.isEmpty();

    beginResetModel();
    m_blocks.clear();
    endResetModel();

    if (hadBlocks) {
        emit countChanged();
    }
}

auto ConversationModel::append(ConversationBlock const& block) -> void {
    beginInsertRows(QModelIndex(), m_blocks.size(), m_blocks.size());
    m_blocks.append(block);
    endInsertRows();

    emit countChanged();
}

auto ConversationModel::appendText(int row, QString const& text) -> void {
    if (row < 0 || row >= m_blocks.size()) {
        return;
    }

    m_blocks[row].text += text;

    QModelIndex const changed = index(row);
    emit dataChanged(changed, changed, {TextRole});
}

auto ConversationModel::setText(int row, QString const& text) -> void {
    if (row < 0 || row >= m_blocks.size()) {
        return;
    }

    m_blocks[row].text = text;

    QModelIndex const changed = index(row);
    emit dataChanged(changed, changed, {TextRole});
}

auto ConversationModel::lastRow() const -> int {
    return static_cast<int>(m_blocks.size()) - 1;
}

auto ConversationModel::count() const -> int {
    return static_cast<int>(m_blocks.size());
}
