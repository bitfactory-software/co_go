# co_go::continuation — design and sequence examples

This document explains how `co_go::continuation<R>` works and how it interacts with `continuation_awaiter` (the `await_callback` / `await_callback_async` adapter). It combines a focused explanation of the core components, the lifecycle and ownership rules, and two compact, annotated sequence examples (synchronous and asynchronous callback flows). All examples reference the implementation in `co_go/continuation.hpp`.

---

## Overview (high level)
- `continuation<R>` is a small, move-only awaitable that wraps a coroutine handle and supports:
  - efficient synchronous completion (inline run; no extra suspension when possible), and
  - asynchronous chaining (wiring a suspended callee to resume its awaiting caller).
- `continuation_awaiter` adapts callback-style APIs into awaitables that store a result and resume the suspended coroutine when the callback fires. It explicitly marks the suspended coroutine as synchronous or asynchronous using a template `sync` parameter.

---

## Key components and behavior

### promise_type (`basic_promise_type<HandleReturn>`)
- `initial_suspend()` returns `std::suspend_never{}` → coroutine starts running immediately.
- `final_suspend()` returns an `await_continuation` awaitable that:
  - resumes `calling_coroutine_` if set;
  - destroys the coroutine if nobody is awaiting it (`awaited_` indicates that).
- `unhandled_exception()` stores exceptions in `exception_` for later propagation.
- Important members:
  - `std::coroutine_handle<> calling_coroutine_` — the awaiting coroutine handle (set when chaining).
  - `std::exception_ptr exception_` — stored exception, propagated in `await_resume`.
  - `bool sync_ = true` — indicates synchronous completion unless marked otherwise.
  - `bool awaited_ = true` — controls whether the coroutine is destroyed by caller or self-destroys.

### return handling (`handle_return` / specialization)
- For non-void `R`: `return_value(Ret)` stores the result; `return_result` returns it and may destroy the coroutine if `awaited_ == false`.
- For `void`: `return_void()` and `return_result` only handle destruction semantics.

### continuation<R> lifecycle & awaitable interface
- Holds `std::coroutine_handle<promise_type> coroutine_`.
- Move-only; destructor handles cleanup:
  - If `coroutine_` is null → no-op.
  - If `coroutine_.promise().sync_` is true → `coroutine_.destroy()` (synchronous case).
  - Else → sets `coroutine_.promise().awaited_ = false` so the coroutine will self-destroy later.
- `await_ready()` returns `false`.
- `await_suspend(calling_coroutine)`:
  - If no handle or `sync_ == true` → immediately `calling_coroutine.resume()` (treat as synchronous).
  - Else → call `build_async_chain(suspended, caller)` which:
    - `suspended.promise().calling_coroutine_ = calling_coroutine;`
    - `calling_coroutine.promise().sync_ = false;`
    - This wires an asynchronous chain: when the suspended coroutine finishes, it will resume the caller.
- `await_resume()` calls `handle_resume()` which:
  - rethrows stored exception (by default),
  - returns the stored result via `promise().return_result(coroutine_)`.

---

## continuation_awaiter (callback adapter)

### Requirements
- Concept `is_noexept_callback_api<R, Api>` enforces that the `Api` is invocable as `void(Api, std::function<void(R)>)` without throwing, and `R` is not `void`.

### Behavior
- `continuation_awaiter<R, Api, bool sync = true>` implements an awaitable adapter:
  - `await_ready()` → `false`.
  - `await_suspend(calling_coroutine)`:
    - sets `calling_coroutine.promise().sync_ = sync;` (marks the suspended coroutine as sync or async for outer awaiters),
    - calls the provided `api_` with a callback that:
      - stores the `R` result to `result_`,
      - ensures the callback only executes once,
      - calls `calling_coroutine.resume()` to resume the suspended coroutine.
  - `await_resume()` returns the stored `result_`.
- Helpers:
  - `await_callback<Api>(api)` → `sync = true` (treat the call as sync unless the callback runs later).
  - `await_callback_async<Api>(api)` → `sync = false` (explicitly mark as async).

---

## Interplay: continuation vs continuation_awaiter (core idea)
- `continuation_awaiter` marks the suspended coroutine's `promise().sync_` before invoking the callback API. That single flag determines whether an outer awaiter will:
  - treat the callee as synchronous and resume immediately (no chain), or
  - form an asynchronous chain so the callee later resumes the caller via `calling_coroutine_`.
- `build_async_chain` wires `calling_coroutine_` into the callee's promise and marks the caller's `sync_` false to avoid premature destruction.
- `final_suspend` (`await_continuation`) on the callee resumes `calling_coroutine_` when the callee completes; `awaited_` and `sync_` together control safe destruction and ownership.

---

## Sequence examples (annotated)

Two compact traces show exact state changes for the important fields:
- `promise().sync_`
- `promise().calling_coroutine_`
- `promise().awaited_`
and who resumes or destroys which coroutine.

### 1) Synchronous callback (callback invoked inline inside `await_suspend`)
Actors:
- A = outer coroutine that does `co_await B()`
- B = inner coroutine that uses `await_callback` (sync = true)
- API = callback-based function

Sequence:
1. A calls/co_await B() → B coroutine created; `B.promise().sync_ == true` initially; B starts running (initial_suspend = `suspend_never`).
2. Inside B, `continuation_awaiter::await_suspend`:
   - sets `calling_coroutine.promise().sync_ = sync` (true).
   - calls `API(cb)` inline. The API calls `cb(...)` synchronously.
   - Callback stores result into `continuation_awaiter::result_` and calls `calling_coroutine.resume()` → resumes B immediately.
3. B resumes, obtains result via `await_resume()`, finishes (`return_value`), reaches `final_suspend`.
   - `B.promise().sync_` stayed `true`.
4. When A awaited B: A's `await_suspend` sees `B.promise().sync_ == true` and does `calling_coroutine.resume()` immediately (no async chain).
5. Destruction:
   - B was synchronous and not chained: `continuation` destructor calls `coroutine_.destroy()`; no leak.

Key states:
- `B.promise().sync_` stays `true`.
- `B.promise().calling_coroutine_` not set.
- No `build_async_chain` invoked.

---

### 2) Asynchronous callback (callback invoked later; use `await_callback_async`)
Actors:
- A = outer coroutine that `co_await`s B()
- B = inner coroutine using `await_callback_async` (`sync = false`)
- API = async callback provider (invokes cb later)

Sequence:
1. A calls/co_await B() → B created and starts running.
2. Inside B, `continuation_awaiter::await_suspend`:
   - sets `calling_coroutine.promise().sync_ = sync` → sets `B.promise().sync_ = false`.
   - calls `API(cb)` which stores `cb` and returns (no inline callback).
   - B suspends.
3. A's `await_suspend` observes `B.promise().sync_ == false` and calls `build_async_chain(B_handle, A_handle)`:
   - `B.promise().calling_coroutine_ = A_handle;`
   - `A.promise().sync_ = false;` (mark the outer as part of the async chain).
   - A remains suspended.
4. Later, API triggers `cb(result)`:
   - callback stores `result_` and calls `B_handle.resume()` → resumes B.
5. B resumes, sets `return_value`, reaches `final_suspend`:
   - `await_continuation` sees `promise().calling_coroutine_` (A_handle) and calls `A_handle.resume()` → resumes A.
   - If `B.promise().awaited_` is true, B won't destroy itself here; ownership ends and `handle_resume` / `return_result` may destroy B.
6. A runs `await_resume` on the `co_await` expression:
   - `handle_resume()` checks exceptions, then calls `promise().return_result(coroutine_)` to obtain the result (may destroy B if `awaited_ == false`).

Destruction/ownership:
- If A remains alive and awaited_ is true → B destroyed in return_result or by explicit destroy path.
- If A was dropped while B pending → A's destructor sets `A.promise().awaited_ = false`, so B will self-destroy when it detects it is unawaited.

Key states:
- After step 2: `B.promise().sync_ == false`.
- After build chain: `B.promise().calling_coroutine_ == A_handle`, `A.promise().sync_ == false`.
- `B.promise().awaited_` controls final destroy behavior.

---

## Variant: Outer caller is dropped before callback fires
- If A is destroyed while B is pending:
  - A's `continuation` destructor sees `A.promise().sync_ == false` and sets `A.promise().awaited_ = false`.
  - Later when B completes:
    - `final_suspend` may try to resume `A_handle`. The design expects `awaited_` semantics to prevent leaks and to ensure the callee destroys itself if nobody awaits the result.
  - Practically: `awaited_ == false` causes `return_result` or `final_suspend` to call `coroutine.destroy()` thereby avoiding leaks.

---

## Exceptions
- Exceptions thrown inside coroutines are captured in `promise().exception_` via `unhandled_exception()`.
- `await_resume()` / `get_sync_result()` call `handle_resume`, which by default rethrows the stored exception (`std::rethrow_exception`), or you can supply a custom exception handler.

---

## Compact mental model
- `continuation<R>` manages lifecycle and a `sync_` flag describing whether it completed inline.
- `continuation_awaiter` adapts callback APIs and sets `sync_` for the suspended coroutine before invoking the callback.
- If `sync_ == true` → outer await treats callee as synchronous (resume immediately).
- If `sync_ == false` → `build_async_chain` wires callee → caller; the callee later resumes the caller on completion.
- `awaited_` and `sync_` together ensure safe destruction and avoid leaks when coroutines are dropped.

---

If you want, I can add a minimal runnable C++ example (single-file) that demonstrates both synchronous and asynchronous flows using a simple stored-callback API, and tests that show the relevant promise fields changing.