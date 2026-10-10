#pragma once

#include <QCanvasPainterItem>
#include <QPointer>
#include <QtQml/qqmlregistration.h>

#include "map_controller.hpp"
#include "map_view.hpp"

class QCanvasPainterItemRenderer;
class QTimer;

class MapCanvas : public QCanvasPainterItem {
    Q_OBJECT
    QML_NAMED_ELEMENT(MapCanvas)
    Q_PROPERTY(MapController* controller READ controller WRITE setController NOTIFY controllerChanged)
    Q_PROPERTY(float translationX READ translationX WRITE setTranslationX NOTIFY translationXChanged)
    Q_PROPERTY(float translationY READ translationY WRITE setTranslationY NOTIFY translationYChanged)
    Q_PROPERTY(float zDepth READ zDepth WRITE setZDepth NOTIFY zDepthChanged)

public:
    explicit MapCanvas(QQuickItem* parent = nullptr);

    [[nodiscard]] auto controller() const -> MapController*;
    auto setController(MapController* controller) -> void;

    [[nodiscard]] auto translationX() const -> float;
    auto setTranslationX(float translation) -> void;

    [[nodiscard]] auto translationY() const -> float;
    auto setTranslationY(float translation) -> void;

    [[nodiscard]] auto zDepth() const -> float;
    auto setZDepth(float depth) -> void;

    [[nodiscard]] auto view() const -> MapView const&;

    Q_INVOKABLE auto fitToMap() -> void;
    Q_INVOKABLE auto interactionPan(qreal dx, qreal dy) -> void;
    Q_INVOKABLE auto interactionZoom(qreal delta, qreal x, qreal y) -> void;

signals:
    auto controllerChanged() -> void;
    auto translationXChanged() -> void;
    auto translationYChanged() -> void;
    auto zDepthChanged() -> void;

public slots:
    auto handleTileUpdate() -> void;

protected:
    auto componentComplete() -> void override;
    auto createItemRenderer() const -> QCanvasPainterItemRenderer* override;

private:
    auto maybeApplyInitialView() -> void;

    QPointer<MapController> m_controller;
    QTimer* m_idleTimer = nullptr;
    QMetaObject::Connection m_readyConnection;
    bool m_initialViewApplied = false;
    MapView m_view;
};
