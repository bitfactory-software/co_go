#include <coroutine>
#include <exception>
#include <functional>

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
  template <typename Ret>
  struct handle_return {
    void return_value(Ret result) { result_ = std::move(result); }
    auto return_result(this auto& self, auto& coroutine) {
      auto result = std::move(self.result_);
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

template <typename Api, typename... CallbackArgs>
constexpr bool is_noexept_callback_api_v =
    std::is_nothrow_invocable_r_v<void, Api,
                                  std::function<void(CallbackArgs...)>>;

template <typename Api, typename CallbackArg>
constexpr bool is_noexept_callback_api_v<Api, CallbackArg> =
    (!std::same_as<CallbackArg, void>) &&
    std::is_nothrow_invocable_r_v<void, Api, std::function<void(CallbackArg)>>;

template <typename Api, typename... CallbackArgs>
concept is_noexept_callback_api =
    is_noexept_callback_api_v<Api, CallbackArgs...>;

enum class synchronisation { sync, async };

template <synchronisation sync_or_async, typename Api, typename... CallbackArgs>
  requires(is_noexept_callback_api<Api, CallbackArgs...>)
struct continuation_awaiter {
  template <typename... Args>
  struct result_t_impl {
    using type = std::tuple<Args...>;
    static auto make(CallbackArgs... args) {
      return std::make_tuple(std::forward<CallbackArgs>(args)...);
    }
  };
  template <typename Arg>
  struct result_t_impl<Arg> {
    using type = Arg;
    static auto make(Arg arg) { return std::forward<Arg>(arg); }
  };
  using result_t = typename result_t_impl<CallbackArgs...>;
  bool await_ready() { return false; }
  void await_suspend(auto calling_coroutine) {
    calling_coroutine.promise().sync_ = sync_or_async == synchronisation::sync;
    bool called = false;
    api_([this, calling_coroutine, called](CallbackArgs&&... args) mutable {
      if (called) return;
      called = true;
      result_ = result_t::make(std::forward<CallbackArgs>(args)...);
      calling_coroutine.resume();
    });
  }
  auto await_resume() { return result_; }
  const Api api_;
  result_t::type result_ = {};
};

template <synchronisation sync_or_async, typename... CallbackArgs>
auto await_callback(auto&& api) {
  using api_t = decltype(api);
  return continuation_awaiter<sync_or_async, std::decay_t<api_t>,
                              CallbackArgs...>{std::forward<api_t>(api)};
}

template <typename... CallbackArgs>
auto await_callback_sync(auto&& api) {
  using api_t = decltype(api);
  return await_callback<synchronisation::sync, CallbackArgs...>(
      std::forward<api_t>(api));
}

template <typename... CallbackArgs>
auto await_callback_async(auto&& api) {
  using api_t = decltype(api);
  return await_callback<synchronisation::async, CallbackArgs...>(
      std::forward<api_t>(api));
}

template <typename R>
void dont_await([[maybe_unused]] continuation<R>&& c) {}

}  // namespace co_go
