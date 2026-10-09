#include <catch2/catch_test_macros.hpp>

import std;
import core;
import runtime;
import imagery;

using namespace mts::imagery;

namespace {

class FakeTileSource {
public:
    FakeTileSource(int tileSize, int tilesAcross)
        : m_tileSize(tileSize)
        , m_tilesAcross(tilesAcross) {}

    [[nodiscard]] auto tileSize() const -> int {
        return this->m_tileSize;
    }

    [[nodiscard]] auto levelCount() const -> int {
        return 1;
    }

    [[nodiscard]] auto levelInfo(int level) const -> LevelInfo {
        return levelInfoForBase(this->baseSize(), this->m_tileSize, level);
    }

    [[nodiscard]] auto baseSize() const -> QSize {
        return QSize(this->m_tilesAcross * this->m_tileSize, this->m_tileSize);
    }

    [[nodiscard]] auto levelReady(int) const -> bool {
        return true;
    }

    auto startBuild(mts::runtime::ThreadPool&) -> void {}

    [[nodiscard]] auto readTile(TileId const& tile) -> mts::core::Result<SharedImage> {
        if (tile.x < 0 || tile.x >= this->m_tilesAcross || tile.y != 0) {
            return mts::core::Result<SharedImage>::err(ImageryErrorKind::NotFound, "no tile");
        }

        SharedImage image(this->m_tileSize, this->m_tileSize, QImage::Format_ARGB32_Premultiplied);
        image.fill(0xFFFF0000u);

        return image;
    }

private:
    int m_tileSize;
    int m_tilesAcross;
};

static_assert(TileSource<FakeTileSource>);
static_assert(TileSource<AnyTileSource>);

}

TEST_CASE("any tile source erases a conforming source", "[imagery][contract]") {
    FakeTileSource const fake(16, 3);
    AnyTileSource const erased = fake;

    REQUIRE(erased.tileSize() == 16);
    REQUIRE(erased.levelCount() == 1);
    REQUIRE(erased.baseSize() == QSize(48, 16));
    REQUIRE(erased.levelReady(0));
}

TEST_CASE("any tile source reads tiles through the erased source", "[imagery][contract]") {
    AnyTileSource erased = FakeTileSource(16, 3);

    auto const tile = erased.readTile(TileId{0, 1, 0});

    REQUIRE(tile.isOk());
    REQUIRE(tile.value().size() == QSize(16, 16));
}

TEST_CASE("any tile source propagates underlying errors", "[imagery][contract]") {
    AnyTileSource erased = FakeTileSource(16, 1);

    auto const tile = erased.readTile(TileId{0, 5, 0});

    REQUIRE(tile.isErr());
    REQUIRE(tile.error().kind<ImageryErrorKind>() == ImageryErrorKind::NotFound);
}
