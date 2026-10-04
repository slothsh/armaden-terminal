#include <QGuiApplication>
#include <QQuickWindow>
#include <QQmlApplicationEngine>
#include <QUrl>

import std;
import logging;

auto main(int argc, char** argv) -> int {
    initialize_logging();
    mts::log::info("Initializing application...");

    QGuiApplication app(argc, argv);
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);
    QQmlApplicationEngine engine;

    mts::log::info("Done! Connecting application to GUI engine...");

    QUrl const url("qrc:/qt/qml/Application/qml/Main.qml");

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        [] { QCoreApplication::exit(EXIT_FAILURE); },
        Qt::QueuedConnection
    );

    engine.load(url);

    mts::log::info("Done! Running application...");

    return app.exec();
}
