#pragma once

#include <QCanvasPainterItem>
#include <QPointF>

class QCanvasPainterItemRenderer;
class QMouseEvent;
class QWheelEvent;

class PainterCanvas : public QCanvasPainterItem {
    Q_OBJECT
    QML_NAMED_ELEMENT(PainterCanvas)
    Q_PROPERTY(float translationX READ translationX WRITE setTranslationX NOTIFY translationXChanged)
    Q_PROPERTY(float translationY READ translationY WRITE setTranslationY NOTIFY translationYChanged)
    Q_PROPERTY(float zDepth READ zDepth WRITE setZDepth NOTIFY zDepthChanged)

public:
    explicit PainterCanvas(QQuickItem* parent = nullptr);

    float translationX() const;
    void setTranslationX(float translation);

    float translationY() const;
    void setTranslationY(float translation);

    float zDepth() const;
    void setZDepth(float depth);

signals:
    void translationXChanged();
    void translationYChanged();
    void zDepthChanged();

protected:
    QCanvasPainterItemRenderer* createItemRenderer() const override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    float m_translationX = 0.0f;
    float m_translationY = 0.0f;
    float m_zDepth = 0.0f;
    QPointF m_lastDragPosition;
    bool m_dragging = false;
};
