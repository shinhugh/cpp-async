# cpp-async

A C++ framework that provides asynchronous task management with coroutines and threads.

### Capabilities

- Capture any asynchronous routine (e.g. writing to a socket) with a future instance that can be passed around and awaited on from any context.
- Asynchronously execute a task on a new coroutine.
- Asynchronously execute a task on a new thread.
- Yield from the current context until an asynchronous operation completes.
- Aggregate multiple asynchronous operations into a single operation that, to complete, requires either all or one of its sub-operations to complete.
- Explicitly yield from the current context and schedule it to be resumed with or without a configurable delay.
- Designate a single thread to serve as the task processor for a single-threaded task runtime model.
- Designate multiple threads to serve as parallel task processors for a multi-threaded task runtime model.

---

## Getting started

### Prerequisites

- Clang must be installed for the provided build script to work.\
Developers are free to use other build systems, but this repository supports only Clang out of the box.
- Boost C++ libraries must be installed.

### Building

Navigate to the project's root directory.

Run `build.sh`.

This creates the subdirectory `build/` and populates it with the build artifacts.

### Running

Run `build/main.out`. No application logic is provided, so the program will immediately exit if built and run as is.

### Integration

`main/src/main.cpp` contains the program's entry point.

`main()` is set up to bootstrap the runtime and execute `Application()`. To enable multiple processor threads, pass in the desired number into `RunApplication()`.

`Application()` is the intended entry point for any application-specific logic. This function occupies the very first coroutine.

When adding new files, adhere to the existing repository structure.

- Create a top-level directory for the project (e.g. `async/`, `main/`, `test/`).
- Inside the project's directory, create the subdirectories `include/` and `src/`.
- Add any new file to one of these two subdirectories.
- Update `build.sh`.
  - Copy all header files in `<project>/include/` to `build/_include/<project>/`.
  - Compile all translation units in `<project>/src/`.\
Use the options `-Ibuild/_include` and `-Ibuild/_include/<project>` to allow headers to be resolved.\
Save the binaries in `build/<project>/`.
  - Include the new binaries in the existing linker step for `main.out`.

---

## Interface

The framework defines a single unified interface. This interface is used regardless of:

- **whether the current context is running on a coroutine or a dedicated thread**\
Yielding from the current context, regardless of whether it's a coroutine or a dedicated thread, is achieved via the same functions (`Future::Await()` and `Yield*()`).
- **whether an asynchronous operation (that's not the current context) is running on a coroutine or a dedicated thread**\
Once tasks are posted, the interface does not distinguish between coroutines and dedicated threads. Every asynchronous operation is captured by the type `Future`.

---

### `class async::Future<T>`

`#include "async/future.h"`

`async::Future<T>` captures an asynchronous operation that, upon completion, resolves into a value of type `T`.

---

```c++
T Await()
```

Yield from the current context and resume when the asynchronous operation completes.\
When the current context resumes, this returns the value of type `T` that the operation resolved into.

---

```c++
static async::Future<std::vector<T>> RequireAll(
    std::vector<async::Future<T>>)
```

Create a single `Future` instance that captures multiple `Future` instances. The created `Future` resolves when every captured `Future` resolves.\
The created `Future` resolves into a `vector`, where each element is the value that a captured `Future` resolved into, located at the same index as the `Future` in the argument `vector`.

---

```c++
static async::Future<std::pair<size_t, T>> RequireOne(
    std::vector<async::Future<T>>)
```

Create a single `Future` instance that captures multiple `Future` instances. The created `Future` resolves when any captured `Future` resolves.\
The created `Future` resolves into a `pair`, where the first element is the index in the provided `vector` of the captured `Future` that resolved first and the second element is the value that this captured `Future` resolved into.

---

### `class async::Future<void>`

`#include "async/future.h"`

`async::Future<void>` captures an asynchronous operation that does not resolve into any value upon completion.

---

```c++
void Await()
```

Yield from the current context and resume when the asynchronous operation completes.

---

```c++
static async::Future<void> RequireAll(
    std::vector<async::Future<void>>)
```

Create a single `Future` instance that captures multiple `Future` instances. The created `Future` resolves when every captured `Future` resolves.

---

```c++
static async::Future<size_t> RequireOne(
    std::vector<async::Future<void>>)
```

Create a single `Future` instance that captures multiple `Future` instances. The created `Future` resolves when any captured `Future` resolves.\
The created `Future` resolves into the index in the provided `vector` of the captured `Future` that resolved first.

---

### Task creation functions

`#include "async/async.h"`

---

```c++
async::Future<T> async::RunOnCurrentContext(
    const std::function<void(Promise<T>)>&)
```

Immediately execute the provided `function` on the current context, whether that's a coroutine or a thread.\
The framework creates a `Promise` for the provided `function`. The returned `Future` resolves when the `Promise` is fulfilled, into the value that the `Promise` is fulfilled with.\
This provides a way to wrap third-party asynchronous routines with `Future` instances.

---

```c++
async::Future<void> async::RunOnCurrentContext(
    const std::function<void(Promise<void>)>&)
```

Immediately execute the provided `function` on the current context, whether that's a coroutine or a thread.\
The framework creates a `Promise` for the provided `function`. The returned `Future` resolves when the `Promise` is fulfilled.\
This provides a way to wrap third-party asynchronous routines with `Future` instances.

---

```c++
async::Future<T> async::RunOnNewCoroutine(
    std::function<void(Promise<T>)>&&)
```

Create a new coroutine that executes the provided `function`.\
The framework creates a `Promise` for the provided `function`. The returned `Future` resolves when the `Promise` is fulfilled, into the value that the `Promise` is fulfilled with.

---

```c++
async::Future<void> async::RunOnNewCoroutine(
    std::function<void(Promise<void>)>&&)
```

Create a new coroutine that executes the provided `function`.\
The framework creates a `Promise` for the provided `function`. The returned `Future` resolves when the `Promise` is fulfilled.

---

```c++
async::Future<T> async::RunOnNewCoroutine(
    std::function<T()>&&)
```

Create a new coroutine that executes the provided `function`.\
The returned `Future` resolves when the `function` returns, into the returned value.

---

```c++
async::Future<void> async::RunOnNewCoroutine(
    std::function<void()>&&)
```

Create a new coroutine that executes the provided `function`.\
The returned `Future` resolves when the `function` returns.

---

```c++
async::Future<T> async::RunOnNewThread(
    std::function<void(Promise<T>)>&&)
```

Create a new thread that executes the provided `function`.\
The framework creates a `Promise` for the provided `function`. The returned `Future` resolves when the `Promise` is fulfilled, into the value that the `Promise` is fulfilled with.

---

```c++
async::Future<void> async::RunOnNewThread(
    std::function<void(Promise<void>)>&&)
```

Create a new thread that executes the provided `function`.\
The framework creates a `Promise` for the provided `function`. The returned `Future` resolves when the `Promise` is fulfilled.

---

```c++
async::Future<T> async::RunOnNewThread(
    std::function<T()>&&)
```

Create a new thread that executes the provided `function`.\
The returned `Future` resolves when the `function` returns, into the returned value.

---

```c++
async::Future<void> async::RunOnNewThread(
    std::function<void()>&&)
```

Create a new thread that executes the provided `function`.\
The returned `Future` resolves when the `function` returns.

---

### Yielding functions

`#include "async/async.h"`

---

```c++
void async::Yield()
```

Yield from the current context, and immediately queue it to be resumed as soon as possible.

---

```c++
void async::YieldFor(
    std::chrono::steady_clock::duration)
```

Yield from the current context, and schedule it to be queued to be resumed after the provided duration.

---

```c++
void async::YieldUntil(
    std::chrono::steady_clock::time_point)
```

Yield from the current context, and schedule it to be queued to be resumed at the provided point in time.

---

### Runtime management functions

`#include "async/async.h"`

---

```c++
int async::RunApplication(
    std::function<int()>&&, int argc, char* argv[], size_t processorThreadCount)
```

Start the asynchronous runtime, using the provided function as the top-level program logic. Specifically, this function does the following:

1. It creates a new coroutine to capture the provided function.
2. It adds it to the coroutine queue as the first task to be executed.
3. If `processorThreadCount` is greater than 1, the function spins up new threads to serve as additional task processors.
4. It itself starts processing tasks.

The runtime remains alive and continues to process tasks until all tasks have exited. This applies even after the top-level application task has exited; if any sub-tasks remain alive, the runtime stays alive.\
This function should only be used to bootstrap the runtime (typically in `main()`). It should not be used by any application logic to run individual tasks.

---

## Notes

### Coroutine vs thread

Coroutines are created by using the various `RunOnNewCoroutine()` functions, and threads are created by using the various `RunOnNewThread()` functions.

The two sets of functions are structurally identical; for every overload of the former, there is an equivalent overload of the latter. However, the choice to use one over the other should not be arbitrary.

Creating a new thread is useful for making blocking routines asynchronous. A blocking routine should not be executed in a coroutine because it will block the processor thread and halt the application until it completes.

Creating a thread is also the better choice for tasks that are highly time-sensitive. Although the framework itself is quite snappy, a queue is still a queue, so a time-consuming coroutine can delay other coroutines. The framework cannot force coroutines to yield in the way the OS does for threads, so it ultimately cannot make strong guarantees on timeliness with coroutines.

Otherwise, developers should lean towards creating coroutines over threads. Creating too many threads adds significant overhead from context switching and slows down the process. Coroutines require context switching as well, but this is managed by the framework, where each task is allowed to run until it itself yields or exits. Context switching for threads is entirely managed by the OS, so the framework cannot control these interruptions.

Also, coroutines can provide protection against race conditions in an application with a single processor thread, given that certain conditions are met (more on this below).

### Single processor thread vs multiple processor threads

Using a single processor thread and using multiple processor threads both have their pros and cons.

#### Single processor thread

Using a single processor thread greatly simplifies the runtime paradigm. It provides additional protection against race conditions because at most one coroutine will ever be running at any given time.

This protection is not unconditional. The framework does not protect against race conditions if any of the following cases are possible:

- A coroutine yields (via the `Future::Await` or `Yield*` functions) while in the middle of an operation that should be atomic, and the operation reads from or writes to shared memory (i.e. objects that are not local to just the single coroutine).
- A thread other than the processor thread reads from or writes to data that a coroutine also reads from or writes to.

In these cases, the normal guards (e.g. `mutex`) should still be applied to enforce atomicity.

#### Multiple processor threads

An application with multiple processor threads has potential to have higher performance than one with a single processor thread. However, this voids all protections against race conditions and requires the developer to always apply the normal guards.

If the program will always run on a machine without enough cores to benefit from multithreading in general, there is no reason to opt for multiple processor threads.

---

## Sample code

### Capture a blocking routine as an asynchronous task

```c++
// A synchronous function that blocks the thread until the requested information
// is ready
int GetTemperature();
```

```c++
// Create a new thread
async::Future<int> getTemperatureTask = async::RunOnNewThread<int>(
    []()
    {
      // Kick off the process to get the temperature
      // The spawned thread blocks until the data is ready...
      return GetTemperature();
    });

// ... but the caller context is not blocked...

// ... until we explicitly wait for the data
int temperature = getTemperatureTask.Await();

// We could even pass getTemperatureTask around and have a different
// coroutine/thread invoke Await() instead
```

### Capture a callback-based asynchronous routine as an asynchronous task

```c++
// An asynchronous function that returns immediately
// Notifies the caller when the response arrives by invoking a caller-provided
// callback
void SendHttpRequest(
    HttpRequest request, std::function<void(HttpResponse)> onResponseCallback);
```

```c++
// Create an HTTP request
HttpRequest request{/* ... */};

// Run on the current context
async::Future<HttpResponse> sendHttpRequestTask =
    async::RunOnCurrentContext<HttpResponse>(
        [
            request = std::move(request)](
            Promise<HttpResponse> promise)
        {
          // We are now in a new scope but on the same context
          // (coroutine/thread) The framework has created a Promise for us,
          // which is connected to the Future returned to the outer scope

          // Send the request, registering an on-response callback
          SendHttpRequest(
              std::move(request),
              [
                  promise = std::move(promise)](
                  HttpResponse response)
              {
                // This is code that is to be executed later, when the response
                // arrives

                // Allow the outer Future to resolve into the received
                // HttpResponse
                promise.Fulfill(std::move(response));
              });

          // Immediately return...
        });

// ... back to the original scope

// Explicitly block the current context until the response is ready
HttpResponse response = sendHttpRequestTask.Await();

// We could even pass sendHttpRequestTask around and have a different
// coroutine/thread invoke Await() instead
```
