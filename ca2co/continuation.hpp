#include <coroutine>
#include <exception>
#include <functional>
#include <variant>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunneeded-internal-declaration"
#pragma clang diagnostic ignored "-Wunused-function"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunneeded-internal-declaration"
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

/* clang-tidy: -functionStatic -misc-function-static */
/* NOLINTBEGIN(functionStatic,misc-function-static) */

namespace ca2co {

#ifdef CA2CO_TEST
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

template <typename... CallbackArgs>
class callback_awaiter {
 public:
  using callback_t = std::function<void(CallbackArgs...)>;
  using api_t = std::function<void(callback_t const&)>;
  template <typename Api>
  callback_awaiter(synchronisation sync_or_async, Api api)
    requires is_noexept_callback_api<Api, CallbackArgs...>
      : sync_or_async_(sync_or_async), api_(std::move(api)) {}
  static bool await_ready() noexcept { return false; }
  void await_suspend(auto calling_coroutine) {
    calling_coroutine.promise().sync_ = sync_or_async_;
    bool called = false;
    api_([this, calling_coroutine, called](CallbackArgs&&... args) mutable {
      if (called) return;
      called = true;
      result_ =
          result_t<CallbackArgs...>::make(std::forward<CallbackArgs>(args)...);
      calling_coroutine.resume();
    });
  }
  [[clang::suppress("functionStatic")]]
  auto await_resume() /* NOLINT(functionStatic) */ {
    return std::move(result_);
  }

 private:
  synchronisation sync_or_async_;
  api_t api_;
  result_t<CallbackArgs...>::type result_ = {};
};

template <>
class callback_awaiter<> : public std::suspend_never {
};  // dummy for continuation<>

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

  static bool await_ready() noexcept { return false; }
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
#ifdef CA2CO_TEST
  basic_promise_type() noexcept { ++continuation_promise_count; }
  ~basic_promise_type() noexcept { --continuation_promise_count; }
#endif

  continuation<Args...> get_return_object(
      this auto& self);  // NOLINT(functionStatic)

  struct await_continuation {
    await_continuation() noexcept {}
    static bool await_ready() noexcept { return false; }
    static void await_suspend(
        std::coroutine_handle<basic_promise_type> this_coroutine) noexcept {
      auto& promise = this_coroutine.promise();
      if (promise.calling_coroutine_) promise.calling_coroutine_.resume();
      if (promise.awaited_) return;
      this_coroutine.destroy();
    }
    static void await_resume() noexcept {}
  };

  static auto initial_suspend() noexcept { return std::suspend_never{}; }
  static auto final_suspend() noexcept { return await_continuation{}; }

  void unhandled_exception() noexcept { exception_ = std::current_exception(); }

  std::coroutine_handle<> calling_coroutine_ = {};
  std::exception_ptr exception_ = {};
  synchronisation sync_ = synchronisation::sync;
  bool awaited_ = true;
  void destroy_if_not_awaited(auto& coroutine) const {
    if (!awaited_) coroutine.destroy();
  }
};

template <typename... Rs>
struct handle_return {
  [[clang::suppress("functionStatic")]]
  void return_value(this auto& self,
                    std::tuple<Rs...> result) /* NOLINT(functionStatic) */ {
    self.result_ = std::move(result);
  }
  [[clang::suppress("functionStatic")]]
  auto return_result(this auto& self,
                     auto& coroutine) /* NOLINT(functionStatic) */ {
    auto result = std::move(self.result_);
    self.destroy_if_not_awaited(coroutine);
    return result;
  }
  std::tuple<Rs...> result_ = {};
};
template <typename Ret>
struct handle_return<Ret> {
  void return_value(Ret result) { result_ = std::move(result); }
  [[clang::suppress("functionStatic")]]
  auto return_result(this auto& self,
                     auto& coroutine) /* NOLINT(functionStatic) */ {
    auto result = std::move(self.result_);
    self.destroy_if_not_awaited(coroutine);
    return result;
  }
  Ret result_ = {};
};
template <>
struct handle_return<> {
  static void return_void() {};
  [[clang::suppress("functionStatic")]]
  auto return_result(this auto& self,
                     auto& coroutine) /* NOLINT(functionStatic) */ {
    self.destroy_if_not_awaited(coroutine);
  }
};

template <typename... Args>
class continuation {
 public:
  using promise_type = basic_promise_type<handle_return<Args...>, Args...>;
  using continuation_awaiter_t = continuation_awaiter<promise_type, Args...>;
  using callback_awaiter_t = callback_awaiter<Args...>;

  continuation& operator=(const continuation&) = delete;
  continuation& operator=(continuation&& r) = delete;
  continuation(const continuation&) = delete;
  continuation(continuation&& r) noexcept : awaiter_(std::move(r.awaiter_)) {};

  continuation() noexcept = default;
  explicit continuation(std::coroutine_handle<promise_type> coroutine)
      : awaiter_(continuation_awaiter_t{coroutine}) {}
  explicit continuation(callback_awaiter_t awaiter)
      : awaiter_(std::move(awaiter)) {}

  bool await_ready() const noexcept {
    return std::visit([](auto& awaiter) { return awaiter.await_ready(); },
                      awaiter_);
  }
  void await_suspend(auto calling_coroutine) noexcept {
    std::visit([&](auto& awaiter) { awaiter.await_suspend(calling_coroutine); },
               awaiter_);
  }
  auto await_resume() {
    return std::visit([](auto& awaiter) { return awaiter.await_resume(); },
                      awaiter_);
  }

  auto get_sync_result(auto handle_exception) {
    return std::get<continuation_awaiter_t>(awaiter_).get_sync_result(
        handle_exception);
  }
  auto get_sync_result() {
    return std::get<continuation_awaiter_t>(awaiter_).get_sync_result();
  }

  auto coroutine() const {
    return std::get<continuation_awaiter_t>(awaiter_).coroutine();
  }
  bool is_sync() const {
    return std::get<continuation_awaiter_t>(awaiter_).is_sync();
  }

 private:
  std::variant<continuation_awaiter_t, callback_awaiter_t> awaiter_;
};

template <typename HandleReturn, typename... Args>
[[clang::suppress("functionStatic")]]
continuation<Args...>
basic_promise_type<HandleReturn, Args...>::get_return_object(
    this auto& self) /* NOLINT(functionStatic) */ {
  return continuation<Args...>{
      std::coroutine_handle<basic_promise_type>::from_promise(self)};
}

template <typename... CallbackArgs>
auto callback(synchronisation sync_or_async, auto&& api) {
  using api_t = decltype(api);
  return continuation<CallbackArgs...>{callback_awaiter<CallbackArgs...>{
      sync_or_async, std::forward<api_t>(api)}};
}
template <typename... CallbackArgs>
auto callback_sync(auto&& api) {
  using api_t = decltype(api);
  return callback<CallbackArgs...>(synchronisation::sync,
                                   std::forward<api_t>(api));
}
template <typename... CallbackArgs>
auto callback_async(auto&& api) {
  using api_t = decltype(api);
  return callback<CallbackArgs...>(synchronisation::async,
                                   std::forward<api_t>(api));
}

template <typename... R>
void spawn([[maybe_unused]] continuation<R...>&& c) {}

}  // namespace ca2co

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

/* NOLINTEND(functionStatic,misc-function-static) */
