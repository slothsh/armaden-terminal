#include <stdexec/execution.hpp>
#include <exec/static_thread_pool.hpp>
#include <exec/start_detached.hpp>
#include <exec/task.hpp>

export module runtime;

import std;
import agent;

template <class Type>
using Task = exec::task<Type>;

template <class Sender>
auto syncWait(Sender&& sender) -> decltype(auto) {
    return stdexec::sync_wait(std::forward<Sender>(sender));
}

export namespace mts::runtime {

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

    [[nodiscard]] auto agentToken() -> mts::agent::CancellationToken;

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

namespace {

struct AgentCancelBridge {
    std::shared_ptr<mts::agent::CancellationSource> source;

    auto operator()() const -> void {
        if (source) {
            source->cancel();
        }
    }
};

} // END OF NAMESPACE anonymous

struct mts::runtime::Cancellation::Impl {
    std::stop_source source;
    std::shared_ptr<mts::agent::CancellationSource> agentSource;
    std::unique_ptr<std::stop_callback<AgentCancelBridge>> bridge;
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

auto mts::runtime::Cancellation::agentToken() -> mts::agent::CancellationToken {
    if (!m_impl->agentSource) {
        m_impl->agentSource = std::make_shared<mts::agent::CancellationSource>();
        m_impl->bridge = std::make_unique<std::stop_callback<AgentCancelBridge>>(
            m_impl->source.get_token(),
            AgentCancelBridge{m_impl->agentSource}
        );
    }

    return m_impl->agentSource->token();
}
