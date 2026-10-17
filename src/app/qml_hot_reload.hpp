#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QUrl>

namespace mts::app {

class QmlHotReload : public QObject {
    Q_OBJECT
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)
    Q_PROPERTY(QString sourceDirectory READ sourceDirectory CONSTANT)
    Q_PROPERTY(QUrl mainSource READ mainSource CONSTANT)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    explicit QmlHotReload(QString const& defaultSourceDirectory, QObject* parent);

    [[nodiscard]] auto revision() const -> int;
    [[nodiscard]] auto sourceDirectory() const -> QString const&;
    [[nodiscard]] auto mainSource() const -> QUrl const&;
    [[nodiscard]] auto lastError() const -> QString const&;

    Q_INVOKABLE auto reload() -> void;
    Q_INVOKABLE auto reportError(QString const& message) -> void;

    [[nodiscard]] auto hotReloadImportPaths() -> QStringList;

    static auto resolveSourceDirectory(
        QString const& defaultSourceDirectory,
        QString& resolved,
        QString& error
    ) -> bool;
    static auto buildHotReloadImportRoot(
        QString const& sourceDirectory,
        QString const& importRoot,
        QString& error
    ) -> bool;

signals:
    auto reloadRequested() -> void;
    auto errorReported(QString const& message) -> void;
    auto revisionChanged() -> void;
    auto lastErrorChanged() -> void;

private:
    auto setLastError(QString const& message) -> void;

    int m_revision = 0;
    QString m_sourceDirectory;
    QUrl m_mainSource;
    QString m_lastError;
    std::unique_ptr<QTemporaryDir> m_importDir;
};

} // END OF NAMESPACE `mts::app`
