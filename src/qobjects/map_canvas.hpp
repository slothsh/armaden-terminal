#pragma once

#include <QCanvasPainterItem>
#include <QPointer>
#include <QPointF>
#include <QtQml/qqmlregistration.h>

#include "map_controller.hpp"

class QCanvasPainterItemRenderer;
class QMouseEvent;
class QWheelEvent;

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
    void setController(MapController* controller);

    [[nodiscard]] auto translationX() const -> float;
    void setTranslationX(float translation);

    [[nodiscard]] auto translationY() const -> float;
    void setTranslationY(float translation);

    [[nodiscard]] auto zDepth() const -> float;
    void setZDepth(float depth);

signals:
    void controllerChanged();
    void translationXChanged();
    void translationYChanged();
    void zDepthChanged();

public slots:
    void handleTileUpdate();

protected:
    QCanvasPainterItemRenderer* createItemRenderer() const override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    QPointer<MapController> m_controller;
    float m_translationX = 0.0f;
    float m_translationY = 0.0f;
    float m_zDepth = 0.0f;
    QPointF m_lastDragPosition;
    bool m_dragging = false;
};
