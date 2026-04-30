# cpp-async

A C++ framework that provides asynchronous task management with threads and coroutines.

### Capabilities

- Create a new thread to asynchronously execute a task.
- Create a new coroutine to asynchronously execute a task.
- Yield from the current context until an asynchronous operation completes.
- Group multiple asynchronous operations into a single operation that, to complete, requires either all or one of its sub-operations to complete.
- Explicitly yield from the current context and schedule it to be resumed with or without a delay.
- Designate a single thread to serve as the coroutine processor for a single-threaded coroutine runtime model.
- Designate multiple threads to serve as parallel coroutine processors for a multi-threaded coroutine runtime model.

## Interface

The framework defines a single unified interface. This interface is used regardless of:

- **whether the current context is running on a dedicated thread or a coroutine**\
Yielding from the current context, regardless of whether it's a dedicated thread or a coroutine, is achieved via the same functions (`Future::Await()` and `Yield*()`).
- **whether an asynchronous operation (that's not the current context) is running on a dedicated thread or a coroutine**\
Once tasks are posted, the interface does not distinguish between dedicated threads and coroutines. Every asynchronous operation is captured by the type `Future`.

---

### `class async::Future<T>`

`async::Future<T>` captures an asynchronous operation that, upon completion, resolves into a value of type `T`.

```
T Await()
```

Yield from the current context and resume when the asynchronous operation completes.\
When the current context resumes, this returns the value of type `T` that the operation resolved into.

```
static async::Future<std::vector<T>> RequireAll(std::vector<async::Future<T>>)
```

Create a single `Future` instance that captures multiple `Future` instances. The created `Future` completes when every captured `Future` completes.\
The created `Future` resolves into a `vector`. Each element is the value that a captured `Future` resolved into, located at the same index as the `Future` in the argument `vector`.

```
static async::Future<std::pair<size_t, T>> RequireOne(std::vector<async::Future<T>>)
```

Create a single `Future` instance that captures multiple `Future` instances. The created `Future` completes when any captured `Future` completes.\
The created `Future` resolves into a `pair`. The first element is the index in the provided `vector` of the captured `Future` that completed first. The second element is the value that this captured `Future` resolved into.

---

### `class async::Future<void>`

`async::Future<void>` captures an asynchronous operation that does not resolve into any value upon completion.

```
void Await()
```

Yield from the current context and resume when the asynchronous operation completes.

```
static async::Future<void> RequireAll(std::vector<async::Future<void>>)
```

Create a single `Future` instance that captures multiple `Future` instances. The created `Future` completes when every captured `Future` completes.

```
static async::Future<size_t> RequireOne(std::vector<async::Future<void>>)
```

Create a single `Future` instance that captures multiple `Future` instances. The created `Future` completes when any captured `Future` completes.\
The created `Future` resolves into the index in the provided `vector` of the captured `Future` that completed first.

---

### Non-member functions

```
async::Future<T> async::RunTaskOnNewThread<T>(std::function<T()>&&)
```

Create a new dedicated thread that executes the provided `function`.\
The `function` must return a value of type `T`. The `Future` resolves into this value.

```
async::Future<void> async::RunTaskOnNewThread<void>(std::function<void()>&&)
```

Create a new dedicated thread that executes the provided `function`.\
The `Future` completes when the `function` returns.

```
async::Future<T> async::RunTaskOnNewCoroutine<T>(std::function<T()>&&)
```

Create a new coroutine that executes the provided `function`.\
The `function` must return a value of type `T`. The `Future` resolves into this value.

```
async::Future<void> async::RunTaskOnNewCoroutine<void>(std::function<void()>&&)
```

Create a new coroutine that executes the provided `function`.\
The `Future` completes when the `function` returns.

```
void async::Yield()
```

Yield from the current context, and immediately queue it to be resumed.

```
void async::YieldFor(std::chrono::steady_clock::duration)
```

Yield from the current context, and schedule it to be queued to be resumed after the provided duration.

```
void async::YieldUntil(std::chrono::steady_clock::time_point)
```

Yield from the current context, and schedule it to be queued to be resumed at the provided point in time.

```
int async::ExecuteProgram(std::function<int()>&&)
```

Start the asynchronous runtime, using the provided function as the top-level program logic. This creates a new coroutine to capture the provided function, adds it to the coroutine queue as the first element, and starts processing the queue.\
This function should only be used at the framework level (`main()` in most cases). It is not meant to be used by any application logic.
