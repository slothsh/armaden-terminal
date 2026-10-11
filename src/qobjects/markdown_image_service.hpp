#pragma once

#include <QObject>
#include <QSize>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include <cstdint>
#include <memory>

namespace mts::config {
struct MarkdownImageConfig;
}

class MarkdownImageService : public QObject {
    Q_OBJECT
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)

public:
    explicit MarkdownImageService(
        mts::config::MarkdownImageConfig const&,
        QObject* parent = nullptr
    );
    ~MarkdownImageService() override;

    [[nodiscard]] auto revision() const -> int;

    Q_INVOKABLE auto cachedImageUrl(QString const& source) -> QString;
    Q_INVOKABLE auto cachedImageSize(QString const& source) const -> QSize;
    Q_INVOKABLE auto requestImage(QString const& source) -> QString;
    Q_INVOKABLE auto releaseImage(QString const& source) -> void;

signals:
    auto imageReady(QString source, QString localUrl, bool animated, QSize dimensions) -> void;
    auto imageFailed(QString source, QString reason) -> void;
    auto revisionChanged() -> void;

private:
    struct State;
    std::unique_ptr<State> m_state;
    int m_revision = 0;
};
