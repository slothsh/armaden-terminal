#include <QImage>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QSizeF>
#include <QtGlobal>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <list>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

export module imagery;

import std;
import agent;
import runtime;

export namespace mts::imagery {

using ::QImage;
using ::QPointF;
using ::QRectF;
using ::QSize;
using ::QSizeF;

using SharedImage = QImage;

enum class TileState : std::uint8_t {
    Missing,
    Queued,
    Ready,
    Failed,
};

struct TileId {
    int level = 0;
    int x = 0;
    int y = 0;

    friend auto operator==(TileId const&, TileId const&) -> bool = default;
};

struct TileIdHash {
    [[nodiscard]] auto operator()(TileId const& tile) const noexcept -> std::size_t {
        std::size_t seed = std::hash<int>{}(tile.level);
        seed ^= std::hash<int>{}(tile.x) + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
        seed ^= std::hash<int>{}(tile.y) + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
        return seed;
    }
};

struct LevelInfo {
    int level = 0;
    int tilesAcross = 1;
    int tilesDown = 1;
    int tileSize = 256;

    [[nodiscard]] auto pixelSize() const -> QSize {
        return QSize(tilesAcross * tileSize, tilesDown * tileSize);
    }

    [[nodiscard]] auto contains(TileId const& tile) const -> bool {
        return tile.level == level
            && tile.x >= 0 && tile.x < tilesAcross
            && tile.y >= 0 && tile.y < tilesDown;
    }
};

[[nodiscard]] auto levelPixelScale(int level) -> double {
    return std::exp2(double(level));
}

[[nodiscard]] auto levelInfoForBase(QSize const& basePixelSize, int tileSize, int level) -> LevelInfo {
    double const levelTileExtent = double(tileSize) * levelPixelScale(level);
    int const tilesAcross = int(std::ceil(double(basePixelSize.width()) / levelTileExtent));
    int const tilesDown = int(std::ceil(double(basePixelSize.height()) / levelTileExtent));
    return LevelInfo{
        level,
        std::clamp(tilesAcross, 1, std::numeric_limits<int>::max()),
        std::clamp(tilesDown, 1, std::numeric_limits<int>::max()),
        tileSize,
    };
}

[[nodiscard]] auto baseRectToLevelRect(QRectF const& baseRect, int level) -> QRectF {
    double const scale = levelPixelScale(level);
    return QRectF(
        QPointF(baseRect.left() / scale, baseRect.top() / scale),
        QSizeF(baseRect.width() / scale, baseRect.height() / scale)
    );
}

[[nodiscard]] auto levelRectToBaseRect(QRectF const& levelRect, int level) -> QRectF {
    double const scale = levelPixelScale(level);
    return QRectF(
        QPointF(levelRect.left() * scale, levelRect.top() * scale),
        QSizeF(levelRect.width() * scale, levelRect.height() * scale)
    );
}

[[nodiscard]] auto tileOrigin(LevelInfo const& level, TileId const& tile) -> QPointF {
    return QPointF(float(tile.x * level.tileSize), float(tile.y * level.tileSize));
}

[[nodiscard]] auto tileRect(LevelInfo const& level, TileId const& tile) -> QRectF {
    return QRectF(
        tileOrigin(level, tile),
        QSizeF(float(level.tileSize), float(level.tileSize))
    );
}

[[nodiscard]] auto tileAt(LevelInfo const& level, QPointF const& levelPixel) -> TileId {
    int const tileX = qBound(0, int(std::floor(levelPixel.x() / float(level.tileSize))), level.tilesAcross - 1);
    int const tileY = qBound(0, int(std::floor(levelPixel.y() / float(level.tileSize))), level.tilesDown - 1);
    return TileId{level.level, tileX, tileY};
}

[[nodiscard]] auto tilesForRect(LevelInfo const& level, QRectF const& levelPixelRect) -> std::vector<TileId> {
    if (levelPixelRect.isEmpty())
        return {};

    QRectF const rect = levelPixelRect.normalized();
    QSize const levelSize = level.pixelSize();
    if (rect.right() < 0.0 || rect.bottom() < 0.0
        || rect.left() > double(levelSize.width()) || rect.top() > double(levelSize.height())) {
        return {};
    }

    int const tileSize = level.tileSize;
    int const firstX = qBound(0, int(std::floor(rect.left() / float(tileSize))), level.tilesAcross - 1);
    int const lastX = qBound(firstX, int(std::ceil(rect.right() / float(tileSize))) - 1, level.tilesAcross - 1);
    int const firstY = qBound(0, int(std::floor(rect.top() / float(tileSize))), level.tilesDown - 1);
    int const lastY = qBound(firstY, int(std::ceil(rect.bottom() / float(tileSize))) - 1, level.tilesDown - 1);

    std::vector<TileId> tiles;
    tiles.reserve(std::size_t((lastX - firstX + 1) * (lastY - firstY + 1)));
    for (int y = firstY; y <= lastY; ++y) {
        for (int x = firstX; x <= lastX; ++x)
            tiles.push_back(TileId{level.level, x, y});
    }
    return tiles;
}

[[nodiscard]] auto orderCenterFirst(std::vector<TileId> tiles, LevelInfo const& level, QPointF const& center) -> std::vector<TileId> {
    float const tileSize = float(level.tileSize);
    auto const distanceToCenter = [tileSize, &center](TileId const& tile) -> double {
        QPointF const tileCenter(
            float(tile.x) * tileSize + tileSize * 0.5f,
            float(tile.y) * tileSize + tileSize * 0.5f
        );
        QPointF const delta = tileCenter - center;
        return double(delta.x() * delta.x() + delta.y() * delta.y());
    };
    std::stable_sort(tiles.begin(), tiles.end(), [&distanceToCenter](TileId const& a, TileId const& b) {
        return distanceToCenter(a) < distanceToCenter(b);
    });
    return tiles;
}

struct LodPolicy {
    double bias = 0.0;
    int minimumLevel = 0;
};

[[nodiscard]] auto selectLevel(double zoom, double devicePixelRatio, int maxLevel, LodPolicy const& policy) -> int {
    int const minimumLevel = std::min(policy.minimumLevel, maxLevel);
    int const maximumLevel = std::max(minimumLevel, maxLevel);
    if (!(zoom > 0.0))
        return minimumLevel;

    double const ratio = std::max(devicePixelRatio, 1.0);
    double const raw = std::log2(zoom / ratio) + policy.bias;
    return qBound(minimumLevel, int(std::round(raw)), maximumLevel);
}

struct ViewportRequest {
    QRectF baseRect;
    QSize baseSize;
    QSizeF viewportSize;
    double devicePixelRatio = 1.0;
    double zoom = 1.0;
    int tileSize = 256;
    int maxLevel = 0;
    LodPolicy lod;
};

struct ViewportTiles {
    int level = 0;
    LevelInfo levelInfo;
    QRectF levelRect;
    std::vector<TileId> tiles;
};

[[nodiscard]] auto resolveViewport(ViewportRequest const& request) -> ViewportTiles {
    ViewportTiles result;
    if (request.baseRect.isEmpty() || request.baseSize.isEmpty() || request.viewportSize.isEmpty())
        return result;

    result.level = selectLevel(request.zoom, request.devicePixelRatio, request.maxLevel, request.lod);
    result.levelInfo = levelInfoForBase(request.baseSize, request.tileSize, result.level);
    result.levelRect = baseRectToLevelRect(request.baseRect, result.level);
    result.tiles = orderCenterFirst(
        tilesForRect(result.levelInfo, result.levelRect),
        result.levelInfo,
        result.levelRect.center()
    );
    return result;
}

template <class Type>
concept TileSource = requires(Type&& source, TileId const& tile, int level) {
    { source.tileSize() } -> std::convertible_to<int>;
    { source.levelCount() } -> std::convertible_to<int>;
    { source.levelInfo(level) } -> std::same_as<LevelInfo>;
    { source.readTile(tile) } -> std::same_as<mts::agent::Result<SharedImage>>;
};

class TileCache {
public:
    struct Entry {
        TileState state = TileState::Missing;
        SharedImage image;
        std::string error;
        qsizetype bytes = 0;
    };

    using Listener = std::function<void(TileId const&, TileState)>;

    explicit TileCache(qsizetype byteBudget = 128 * 1024 * 1024)
        : m_byteBudget(std::max<qsizetype>(byteBudget, 0)) {}

    TileCache(TileCache const&) = delete;
    TileCache& operator=(TileCache const&) = delete;

    [[nodiscard]] auto find(TileId const& tile) const -> std::optional<Entry> {
        std::lock_guard const lock(m_mutex);
        auto const it = m_index.find(tile);
        if (it == m_index.end())
            return std::nullopt;
        m_records.splice(m_records.begin(), m_records, it->second);
        return it->second->second;
    }

    [[nodiscard]] auto state(TileId const& tile) const -> TileState {
        std::lock_guard const lock(m_mutex);
        auto const it = m_index.find(tile);
        return it == m_index.end() ? TileState::Missing : it->second->second.state;
    }

    void markQueued(TileId const& tile) {
        std::vector<Notification> notifications;
        {
            std::lock_guard const lock(m_mutex);
            this->replaceRecord(tile, Entry{TileState::Queued, SharedImage{}, {}, 0});
            notifications.emplace_back(tile, TileState::Queued);
        }
        this->notify(notifications);
    }

    void store(TileId const& tile, SharedImage image) {
        std::vector<Notification> notifications;
        {
            std::lock_guard const lock(m_mutex);
            qsizetype const bytes = image.sizeInBytes();
            this->replaceRecord(tile, Entry{TileState::Ready, std::move(image), {}, bytes});
            m_byteUsage += bytes;
            notifications.emplace_back(tile, TileState::Ready);
            this->evictOverBudget(notifications, tile);
        }
        this->notify(notifications);
    }

    void markFailed(TileId const& tile, std::string error) {
        std::vector<Notification> notifications;
        {
            std::lock_guard const lock(m_mutex);
            this->replaceRecord(tile, Entry{TileState::Failed, SharedImage{}, std::move(error), 0});
            notifications.emplace_back(tile, TileState::Failed);
        }
        this->notify(notifications);
    }

    void evict(TileId const& tile) {
        std::vector<Notification> notifications;
        {
            std::lock_guard const lock(m_mutex);
            this->eraseRecord(tile, notifications);
        }
        this->notify(notifications);
    }

    void clear() {
        std::vector<Notification> notifications;
        {
            std::lock_guard const lock(m_mutex);
            notifications.reserve(m_records.size());
            for (auto const& record : m_records)
                notifications.emplace_back(record.first, TileState::Missing);
            m_records.clear();
            m_index.clear();
            m_byteUsage = 0;
        }
        this->notify(notifications);
    }

    void setByteBudget(qsizetype budget) {
        std::vector<Notification> notifications;
        {
            std::lock_guard const lock(m_mutex);
            m_byteBudget = std::max<qsizetype>(budget, 0);
            this->evictOverBudget(notifications, std::nullopt);
        }
        this->notify(notifications);
    }

    [[nodiscard]] auto byteBudget() const -> qsizetype {
        std::lock_guard const lock(m_mutex);
        return m_byteBudget;
    }

    [[nodiscard]] auto byteUsage() const -> qsizetype {
        std::lock_guard const lock(m_mutex);
        return m_byteUsage;
    }

    [[nodiscard]] auto size() const -> std::size_t {
        std::lock_guard const lock(m_mutex);
        return m_index.size();
    }

    auto addListener(Listener listener) -> std::size_t {
        std::lock_guard const lock(m_mutex);
        std::size_t const id = m_nextListenerId++;
        m_listeners.emplace(id, std::move(listener));
        return id;
    }

    void removeListener(std::size_t id) {
        std::lock_guard const lock(m_mutex);
        m_listeners.erase(id);
    }

private:
    using Records = std::list<std::pair<TileId, Entry>>;
    using Notification = std::pair<TileId, TileState>;

    void insertRecord(TileId const& tile, Entry&& entry) {
        m_records.emplace_front(tile, std::move(entry));
        m_index.emplace(tile, m_records.begin());
    }

    void replaceRecord(TileId const& tile, Entry&& entry) {
        auto const existing = m_index.find(tile);
        if (existing != m_index.end()) {
            m_byteUsage -= existing->second->second.bytes;
            m_records.erase(existing->second);
            m_index.erase(existing);
        }
        this->insertRecord(tile, std::move(entry));
    }

    void eraseRecord(TileId const& tile, std::vector<Notification>& notifications) {
        auto const it = m_index.find(tile);
        if (it == m_index.end())
            return;
        m_byteUsage -= it->second->second.bytes;
        m_records.erase(it->second);
        m_index.erase(it);
        notifications.emplace_back(tile, TileState::Missing);
    }

    void evictOverBudget(std::vector<Notification>& notifications, std::optional<TileId> const& pinned) {
        auto it = m_records.end();
        while (m_byteUsage > m_byteBudget && m_index.size() > 1) {
            if (it == m_records.begin())
                break;
            --it;
            if ((pinned.has_value() && it->first == *pinned) || it->second.state == TileState::Queued)
                continue;
            m_byteUsage -= it->second.bytes;
            m_index.erase(it->first);
            notifications.emplace_back(it->first, TileState::Missing);
            it = m_records.erase(it);
        }
    }

    void notify(std::vector<Notification> const& notifications) const {
        if (notifications.empty())
            return;
        std::vector<Listener> listeners;
        {
            std::lock_guard const lock(m_mutex);
            listeners.reserve(m_listeners.size());
            for (auto const& [id, listener] : m_listeners)
                listeners.push_back(listener);
        }
        for (auto const& listener : listeners) {
            for (auto const& [tile, state] : notifications)
                listener(tile, state);
        }
    }

    qsizetype m_byteBudget;
    mutable std::mutex m_mutex;
    mutable Records m_records;
    mutable std::unordered_map<TileId, Records::iterator, TileIdHash> m_index;
    qsizetype m_byteUsage = 0;
    std::unordered_map<std::size_t, Listener> m_listeners;
    std::size_t m_nextListenerId = 1;
};

class TileScheduler {
public:
    using Decoder = std::function<mts::agent::Result<SharedImage>(TileId const&)>;
    using Completion = std::function<void(mts::agent::Result<SharedImage> const&)>;
    using Generation = std::uint64_t;

    TileScheduler(mts::runtime::ThreadPool& pool, Decoder decoder)
        : m_pool(&pool)
        , m_decoder(std::move(decoder))
        , m_alive(std::make_shared<std::atomic_bool>(true)) {}

    ~TileScheduler() {
        {
            std::lock_guard const lock(m_mutex);
            m_alive->store(false, std::memory_order_release);
        }
        std::unique_lock lock(m_mutex);
        m_drained.wait(lock, [this] { return m_inflight.empty(); });
    }

    TileScheduler(TileScheduler const&) = delete;
    TileScheduler& operator=(TileScheduler const&) = delete;

    void request(TileId const& tile, Generation generation, Completion completion) {
        std::shared_ptr<Inflight> dispatchEntry;
        {
            std::lock_guard const lock(m_mutex);
            if (!m_alive->load(std::memory_order_acquire))
                return;

            auto [it, inserted] = m_inflight.try_emplace(tile, std::make_shared<Inflight>());
            bool stale = false;
            {
                std::lock_guard const entryLock(it->second->mutex);
                stale = it->second->drained || it->second->cancelled;
            }
            if (stale)
                it->second = std::make_shared<Inflight>();

            std::lock_guard const entryLock(it->second->mutex);
            it->second->requests.push_back(Request{generation, std::move(completion)});
            dispatchEntry = it->second->decodeStarted ? nullptr : it->second;
            it->second->decodeStarted = true;
        }

        if (dispatchEntry)
            this->dispatch(tile, std::move(dispatchEntry));
    }

    void setGeneration(Generation generation) {
        std::lock_guard const lock(m_mutex);
        m_generation = std::max(m_generation, generation);
        for (auto const& [tile, entry] : m_inflight) {
            std::lock_guard const entryLock(entry->mutex);
            std::erase_if(entry->requests, [generation](Request const& request) {
                return request.generation < generation;
            });
        }
    }

    [[nodiscard]] auto generation() const -> Generation {
        std::lock_guard const lock(m_mutex);
        return m_generation;
    }

    void cancelPending() {
        std::lock_guard const lock(m_mutex);
        for (auto const& [tile, entry] : m_inflight) {
            std::lock_guard const entryLock(entry->mutex);
            entry->cancelled = true;
            entry->requests.clear();
        }
    }

    [[nodiscard]] auto pending(TileId const& tile) const -> bool {
        std::lock_guard const lock(m_mutex);
        return m_inflight.contains(tile);
    }

    [[nodiscard]] auto pendingCount() const -> std::size_t {
        std::lock_guard const lock(m_mutex);
        return m_inflight.size();
    }

private:
    struct Request {
        Generation generation = 0;
        Completion completion;
    };

    struct Inflight {
        std::mutex mutex;
        std::vector<Request> requests;
        bool decodeStarted = false;
        bool drained = false;
        bool cancelled = false;
    };

    void dispatch(TileId const& tile, std::shared_ptr<Inflight> entry) {
        auto const alive = m_alive;
        auto const decoder = m_decoder;
        m_pool->schedule([this, alive = std::move(alive), entry = std::move(entry), tile, decoder] {
            bool cancelled = false;
            {
                std::lock_guard const entryLock(entry->mutex);
                cancelled = entry->cancelled;
            }

            mts::agent::Result<SharedImage> result = mts::agent::Result<SharedImage>::fromStatus(
                mts::agent::Status::internal("tile decode did not run")
            );
            if (!cancelled && alive->load(std::memory_order_acquire)) {
                try {
                    result = decoder(tile);
                } catch (...) {
                    result = mts::agent::Result<SharedImage>::fromStatus(
                        mts::agent::Status::internal("tile decoder threw an exception")
                    );
                }
            }

            this->finish(tile, entry, std::move(result), alive);
        });
    }

    void finish(TileId const& tile,
                std::shared_ptr<Inflight> const& entry,
                mts::agent::Result<SharedImage> result,
                std::shared_ptr<std::atomic_bool> const& alive) {
        std::vector<Request> drained;
        {
            std::lock_guard const entryLock(entry->mutex);
            drained = std::move(entry->requests);
            entry->requests.clear();
            entry->drained = true;
        }

        bool const deliver = alive->load(std::memory_order_acquire);
        {
            std::lock_guard const lock(m_mutex);
            auto const it = m_inflight.find(tile);
            if (it != m_inflight.end() && it->second == entry)
                m_inflight.erase(it);
            m_drained.notify_all();
        }

        if (!deliver)
            return;
        for (auto const& request : drained) {
            if (request.completion)
                request.completion(result);
        }
    }

    mts::runtime::ThreadPool* m_pool;
    Decoder m_decoder;
    mutable std::mutex m_mutex;
    std::condition_variable m_drained;
    std::unordered_map<TileId, std::shared_ptr<Inflight>, TileIdHash> m_inflight;
    std::shared_ptr<std::atomic_bool> m_alive;
    Generation m_generation = 0;
};

}
