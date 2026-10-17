#include "qml_hot_reload.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTextStream>

#include <memory>

namespace {

auto isSingletonQmlFile(QString const& path) -> bool {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    return QString::fromUtf8(file.readAll()).contains("pragma Singleton");
}

auto writeFileContents(QString const& path, QString const& contents) -> bool {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    file.write(contents.toUtf8());
    return true;
}

auto linkFile(QString const& target, QString const& link) -> bool {
    QFile::remove(link);
    return QFile::link(target, link);
}

} // END OF NAMESPACE anonymous

namespace mts::app {

QmlHotReload::QmlHotReload(QString const& defaultSourceDirectory, QObject* parent)
    : QObject(parent) {
    QString resolved;
    QString error;

    if (!this->resolveSourceDirectory(defaultSourceDirectory, resolved, error)) {
        this->setLastError(error);
        return;
    }

    m_sourceDirectory = resolved;
    m_mainSource = QUrl::fromLocalFile(resolved + QStringLiteral("/Main.qml"));

    m_importDir = std::make_unique<QTemporaryDir>(
        QDir::temp().filePath(QStringLiteral("mindtosis-hotreload-XXXXXX"))
    );

    if (!m_importDir->isValid()) {
        this->setLastError(QStringLiteral("Failed to create the hot reload import directory."));
        return;
    }

    if (!this->buildHotReloadImportRoot(resolved, m_importDir->path(), error)) {
        this->setLastError(error);
    }
}

auto QmlHotReload::revision() const -> int {
    return m_revision;
}

auto QmlHotReload::sourceDirectory() const -> QString const& {
    return m_sourceDirectory;
}

auto QmlHotReload::mainSource() const -> QUrl const& {
    return m_mainSource;
}

auto QmlHotReload::lastError() const -> QString const& {
    return m_lastError;
}

auto QmlHotReload::reload() -> void {
    ++m_revision;
    emit this->revisionChanged();
    emit this->reloadRequested();
}

auto QmlHotReload::reportError(QString const& message) -> void {
    this->setLastError(message);
    emit this->errorReported(message);
}

auto QmlHotReload::hotReloadImportPaths() -> QStringList {
    if (m_importDir == nullptr || !m_importDir->isValid()) {
        return {};
    }

    return {m_importDir->path()};
}

auto QmlHotReload::resolveSourceDirectory(
    QString const& defaultSourceDirectory,
    QString& resolved,
    QString& error
) -> bool {
    QString candidate = qEnvironmentVariable("MTS_QML_SOURCE_DIR");
    if (candidate.isEmpty()) {
        candidate = defaultSourceDirectory;
    }

    QFileInfo const info(candidate);
    if (!info.exists() || !info.isDir()) {
        error = QStringLiteral("QML source directory does not exist: %1").arg(candidate);
        return false;
    }

    QFileInfo const entry(candidate + QStringLiteral("/Main.qml"));
    if (!entry.exists() || !entry.isFile()) {
        error = QStringLiteral("QML source directory has no Main.qml: %1").arg(candidate);
        return false;
    }

    resolved = QDir(candidate).absolutePath();
    return true;
}

auto QmlHotReload::buildHotReloadImportRoot(
    QString const& sourceDirectory,
    QString const& importRoot,
    QString& error
) -> bool {
    struct ModuleSpec {
        QString uri;
        QString sourceSubDir;
    };

    QList<ModuleSpec> const modules = {
        {QStringLiteral("Lib.Theme"), QStringLiteral("theme")},
        {QStringLiteral("Lib.Controls"), QStringLiteral("controls")},
    };

    QDir const rootDir(importRoot);
    if (!rootDir.exists() && !QDir().mkpath(importRoot)) {
        error = QStringLiteral("Failed to create hot reload import root: %1").arg(importRoot);
        return false;
    }

    for (ModuleSpec const& module : modules) {
        QString moduleDirPath = importRoot + QStringLiteral("/") + module.uri;
        moduleDirPath.replace(QStringLiteral("."), QStringLiteral("/"));

        if (!QDir().mkpath(moduleDirPath)) {
            error = QStringLiteral("Failed to create module directory: %1").arg(moduleDirPath);
            return false;
        }

        QDir const moduleSourceDir(sourceDirectory + QStringLiteral("/") + module.sourceSubDir);
        if (!moduleSourceDir.exists()) {
            error = QStringLiteral("Hot reload module source directory does not exist: %1")
                        .arg(moduleSourceDir.absolutePath());
            return false;
        }

        QStringList const qmlFiles = moduleSourceDir.entryList({QStringLiteral("*.qml")}, QDir::Files);
        if (qmlFiles.isEmpty()) {
            error = QStringLiteral("Hot reload module has no QML files: %1")
                        .arg(moduleSourceDir.absolutePath());
            return false;
        }

        QString qmldir = QStringLiteral("module %1\n").arg(module.uri);
        for (QString const& qmlFile : qmlFiles) {
            if (qmlFile.startsWith(QStringLiteral("._"))) {
                continue;
            }

            QString const target = moduleSourceDir.absoluteFilePath(qmlFile);
            if (!linkFile(target, moduleDirPath + QStringLiteral("/") + qmlFile)) {
                error = QStringLiteral("Failed to link %1 into the hot reload module.").arg(target);
                return false;
            }

            QString const typeName = QFileInfo(qmlFile).completeBaseName();
            QString const prefix = isSingletonQmlFile(target) ? QStringLiteral("singleton ") : QString();
            qmldir += QStringLiteral("%1%2 1.0 %3\n").arg(prefix, typeName, qmlFile);
        }

        if (!writeFileContents(moduleDirPath + QStringLiteral("/qmldir"), qmldir)) {
            error = QStringLiteral("Failed to write qmldir for module %1.").arg(module.uri);
            return false;
        }
    }

    return true;
}

auto QmlHotReload::setLastError(QString const& message) -> void {
    if (m_lastError == message) {
        return;
    }

    m_lastError = message;
    emit this->lastErrorChanged();
}

} // END OF NAMESPACE `mts::app`
