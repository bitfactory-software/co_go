# co_go: An opaque C++ first-class continuation coroutine

## Why `co_go::continuation`?

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

With `co_go::continuation<T>`, you can keep the sequential structure:

```cpp
if (co_await show_message_box("Continue?", {"Yes", "No"}) != "Yes")
    co_return;
// continue(!) with further processing
```

---

### ✅ Key Advantages

`co_go::continuation<T>` provides:

* ✅ Suspend & resume anywhere including on the UI thread
* ✅ Zero callback nesting → linear readable control flow
* ✅ One coroutine API supports both modal and async UI
* ✅ GUI event loop compatible (Qt/QML/etc.)
* ✅ Ideal migration path for large legacy codebases

You change **the implementation**, not **the caller**.

---

### Architecture Overview

```
Legacy sequential UI
 (modal dialogs)
        │
        ▼
┌─────────────────────────────┐
│ co_go::continuation<T> API  │  Portable async business logic
└─────────────────────────────┘
        │
        ▼
Modern callback-based UI
   (Qt/QML/etc.)
```

---

### Portable Implementations

#### Modal / synchronous backend

```cpp
co_go::continuation<std::string>
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
co_go::continuation<std::string>
co_show_message_box(std::string const& prompt,
                    std::initializer_list<std::string> const& choices)
{
    using namespace std::placeholders;
    co_return co_await co_go::await_callback_async<std::string>(
        std::bind(show_message_box_qml, prompt, choices, _1)
    );
}
```

Here, `_1` represents the callback that `await_callback_async` uses to resume the coroutine with the result.

---

## Why not `cppcoro`?

### `cppcoro` solves a *different* problem

cppcoro’s `task<T>` is about **asynchronous computation**:

* Start work → finish later
* One-way, linear execution
* Not externally resumable
* Suited for background tasks (I/O, threading)

But UI workflows rely on **external events** (user actions, UI signals) controlling when code resumes.

### What UI code really needs

> “Pause here until the UI tells me to continue.”

That means:

* No worker thread running the task
* The **UI event loop** resumes the coroutine
* The continuation must be **first-class and externally controlled**

cppcoro does **not** provide:

| Feature                                           |        cppcoro::task        | co_go::continuation |
| ------------------------------------------------- | :-------------------------: | :-----------------: |
| Externally resumable continuation (opaque handle) |              ❌              |          ✅          |
| Suspend waiting on UI/user callback               | ⚠️ custom bridging required |      ✅ built-in     |
| Portable control-flow abstraction                 |              ❌              |          ✅          |
| UI-focused async design                           |              ❌              |          ✅          |

### Summary

> cppcoro is about async tasks.
> co_go is about **portable UI-driven suspend/resume**.

They solve **complementary but distinct** problems.

---

## Summary

`co_go::continuation<T>` lets us:

* keep sequential UI-driven logic
* port to async architectures cleanly
* without rewriting business workflows into callbacks

You write code *like on Windows*, while the runtime adapts to modern platforms’ event loops under the hood.

> ✅ Same logic, any UI architecture — modal or async.

---

## Getting Started

### Include and Basic Usage

1. Include the `co_go` header:

```cpp
#include <co_go/continuation.hpp>
```

2. Write coroutine-based UI logic:

```cpp
co_go::continuation<void> run_flow()
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
* UI-side cancellations (e.g. dialog closed) should resume with an error value or exception.

Example:

```cpp
try {
    co_await wait_for_user();
} catch (std::exception const& e) {
    log_error(e.what());
    co_return;
}
```

Cancellation behavior is entirely under the application's control.

---

## Thread Context & Safety

`co_go::continuation` does **not** switch threads on its own.

Where the coroutine resumes depends entirely on **where the callback is invoked**.

This means:

* If the callback fires on the UI thread → coroutine continues on the UI thread ✅
* If the callback fires on a worker thread → coroutine continues there ⚠️

This allows integration with Qt/QML, Win32, and other event-loop environments **without forcing a specific threading model**.

If thread marshalling is required (e.g. enforcing GUI-thread only),
this should be handled by the UI framework or a helper layer.

