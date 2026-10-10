#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include <QPointF>
#include <QRectF>
#include <QSizeF>

#include "map_view.hpp"

TEST_CASE("map view pan updates the camera translation", "[map][view]") {
    MapView view;

    view.applyPan(QPointF(12.0, -7.0));

    REQUIRE(view.translationX() == 12.0f);
    REQUIRE(view.translationY() == -7.0f);
}

TEST_CASE("map view zoom keeps the cursor anchored", "[map][view]") {
    MapView view;
    view.applyPan(QPointF(9.0, -4.0));
    QPointF const cursor(73.0, 19.0);
    QSizeF const canvas(120.0, 80.0);
    QPointF const offset = cursor - QPointF(canvas.width() * 0.5, canvas.height() * 0.5);
    double const worldX = (offset.x() - view.translationX()) / view.scale();
    double const worldY = (offset.y() - view.translationY()) / view.scale();

    view.applyZoom(3.0f, cursor, canvas);

    REQUIRE(std::abs((offset.x() - view.translationX()) / view.scale() - worldX) < 0.00001);
    REQUIRE(std::abs((offset.y() - view.translationY()) / view.scale() - worldY) < 0.00001);
}

TEST_CASE("map view clamps zoom depth and fits world bounds", "[map][view]") {
    MapView view;

    REQUIRE(view.setZDepth(100.0f));
    REQUIRE(view.zDepth() == 2.0f);
    REQUIRE(view.setZDepth(-100.0f));
    REQUIRE(view.zDepth() == -4.0f);
    REQUIRE(view.fitScale(QRectF(0.0, 0.0, 200.0, 100.0), QSizeF(100.0, 100.0)) == 0.46);
    REQUIRE(view.fitScale(QRectF{}, QSizeF(100.0, 100.0)) == 0.0);
}
