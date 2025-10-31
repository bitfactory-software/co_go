#include <coroutine>
#include <exception>

namespace co_go {

template <typename R>
[[noreturn]]
R rethrow_exception(std::exception_ptr exception) {
  std::rethrow_exception(exception);
}

template <typename R>
struct callback {
  template <typename HandleReturn>
  struct basic_promise_type : HandleReturn {
    callback<R> get_return_object(this auto& self) {
      return callback<R>{std::coroutine_handle<basic_promise_type>::from_promise(self)};
    }

    struct await_callback {
      await_callback() noexcept {}
      bool await_ready() const noexcept { return false; }
      void await_suspend(
          std::coroutine_handle<basic_promise_type> thisCoroutine) noexcept {
        auto& promise = thisCoroutine.promise();
        if (promise.callingCoroutine_) promise.callingCoroutine_.resume();
      }
      void await_resume() noexcept {}
    };
    auto initial_suspend() noexcept { return std::suspend_never{}; }
    auto final_suspend() noexcept { return await_callback{}; }
    void unhandled_exception() noexcept {
      exception_ = std::current_exception();
    }
    std::coroutine_handle<> callingCoroutine_ = {};
    std::exception_ptr exception_ = {};
  };
  template <typename R>
  struct handle_return {
    void return_value(R result) { result_ = result; }
    R result_ = {};
  };
  template <>
  struct handle_return<void> {
    void return_void() {};
  };
  using promise_type = basic_promise_type<handle_return<R>>;

  callback(const callback&) = delete;
  callback& operator=(const callback&) = delete;
  callback& operator=(callback&& r) noexcept = delete;

  callback() noexcept = default;
  callback(callback&& t) noexcept { std::swap(coroutine_, t.coroutine_); }
  explicit callback(std::coroutine_handle<promise_type> coroutine)
      : coroutine_(coroutine) {}
  ~callback() noexcept {
    if (coroutine_) coroutine_.destroy();
  }

  bool await_ready() const noexcept { return false; }
  void await_suspend(auto callingCoroutine) noexcept {
    callingCoroutine.resume();
  }
  auto handle_resume(auto handle_exception) {
    if (!coroutine_) return R{};
    if (auto exception = coroutine_.promise().exception_)
      return handle_exception(exception);
    return coroutine_.promise().result_;
  }
  auto await_resume() { return handle_resume(rethrow_exception<R>); }
  auto get_result(auto handle_exception) {
    return handle_resume(handle_exception);
  }
  auto get_result() { return get_result(rethrow_exception<R>); }

  std::coroutine_handle<promise_type> coroutine_;
};

template <typename R, typename Api>
struct callback_awaiter {
  bool await_ready() { return false; }
  void await_suspend(auto callingContinuation) {
    bool called = false;
    api_([this, callingContinuation, called](const R& r) mutable {
      if (called) return;
      called = true;
      result_ = r;
      callingContinuation.resume();
    });
  }
  R await_resume() { return result_; }
  const Api api_;
  R result_ = {};
};
template <typename Api>
struct callback_awaiter<void, Api> {
  bool await_ready() { return false; }
  void await_suspend(auto callingContinuation) {
    bool called = false;
    api_([this, callingContinuation, called]() mutable {
      if (called) return;
      called = true;
      callingContinuation.resume();
    });
  }
  void await_resume() {}
  const Api api_;
};
template <typename R, typename Api>
auto wrap(Api&& api) {
  return callback_awaiter<R, std::decay_t<Api>>{std::move(api)};
}

}  // namespace coro_callback
