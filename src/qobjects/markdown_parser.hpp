#pragma once

#include <QObject>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class MarkdownParser : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(MarkdownParser)

public:
    explicit MarkdownParser(QObject* parent = nullptr);

    Q_INVOKABLE auto parse(QString const& source) const -> QVariantList;
};
