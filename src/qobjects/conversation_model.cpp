#include "conversation_model.hpp"

ConversationModel::ConversationModel(QObject* parent)
    : QAbstractListModel(parent) {}

int ConversationModel::rowCount(QModelIndex const& parent) const {
    if (parent.isValid())
        return 0;
    return static_cast<int>(m_blocks.size());
}

QVariant ConversationModel::data(QModelIndex const& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_blocks.size())
        return {};

    ConversationBlock const& block = m_blocks.at(index.row());
    switch (role) {
        case KindRole: return block.kind;
        case TextRole: return block.text;
        case PayloadRole: return block.payload;
        default: return {};
    }
}

QHash<int, QByteArray> ConversationModel::roleNames() const {
    return {
        {KindRole, "kind"},
        {TextRole, "text"},
        {PayloadRole, "payload"},
    };
}

void ConversationModel::clear() {
    beginResetModel();
    m_blocks.clear();
    endResetModel();
}

void ConversationModel::append(ConversationBlock const& block) {
    beginInsertRows(QModelIndex(), m_blocks.size(), m_blocks.size());
    m_blocks.append(block);
    endInsertRows();
}

void ConversationModel::appendText(int row, QString const& text) {
    if (row < 0 || row >= m_blocks.size())
        return;
    m_blocks[row].text += text;
    QModelIndex const changed = index(row);
    emit dataChanged(changed, changed, {TextRole});
}

void ConversationModel::setText(int row, QString const& text) {
    if (row < 0 || row >= m_blocks.size())
        return;
    m_blocks[row].text = text;
    QModelIndex const changed = index(row);
    emit dataChanged(changed, changed, {TextRole});
}

int ConversationModel::lastRow() const {
    return static_cast<int>(m_blocks.size()) - 1;
}
