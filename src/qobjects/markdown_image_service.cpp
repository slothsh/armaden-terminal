#include "markdown_image_service.hpp"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QQueue>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QThreadPool>
#include <QUrl>

#include <algorithm>
#include <functional>
#include <limits>
#include <utility>

import std;
import config;

namespace {

struct ValidatedImage {
    bool valid = false;
    bool animated = false;
    QByteArray data;
    QString extension;
    QString error;
    QSize dimensions;
};

auto cacheKey(QString const& source) -> QString {
    QByteArray const digest = QCryptographicHash::hash(source.toUtf8(), QCryptographicHash::Sha256).toHex();

    return QString::fromLatin1(digest);
}

auto cacheExtensions() -> std::array<QString, 3> {
    return {QStringLiteral(".jpg"), QStringLiteral(".png"), QStringLiteral(".gif")};
}

auto validateImage(
    QByteArray data,
    QString contentType,
    mts::config::MarkdownImageConfig const& config
) -> ValidatedImage {
    contentType = contentType.section(QLatin1Char(';'), 0, 0).trimmed().toLower();
    bool const allowedContentType = contentType.isEmpty()
        || contentType == QStringLiteral("application/octet-stream")
        || (config.allowJpeg && (contentType == QStringLiteral("image/jpeg") || contentType == QStringLiteral("image/jpg")))
        || (config.allowPng && contentType == QStringLiteral("image/png"))
        || (config.allowGif && contentType == QStringLiteral("image/gif"));
    if (!allowedContentType) {
        return ValidatedImage{false, false, {}, {}, QStringLiteral("Unsupported image content type")};
    }

    QBuffer buffer(&data);
    if (!buffer.open(QIODevice::ReadOnly)) {
        return ValidatedImage{false, false, {}, {}, QStringLiteral("Image data could not be read")};
    }

    QImageReader reader(&buffer);
    reader.setDecideFormatFromContent(true);
    QByteArray const format = reader.format().toLower();
    bool const allowedFormat = ((format == "jpg" || format == "jpeg") && config.allowJpeg)
        || (format == "png" && config.allowPng)
        || (format == "gif" && config.allowGif);
    if (!allowedFormat) {
        return ValidatedImage{false, false, {}, {}, QStringLiteral("Unsupported image format")};
    }

    QSize const dimensions = reader.size();
    quint64 const pixels = dimensions.width() > 0 && dimensions.height() > 0
        ? static_cast<quint64>(dimensions.width()) * static_cast<quint64>(dimensions.height())
        : std::numeric_limits<quint64>::max();
    if (dimensions.isEmpty() || pixels > config.maxDecodedPixels) {
        return ValidatedImage{false, false, {}, {}, QStringLiteral("Image dimensions exceed configured limits")};
    }

    int const frameCount = format == "gif" ? reader.imageCount() : 1;
    bool const animate = format == "gif"
        && reader.supportsAnimation()
        && frameCount > 1
        && static_cast<std::uint32_t>(frameCount) <= config.maxGifFrames
        && pixels * static_cast<quint64>(frameCount) <= config.maxDecodedPixels;
    QImage const firstFrame = reader.read();
    if (firstFrame.isNull()) {
        return ValidatedImage{false, false, {}, {}, QStringLiteral("Image data could not be decoded")};
    }

    if (format == "gif" && !animate) {
        QByteArray firstFrameData;
        QBuffer firstFrameBuffer(&firstFrameData);
        if (!firstFrameBuffer.open(QIODevice::WriteOnly) || !firstFrame.save(&firstFrameBuffer, "PNG")) {
            return ValidatedImage{false, false, {}, {}, QStringLiteral("GIF frame could not be cached")};
        }

        return ValidatedImage{true, false, std::move(firstFrameData), QStringLiteral(".png"), {}, dimensions};
    }

    QString extension = format == "jpg" || format == "jpeg"
        ? QStringLiteral(".jpg")
        : (format == "png" ? QStringLiteral(".png") : QStringLiteral(".gif"));

    return ValidatedImage{true, animate, std::move(data), std::move(extension), {}, dimensions};
}

auto allowedUrl(QUrl const& url, mts::config::MarkdownImageConfig const& config) -> bool {
    if (!url.isValid() || url.host().isEmpty() || !url.userName().isEmpty() || !url.password().isEmpty()) {
        return false;
    }

    return (url.scheme() == QStringLiteral("http") && config.allowHttp)
        || (url.scheme() == QStringLiteral("https") && config.allowHttps);
}

auto pumpRequests(std::weak_ptr<std::function<void()>> const& pending) -> void {
    if (auto const pump = pending.lock(); pump != nullptr) {
        (*pump)();
    }
}

}

struct MarkdownImageService::State {
    struct Transfer {
        QUrl url;
        QByteArray data;
        QString contentType;
        std::uint32_t redirectCount = 0;
        QString error;
    };

    explicit State(mts::config::MarkdownImageConfig const& config)
        : m_config(config)
        , m_cacheDirectory(QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/markdown-images"))
        , m_manager(new QNetworkAccessManager) {
        QDir().mkpath(m_cacheDirectory);
    }

    mts::config::MarkdownImageConfig m_config;
    QString m_cacheDirectory;
    QNetworkAccessManager* m_manager = nullptr;
    QQueue<QString> m_pending;
    QHash<QString, QUrl> m_pendingUrls;
    QHash<QString, std::uint32_t> m_pendingRedirects;
    QHash<QString, QPointer<QNetworkReply>> m_active;
    QHash<QString, Transfer> m_transfers;
    QSet<QString> m_validating;
    QHash<QString, int> m_references;
    std::shared_ptr<std::function<void()>> m_pump;
};

MarkdownImageService::MarkdownImageService(
    mts::config::MarkdownImageConfig const& config,
    QObject* parent
)
    : QObject(parent)
    , m_state(std::make_unique<State>(config)) {
    m_state->m_manager->setParent(this);
}

MarkdownImageService::~MarkdownImageService() = default;

auto MarkdownImageService::revision() const -> int {
    return m_revision;
}

auto MarkdownImageService::cachedImageUrl(QString const& source) -> QString {
    QString const key = cacheKey(source);
    for (QString const& extension : cacheExtensions()) {
        QString const path = m_state->m_cacheDirectory + QLatin1Char('/') + key + extension;
        QFileInfo const info(path);
        if (info.isFile() && info.size() > 0) {
            QFile file(path);
            if (file.open(QIODevice::ReadOnly)) {
                file.setFileTime(QDateTime::currentDateTimeUtc(), QFileDevice::FileModificationTime);
            }
            return QUrl::fromLocalFile(path).toString();
        }
    }

    return {};
}

auto MarkdownImageService::cachedImageSize(QString const& source) const -> QSize {
    QString const key = cacheKey(source);
    for (QString const& extension : cacheExtensions()) {
        QString const path = m_state->m_cacheDirectory + QLatin1Char('/') + key + extension;
        QFileInfo const info(path);
        if (info.isFile() && info.size() > 0) {
            QImageReader reader(path);
            return reader.size();
        }
    }

    return {};
}

auto MarkdownImageService::requestImage(QString const& source) -> QString {
    if (QString const cached = this->cachedImageUrl(source); !cached.isEmpty()) {
        return cached;
    }

    QUrl const url(source, QUrl::StrictMode);
    if (source.size() > 8192 || !allowedUrl(url, m_state->m_config)) {
        emit this->imageFailed(source, QStringLiteral("Invalid or unsupported HTTP(S) image source"));
        m_revision += 1;
        emit this->revisionChanged();

        return {};
    }

    if (m_state->m_active.contains(source) || m_state->m_pendingUrls.contains(source) || m_state->m_validating.contains(source)) {
        m_state->m_references[source] += 1;
        return {};
    }
    if (m_state->m_active.size() + m_state->m_validating.size() >= static_cast<qsizetype>(m_state->m_config.maxConcurrentRequests)
        && m_state->m_pending.size() >= static_cast<qsizetype>(m_state->m_config.maxQueuedRequests)) {
        emit this->imageFailed(source, QStringLiteral("Image request queue is full"));
        m_revision += 1;
        emit this->revisionChanged();

        return {};
    }

    m_state->m_references.insert(source, 1);
    m_state->m_pending.enqueue(source);
    m_state->m_pendingUrls.insert(source, url);
    m_state->m_pendingRedirects.insert(source, 0);

    std::shared_ptr<std::function<void()>> launchNext = m_state->m_pump;
    if (launchNext == nullptr) {
        launchNext = std::make_shared<std::function<void()>>();
        std::weak_ptr<std::function<void()>> const weakLaunchNext = launchNext;
        *launchNext = [this, weakLaunchNext] {
            while (m_state->m_active.size() + m_state->m_validating.size() < static_cast<qsizetype>(m_state->m_config.maxConcurrentRequests)
                && !m_state->m_pending.isEmpty()) {
                QString const nextSource = m_state->m_pending.dequeue();
                QUrl const nextUrl = m_state->m_pendingUrls.take(nextSource);
                std::uint32_t const redirectCount = m_state->m_pendingRedirects.take(nextSource);
                QNetworkRequest request(nextUrl);
                request.setTransferTimeout(static_cast<int>(m_state->m_config.timeoutMilliseconds));
                request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
                request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
                request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
                request.setAttribute(QNetworkRequest::AuthenticationReuseAttribute, QNetworkRequest::Manual);
                request.setRawHeader("User-Agent", "mindtosis");

                QNetworkReply* const reply = m_state->m_manager->get(request);
                m_state->m_active.insert(nextSource, reply);
                m_state->m_transfers.insert(nextSource, State::Transfer{nextUrl, {}, {}, redirectCount, {}});

                QObject::connect(reply, &QIODevice::readyRead, this, [this, nextSource, reply] {
                    auto found = m_state->m_transfers.find(nextSource);
                    if (found == m_state->m_transfers.end()) {
                        return;
                    }

                    found->data += reply->readAll();
                    if (static_cast<quint64>(found->data.size()) > m_state->m_config.maxResponseBytes) {
                        found->error = QStringLiteral("Image response exceeds configured byte limit");
                        reply->abort();
                    }
                });

                QObject::connect(reply, &QNetworkReply::finished, this, [this, nextSource, reply, weakLaunchNext] {
                    auto found = m_state->m_transfers.find(nextSource);
                    if (found == m_state->m_transfers.end()) {
                        reply->deleteLater();
                        return;
                    }
                    if (!m_state->m_references.contains(nextSource)) {
                        m_state->m_active.remove(nextSource);
                        m_state->m_transfers.remove(nextSource);
                        reply->deleteLater();
                        pumpRequests(weakLaunchNext);
                        return;
                    }

                    found->data += reply->readAll();
                    if (static_cast<quint64>(found->data.size()) > m_state->m_config.maxResponseBytes) {
                        found->error = QStringLiteral("Image response exceeds configured byte limit");
                    }

                    QUrl const redirect = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
                    if (found->error.isEmpty() && redirect.isValid()) {
                        QUrl const target = found->url.resolved(redirect);
                        std::uint32_t const count = found->redirectCount + 1;
                        m_state->m_active.remove(nextSource);
                        m_state->m_transfers.remove(nextSource);
                        reply->deleteLater();
                        if (count > m_state->m_config.maxRedirects || !allowedUrl(target, m_state->m_config)) {
                            m_state->m_references.remove(nextSource);
                            m_revision += 1;
                            emit this->imageFailed(nextSource, QStringLiteral("Image redirect was rejected"));
                            emit this->revisionChanged();
                            pumpRequests(weakLaunchNext);
                            return;
                        }

                        m_state->m_pending.prepend(nextSource);
                        m_state->m_pendingUrls.insert(nextSource, target);
                        m_state->m_pendingRedirects.insert(nextSource, count);
                        pumpRequests(weakLaunchNext);
                        return;
                    }

                    QString error = found->error;
                    if (error.isEmpty() && reply->error() != QNetworkReply::NoError) {
                        error = reply->errorString();
                    }
                    int const status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                    if (error.isEmpty() && status >= 400) {
                        error = QStringLiteral("Image server returned HTTP %1").arg(status);
                    }
                    QString const contentType = QString::fromLatin1(reply->header(QNetworkRequest::ContentTypeHeader).toByteArray());
                    QByteArray data = std::move(found->data);
                    m_state->m_active.remove(nextSource);
                    m_state->m_transfers.remove(nextSource);
                    reply->deleteLater();

                    if (!error.isEmpty()) {
                        m_state->m_references.remove(nextSource);
                        m_revision += 1;
                        emit this->imageFailed(nextSource, error);
                        emit this->revisionChanged();
                        pumpRequests(weakLaunchNext);
                        return;
                    }

                    m_state->m_validating.insert(nextSource);
                    mts::config::MarkdownImageConfig const config = m_state->m_config;
                    QPointer<MarkdownImageService> const self(this);
                    QThreadPool::globalInstance()->start(QRunnable::create([self, nextSource, payload = std::move(data), contentType, config, weakLaunchNext]() mutable {
                        ValidatedImage const image = validateImage(
                            std::move(payload),
                            contentType,
                            config
                        );
                        if (self.isNull()) {
                            return;
                        }

                        QMetaObject::invokeMethod(self, [self, nextSource, image, weakLaunchNext] {
                            if (self.isNull()) {
                                return;
                            }

                            self->m_state->m_validating.remove(nextSource);
                            if (!self->m_state->m_references.contains(nextSource)) {
                                pumpRequests(weakLaunchNext);
                                return;
                            }
                            if (!image.valid) {
                                self->m_state->m_references.remove(nextSource);
                                self->m_revision += 1;
                                emit self->imageFailed(nextSource, image.error);
                                emit self->revisionChanged();
                                pumpRequests(weakLaunchNext);
                                return;
                            }

                            if (static_cast<quint64>(image.data.size()) > self->m_state->m_config.maxCacheBytes) {
                                self->m_state->m_references.remove(nextSource);
                                self->m_revision += 1;
                                emit self->imageFailed(nextSource, QStringLiteral("Image exceeds configured cache capacity"));
                                emit self->revisionChanged();
                                pumpRequests(weakLaunchNext);
                                return;
                            }

                            QString const key = cacheKey(nextSource);
                            for (QString const& extension : cacheExtensions()) {
                                if (extension != image.extension) {
                                    QFile::remove(self->m_state->m_cacheDirectory + QLatin1Char('/') + key + extension);
                                }
                            }
                            QString const path = self->m_state->m_cacheDirectory + QLatin1Char('/') + key + image.extension;
                            QSaveFile file(path);
                            if (!file.open(QIODevice::WriteOnly)
                                || file.write(image.data) != image.data.size()
                                || !file.commit()) {
                                self->m_state->m_references.remove(nextSource);
                                self->m_revision += 1;
                                emit self->imageFailed(nextSource, QStringLiteral("Image cache could not be written"));
                                emit self->revisionChanged();
                                pumpRequests(weakLaunchNext);
                                return;
                            }

                            auto const files = QDir(self->m_state->m_cacheDirectory).entryInfoList(
                                {QStringLiteral("*.jpg"), QStringLiteral("*.png"), QStringLiteral("*.gif")},
                                QDir::Files,
                                QDir::Time | QDir::Reversed
                            );
                            quint64 cacheBytes = 0;
                            for (QFileInfo const& info : files) {
                                cacheBytes += static_cast<quint64>(info.size());
                            }
                            for (QFileInfo const& info : files) {
                                if (cacheBytes <= self->m_state->m_config.maxCacheBytes) {
                                    break;
                                }
                                if (info.filePath() == path) {
                                    continue;
                                }
                                cacheBytes -= static_cast<quint64>(info.size());
                                QFile::remove(info.filePath());
                            }

                            self->m_state->m_references.remove(nextSource);
                            self->m_revision += 1;
                            emit self->imageReady(nextSource, QUrl::fromLocalFile(path).toString(), image.animated, image.dimensions);
                            emit self->revisionChanged();
                            pumpRequests(weakLaunchNext);
                        }, Qt::QueuedConnection);
                    }));
                    pumpRequests(weakLaunchNext);
                });
            }

            if (m_state->m_active.isEmpty() && m_state->m_pending.isEmpty() && m_state->m_validating.isEmpty()) {
                m_state->m_pump.reset();
            }
        };
        m_state->m_pump = launchNext;
    }

    pumpRequests(m_state->m_pump);

    return {};
}

auto MarkdownImageService::releaseImage(QString const& source) -> void {
    auto reference = m_state->m_references.find(source);
    if (reference == m_state->m_references.end()) {
        return;
    }

    *reference -= 1;
    if (*reference > 0) {
        return;
    }
    m_state->m_references.erase(reference);

    if (m_state->m_pendingUrls.remove(source) > 0) {
        m_state->m_pending.removeAll(source);
        m_state->m_pendingRedirects.remove(source);
        pumpRequests(m_state->m_pump);
        return;
    }

    if (QPointer<QNetworkReply> const reply = m_state->m_active.value(source); reply != nullptr) {
        reply->abort();
    }
}
