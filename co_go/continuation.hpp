#include <coroutine>
#include <exception>

namespace co_go {

#ifdef CO_GO_CONTINUATION_TEST
static int continuation_promise_count = 0;
#endif

template <typename R = void>
class continuation {
  static void build_async_chain(auto suspended_coroutine,
                                auto calling_coroutine) {
    suspended_coroutine.promise().calling_coroutine_ = calling_coroutine;
    calling_coroutine.promise().sync_ = false;
  }

  template <typename HandleReturn>
  struct basic_promise_type : HandleReturn {
#ifdef CO_GO_CONTINUATION_TEST
    basic_promise_type() noexcept { ++continuation_promise_count; }
    ~basic_promise_type() noexcept { --continuation_promise_count; }
#endif

    continuation<R> get_return_object(this auto& self) {
      return continuation<R>{
          std::coroutine_handle<basic_promise_type>::from_promise(self)};
    }

    struct await_continuation {
      await_continuation() noexcept {}
      bool await_ready() const noexcept { return false; }
      void await_suspend(
          std::coroutine_handle<basic_promise_type> this_coroutine) noexcept {
        auto& promise = this_coroutine.promise();
        if (promise.calling_coroutine_) promise.calling_coroutine_.resume();
        if (promise.awaited_) return;
        this_coroutine.destroy();
      }
      void await_resume() noexcept {}
    };
    auto initial_suspend() noexcept { return std::suspend_never{}; }
    auto final_suspend() noexcept { return await_continuation{}; }
    void unhandled_exception() noexcept {
      exception_ = std::current_exception();
    }
    std::coroutine_handle<> calling_coroutine_ = {};
    std::exception_ptr exception_ = {};
    bool sync_ = true;
    bool awaited_ = true;
  };
  template <typename R>
  struct handle_return {
    void return_value(R result) { result_ = result; }
    auto return_result(this auto& self, auto& coroutine) {
      auto result = self.result_;
      if (!self.awaited_) coroutine.destroy();
      return result;
    }
    R result_ = {};
  };
  template <>
  struct handle_return<void> {
    void return_void() {};
    auto return_result(this auto& self, auto& coroutine) {
      if (!self.awaited_) coroutine.destroy();
    }
  };

 public:
  using promise_type = basic_promise_type<handle_return<R>>;

 private:
  std::coroutine_handle<promise_type> coroutine_;

 public:
  continuation& operator=(const continuation&) = delete;
  continuation& operator=(continuation&& r) = delete;
  continuation(const continuation&) = delete;
  continuation(continuation&& r) noexcept {
    std::swap(coroutine_, r.coroutine_);
  }

  continuation() noexcept = default;
  explicit continuation(std::coroutine_handle<promise_type> coroutine)
      : coroutine_(coroutine) {}
  ~continuation() noexcept {
    if (!coroutine_) return;
    if (coroutine_.promise().sync_)
      coroutine_.destroy();
    else
      coroutine_.promise().awaited_ = false;
  }

  bool await_ready() const noexcept { return false; }
  void await_suspend(auto calling_coroutine) noexcept {
    if (!coroutine_ || coroutine_.promise().sync_)
      calling_coroutine.resume();
    else
      build_async_chain(this->coroutine_, calling_coroutine);
  }
  auto await_resume() { return handle_resume(); }
  auto get_sync_result(auto handle_exception) {
    return handle_resume(handle_exception);
  }
  auto get_sync_result() { return handle_resume(); }
  auto handle_resume() {
    return handle_resume([](auto e) { std::rethrow_exception(e); });
  }
  auto handle_resume(auto handle_exception) {
    if (!coroutine_) return R{};
    if (auto exception = coroutine_.promise().exception_)
      handle_exception(exception);
    return coroutine_.promise().return_result(coroutine_);
  }

  auto coroutine() const { return coroutine_; }
  bool is_sync() const { return coroutine_.promise().sync_; }
};

template <typename R, typename Api, bool sync = true>
struct continuation_awaiter {
  bool await_ready() { return false; }
  void await_suspend(auto calling_continuation) {
    calling_continuation.promise().sync_ = sync;
    bool called = false;
    api_([this, calling_continuation, called](const R& r) mutable {
      if (called) return;
      called = true;
      result_ = r;
      calling_continuation.resume();
    });
  }
  R await_resume() { return result_; }
  const Api api_;
  R result_ = {};
};
template <typename Api, bool sync>
struct continuation_awaiter<void, Api, sync> {
  bool await_ready() { return false; }
  void await_suspend(auto calling_continuation) {
    calling_continuation.promise().sync_ = sync;
    bool called = false;
    api_([this, calling_continuation, called]() mutable {
      if (called) return;
      called = true;
      calling_continuation.resume();
    });
  }
  void await_resume() {}
  const Api api_;
};
template <typename R, typename Api, bool sync = true>
auto await_callback(Api&& api) {
  return continuation_awaiter<R, std::decay_t<Api>, sync>{std::move(api)};
}

template <typename R, typename Api>
auto await_callback_async(Api&& api) {
  return await_callback<R, Api, false>(std::move(api));
}

template <typename R>
void dont_await([[maybe_unused]] continuation<R>&& c) {}

}  // namespace co_go
