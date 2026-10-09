#include <stdexec/execution.hpp>
#include <exec/static_thread_pool.hpp>
#include <exec/start_detached.hpp>

export module runtime;

import std;
import foundation;
import config;

export namespace mts::runtime {

struct RuntimeConfig {
    std::uint32_t workers = 4;
};

[[nodiscard]] inline auto applyConfig(mts::config::ConfigView const& view, RuntimeConfig& target) -> mts::foundation::Status {
    return view.section("runtime").readUInt32("workers", target.workers);
}

class ThreadPool {
public:
    explicit ThreadPool(std::uint32_t numThreads);
    ~ThreadPool();

    ThreadPool(ThreadPool const&) = delete;
    auto operator=(ThreadPool const&) -> ThreadPool& = delete;
    ThreadPool(ThreadPool&&) noexcept;
    auto operator=(ThreadPool&&) noexcept -> ThreadPool&;

    auto schedule(std::function<void()> function) -> void;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

class Cancellation {
public:
    Cancellation();
    ~Cancellation();

    Cancellation(Cancellation const&) = delete;
    auto operator=(Cancellation const&) -> Cancellation& = delete;
    Cancellation(Cancellation&&) noexcept;
    auto operator=(Cancellation&&) noexcept -> Cancellation&;

    [[nodiscard]] auto token() const noexcept -> std::stop_token;
    [[nodiscard]] auto cancelled() const noexcept -> bool;
    auto cancel() -> void;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // END OF NAMESPACE `mts::runtime`

struct mts::runtime::ThreadPool::Impl {
    exec::static_thread_pool pool;

    explicit Impl(std::uint32_t numThreads)
        : pool(numThreads) {}
};

mts::runtime::ThreadPool::ThreadPool(std::uint32_t numThreads)
    : m_impl(std::make_unique<Impl>(numThreads)) {}

mts::runtime::ThreadPool::~ThreadPool() = default;

mts::runtime::ThreadPool::ThreadPool(ThreadPool&&) noexcept = default;

auto mts::runtime::ThreadPool::operator=(ThreadPool&&) noexcept -> ThreadPool& = default;

auto mts::runtime::ThreadPool::schedule(std::function<void()> function) -> void {
    exec::start_detached(
        stdexec::schedule(m_impl->pool.get_scheduler())
        | stdexec::then(std::move(function))
    );
}

struct mts::runtime::Cancellation::Impl {
    std::stop_source source;
};

mts::runtime::Cancellation::Cancellation()
    : m_impl(std::make_unique<Impl>()) {}

mts::runtime::Cancellation::~Cancellation() = default;

mts::runtime::Cancellation::Cancellation(Cancellation&&) noexcept = default;

auto mts::runtime::Cancellation::operator=(Cancellation&&) noexcept -> Cancellation& = default;

auto mts::runtime::Cancellation::token() const noexcept -> std::stop_token {
    return m_impl->source.get_token();
}

auto mts::runtime::Cancellation::cancelled() const noexcept -> bool {
    return m_impl->source.stop_requested();
}

auto mts::runtime::Cancellation::cancel() -> void {
    m_impl->source.request_stop();
}
