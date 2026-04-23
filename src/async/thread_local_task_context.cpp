#include "thread_local_task_context.h"

#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>

// -----------------------------------------------------------------------------

static std::unordered_map<
  std::thread::id,
  async::impl::ThreadLocalThreadTaskContext>
  s_threadLocalThreadTaskContexts;
static std::mutex s_threadLocalThreadTaskContextsMutex;
static std::unordered_map<
  std::thread::id,
  async::impl::ThreadLocalCoroutineTaskContext>
  s_threadLocalCoroutineTaskContexts;
static std::mutex s_threadLocalCoroutineTaskContextsMutex;

// -----------------------------------------------------------------------------

async::impl::ThreadLocalThreadTaskContext*
  async::impl::GetThreadLocalThreadTaskContext()
{
  std::lock_guard lock{ s_threadLocalThreadTaskContextsMutex };
  auto it = s_threadLocalThreadTaskContexts.find(std::this_thread::get_id());
  if (it == s_threadLocalThreadTaskContexts.end())
  {
    return nullptr;
  }
  return &it->second;
}

// -----------------------------------------------------------------------------

async::impl::ThreadLocalThreadTaskContext*
  async::impl::CreateThreadLocalThreadTaskContext()
{
  std::lock_guard lock{ s_threadLocalThreadTaskContextsMutex };
  return &s_threadLocalThreadTaskContexts[std::this_thread::get_id()];
}

// -----------------------------------------------------------------------------

void async::impl::DestroyThreadLocalThreadTaskContext()
{
  std::lock_guard lock{ s_threadLocalThreadTaskContextsMutex };
  s_threadLocalThreadTaskContexts.erase(std::this_thread::get_id());
}

// -----------------------------------------------------------------------------

async::impl::ThreadLocalCoroutineTaskContext*
  async::impl::GetThreadLocalCoroutineTaskContext()
{
  std::lock_guard lock{ s_threadLocalCoroutineTaskContextsMutex };
  auto it = s_threadLocalCoroutineTaskContexts.find(std::this_thread::get_id());
  if (it == s_threadLocalCoroutineTaskContexts.end())
  {
    return nullptr;
  }
  return &it->second;
}

// -----------------------------------------------------------------------------

async::impl::ThreadLocalCoroutineTaskContext*
  async::impl::CreateThreadLocalCoroutineTaskContext()
{
  std::lock_guard lock{ s_threadLocalCoroutineTaskContextsMutex };
  return &s_threadLocalCoroutineTaskContexts[std::this_thread::get_id()];
}

// -----------------------------------------------------------------------------

void async::impl::DestroyThreadLocalCoroutineTaskContext()
{
  std::lock_guard lock{ s_threadLocalCoroutineTaskContextsMutex };
  s_threadLocalCoroutineTaskContexts.erase(std::this_thread::get_id());
}
