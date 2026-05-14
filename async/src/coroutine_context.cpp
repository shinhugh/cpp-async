#include "coroutine_context.h"

#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>

// -----------------------------------------------------------------------------

static std::unordered_map<std::thread::id, async::impl::CoroutineContext *>
    s_threadLocalCoroutineContexts;
static std::mutex s_threadLocalCoroutineContextsMutex;

// -----------------------------------------------------------------------------

async::impl::CoroutineContext *async::impl::GetThreadLocalCoroutineContext()
{
  std::lock_guard lock{s_threadLocalCoroutineContextsMutex};
  auto it = s_threadLocalCoroutineContexts.find(std::this_thread::get_id());
  if (it == s_threadLocalCoroutineContexts.end())
  {
    return nullptr;
  }
  return it->second;
}

// -----------------------------------------------------------------------------

void async::impl::SetThreadLocalCoroutineContext(
    async::impl::CoroutineContext *coroutineContext)
{
  std::lock_guard lock{s_threadLocalCoroutineContextsMutex};
  s_threadLocalCoroutineContexts[std::this_thread::get_id()] = coroutineContext;
}

// -----------------------------------------------------------------------------

void async::impl::UnsetThreadLocalCoroutineContext()
{
  std::lock_guard lock{s_threadLocalCoroutineContextsMutex};
  s_threadLocalCoroutineContexts.erase(std::this_thread::get_id());
}
