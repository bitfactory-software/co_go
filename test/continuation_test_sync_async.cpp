#include <catch2/catch_test_macros.hpp>
#include <chrono>  // NOLINT(misc-include-cleaner)
#include <functional>
#include <print>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility> // NOLINT(misc-include-cleaner)
#define CA2CO_TEST
#include <ca2co/continuation.hpp>

using namespace std::chrono_literals;

namespace {
namespace fixture {
//#if defined __clang__ || defined _MSC_VER
//
//#if defined __clang__
//#pragma GCC diagnostic push
//#pragma GCC diagnostic ignored "-Wunneeded-internal-declaration"
//#endif
using func1 = auto (std::function<void(int)> const& callback) noexcept(false) -> void;
using func2 = auto (std::function<void(int)> const& callback) noexcept -> void;
//#ifdef __GNUC__
//#pragma GCC diagnostic pop
//#endif
static_assert(!ca2co::is_noexept_callback_api<func1, int>);
static_assert(ca2co::is_noexept_callback_api<func2, int>);
//#endif

std::thread
    a_thread;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
std::thread::id
    a_threads_id =  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
    {};
bool
    continuations_run =  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
    false;
constexpr auto n_41 = 41;
constexpr auto n_42 = 42;
constexpr auto d_42 = 42.0;
constexpr auto short_break = 10ms;  // NOLINT(misc-include-cleaner)

void api_async(const std::function<void(int)>& callback) noexcept {
  a_thread = std::thread([=] {
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(short_break);
    callback(n_41);
  });
}
static_assert(ca2co::is_noexept_callback_api<decltype(api_async), int>);

void api_async_callback_no_called(
    [[maybe_unused]] const std::function<void(int)>& callback) noexcept {
  a_thread = std::thread([=] {
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(short_break);
    // this will leak the waiting coroutines...
  });
}

void api_async_callback_throws_in_background_thread(
    [[maybe_unused]] const std::function<void(int)>& callback) noexcept {
  a_thread = std::thread([=] {
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(short_break);
    try {
      throw std::runtime_error("Exception in worker thread");
    } catch (...) {  // must catch
      a_threads_id = std::this_thread::get_id();
      callback(-1);  // signal error
    }
  });
}

ca2co::continuation<int> test_1_async() {
  const int initalValue = 1;
  std::println("Start test_1_async");
  int x = co_await ca2co::callback_async<int>(&api_async);
  x += initalValue;
  CHECK(x == n_42);
  std::println("test_1_async");
  co_return x + 1;
}
ca2co::continuation<int> test_1_sync() {
  const int initalValue = 1;
  std::println("Start test_1_sync");
  int x = n_41;
  x += initalValue;
  CHECK(x == n_42);
  std::println("test_1_sync");
  co_return x + 1;
}

ca2co::continuation<int> test_1_sync_with_exception() {
  std::println("Start test_1_sync_with_exception");
  throw std::runtime_error("test_Exception");
   // NOLINTNEXTLINE(unreachableCode)
  co_return n_42;
}

ca2co::continuation<int> test_1_async_with_exception() {
  std::println("Start test_1_async_with_exception");
  const int x = co_await ca2co::callback_async<int>(&api_async);
  CHECK(x == n_41);
  throw std::runtime_error("test_Exception");
   // NOLINTNEXTLINE(unreachableCode)
  co_return n_42;
}

ca2co::continuation<double> test_2(auto&& test1) {  // NOLINT
  std::println("Start test_2");
  const auto x = co_await test1();
  CHECK(x == n_42 + 1);
  std::println("test_2");
  co_return x + 1.0;
}

ca2co::continuation<double> test_3(auto&& callbackContinuation) {  // NOLINT
  std::println("Start test_3");
  auto x = co_await test_2(callbackContinuation);
  CHECK(x == d_42 + 1.0 + 1.0);
  std::println("test_3");
  co_return x + 1.0;
}

ca2co::continuation<> test_4(auto&& callbackContinuation) {  // NOLINT
  std::println("Start test_4");
  auto x = co_await test_3(callbackContinuation);
  CHECK(x == d_42 + 1.0 + 1.0 + 1.0);
  std::println("test_4");
  continuations_run = true;
  co_return;
}

ca2co::continuation<double> test_4_return_value(
    auto&& callbackContinuation) {  // NOLINT
  std::println("Start test_4_return_value");
  auto x = co_await test_3(callbackContinuation);
  CHECK(x == d_42 + 1.0 + 1.0 + 1.0);
  std::println("test_4_return_value");
  continuations_run = true;
  co_return x;
}

ca2co::continuation<> test_5_catched(auto&& throws) {  // NOLINT
  std::println("Start test_5_catched");

  continuations_run = false;
  try {
    co_await test_4(throws);
    CHECK(false);
  } catch (std::runtime_error& e) {
    CHECK(e.what() == std::string("test_Exception"));
    std::println("test_5_catched catched: {}", e.what());
  }
  CHECK(!continuations_run);
}
}  // namespace fixture
}  // namespace

TEST_CASE("SimpleSynchron") {
  {
    spawn(fixture::test_1_sync());
  }
  CHECK(ca2co::continuation_promise_count == 0);
}

TEST_CASE("Synchron") {
  {
    fixture::continuations_run = false;
    spawn(fixture::test_4(&fixture::test_1_sync));
    CHECK(fixture::continuations_run);
  }
  CHECK(ca2co::continuation_promise_count == 0);
}

TEST_CASE("SynchronAccess_return_value") {
  {
    fixture::continuations_run = false;
    ca2co::continuation<double> continuation =
        fixture::test_4_return_value(&fixture::test_1_sync);
    CHECK(fixture::continuations_run);
    CHECK(continuation.is_sync());
    const double value = continuation.await_resume();
    CHECK(value == fixture::d_42 + 1.0 + 1.0 + 1.0);
    const double value1 = continuation.get_sync_result();
    CHECK(value1 == value);
  }
  CHECK(ca2co::continuation_promise_count == 0);
}

TEST_CASE("SynchronWithException") {
  {
    spawn(fixture::test_5_catched(&fixture::test_1_sync_with_exception));
  }
  CHECK(ca2co::continuation_promise_count == 0);
}

TEST_CASE("AsynchronWithException") {
  {
    spawn(fixture::test_5_catched(&fixture::test_1_async_with_exception));
    fixture::a_thread.join();
  }
  CHECK(ca2co::continuation_promise_count == 0);
}

TEST_CASE("api_async_callback_no_called") {
  CHECK(ca2co::continuation_promise_count == 0);
  {
    bool resumed = false;
    [[maybe_unused]] auto _ = [&] -> ca2co::continuation<> {  // NOLINT
      co_await ca2co::callback_async<int>(
          fixture::api_async_callback_no_called);
      resumed = true;
    }();
    fixture::a_thread.join();
    CHECK(!resumed);
  }
  // CHECK(ca2co::continuation_promise_count ==
  //       1);  // <- LEAKS, because callback not invoked!
  ca2co::continuation_promise_count = 0;
}

namespace {
namespace fixture {
ca2co::continuation<int>
api_async_callback_throws_in_background_thread_wrapped() {
  co_return co_await ca2co::callback_async<int>(
      fixture::api_async_callback_throws_in_background_thread);
}
}  // namespace fixture
}  // namespace

TEST_CASE(  // NOLINT
    "api_async_callback_throws_in_background_thread [continuation]") {  // NOLINT
  CHECK(ca2co::continuation_promise_count == 0);  // NOLINT
  {
    bool resumed = false;
    ca2co::spawn(
        [&]  // NOLINT(cppcoreguidelines-avoid-capturing-lambda-coroutines)
        -> ca2co::  // NOLINT(cppcoreguidelines-avoid-capturing-lambda-coroutines)
        continuation<> {  // NOLINT(cppcoreguidelines-avoid-capturing-lambda-coroutines)
          auto id_start = std::this_thread::get_id();
          auto error = co_await fixture::
              api_async_callback_throws_in_background_thread_wrapped();
          CHECK(error == -1);
          auto id_continuation = std::this_thread::get_id();
          CHECK(id_start != id_continuation);
          CHECK(fixture::a_threads_id == id_continuation);
          resumed = true;
        }());
    fixture::a_thread.join();
    CHECK(resumed);
  }
  CHECK(ca2co::continuation_promise_count == 0);  // NOLINT
}

TEST_CASE("Asynchron") {
  {
    fixture::continuations_run = false;
    spawn(fixture::test_4(&fixture::test_1_async));
  }
  CHECK(!fixture::continuations_run);
  std::println("main after test_4");
  fixture::a_thread.join();
  CHECK(fixture::continuations_run);
  std::println("after join");
  CHECK(ca2co::continuation_promise_count == 0);
}

#if defined _MSC_VER

TEST_CASE("MoveConstructContinuation") {
  auto original([]() -> ca2co::continuation<> { co_return; }());
  CHECK(original.coroutine());
  const ca2co::continuation<> movedTo(std::move(original));
#pragma warning(push)             // NOLINT(clang-diagnostic-unknown-pragmas)
#pragma warning(disable : 26800)  // NOLINT(clang-diagnostic-unknown-pragmas)
  CHECK(
      !original  // NOLINT(bugprone-use-after-move,hicpp-invalid-access-moved)
           .coroutine());  // NOLINT(bugprone-use-after-move,hicpp-invalid-access-moved)
#pragma warning(pop)  // NOLINT(clang-diagnostic-unknown-pragmas)
  CHECK(movedTo.coroutine());
}

#endif