#include <QCryptographicHash>
#include <QImage>
#include <QImageReader>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QSizeF>
#include <QtGlobal>

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
    int pixelWidth = 0;
    int pixelHeight = 0;

    [[nodiscard]] auto pixelSize() const -> QSize {
        int const width = pixelWidth > 0 ? pixelWidth : tilesAcross * tileSize;
        int const height = pixelHeight > 0 ? pixelHeight : tilesDown * tileSize;

        return QSize(width, height);
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
    double const scale = levelPixelScale(level);
    int const pixelWidth = int(std::ceil(double(basePixelSize.width()) / scale));
    int const pixelHeight = int(std::ceil(double(basePixelSize.height()) / scale));
    int const tilesAcross = int(std::ceil(double(pixelWidth) / double(tileSize)));
    int const tilesDown = int(std::ceil(double(pixelHeight) / double(tileSize)));

    return LevelInfo{
        level,
        std::clamp(tilesAcross, 1, std::numeric_limits<int>::max()),
        std::clamp(tilesDown, 1, std::numeric_limits<int>::max()),
        tileSize,
        pixelWidth,
        pixelHeight,
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
    QPointF const origin = tileOrigin(level, tile);
    QSize const extent = level.pixelSize();
    float const width = std::max(0, std::min(level.tileSize, extent.width() - tile.x * level.tileSize));
    float const height = std::max(0, std::min(level.tileSize, extent.height() - tile.y * level.tileSize));

    return QRectF(origin, QSizeF(width, height));
}

[[nodiscard]] auto tileAt(LevelInfo const& level, QPointF const& levelPixel) -> TileId {
    int const tileX = qBound(0, int(std::floor(levelPixel.x() / float(level.tileSize))), level.tilesAcross - 1);
    int const tileY = qBound(0, int(std::floor(levelPixel.y() / float(level.tileSize))), level.tilesDown - 1);

    return TileId{level.level, tileX, tileY};
}

[[nodiscard]] auto tilesForRect(LevelInfo const& level, QRectF const& levelPixelRect) -> std::vector<TileId> {
    if (levelPixelRect.isEmpty()) {
        return {};
    }

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
        for (int x = firstX; x <= lastX; ++x) {
            tiles.push_back(TileId{level.level, x, y});
        }
    }

    return tiles;
}

[[nodiscard]] auto orderCenterFirst(
    std::vector<TileId> tiles,
    LevelInfo const& level,
    QPointF const& center
) -> std::vector<TileId> {
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

[[nodiscard]] auto selectLevel(
    double zoom,
    double devicePixelRatio,
    int maxLevel,
    LodPolicy const& policy
) -> int {
    int const minimumLevel = std::min(policy.minimumLevel, maxLevel);
    int const maximumLevel = std::max(minimumLevel, maxLevel);
    if (!(zoom > 0.0)) {
        return minimumLevel;
    }

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
    if (request.baseRect.isEmpty() || request.baseSize.isEmpty() || request.viewportSize.isEmpty()) {
        return result;
    }

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
concept TileSource = requires(
    Type&& source,
    TileId const& tile,
    int level
) {
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
    auto operator=(TileCache const&) -> TileCache& = delete;

    [[nodiscard]] auto find(TileId const& tile) const -> std::optional<Entry> {
        std::lock_guard const lock(m_mutex);
        auto const it = m_index.find(tile);
        if (it == m_index.end()) {
            return std::nullopt;
        }

        m_records.splice(m_records.begin(), m_records, it->second);

        return it->second->second;
    }

    [[nodiscard]] auto state(TileId const& tile) const -> TileState {
        std::lock_guard const lock(m_mutex);
        auto const it = m_index.find(tile);

        return it == m_index.end() ? TileState::Missing : it->second->second.state;
    }

    auto markQueued(TileId const& tile) -> void {
        std::vector<Notification> notifications;
        {
            std::lock_guard const lock(m_mutex);
            this->replaceRecord(tile, Entry{TileState::Queued, SharedImage{}, {}, 0});
            notifications.emplace_back(tile, TileState::Queued);
        }

        this->notify(notifications);
    }

    auto store(TileId const& tile, SharedImage image) -> void {
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

    auto markFailed(TileId const& tile, std::string error) -> void {
        std::vector<Notification> notifications;
        {
            std::lock_guard const lock(m_mutex);
            this->replaceRecord(tile, Entry{TileState::Failed, SharedImage{}, std::move(error), 0});
            notifications.emplace_back(tile, TileState::Failed);
        }

        this->notify(notifications);
    }

    auto evict(TileId const& tile) -> void {
        std::vector<Notification> notifications;
        {
            std::lock_guard const lock(m_mutex);
            this->eraseRecord(tile, notifications);
        }

        this->notify(notifications);
    }

    auto clear() -> void {
        std::vector<Notification> notifications;
        {
            std::lock_guard const lock(m_mutex);
            notifications.reserve(m_records.size());
            for (auto const& record : m_records) {
                notifications.emplace_back(record.first, TileState::Missing);
            }
            m_records.clear();
            m_index.clear();
            m_byteUsage = 0;
        }

        this->notify(notifications);
    }

    auto setByteBudget(qsizetype budget) -> void {
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

    auto removeListener(std::size_t id) -> void {
        std::lock_guard const lock(m_mutex);
        m_listeners.erase(id);
    }

private:
    using Records = std::list<std::pair<TileId, Entry>>;
    using Notification = std::pair<TileId, TileState>;

    auto insertRecord(TileId const& tile, Entry&& entry) -> void {
        m_records.emplace_front(tile, std::move(entry));
        m_index.emplace(tile, m_records.begin());
    }

    auto replaceRecord(TileId const& tile, Entry&& entry) -> void {
        auto const existing = m_index.find(tile);
        if (existing != m_index.end()) {
            m_byteUsage -= existing->second->second.bytes;
            m_records.erase(existing->second);
            m_index.erase(existing);
        }

        this->insertRecord(tile, std::move(entry));
    }

    auto eraseRecord(TileId const& tile, std::vector<Notification>& notifications) -> void {
        auto const it = m_index.find(tile);
        if (it == m_index.end()) {
            return;
        }

        m_byteUsage -= it->second->second.bytes;
        m_records.erase(it->second);
        m_index.erase(it);
        notifications.emplace_back(tile, TileState::Missing);
    }

    auto evictOverBudget(std::vector<Notification>& notifications, std::optional<TileId> const& pinned) -> void {
        auto it = m_records.end();
        while (m_byteUsage > m_byteBudget && m_index.size() > 1) {
            if (it == m_records.begin()) {
                break;
            }

            --it;
            if ((pinned.has_value() && it->first == *pinned) || it->second.state == TileState::Queued) {
                continue;
            }

            m_byteUsage -= it->second.bytes;
            m_index.erase(it->first);
            notifications.emplace_back(it->first, TileState::Missing);
            it = m_records.erase(it);
        }
    }

    auto notify(std::vector<Notification> const& notifications) const -> void {
        if (notifications.empty()) {
            return;
        }

        std::vector<Listener> listeners;
        {
            std::lock_guard const lock(m_mutex);
            listeners.reserve(m_listeners.size());
            for (auto const& [id, listener] : m_listeners) {
                listeners.push_back(listener);
            }
        }

        for (auto const& listener : listeners) {
            for (auto const& [tile, state] : notifications) {
                listener(tile, state);
            }
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
    auto operator=(TileScheduler const&) -> TileScheduler& = delete;

    auto request(TileId const& tile, Generation generation, Completion completion) -> void {
        std::shared_ptr<Inflight> dispatchEntry;
        {
            std::lock_guard const lock(m_mutex);
            if (!m_alive->load(std::memory_order_acquire)) {
                return;
            }

            auto [it, inserted] = m_inflight.try_emplace(tile, std::make_shared<Inflight>());
            bool stale = false;
            {
                std::lock_guard const entryLock(it->second->mutex);
                stale = it->second->drained || it->second->cancelled;
            }

            if (stale) {
                it->second = std::make_shared<Inflight>();
            }

            std::lock_guard const entryLock(it->second->mutex);
            it->second->requests.push_back(Request{generation, std::move(completion)});
            dispatchEntry = it->second->decodeStarted ? nullptr : it->second;
            it->second->decodeStarted = true;
        }

        if (dispatchEntry) {
            this->dispatch(tile, std::move(dispatchEntry));
        }
    }

    auto setGeneration(Generation generation) -> void {
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

    auto cancelPending() -> void {
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

    auto dispatch(TileId const& tile, std::shared_ptr<Inflight> entry) -> void {
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

    auto finish(
        TileId const& tile,
        std::shared_ptr<Inflight> const& entry,
        mts::agent::Result<SharedImage> result,
        std::shared_ptr<std::atomic_bool> const& alive
    ) -> void {
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
            if (it != m_inflight.end() && it->second == entry) {
                m_inflight.erase(it);
            }
            m_drained.notify_all();
        }

        if (!deliver) {
            return;
        }

        for (auto const& request : drained) {
            if (request.completion) {
                request.completion(result);
            }
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

[[nodiscard]] auto expandHomePath(std::string const& path) -> std::filesystem::path {
    if (path.empty() || path.front() != '~') {
        return std::filesystem::path(path);
    }

    std::string const home = []() -> std::string {
        if (char const* env = std::getenv("HOME"); env != nullptr && *env != '\0') {
            return std::string(env);
        }

        return {};
    }();
    if (home.empty()) {
        return std::filesystem::path(path);
    }

    if (path.size() == 1) {
        return std::filesystem::path(home);
    }

    if (path.at(1) == '/' || path.at(1) == std::filesystem::path::preferred_separator) {
        return std::filesystem::path(home) / std::filesystem::path(path.substr(2));
    }

    return std::filesystem::path(path);
}

struct TileBuildProgress {
    bool building = false;
    bool ready = false;
    bool failed = false;
    std::string error;
    int levelsReady = 0;
    int levelCount = 0;
};

namespace detail {

struct PyramidBuildState {
    enum class State : std::uint8_t {
        Idle,
        Building,
        Ready,
        Failed,
    };

    using Listener = std::function<void(TileBuildProgress const&)>;

    [[nodiscard]] auto progress(int levelCount) const -> TileBuildProgress {
        std::lock_guard const lock(mutex);

        TileBuildProgress progress;
        progress.building = state == State::Building;
        progress.ready = state == State::Ready;
        progress.failed = state == State::Failed;
        progress.error = error;
        progress.levelsReady = levelsReady.load(std::memory_order_acquire);
        progress.levelCount = levelCount;

        return progress;
    }

    auto notify(TileBuildProgress const& progress) const -> void {
        std::vector<Listener> snapshot;
        {
            std::lock_guard const lock(mutex);
            snapshot.reserve(listeners.size());
            for (auto const& [id, listener] : listeners) {
                snapshot.push_back(listener);
            }
        }

        for (auto const& listener : snapshot) {
            listener(progress);
        }
    }

    mutable std::mutex mutex;
    State state = State::Idle;
    std::string error;
    std::atomic<int> levelsReady{0};
    std::unordered_map<std::size_t, Listener> listeners;
    std::size_t nextListenerId = 1;
};

} // END OF NAMESPACE detail

class SingleFileTileSource {
public:
    struct Options {
        int tileSize = 256;
        std::filesystem::path cacheDir;
    };

    static constexpr int kMaximumLevels = 16;
    static constexpr int kMaximumImageAllocationMiB = 4096;
    static constexpr std::uint64_t kAllocationSafetyMarginBytes = 64ULL * 1024ULL * 1024ULL;

    [[nodiscard]] static auto create(
        std::filesystem::path const& file,
        Options options
    ) -> mts::agent::Result<SingleFileTileSource> {
        if (options.tileSize <= 0) {
            return mts::agent::Result<SingleFileTileSource>::fromStatus(
                mts::agent::Status::invalidArgument("tile size must be positive"));
        }

        std::error_code pathError;
        if (!std::filesystem::exists(file, pathError) || pathError) {
            return mts::agent::Result<SingleFileTileSource>::fromStatus(
                mts::agent::Status::invalidArgument("map source file does not exist: " + file.string()));
        }

        QImageReader reader(QString::fromStdString(file.string()));
        QSize const baseSize = reader.size();
        if (baseSize.isEmpty()) {
            return mts::agent::Result<SingleFileTileSource>::fromStatus(
                mts::agent::Status::internal("could not read source image dimensions: " + file.string()));
        }

        std::uint64_t const pixelBytes = std::uint64_t(baseSize.width())
            * std::uint64_t(baseSize.height()) * 4ULL;
        std::uint64_t const requiredBytes = pixelBytes + kAllocationSafetyMarginBytes;
        std::uint64_t const requiredMiB = (requiredBytes + (1024ULL * 1024ULL - 1ULL)) / (1024ULL * 1024ULL);
        if (requiredMiB > std::uint64_t(kMaximumImageAllocationMiB)) {
            return mts::agent::Result<SingleFileTileSource>::fromStatus(
                mts::agent::Status::resourceExhausted(
                    "source image " + std::to_string(baseSize.width()) + "x" + std::to_string(baseSize.height())
                    + " requires about " + std::to_string(requiredMiB) + " MiB to decode; limit is "
                    + std::to_string(kMaximumImageAllocationMiB) + " MiB"));
        }

        {
            static std::mutex allocationLimitMutex;
            std::lock_guard const lock(allocationLimitMutex);
            int const currentLimit = QImageReader::allocationLimit();
            if (currentLimit > 0 && currentLimit < int(requiredMiB)) {
                QImageReader::setAllocationLimit(int(requiredMiB));
            }
        }

        return SingleFileTileSource(file, baseSize, std::move(options));
    }

    [[nodiscard]] auto tileSize() const -> int {
        return m_tileSize;
    }

    [[nodiscard]] auto levelCount() const -> int {
        return m_levelCount;
    }

    [[nodiscard]] auto levelInfo(int level) const -> LevelInfo {
        int const clamped = qBound(0, level, m_levelCount - 1);

        return levelInfoForBase(m_baseSize, m_tileSize, clamped);
    }

    [[nodiscard]] auto baseSize() const -> QSize {
        return m_baseSize;
    }

    [[nodiscard]] auto filePath() const -> std::filesystem::path const& {
        return m_file;
    }

    [[nodiscard]] auto pyramidDirectory() const -> std::filesystem::path const& {
        return m_pyramidDir;
    }

    auto startBuild(mts::runtime::ThreadPool& pool) -> void {
        {
            std::lock_guard const lock(m_core->mutex);
            if (m_core->state != detail::PyramidBuildState::State::Idle) {
                return;
            }

            int const ready = this->countReadyLevels();
            if (ready == m_levelCount) {
                m_core->state = detail::PyramidBuildState::State::Ready;
                m_core->levelsReady.store(ready, std::memory_order_release);
            } else {
                m_core->state = detail::PyramidBuildState::State::Building;
                m_core->levelsReady.store(ready, std::memory_order_release);

                auto const core = m_core;
                auto const file = m_file;
                auto const pyramidDir = m_pyramidDir;
                auto const baseSize = m_baseSize;
                int const tileSize = m_tileSize;
                int const levelCount = m_levelCount;
                pool.schedule([core, file, pyramidDir, baseSize, tileSize, levelCount] {
                    runPyramidBuild(core, file, pyramidDir, baseSize, tileSize, levelCount);
                });
            }
        }

        m_core->notify(m_core->progress(m_levelCount));
    }

    [[nodiscard]] auto buildProgress() const -> TileBuildProgress {
        return m_core->progress(m_levelCount);
    }

    [[nodiscard]] auto levelReady(int level) const -> bool {
        int const clamped = qBound(0, level, m_levelCount - 1);
        std::error_code error;

        return std::filesystem::exists(this->levelMarkerPath(clamped), error) && !error;
    }

    auto addProgressListener(detail::PyramidBuildState::Listener listener) -> std::size_t {
        std::lock_guard const lock(m_core->mutex);
        std::size_t const id = m_core->nextListenerId++;
        m_core->listeners.emplace(id, std::move(listener));

        return id;
    }

    auto removeProgressListener(std::size_t id) -> void {
        std::lock_guard const lock(m_core->mutex);
        m_core->listeners.erase(id);
    }

    [[nodiscard]] auto readTile(TileId const& tile) -> mts::agent::Result<SharedImage> {
        if (tile.level < 0 || tile.level >= m_levelCount) {
            return mts::agent::Result<SharedImage>::fromStatus(
                mts::agent::Status::invalidArgument("tile level out of range"));
        }

        double const scale = levelPixelScale(tile.level);
        int const levelWidth = int(std::ceil(double(m_baseSize.width()) / scale));
        int const levelHeight = int(std::ceil(double(m_baseSize.height()) / scale));
        if (tile.x < 0 || std::int64_t(tile.x) * m_tileSize >= levelWidth
            || tile.y < 0 || std::int64_t(tile.y) * m_tileSize >= levelHeight) {
            return mts::agent::Result<SharedImage>::fromStatus(
                mts::agent::Status::invalidArgument("tile coordinates out of range"));
        }

        SharedImage image(QString::fromStdString(this->tilePath(tile).string()));
        if (image.isNull()) {
            TileBuildProgress const progress = m_core->progress(m_levelCount);
            if (progress.failed) {
                return mts::agent::Result<SharedImage>::fromStatus(
                    mts::agent::Status::internal("map pyramid build failed: " + progress.error));
            }

            if (!progress.building && !progress.ready) {
                return mts::agent::Result<SharedImage>::fromStatus(
                    mts::agent::Status::notFound("map pyramid has not been built"));
            }

            return mts::agent::Result<SharedImage>::fromStatus(
                mts::agent::Status::notFound("tile not yet available"));
        }

        return image;
    }

private:
    SingleFileTileSource(std::filesystem::path file, QSize baseSize, Options options)
        : m_file(std::move(file))
        , m_baseSize(baseSize)
        , m_tileSize(options.tileSize)
        , m_core(std::make_shared<detail::PyramidBuildState>()) {
        int width = m_baseSize.width();
        int height = m_baseSize.height();
        int levels = 1;
        while ((std::max(width, height) > m_tileSize) && levels < kMaximumLevels) {
            width = (width + 1) / 2;
            height = (height + 1) / 2;
            ++levels;
        }
        m_levelCount = levels;

        std::filesystem::path const cacheRoot = options.cacheDir.empty()
            ? expandHomePath("~/.cache/mindtosis/tiles")
            : expandHomePath(options.cacheDir.string());

        std::error_code pathError;
        auto const absolute = std::filesystem::absolute(m_file, pathError);
        std::uintmax_t const sizeBytes = pathError ? 0 : std::filesystem::file_size(m_file, pathError);
        auto const modified = std::filesystem::last_write_time(m_file, pathError);
        auto const modifiedTicks = pathError ? 0 : modified.time_since_epoch().count();

        std::string const identity = absolute.string() + "|" + std::to_string(sizeBytes)
            + "|" + std::to_string(modifiedTicks);
        QByteArray const digest = QCryptographicHash::hash(
            QByteArray::fromStdString(identity), QCryptographicHash::Sha1).toHex();

        m_pyramidDir = cacheRoot / "v1" / std::filesystem::path(digest.constData())
            / std::to_string(m_tileSize);
    }

    [[nodiscard]] auto countReadyLevels() const -> int {
        int ready = 0;
        for (int level = 0; level < m_levelCount; ++level) {
            std::error_code error;
            if (std::filesystem::exists(this->levelMarkerPath(level), error) && !error) {
                ++ready;
            }
        }

        return ready;
    }

    [[nodiscard]] auto tilePath(TileId const& tile) const -> std::filesystem::path {
        return m_pyramidDir / std::to_string(tile.level)
            / (std::to_string(tile.x) + "_" + std::to_string(tile.y) + ".png");
    }

    [[nodiscard]] auto levelMarkerPath(int level) const -> std::filesystem::path {
        return m_pyramidDir / ("level_" + std::to_string(level) + ".done");
    }

    [[nodiscard]] static auto levelImageFor(QImage const& base, QSize baseSize, int level) -> QImage {
        if (level == 0) {
            return base;
        }

        double const scale = levelPixelScale(level);
        QSize const target(
            int(std::ceil(double(baseSize.width()) / scale)),
            int(std::ceil(double(baseSize.height()) / scale))
        );

        return base.scaled(target, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    [[nodiscard]] static auto writeTile(
        QImage const& levelImage,
        QSize levelSize,
        std::filesystem::path const& pyramidDir,
        int tileSize,
        int level,
        int x,
        int y
    ) -> bool {
        int const left = x * tileSize;
        int const top = y * tileSize;
        int const width = std::min(tileSize, levelSize.width() - left);
        int const height = std::min(tileSize, levelSize.height() - top);
        if (width <= 0 || height <= 0) {
            return true;
        }

        QImage const tile = levelImage.copy(left, top, width, height);
        std::filesystem::path const directory = pyramidDir / std::to_string(level);
        std::error_code error;
        std::filesystem::create_directories(directory, error);
        if (error) {
            return false;
        }

        auto const tilePath = directory / (std::to_string(x) + "_" + std::to_string(y) + ".png");

        return tile.save(QString::fromStdString(tilePath.string()), "png");
    }

    static auto failPyramidBuild(
        std::shared_ptr<detail::PyramidBuildState> const& core,
        int levelCount,
        std::string message
    ) -> void {
        {
            std::lock_guard const lock(core->mutex);
            core->state = detail::PyramidBuildState::State::Failed;
            core->error = std::move(message);
        }

        core->notify(core->progress(levelCount));
    }

    static auto runPyramidBuild(
        std::shared_ptr<detail::PyramidBuildState> const& core,
        std::filesystem::path const& file,
        std::filesystem::path const& pyramidDir,
        QSize baseSize,
        int tileSize,
        int levelCount
    ) -> void {
        QImageReader reader(QString::fromStdString(file.string()));
        QImage const base = reader.read();
        if (base.isNull()) {
            failPyramidBuild(core, levelCount, "could not decode map source: " + reader.errorString().toStdString());
            return;
        }

        int ready = 0;
        for (int level = 0; level < levelCount; ++level) {
            std::error_code error;
            if (std::filesystem::exists(pyramidDir / ("level_" + std::to_string(level) + ".done"), error) && !error) {
                ++ready;
            }
        }
        core->levelsReady.store(ready, std::memory_order_release);

        for (int level = levelCount - 1; level >= 0; --level) {
            std::error_code error;
            std::filesystem::path const marker = pyramidDir / ("level_" + std::to_string(level) + ".done");
            if (std::filesystem::exists(marker, error) && !error) {
                continue;
            }

            double const scale = levelPixelScale(level);
            QSize const levelSize(
                int(std::ceil(double(baseSize.width()) / scale)),
                int(std::ceil(double(baseSize.height()) / scale))
            );
            QImage const levelImage = levelImageFor(base, baseSize, level);
            if (levelImage.isNull()) {
                failPyramidBuild(core, levelCount, "could not scale source to level " + std::to_string(level));
                return;
            }

            int const tilesAcross = int(std::ceil(double(levelSize.width()) / double(tileSize)));
            int const tilesDown = int(std::ceil(double(levelSize.height()) / double(tileSize)));
            for (int y = 0; y < tilesDown; ++y) {
                for (int x = 0; x < tilesAcross; ++x) {
                    if (!writeTile(levelImage, levelSize, pyramidDir, tileSize, level, x, y)) {
                        failPyramidBuild(core, levelCount,
                            "could not write tile " + std::to_string(x) + "_" + std::to_string(y)
                            + " at level " + std::to_string(level));
                        return;
                    }
                }
            }

            std::ofstream markerFile(marker, std::ios::out | std::ios::trunc);
            if (!markerFile.is_open()) {
                failPyramidBuild(core, levelCount, "could not write level marker " + marker.string());
                return;
            }

            core->levelsReady.fetch_add(1, std::memory_order_acq_rel);
            core->notify(core->progress(levelCount));
        }

        {
            std::lock_guard const lock(core->mutex);
            core->state = detail::PyramidBuildState::State::Ready;
        }

        core->notify(core->progress(levelCount));
    }

    std::filesystem::path m_file;
    QSize m_baseSize;
    int m_tileSize;
    int m_levelCount = 1;
    std::filesystem::path m_pyramidDir;
    std::shared_ptr<detail::PyramidBuildState> m_core;
};

} // END OF NAMESPACE `mts::imagery`
