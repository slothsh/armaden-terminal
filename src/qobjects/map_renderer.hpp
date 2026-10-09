#pragma once

#include <QCanvasPainterItemRenderer>

#include <memory>

class MapRenderer final : public QCanvasPainterItemRenderer {
public:
    MapRenderer();
    ~MapRenderer() override;

    MapRenderer(MapRenderer const&) = delete;
    auto operator=(MapRenderer const&) -> MapRenderer& = delete;

    auto initializeResources(QCanvasPainter* painter) -> void override;
    auto synchronizeData(QCanvasPainterItem* item) -> void override;
    auto paint(QCanvasPainter* painter) -> void override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
