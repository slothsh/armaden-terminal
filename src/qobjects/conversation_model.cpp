#include "conversation_model.hpp"

#include <QtGlobal>

#include <algorithm>

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

auto ConversationModel::kindAt(int row) const -> int {
    if (row < 0 || row >= m_blocks.size()) {
        return AssistantText;
    }

    return m_blocks.at(row).kind;
}

auto ConversationModel::heightHint(int row, qreal width) const -> qreal {
    if (row < 0 || row >= m_blocks.size()) {
        return 0.0;
    }

    ConversationBlock const& block = m_blocks.at(row);

    constexpr qreal kBlockPadding = 24.0;
    constexpr qreal kPlainLineHeight = 19.0;
    constexpr qreal kCodeLineHeight = 17.0;
    constexpr qreal kMarkdownLineHeight = 21.0;
    constexpr qreal kCharacterWidth = 7.2;
    constexpr qreal kMinimumContentWidth = 160.0;

    qreal const contentWidth = std::max(kMinimumContentWidth, width - 2.0 * kBlockPadding);
    int const charactersPerLine = std::max(12, static_cast<int>(contentWidth / kCharacterWidth));

    QString const& source = block.kind == ToolCall || block.kind == ToolResult ? block.payload : block.text;

    int lines = 0;
    for (qsizetype start = 0; start < source.size();) {
        qsizetype const newline = source.indexOf(QLatin1Char('\n'), start);
        qsizetype const end = newline < 0 ? source.size() : newline;
        qsizetype const length = end - start;
        int const wrapped = static_cast<int>((length + charactersPerLine - 1) / charactersPerLine);
        lines += std::max(1, wrapped);
        if (newline < 0) {
            break;
        }
        start = newline + 1;
    }
    lines = std::max(1, lines);

    switch (block.kind) {
        case UserText:
            return kBlockPadding + lines * kPlainLineHeight + 8.0;

        case AssistantText:
            return kBlockPadding + lines * kMarkdownLineHeight + 24.0;

        case Reasoning:
            return kBlockPadding + lines * kPlainLineHeight;

        case ToolCall:
            return kBlockPadding + 42.0 + lines * kPlainLineHeight;

        case ToolResult:
            return kBlockPadding + 42.0 + std::min(lines, 10) * kCodeLineHeight;

        case Error:
            return kBlockPadding + lines * kPlainLineHeight;
    }

    return kBlockPadding + lines * kPlainLineHeight;
}

auto ConversationModel::count() const -> int {
    return static_cast<int>(m_blocks.size());
}
