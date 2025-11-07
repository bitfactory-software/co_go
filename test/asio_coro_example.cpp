#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <functional>
#include <future>
#include <print>
#include <thread>

#define CO_GO_CONTINUATION_TEST
#include <co_go/continuation.hpp>

// namespace co_go {
//
//
//
// template < typename... CallbackArgs,
//           synchronisation sync_or_async = synchronisation::sync>
//   requires(is_noexept_callback_api<R, Api>)
// struct callback {
//   bool await_ready() { return false; }
//   void await_suspend(auto calling_coroutine) {
//     calling_coroutine.promise().sync_ = sync_or_async ==
//     synchronisation::sync; bool called = false; api_([this,
//     calling_coroutine, called](const R& r) mutable {
//       if (called) return;
//       called = true;
//       result_ = r;
//       calling_coroutine.resume();
//     });
//   }
//   R await_resume() { return result_; }
//   const Api api_;
//   R result_ = {};
// };
//
// template<synchronisation sync_or_async, typename R, typename... Args >
// class callback_api;
// template<synchronisation sync_or_async, typename R, typename... Args >
// class callback_api<sync_or_async, R(Args...)>;
//
//     co_go::callback_api<std::tuple<std::string, int>(std::string_view)>
//
// }
//
namespace {
namespace fixture {
void async_api_string_view_int(
    std::function<void(std::string_view, int)> callback) noexcept {
  auto unused = std::async(std::launch::async, [=] {
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(10ms);
    std::println("sleep on thread {}", std::this_thread::get_id());
    callback("hello world", 42);
    std::println("after call to continuation async_api");
  });
}
  void async_api_bool_double_to_string_view_int(
      bool, int, std::function<void(std::string_view, int)> callback) noexcept {
    auto unused = std::async(std::launch::async, [=] {
      using namespace std::chrono_literals;
      std::this_thread::sleep_for(10ms);
      std::println("sleep on thread {}", std::this_thread::get_id());
      callback("hello world", 42);
      std::println("after call to continuation async_api");
    });
  }

  static_assert(
      !co_go::is_noexept_callback_api_v<decltype(async_api_string_view_int),
                                        std::tuple<std::string_view, int>>);
  static_assert(
      co_go::is_noexept_callback_api_v<decltype(async_api_string_view_int),
                                       std::string_view, int>);
}  // namespace fixture
}  // namespace

TEST_CASE("async_api_string_view_int") {
  auto called = false;
  [&] -> co_go::continuation<void> {
    auto [s, i] = co_await co_go::callback_async<std::string_view, int>(
        fixture::async_api_string_view_int);
    CHECK(s == "hello world");
    CHECK(i == 42);
    called = true;
  }();
  CHECK(called);
}

//TEST_CASE("async_api_bool_double_to_string_view_int") {
//  auto called = false;
//  [&] -> co_go::continuation<void> {
//    auto [s, i] = co_await co_go::callback_async<std::string_view, int>(
//        fixture::async_api_bool_double_to_string_view_int, true, 3.14);
//    CHECK(s == "hello world");
//    CHECK(i == 42);
//    called = true;
//  }();
//  CHECK(called);
//}
