#include <catch2/catch_test_macros.hpp>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

#include <cstdlib>

#include "qml_hot_reload.hpp"

namespace {

auto writeFile(QString const& path, QString const& contents) -> void {
    QFile file(path);
    file.open(QIODevice::WriteOnly | QIODevice::Truncate);
    file.write(contents.toUtf8());
}

auto makeFakeSourceTree(QString const& root) -> void {
    QDir().mkpath(root + QStringLiteral("/theme"));
    QDir().mkpath(root + QStringLiteral("/controls"));
    QDir().mkpath(root + QStringLiteral("/views"));
    writeFile(root + QStringLiteral("/Main.qml"), QStringLiteral("import QtQuick\nItem {}\n"));
    writeFile(root + QStringLiteral("/theme/Theme.qml"), QStringLiteral("pragma Singleton\nimport QtQml\nQtObject {}\n"));
    writeFile(root + QStringLiteral("/theme/._Junk.qml"), QStringLiteral("junk\n"));
    writeFile(root + QStringLiteral("/controls/TextButton.qml"), QStringLiteral("import QtQuick\nItem {}\n"));
    writeFile(root + QStringLiteral("/controls/DrawerSide.qml"), QStringLiteral("pragma Singleton\nimport QtQml\nQtObject {}\n"));
}

auto readAll(QString const& path) -> QString {
    QFile file(path);
    file.open(QIODevice::ReadOnly);
    return QString::fromUtf8(file.readAll());
}

} // namespace

TEST_CASE("source directory resolution prefers the environment override", "[hot_reload]") {
    QTemporaryDir const defaultDir;
    QTemporaryDir const overrideDir;
    makeFakeSourceTree(defaultDir.path());
    makeFakeSourceTree(overrideDir.path());

    qputenv("MTS_QML_SOURCE_DIR", overrideDir.path().toUtf8());

    QString resolved;
    QString error;
    bool const ok = mts::app::QmlHotReload::resolveSourceDirectory(defaultDir.path(), resolved, error);

    qunsetenv("MTS_QML_SOURCE_DIR");

    REQUIRE(ok);
    REQUIRE(resolved == QDir(overrideDir.path()).absolutePath());
}

TEST_CASE("source directory resolution falls back to the default when the override is empty", "[hot_reload]") {
    QTemporaryDir const defaultDir;
    makeFakeSourceTree(defaultDir.path());

    qputenv("MTS_QML_SOURCE_DIR", QByteArrayLiteral(""));

    QString resolved;
    QString error;
    bool const ok = mts::app::QmlHotReload::resolveSourceDirectory(defaultDir.path(), resolved, error);

    qunsetenv("MTS_QML_SOURCE_DIR");

    REQUIRE(ok);
    REQUIRE(resolved == QDir(defaultDir.path()).absolutePath());
}

TEST_CASE("source directory resolution rejects a missing directory", "[hot_reload]") {
    qputenv("MTS_QML_SOURCE_DIR", QString{QDir::temp().filePath(QStringLiteral("mindtosis-missing-dir"))}.toUtf8());

    QString resolved;
    QString error;
    bool const ok = mts::app::QmlHotReload::resolveSourceDirectory(QStringLiteral("/nowhere"), resolved, error);

    qunsetenv("MTS_QML_SOURCE_DIR");

    REQUIRE_FALSE(ok);
    REQUIRE(error.contains(QStringLiteral("does not exist")));
    REQUIRE(error.contains(QStringLiteral("mindtosis-missing-dir")));
}

TEST_CASE("source directory resolution rejects a directory without Main.qml", "[hot_reload]") {
    QTemporaryDir const noEntry;
    QDir().mkpath(noEntry.path() + QStringLiteral("/theme"));
    QDir().mkpath(noEntry.path() + QStringLiteral("/controls"));

    qputenv("MTS_QML_SOURCE_DIR", noEntry.path().toUtf8());

    QString resolved;
    QString error;
    bool const ok = mts::app::QmlHotReload::resolveSourceDirectory(QStringLiteral("/nowhere"), resolved, error);

    qunsetenv("MTS_QML_SOURCE_DIR");

    REQUIRE_FALSE(ok);
    REQUIRE(error.contains(QStringLiteral("Main.qml")));
}

TEST_CASE("hot reload import root builds shadow modules with qmldirs", "[hot_reload]") {
    QTemporaryDir const sourceDir;
    QTemporaryDir const importRoot;
    makeFakeSourceTree(sourceDir.path());

    QString error;
    bool const ok = mts::app::QmlHotReload::buildHotReloadImportRoot(
        sourceDir.path(),
        importRoot.path() + QStringLiteral("/imports"),
        error
    );

    REQUIRE(ok);

    QString const themeQmldir = readAll(importRoot.path() + QStringLiteral("/imports/Lib/Theme/qmldir"));
    REQUIRE(themeQmldir.contains(QStringLiteral("module Lib.Theme\n")));
    REQUIRE(themeQmldir.contains(QStringLiteral("singleton Theme 1.0 Theme.qml\n")));
    REQUIRE_FALSE(themeQmldir.contains(QStringLiteral("Junk")));

    QString const controlsQmldir = readAll(importRoot.path() + QStringLiteral("/imports/Lib/Controls/qmldir"));
    REQUIRE(controlsQmldir.contains(QStringLiteral("module Lib.Controls\n")));
    REQUIRE(controlsQmldir.contains(QStringLiteral("singleton DrawerSide 1.0 DrawerSide.qml\n")));
    REQUIRE(controlsQmldir.contains(QStringLiteral("TextButton 1.0 TextButton.qml\n")));
    REQUIRE_FALSE(controlsQmldir.contains(QStringLiteral("singleton TextButton")));
}

TEST_CASE("hot reload import root rejects a missing module source directory", "[hot_reload]") {
    QTemporaryDir const sourceDir;
    QTemporaryDir const importRoot;
    QDir().mkpath(sourceDir.path() + QStringLiteral("/theme"));
    writeFile(sourceDir.path() + QStringLiteral("/theme/Theme.qml"), QStringLiteral("pragma Singleton\nimport QtQml\nQtObject {}\n"));

    QString error;
    bool const ok = mts::app::QmlHotReload::buildHotReloadImportRoot(
        sourceDir.path(),
        importRoot.path() + QStringLiteral("/imports"),
        error
    );

    REQUIRE_FALSE(ok);
    REQUIRE(error.contains(QStringLiteral("controls")));
}

TEST_CASE("controller exposes the resolved source and counts reload revisions", "[hot_reload]") {
    QTemporaryDir const defaultDir;
    makeFakeSourceTree(defaultDir.path());

    qputenv("MTS_QML_SOURCE_DIR", defaultDir.path().toUtf8());

    mts::app::QmlHotReload controller(QStringLiteral("/nowhere"), nullptr);

    qunsetenv("MTS_QML_SOURCE_DIR");

    REQUIRE(controller.sourceDirectory() == QDir(defaultDir.path()).absolutePath());
    REQUIRE(controller.mainSource().toLocalFile().endsWith(QStringLiteral("/Main.qml")));
    REQUIRE(controller.hotReloadImportPaths().size() == 1);
    REQUIRE(controller.lastError().isEmpty());

    int revisions = 0;
    QObject::connect(&controller, &mts::app::QmlHotReload::revisionChanged, [&revisions] { ++revisions; });

    controller.reload();
    controller.reload();

    REQUIRE(controller.revision() == 2);
    REQUIRE(revisions == 2);

    controller.reportError(QStringLiteral("boom"));
    REQUIRE(controller.lastError() == QStringLiteral("boom"));
}
