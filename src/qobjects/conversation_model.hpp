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

    int rowCount(QModelIndex const& parent = QModelIndex()) const override;
    QVariant data(QModelIndex const& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void clear();
    void append(ConversationBlock const& block);
    void appendText(int row, QString const& text);
    void setText(int row, QString const& text);
    int lastRow() const;

private:
    QList<ConversationBlock> m_blocks;
};
