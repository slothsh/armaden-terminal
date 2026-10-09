#pragma once

#include <QAbstractListModel>
#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>
#include <QtQml/qqmlregistration.h>

struct SessionEntry {
    QString id;
    QString title;
    qint64 updatedAt = 0;
    bool running = false;
};

class SessionListModel : public QAbstractListModel {
    Q_OBJECT
    QML_NAMED_ELEMENT(SessionListModel)

public:
    enum Role {
        SessionIdRole = Qt::UserRole + 1,
        TitleRole,
        UpdatedAtRole,
        RunningRole,
    };
    Q_ENUM(Role)

    explicit SessionListModel(QObject* parent = nullptr);

    auto rowCount(QModelIndex const& parent = QModelIndex()) const -> int override;
    auto data(QModelIndex const& index, int role) const -> QVariant override;
    auto roleNames() const -> QHash<int, QByteArray> override;

    auto replace(QList<SessionEntry> const& entries) -> void;
    auto upsert(SessionEntry const& entry) -> void;
    auto setRunning(QString const& id, bool running) -> void;
    auto indexOf(QString const& id) const -> int;

private:
    QList<SessionEntry> m_entries;
};
