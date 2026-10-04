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
};

class SessionListModel : public QAbstractListModel {
    Q_OBJECT
    QML_NAMED_ELEMENT(SessionListModel)

public:
    enum Role {
        SessionIdRole = Qt::UserRole + 1,
        TitleRole,
        UpdatedAtRole,
    };
    Q_ENUM(Role)

    explicit SessionListModel(QObject* parent = nullptr);

    int rowCount(QModelIndex const& parent = QModelIndex()) const override;
    QVariant data(QModelIndex const& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void replace(QList<SessionEntry> const& entries);
    void upsert(SessionEntry const& entry);
    int indexOf(QString const& id) const;

private:
    QList<SessionEntry> m_entries;
};
