#include <coroutine>
#include <exception>
#include <functional>

namespace co_go {

#ifdef CO_GO_CONTINUATION_TEST
static int continuation_promise_count = 0;
#endif

enum class synchronisation { sync, async };

template <typename... Args>
struct result_t_impl {
  using type = std::tuple<Args...>;
  static auto make(Args... args) {
    return std::make_tuple(std::forward<Args>(args)...);
  }
  static auto default_value() { return type{}; }
};
template <typename Arg>
struct result_t_impl<Arg> {
  using type = Arg;
  static auto make(Arg arg) { return std::forward<Arg>(arg); }
  static auto default_value() { return type{}; }
};
template <>
struct result_t_impl<> {
  using type = void;
  static auto make() { return; }
  static auto default_value() { return; }
};
template <typename... Args>
using result_t = result_t_impl<Args...>;

template <synchronisation sync_or_async, typename Api, typename... CallbackArgs>
  requires(is_noexept_callback_api<Api, CallbackArgs...>)
struct callback_awaiter {
  bool await_ready() { return false; }
  void await_suspend(auto calling_coroutine) {
    calling_coroutine.promise().sync_ = sync_or_async;
    bool called = false;
    api_([this, calling_coroutine, called](CallbackArgs&&... args) mutable {
      if (called) return;
      called = true;
      result_ =
          result_t<CallbackArgs...>::make(std::forward<CallbackArgs>(args)...);
      calling_coroutine.resume();
    });
  }
  auto await_resume() { return result_; }
  const Api api_;
  result_t<CallbackArgs...>::type result_ = {};
};

template <typename Promise, typename... Args>
class continuation_awaiter {
 private:
  std::coroutine_handle<Promise> coroutine_;

 public:
  continuation_awaiter& operator=(const continuation_awaiter&) = delete;
  continuation_awaiter& operator=(continuation_awaiter&& r) = delete;
  continuation_awaiter(const continuation_awaiter&) = delete;
  continuation_awaiter(continuation_awaiter&& r) noexcept {
    std::swap(coroutine_, r.coroutine_);
  }
  explicit continuation_awaiter(std::coroutine_handle<Promise> coroutine)
      : coroutine_(coroutine) {}
  ~continuation_awaiter() noexcept {
    if (!coroutine_) return;
    if (coroutine_.promise().sync_ == synchronisation::sync)
      coroutine_.destroy();
    else
      coroutine_.promise().awaited_ = false;
  }

  static void build_async_chain(auto suspended_coroutine,
                                auto calling_coroutine) {
    suspended_coroutine.promise().calling_coroutine_ = calling_coroutine;
    calling_coroutine.promise().sync_ = synchronisation::async;
  }

  bool await_ready() const noexcept { return false; }
  void await_suspend(auto calling_coroutine) noexcept {
    if (!coroutine_ || coroutine_.promise().sync_ == synchronisation::sync)
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
    if (!coroutine_) return result_t<Args...>::default_value();
    if (auto exception = coroutine_.promise().exception_)
      handle_exception(exception);
    return coroutine_.promise().return_result(coroutine_);
  }
  auto coroutine() const { return coroutine_; }
  bool is_sync() const {
    return coroutine_.promise().sync_ == synchronisation::sync;
  }
};

template <typename... Args>
class continuation;

template <typename HandleReturn, typename... Args>
struct basic_promise_type : HandleReturn {
#ifdef CO_GO_CONTINUATION_TEST
  basic_promise_type() noexcept { ++continuation_promise_count; }
  ~basic_promise_type() noexcept { --continuation_promise_count; }
#endif

  continuation<Args...> get_return_object(this auto& self);

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

  void unhandled_exception() noexcept { exception_ = std::current_exception(); }

  std::coroutine_handle<> calling_coroutine_ = {};
  std::exception_ptr exception_ = {};
  synchronisation sync_ = synchronisation::sync;
  bool awaited_ = true;
  void destroy_if_not_awaited(auto& coroutine) {
    if (!awaited_) coroutine.destroy();
  }
};

template <typename... Rs>
struct handle_return {
  void return_value(this auto& self, std::tuple<Rs...> result) {
    self.result_ = std::move(result);
  }
  auto return_result(this auto& self, auto& coroutine) {
    auto result = std::move(self.result_);
    self.destroy_if_not_awaited(coroutine);
    return result;
  }
  std::tuple<Rs...> result_ = {};
};
template <typename Ret>
struct handle_return<Ret> {
  void return_value(Ret result) { result_ = std::move(result); }
  auto return_result(this auto& self, auto& coroutine) {
    auto result = std::move(self.result_);
    self.destroy_if_not_awaited(coroutine);
    return result;
  }
  Ret result_ = {};
};
template <>
struct handle_return<> {
  void return_void() {};
  auto return_result(this auto& self, auto& coroutine) {
    self.destroy_if_not_awaited(coroutine);
  }
};

template <typename... Args>
class continuation {
 public:
  using promise_type = basic_promise_type<handle_return<Args...>, Args...>;

  continuation& operator=(const continuation&) = delete;
  continuation& operator=(continuation&& r) = delete;
  continuation(const continuation&) = delete;
  continuation(continuation&& r) noexcept : awaiter_(std::move(r.awaiter_)) {};

  continuation() noexcept = default;
  explicit continuation(std::coroutine_handle<promise_type> coroutine)
      : awaiter_(coroutine) {}

  bool await_ready() const noexcept { return awaiter_.await_ready(); }
  void await_suspend(auto calling_coroutine) noexcept {
    awaiter_.await_suspend(calling_coroutine);
  }
  auto await_resume() { return awaiter_.await_resume(); }

  auto get_sync_result(auto handle_exception) {
    return awaiter_.get_sync_result(handle_exception);
  }
  auto get_sync_result() { return awaiter_.get_sync_result(); }

  auto coroutine() const { return awaiter_.coroutine(); }
  bool is_sync() const { return awaiter_.is_sync(); }

 private:
  continuation_awaiter<promise_type, Args...> awaiter_;
};

template <typename HandleReturn, typename... Args>
continuation<Args...>
basic_promise_type<HandleReturn, Args...>::get_return_object(this auto& self) {
  return continuation<Args...>{
      std::coroutine_handle<basic_promise_type>::from_promise(self)};
}

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

template <synchronisation sync_or_async, typename... CallbackArgs>
continuation<CallbackArgs...> callback(auto&& api) {
  using api_t = decltype(api);
  co_return co_await callback_awaiter<sync_or_async, std::decay_t<api_t>,
                                      CallbackArgs...>{
      std::forward<api_t>(api)};
}

template <typename... CallbackArgs>
auto callback_sync(auto&& api) {
  using api_t = decltype(api);
  return callback<synchronisation::sync, CallbackArgs...>(
      std::forward<api_t>(api));
}

template <typename... CallbackArgs>
continuation<CallbackArgs...> callback_async(auto&& api) {
  using api_t = decltype(api);
  return callback<synchronisation::async, CallbackArgs...>(
      std::forward<api_t>(api));
}

template <typename... R>
void spawn([[maybe_unused]] continuation<R...>&& c) {}

}  // namespace co_go
