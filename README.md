# ca2co: Wrap Callbacks into a Coroutine for C++

Write clean **sequential** code — run it on **callback-based** synchronous and asynchronous systems.

`ca2co::continuation` enables porting classic **blocking** code (UI, networking, filesystem, protocols) into event-driven architectures **without rewriting logic into callbacks**:

```cpp
// ✔ Sync blocking API.
std::string blocking_api();

// ✔ Async Callback API
void async_callback_api(std::function<void(std::string)> const& callback) noexcept;

// ✔ Continuation coroutine wrapper
ca2co::continuation<std::string> any_api(bool use_blocking_api)
{
    if (use_blocking_api) {
        co_return blocking_api();
    } else {
        co_return co_await ca2co::async<std::string>(async_callback_api);
    }
}

ca2co::spawn([]->ca2co::continuation<>{
    auto answer = co_await co_op();
    process( answer); // Is executed in the context of the underlying scheduler. Here provided be the Gui event loop. 
});
```

* ✅ Keep linear control flow (`if`, `for`, exceptions)
* ✅ Decouple business logic from UI/network async APIs
* ✅ Works with **any** callback-based API — no specific framework required
* ✅ No thread switching — resumes where the callback runs

---

## Why we made `ca2co::continuation`?

Our product originated as a classic Windows application. In that environment, user interface logic often used **modal dialogs**, making code naturally **sequential**:

```cpp
if (show_message_box_win("Continue?", {"Yes", "No"}) != "Yes")
    return;
// continue(!) with further processing
```

In more modern programming models, such as **QML**, this pattern becomes **callback-based**:

```cpp
show_message_box_qml("Continue?", {"Yes", "No"}, [&](std::string_view choice) {
    // continue(!) with further processing
});
} // usually the end of the function
```

However, rewriting existing business logic into callback pyramids is painful — especially when the UI layer must change but the logic must stay the same.

This is where **continuations** come in.

With `ca2co::continuation<T>`, you can keep the sequential structure:

```cpp
if (co_await co_show_message_box("Continue?", {"Yes", "No"}) != "Yes")
    co_return;
// continue(!) with further processing
```

### Portable Implementations

#### Modal / synchronous backend

```cpp
ca2co::continuation<std::string>
co_show_message_box(std::string const& prompt,
                    std::initializer_list<std::string> const& choices)
{
    co_return show_message_box_win(prompt, choices);
}
```

---

#### Callback-based backend

(using `std::bind` ✅)

```cpp
ca2co::continuation<std::string>
co_show_message_box(std::string const& prompt,
                    std::initializer_list<std::string> const& choices)
{
    using namespace std::placeholders;
    co_return co_await ca2co::callback_async<std::string>(
        std::bind(show_message_box_qml, prompt, choices, _1)
    );
}
```
Here, `_1` represents the callback that `callback_async` uses to resume the coroutine with the result.

### ✅ Key Advantages

`ca2co::continuation<T>` provides:

* ✅ Suspend & resume anywhere including on the UI thread
* ✅ Zero callback nesting → linear readable control flow
* ✅ One coroutine API supports both modal and async UI
* ✅ GUI event loop compatible (Qt/QML/etc.)
* ✅ Ideal migration path for large legacy codebases

You change **the implementation**, not **the caller**.

---

---

## Comparison to other C++20 coroutine libraries

| Library                                             | Primary Focus / Design Goal | Special Strength | Lazy vs Eager Execution | I/O / Executor Integration | Multi-Thread / Parallelism | Interop Difficulty w/ Legacy Callbacks |
|-----------------------------------------------------|-----------------------------|------------------|-------------------------|----------------------------|----------------------------|----------------------------------------|
| ca2co                                               | Turn *callback-based (a)sync* APIs into `co_await`.<br>✅ Scheduler agnostic! | ✅ Callback → `co_await` bridge | *eager* | depends on wrapped API | depends on wrapped API | very easy;<br>✅ core purpose |
| [Boost.Cobalt](https://github.com/boostorg/cobalt)  | Coroutine-enabled async I/O with Boost.Asio | High-level async I/O primitives | mostly *eager* | excellent Asio integration | controlled / single-thread exec | no example found |
| [cppcoro](https://github.com/lewissbaker/cppcoro)   | Generic coroutine primitives & algorithms | Flexible coroutine building blocks | mostly *lazy* | generic & plug-in friendly | moderate | no example found |
| [libcoro](https://github.com/jbaldwin/libcoro)      | Multi-threaded async runtime with schedulers + I/O | Large-scale parallel coroutine runtime | depends on awaitable | built-in I/O & schedulers | strong thread-pool parallelism | no example found |


---

### Porting Synchronous APIs to Asynchronous Callback APIs

`ca2co::continuation` is not limited to UI workflows.
It can be used to modernize **any** blocking API — such as networking, filesystem, or communication protocols.

A typical migration:

* 1️⃣ Wrap existing synchronous APIs using `ca2co::continuation`
* 2️⃣ Update business logic to use `co_await`
* 3️⃣ Replace the underlying implementation with async callbacks

---

### Example: Converting a synchronous network protocol

_Note:_ For a rewrite of a async asio callback example into ca2co coroutines look at the [asio_ca2co](https://github.com/bitfactory-software/asio_ca2co) to compare the [callback](https://github.com/bitfactory-software/asio_ca2co/blob/master/chat_client.cpp) and the [ca2co coroutine](https://github.com/bitfactory-software/asio_ca2co/blob/master/ca2co_chat_client.cpp) variants of the echo_client.

#### Step 1 — Existing blocking API

```cpp
std::string send_request_sync(std::string const& request)
{
    socket.write(request);
    return socket.read(); // blocking
}
```

#### Step 2 — Wrap API using `ca2co::continuation`

```cpp
ca2co::continuation<std::string>
co_send_request(std::string const& request)
{
    co_return send_request_sync(request);
}
```

#### Step 3 — Application logic becomes linear async

```cpp
ca2co::continuation<void> protocol_flow()
{
    auto hello = co_await co_send_request("HELLO");
    if (hello != "OK")
        co_return;

    auto data = co_await co_send_request("GET DATA");
    process(data);
    co_return;
}
```

#### Step 4 — Replace backend with async callback API

```cpp
void send_request_async(
    std::string request,
    std::function<void(std::string)> callback);
```

Updated wrapper:

```cpp
ca2co::continuation<std::string>
co_send_request(std::string const& request)
{
    using namespace std::placeholders;
    co_return co_await ca2co::callback_async<std::string>(
        std::bind(send_request_async, request, _1)
    );
}
```

* ✅ Business logic requires **no changes**
* ✅ Underlying transport switches from sync → async
* ✅ The same coroutine flow now runs without blocking

---

## Summary

`ca2co::continuation<T>` lets us:

* keep sequential UI-driven logic
* port to async architectures cleanly
* without rewriting business workflows into callbacks

---

## Getting Started

### Include and Basic Usage

1. Include the `ca2co` header:

```cpp
#include <ca2co/continuation.hpp>
```

2. Write coroutine-based UI logic:

```cpp
ca2co::continuation<void> run_flow()
{
    if (co_await show_message_box("Proceed?", {"Yes", "No"}) != "Yes")
        co_return;

    std::string name = co_await prompt_user_for_name();
    process_user(name);
}
```

3. Start the coroutine from your UI environment:

```cpp
run_flow(); // automatically starts and resumes on the UI thread
```

---

## Error Handling & Cancellation

* Exceptions thrown inside the coroutine propagate through `co_await`.
* UI-side cancellations (e.g. dialog closed) should resume with a special value.
* because the callback can only recieve one parameter, you must pack an error code into a struct, tuple or a std::expect 

Example:

```cpp
try {
    co_await wait_for_user();
} catch (std::exception const& e) {
    log_error(e.what());
}
```

Cancellation behavior is entirely under the application's control.

---

## Thread Context & Safety

`ca2co::continuation` does **not** switch threads on its own.

Where the coroutine resumes depends entirely on **where the callback is invoked**.

This means:

* If the callback fires on the UI thread → coroutine continues on the UI thread ✅
* If the callback fires on a worker thread → coroutine continues there ⚠️

This allows integration with Qt/QML, Win32, and other event-loop environments **without forcing a specific threading model**.

---
