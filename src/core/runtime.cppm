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
    ThreadPool& operator=(ThreadPool const&) = delete;
    ThreadPool(ThreadPool&&) noexcept;
    ThreadPool& operator=(ThreadPool&&) noexcept;

    void schedule(std::function<void()> function);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

class Cancellation {
public:
    Cancellation();
    ~Cancellation();

    Cancellation(Cancellation const&) = delete;
    Cancellation& operator=(Cancellation const&) = delete;
    Cancellation(Cancellation&&) noexcept;
    Cancellation& operator=(Cancellation&&) noexcept;

    [[nodiscard]] std::stop_token token() const noexcept;
    [[nodiscard]] bool cancelled() const noexcept;
    void cancel();

    [[nodiscard]] mts::agent::CancellationToken agentToken();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}

struct mts::runtime::ThreadPool::Impl {
    exec::static_thread_pool pool;

    explicit Impl(std::uint32_t numThreads)
        : pool(numThreads) {}
};

mts::runtime::ThreadPool::ThreadPool(std::uint32_t numThreads)
    : m_impl(std::make_unique<Impl>(numThreads)) {}

mts::runtime::ThreadPool::~ThreadPool() = default;

mts::runtime::ThreadPool::ThreadPool(ThreadPool&&) noexcept = default;

mts::runtime::ThreadPool& mts::runtime::ThreadPool::operator=(ThreadPool&&) noexcept = default;

void mts::runtime::ThreadPool::schedule(std::function<void()> function) {
    exec::start_detached(
        stdexec::schedule(m_impl->pool.get_scheduler())
        | stdexec::then(std::move(function))
    );
}

namespace {

struct AgentCancelBridge {
    std::shared_ptr<mts::agent::CancellationSource> source;

    void operator()() const {
        if (source)
            source->cancel();
    }
};

}

struct mts::runtime::Cancellation::Impl {
    std::stop_source source;
    std::shared_ptr<mts::agent::CancellationSource> agentSource;
    std::unique_ptr<std::stop_callback<AgentCancelBridge>> bridge;
};

mts::runtime::Cancellation::Cancellation()
    : m_impl(std::make_unique<Impl>()) {}

mts::runtime::Cancellation::~Cancellation() = default;

mts::runtime::Cancellation::Cancellation(Cancellation&&) noexcept = default;

mts::runtime::Cancellation& mts::runtime::Cancellation::operator=(Cancellation&&) noexcept = default;

std::stop_token mts::runtime::Cancellation::token() const noexcept {
    return m_impl->source.get_token();
}

bool mts::runtime::Cancellation::cancelled() const noexcept {
    return m_impl->source.stop_requested();
}

void mts::runtime::Cancellation::cancel() {
    m_impl->source.request_stop();
}

mts::agent::CancellationToken mts::runtime::Cancellation::agentToken() {
    if (!m_impl->agentSource) {
        m_impl->agentSource = std::make_shared<mts::agent::CancellationSource>();
        m_impl->bridge = std::make_unique<std::stop_callback<AgentCancelBridge>>(
            m_impl->source.get_token(),
            AgentCancelBridge{m_impl->agentSource}
        );
    }

    return m_impl->agentSource->token();
}
