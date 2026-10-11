#include <catch2/catch_test_macros.hpp>

#include "markdown_image_service.hpp"

#include <QBuffer>
#include <QCoreApplication>
#include <QEventLoop>
#include <QImage>
#include <QNetworkAccessManager>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

import std;
import config;

namespace {

auto application() -> QCoreApplication& {
    static int argumentCount = 1;
    static char applicationName[] = "markdown-image-service-test";
    static char* arguments[] = {applicationName, nullptr};
    static QCoreApplication instance(argumentCount, arguments);

    return instance;
}

auto imageData(char const* format) -> QByteArray {
    if (QString::fromLatin1(format).compare(QStringLiteral("GIF"), Qt::CaseInsensitive) == 0) {
        return QByteArray::fromBase64("R0lGODlhAQABAAD/ACwAAAAAAQABAAACADs=");
    }

    QImage image(2, 2, QImage::Format_RGB32);
    image.fill(Qt::red);

    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, format);

    return bytes;
}

auto runHttpServer(QTcpServer& server, QByteArray body, QByteArray mime, int* requestCount = nullptr) -> void;

auto loadsImageFormat(char const* format, QByteArray mime, QString path) -> bool {
    static_cast<void>(application());

    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, 0)) {
        return false;
    }
    runHttpServer(server, imageData(format), std::move(mime));

    mts::config::MarkdownImageConfig config;
    MarkdownImageService service(config);
    bool loaded = false;
    QString failure;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&service, &MarkdownImageService::imageReady, &loop,
        [&loaded, &loop](QString, QString, bool, QSize) {
            loaded = true;
            loop.quit();
        }
    );
    QObject::connect(&service, &MarkdownImageService::imageFailed, &loop,
        [&failure, &loop](QString, QString reason) {
            failure = std::move(reason);
            loop.quit();
        }
    );
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

    QString const source = QStringLiteral("http://127.0.0.1:%1/%2?test=%3")
        .arg(server.serverPort())
        .arg(path)
        .arg(QCoreApplication::applicationPid());
    service.requestImage(source);
    timeout.start(5000);
    loop.exec();

    return loaded && failure.isEmpty();
}

auto runHttpServer(QTcpServer& server, QByteArray body, QByteArray mime, int* requestCount) -> void {
    QObject::connect(&server, &QTcpServer::newConnection, &server, [&server, body = std::move(body), mime = std::move(mime), requestCount]() mutable {
        while (server.hasPendingConnections()) {
            QTcpSocket* const socket = server.nextPendingConnection();
            QObject::connect(socket, &QTcpSocket::readyRead, socket, [socket, body, mime, requestCount] {
                static_cast<void>(socket->readAll());
                if (requestCount != nullptr) {
                    *requestCount += 1;
                }
                QByteArray response = "HTTP/1.1 200 OK\r\nContent-Type: " + mime
                    + "\r\nContent-Length: " + QByteArray::number(body.size())
                    + "\r\nConnection: close\r\n\r\n" + body;
                socket->write(response);
                socket->disconnectFromHost();
            });
        }
    });
}

auto runRedirectServer(QTcpServer& server, QByteArray body, int& requestCount) -> void {
    QObject::connect(&server, &QTcpServer::newConnection, &server, [&server, body = std::move(body), &requestCount]() mutable {
        while (server.hasPendingConnections()) {
            QTcpSocket* const socket = server.nextPendingConnection();
            QObject::connect(socket, &QTcpSocket::readyRead, socket, [socket, body, &requestCount] {
                QByteArray const request = socket->readAll();
                requestCount += 1;
                if (request.startsWith("GET /redirect")) {
                    socket->write("HTTP/1.1 302 Found\r\nLocation: /image.png\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                } else {
                    QByteArray response = "HTTP/1.1 200 OK\r\nContent-Type: image/png\r\nContent-Length: "
                        + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
                    socket->write(response);
                }
                socket->disconnectFromHost();
            });
        }
    });
}

}

TEST_CASE("Markdown image service asynchronously caches valid local-network PNGs", "[markdown][images]") {
    static_cast<void>(application());

    QTcpServer server;
    REQUIRE(server.listen(QHostAddress::LocalHost, 0));
    runHttpServer(server, imageData("PNG"), "image/png");

    mts::config::MarkdownImageConfig config;
    MarkdownImageService service(config);
    QString loadedUrl;
    QSize loadedSize;
    QString failure;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);

    QObject::connect(&service, &MarkdownImageService::imageReady, &loop,
        [&loadedUrl, &loadedSize, &loop](QString source, QString localUrl, bool, QSize dimensions) {
            loadedUrl = std::move(localUrl);
            loadedSize = dimensions;
            loop.quit();
        }
    );
    QObject::connect(&service, &MarkdownImageService::imageFailed, &loop,
        [&failure, &loop](QString, QString reason) {
            failure = std::move(reason);
            loop.quit();
        }
    );
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

    QString const source = QStringLiteral("http://127.0.0.1:%1/image.png?test=%2")
        .arg(server.serverPort())
        .arg(QCoreApplication::applicationPid());
    service.requestImage(source);
    timeout.start(5000);
    loop.exec();

    REQUIRE(failure.isEmpty());
    REQUIRE_FALSE(loadedUrl.isEmpty());
    REQUIRE(loadedSize == QSize(2, 2));
    REQUIRE(service.cachedImageUrl(source) == loadedUrl);
    REQUIRE(service.cachedImageSize(source) == QSize(2, 2));
}

TEST_CASE("Markdown image service validates redirects and follows HTTP image locations", "[markdown][images]") {
    static_cast<void>(application());

    QTcpServer server;
    REQUIRE(server.listen(QHostAddress::LocalHost, 0));
    int requestCount = 0;
    runRedirectServer(server, imageData("PNG"), requestCount);

    mts::config::MarkdownImageConfig config;
    MarkdownImageService service(config);
    QString loadedUrl;
    QString failure;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&service, &MarkdownImageService::imageReady, &loop,
        [&loadedUrl, &loop](QString, QString localUrl, bool, QSize) {
            loadedUrl = std::move(localUrl);
            loop.quit();
        }
    );
    QObject::connect(&service, &MarkdownImageService::imageFailed, &loop,
        [&failure, &loop](QString, QString reason) {
            failure = std::move(reason);
            loop.quit();
        }
    );
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

    QString const source = QStringLiteral("http://127.0.0.1:%1/redirect?test=%2")
        .arg(server.serverPort())
        .arg(QCoreApplication::applicationPid());
    service.requestImage(source);
    timeout.start(5000);
    loop.exec();

    REQUIRE(failure.isEmpty());
    REQUIRE_FALSE(loadedUrl.isEmpty());
    REQUIRE(requestCount == 2);
}

TEST_CASE("Markdown image service enforces response byte limits", "[markdown][images]") {
    static_cast<void>(application());

    QTcpServer server;
    REQUIRE(server.listen(QHostAddress::LocalHost, 0));
    runHttpServer(server, imageData("PNG"), "image/png");

    mts::config::MarkdownImageConfig config;
    config.maxResponseBytes = 16;
    MarkdownImageService service(config);
    QString failure;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&service, &MarkdownImageService::imageFailed, &loop,
        [&failure, &loop](QString, QString reason) {
            failure = std::move(reason);
            loop.quit();
        }
    );
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

    QString const source = QStringLiteral("http://127.0.0.1:%1/oversized?test=%2")
        .arg(server.serverPort())
        .arg(QCoreApplication::applicationPid());
    service.requestImage(source);
    timeout.start(5000);
    loop.exec();

    REQUIRE(failure.contains("byte limit"));
}

TEST_CASE("Markdown image service accepts JPEG and GIF payloads", "[markdown][images]") {
    REQUIRE(loadsImageFormat("JPEG", "image/jpeg", QStringLiteral("photo.jpg")));
    REQUIRE(loadsImageFormat("GIF", "image/gif", QStringLiteral("animation.gif")));
}

TEST_CASE("Markdown image limits are configurable through the application config", "[markdown][images][config]") {
    auto const parsed = mts::config::Config::fromJson(
        R"({"markdown":{"images":{"allowHttp":false,"allowJpeg":false,"maxConcurrentRequests":8,"maxQueuedRequests":96,"timeoutMilliseconds":25000,"maxResponseBytes":5242880,"maxDecodedPixels":20000000,"maxCacheBytes":67108864,"maxGifFrames":120}}})"
    );
    REQUIRE(parsed.isOk());

    mts::config::MarkdownImageConfig config;
    REQUIRE(mts::config::applyConfig(parsed.value().view(), config).isOk());
    REQUIRE_FALSE(config.allowHttp);
    REQUIRE_FALSE(config.allowJpeg);
    REQUIRE(config.allowHttps);
    REQUIRE(config.maxConcurrentRequests == 8);
    REQUIRE(config.maxQueuedRequests == 96);
    REQUIRE(config.timeoutMilliseconds == 25000);
    REQUIRE(config.maxResponseBytes == 5242880);
    REQUIRE(config.maxDecodedPixels == 20000000);
    REQUIRE(config.maxCacheBytes == 67108864);
    REQUIRE(config.maxGifFrames == 120);
}

TEST_CASE("Markdown image limits reject unsafe configuration ranges", "[markdown][images][config]") {
    auto const parsed = mts::config::Config::fromJson(
        R"({"markdown":{"images":{"maxConcurrentRequests":64}}})"
    );
    REQUIRE(parsed.isOk());

    mts::config::MarkdownImageConfig config;
    REQUIRE(mts::config::applyConfig(parsed.value().view(), config).isErr());
}

TEST_CASE("Markdown image service deduplicates identical active URL requests", "[markdown][images]") {
    static_cast<void>(application());

    QTcpServer server;
    REQUIRE(server.listen(QHostAddress::LocalHost, 0));
    int requestCount = 0;
    runHttpServer(server, imageData("PNG"), "image/png", &requestCount);

    mts::config::MarkdownImageConfig config;
    MarkdownImageService service(config);
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&service, &MarkdownImageService::imageReady, &loop, &QEventLoop::quit);
    QObject::connect(&service, &MarkdownImageService::imageFailed, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

    QString const source = QStringLiteral("http://127.0.0.1:%1/dedup?test=%2")
        .arg(server.serverPort())
        .arg(QCoreApplication::applicationPid());
    service.requestImage(source);
    service.requestImage(source);
    timeout.start(5000);
    loop.exec();

    REQUIRE(service.cachedImageUrl(source).length() > 0);
    REQUIRE(requestCount == 1);
}

TEST_CASE("Markdown image service cancels network work after its final consumer releases it", "[markdown][images]") {
    static_cast<void>(application());

    QTcpServer server;
    REQUIRE(server.listen(QHostAddress::LocalHost, 0));
    QEventLoop requestLoop;
    QObject::connect(&server, &QTcpServer::newConnection, &server, [&server, &requestLoop] {
        QTcpSocket* const socket = server.nextPendingConnection();
        QObject::connect(socket, &QTcpSocket::readyRead, socket, [&requestLoop] {
            requestLoop.quit();
        });
    });

    mts::config::MarkdownImageConfig config;
    MarkdownImageService service(config);
    bool signalled = false;
    QObject::connect(&service, &MarkdownImageService::imageReady, &service, [&signalled] { signalled = true; });
    QObject::connect(&service, &MarkdownImageService::imageFailed, &service, [&signalled] { signalled = true; });
    QString const source = QStringLiteral("http://127.0.0.1:%1/slow").arg(server.serverPort());
    service.requestImage(source);
    QTimer requestTimeout;
    requestTimeout.setSingleShot(true);
    QObject::connect(&requestTimeout, &QTimer::timeout, &requestLoop, &QEventLoop::quit);
    requestTimeout.start(2000);
    requestLoop.exec();
    REQUIRE(requestTimeout.isActive());

    service.releaseImage(source);
    QEventLoop settleLoop;
    QTimer settleTimer;
    settleTimer.setSingleShot(true);
    QObject::connect(&settleTimer, &QTimer::timeout, &settleLoop, &QEventLoop::quit);
    settleTimer.start(100);
    settleLoop.exec();
    REQUIRE_FALSE(signalled);
}

TEST_CASE("Markdown image service rejects non-HTTP sources", "[markdown][images]") {
    static_cast<void>(application());

    mts::config::MarkdownImageConfig config;
    MarkdownImageService service(config);
    QString failure;
    QObject::connect(&service, &MarkdownImageService::imageFailed, &service,
        [&failure](QString, QString reason) {
            failure = std::move(reason);
        }
    );

    service.requestImage(QStringLiteral("file:///etc/passwd"));

    REQUIRE_FALSE(failure.isEmpty());
    REQUIRE(service.cachedImageUrl(QStringLiteral("file:///etc/passwd")).isEmpty());
}
