#pragma once

#include <QAbstractListModel>
#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>
#include <QtQml/qqmlregistration.h>

struct ConversationBlock {
    int kind = 1;
    QString text;
    QString payload;
};

class ConversationModel : public QAbstractListModel {
    Q_OBJECT
    QML_NAMED_ELEMENT(ConversationModel)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum BlockKind {
        UserText = 0,
        AssistantText = 1,
        Reasoning = 2,
        ToolCall = 3,
        ToolResult = 4,
        Error = 5,
    };
    Q_ENUM(BlockKind)

    enum Role {
        KindRole = Qt::UserRole + 1,
        TextRole,
        PayloadRole,
    };
    Q_ENUM(Role)

    explicit ConversationModel(QObject* parent = nullptr);

    auto rowCount(QModelIndex const& parent = QModelIndex()) const -> int override;
    auto data(QModelIndex const& index, int role) const -> QVariant override;
    auto roleNames() const -> QHash<int, QByteArray> override;

    auto count() const -> int;

signals:
    auto countChanged() -> void;

public:
    auto clear() -> void;
    auto append(ConversationBlock const& block) -> void;
    auto appendText(int row, QString const& text) -> void;
    auto setText(int row, QString const& text) -> void;
    auto lastRow() const -> int;

    Q_INVOKABLE auto kindAt(int row) const -> int;
    Q_INVOKABLE auto heightHint(int row, qreal width) const -> qreal;

private:
    QList<ConversationBlock> m_blocks;
};
