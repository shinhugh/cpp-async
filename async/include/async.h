#pragma once

#include "future.h"
#include "promise.h"

#include <chrono>
#include <cstddef>
#include <functional>

// -----------------------------------------------------------------------------

namespace async
{

// -----------------------------------------------------------------------------

template <typename T>
Future<T> RunOnCurrentContext(
    const std::function<void(Promise<T>)> &);

template <>
Future<void> RunOnCurrentContext(
    const std::function<void(Promise<void>)> &);

template <typename T>
Future<T> RunOnNewCoroutine(
    std::function<void(Promise<T>)> &&);

template <>
Future<void> RunOnNewCoroutine(
    std::function<void(Promise<void>)> &&);

template <typename T>
Future<T> RunOnNewCoroutine(
    std::function<T()> &&);

template <>
Future<void> RunOnNewCoroutine(
    std::function<void()> &&);

template <typename T>
Future<T> RunOnNewThread(
    std::function<void(Promise<T>)> &&);

template <>
Future<void> RunOnNewThread(
    std::function<void(Promise<void>)> &&);

template <typename T>
Future<T> RunOnNewThread(
    std::function<T()> &&);

template <>
Future<void> RunOnNewThread(
    std::function<void()> &&);

void Yield();

void YieldFor(
    std::chrono::steady_clock::duration);

void YieldUntil(
    std::chrono::steady_clock::time_point);

int RunApplication(
    std::function<int(int, char *[])> &&application, int argc, char *argv[],
    size_t processorThreadCount = 1);

// -----------------------------------------------------------------------------

}  // namespace async

// -----------------------------------------------------------------------------

#include "coroutine_context.h"
#include "promise_future_state.h"

#include <boost/context/continuation_fcontext.hpp>

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

// -----------------------------------------------------------------------------

namespace async::impl
{

// -----------------------------------------------------------------------------

struct Coroutine
{
  std::shared_ptr<std::mutex> m_activeMutex;
  boost::context::continuation m_continuation;
  CoroutineContext m_context;
};

// -----------------------------------------------------------------------------

struct Thread
{
  std::thread m_thread;
};

// -----------------------------------------------------------------------------

extern std::unordered_map<uint32_t, Coroutine> g_coroutines;
extern std::vector<uint32_t> g_completeCoroutines;
extern std::vector<Coroutine *> g_readyCoroutines;
extern uint32_t g_nextCoroutineId;
extern std::unordered_map<uint32_t, Thread> g_threads;
extern std::vector<uint32_t> g_completeThreads;
extern uint32_t g_nextThreadId;
extern std::recursive_mutex g_tasksMutex;
extern std::condition_variable_any g_tasksCv;

// -----------------------------------------------------------------------------

}  // namespace async::impl

// -----------------------------------------------------------------------------

template <typename T>
async::Future<T> async::RunOnCurrentContext(
    const std::function<void(Promise<T>)> &task)
{
  std::shared_ptr<impl::PromiseFutureState<T>> promiseFutureState =
      std::make_shared<impl::PromiseFutureState<T>>();

  task(Promise{promiseFutureState});

  return Future{promiseFutureState};
}

// -----------------------------------------------------------------------------

template <typename T>
async::Future<T> async::RunOnNewCoroutine(
    std::function<void(Promise<T>)> &&task)
{
  return RunOnCurrentContext<T>(
      [
          task = std::move(task)](
          Promise<T> promise)
      {
        uint32_t coroutineId;
        impl::Coroutine *coroutine;
        {
          std::lock_guard lock{impl::g_tasksMutex};
          while (
              impl::g_coroutines.find(impl::g_nextCoroutineId) !=
              impl::g_coroutines.end())
          {
            impl::g_nextCoroutineId++;
          }
          coroutineId = impl::g_nextCoroutineId;
          impl::g_nextCoroutineId++;
          coroutine = &impl::g_coroutines[coroutineId];
        };
        impl::g_tasksCv.notify_one();

        coroutine->m_activeMutex = std::make_shared<std::mutex>();

        coroutine->m_continuation = boost::context::callcc(
            [
                task = std::move(task), promise = std::move(promise),
                coroutineId](
                boost::context::continuation &&parentContinuation) mutable
            {
              parentContinuation = parentContinuation.resume();

              impl::CoroutineContext *context =
                  impl::GetThreadLocalCoroutineContext();
              context->m_yieldCallback = [
                  &parentContinuation]()
              {
                parentContinuation = parentContinuation.resume();
              };

              task(std::move(promise));

              {
                std::lock_guard lock{impl::g_tasksMutex};
                impl::g_completeCoroutines.push_back(coroutineId);
              }
              impl::g_tasksCv.notify_one();

              return std::move(parentContinuation);
            });

        coroutine->m_context.m_queueCallback = [
            coroutine]()
        {
          {
            std::lock_guard lock{impl::g_tasksMutex};
            impl::g_readyCoroutines.push_back(coroutine);
          }
          impl::g_tasksCv.notify_one();
        };

        coroutine->m_context.m_queueCallback();
      });
}

// -----------------------------------------------------------------------------

template <typename T>
async::Future<T> async::RunOnNewCoroutine(
    std::function<T()> &&task)
{
  return RunOnNewCoroutine<T>(
      [
          task = std::move(task)](
          Promise<T> promise)
      {
        T result = task();
        promise.Fulfill(std::move(result));
      });
}

// -----------------------------------------------------------------------------

template <typename T>
async::Future<T> async::RunOnNewThread(
    std::function<void(Promise<T>)> &&task)
{
  return RunOnCurrentContext<T>(
      [
          task = std::move(task)](
          Promise<T> promise)
      {
        uint32_t threadId;
        impl::Thread *thread;
        {
          std::lock_guard lock{impl::g_tasksMutex};
          while (
              impl::g_threads.find(impl::g_nextThreadId) !=
              impl::g_threads.end())
          {
            impl::g_nextThreadId++;
          }
          threadId = impl::g_nextThreadId;
          impl::g_nextThreadId++;
          thread = &impl::g_threads[threadId];
        };
        impl::g_tasksCv.notify_one();

        thread->m_thread =
            std::thread{[
                    task = std::move(task), promise = std::move(promise),
                    threadId]() mutable
                {
                  task(std::move(promise));

                  {
                    std::lock_guard lock{impl::g_tasksMutex};
                    impl::g_completeThreads.push_back(threadId);
                  }
                  impl::g_tasksCv.notify_one();
                }};
      });
}

// -----------------------------------------------------------------------------

template <typename T>
async::Future<T> async::RunOnNewThread(
    std::function<T()> &&task)
{
  return RunOnNewThread<T>(
      [
          task = std::move(task)](
          Promise<T> promise)
      {
        T result = task();
        promise.Fulfill(std::move(result));
      });
}
