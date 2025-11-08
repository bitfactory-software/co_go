#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <functional>
#include <print>
#include <thread>

#define CO_GO_CONTINUATION_TEST
#include <co_go/continuation.hpp>

namespace {
namespace fixture {
#ifdef __clang__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunneeded-internal-declaration"
#endif
auto func1(std::function<void(int)> const& callback) noexcept(false) -> void;
auto func2(std::function<void(int)> const& callback) noexcept -> void;
#ifdef __clang__
#pragma GCC diagnostic pop
#endif

static_assert(!co_go::is_noexept_callback_api<decltype(func1), int>);
static_assert(co_go::is_noexept_callback_api<decltype(func2), int>);

std::thread a_thread;
std::thread::id a_threads_id = {};
bool continuations_run = false;

void api_async(const std::function<void(int)>& callback) noexcept {
  a_thread = std::thread([=] {
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(10ms);
    callback(41);
  });
}
static_assert(co_go::is_noexept_callback_api<decltype(api_async), int>);

void api_async_callback_no_called(
    [[maybe_unused]] const std::function<void(int)>& callback) noexcept {
  a_thread = std::thread([=] {
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(10ms);
    // this will leak the waiting coroutines...
  });
}

#ifdef __clang__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
void api_async_callback_throws_unhandled_in_calling_thread(
    [[maybe_unused]] const std::function<void(int)>& callback) noexcept {
  // vvv not allowed, does not compile!
  // throw std::runtime_error("test_Exception in calling thread");
}
#ifdef __clang__
#pragma GCC diagnostic pop
#endif

void api_async_callback_throws_in_background_thread(
    [[maybe_unused]] const std::function<void(int)>& callback) noexcept {
  a_thread = std::thread([=] {
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(10ms);
    try {
      throw std::runtime_error("Exception in worker thread");
    } catch (...) {  // must catch
      a_threads_id = std::this_thread::get_id();
      callback(-1);  // signal error
    }
  });
}

co_go::continuation<int> test_1_async() {
  int initalValue = 1;
  std::println("Start test_1_async");
  int x = co_await co_go::callback_async<int>(&api_async);
  x += initalValue;
  CHECK(x == 42);
  std::println("test_1_async");
  co_return x += 1;
}
co_go::continuation<int> test_1_sync() {
  int initalValue = 1;
  std::println("Start test_1_sync");
  int x = 41;
  x += initalValue;
  CHECK(x == 42);
  std::println("test_1_sync");
  co_return x += 1;
}

co_go::continuation<int> test_1_sync_with_exception() {
  std::println("Start test_1_sync_with_exception");
  throw std::runtime_error("test_Exception");
  co_return 42;
}

co_go::continuation<int> test_1_async_with_exception() {
  std::println("Start test_1_async_with_exception");
  int x = co_await co_go::callback_async<int>(&api_async);
  CHECK(x == 41);
  throw std::runtime_error("test_Exception");
  co_return 42;
}

co_go::continuation<double> test_2(auto&& test1) {
  std::println("Start test_2");
  auto x = co_await test1();
  CHECK(x == 43);
  std::println("test_2");
  co_return x + 1.0;
}

co_go::continuation<double> test_3(auto&& callbackContinuation) {
  std::println("Start test_3");
  auto x = co_await test_2(callbackContinuation);
  CHECK(x == 44.0);
  std::println("test_3");
  co_return x + 1.0;
}

co_go::continuation<> test_4(auto&& callbackContinuation) {
  std::println("Start test_4");
  auto x = co_await test_3(callbackContinuation);
  CHECK(x == 45.0);
  std::println("test_4");
  continuations_run = true;
  co_return;
}

co_go::continuation<double> test_4_return_value(auto&& callbackContinuation) {
  std::println("Start test_4_return_value");
  auto x = co_await test_3(callbackContinuation);
  CHECK(x == 45.0);
  std::println("test_4_return_value");
  continuations_run = true;
  co_return x;
}

co_go::continuation<> test_5_catched(auto&& throws) {
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
  CHECK(co_go::continuation_promise_count == 0);
}

TEST_CASE("Synchron") {
  {
    fixture::continuations_run = false;
    spawn(fixture::test_4(&fixture::test_1_sync));
    CHECK(fixture::continuations_run);
  }
  CHECK(co_go::continuation_promise_count == 0);
}

TEST_CASE("SynchronAccess_return_value") {
  {
    fixture::continuations_run = false;
    co_go::continuation<double> continuation =
        fixture::test_4_return_value(&fixture::test_1_sync);
    CHECK(fixture::continuations_run);
    CHECK(continuation.is_sync());
    double value = continuation.await_resume();
    CHECK(value == 45);
    double value1 = continuation.get_sync_result();
    CHECK(value1 == 45);
  }
  CHECK(co_go::continuation_promise_count == 0);
}

TEST_CASE("SynchronWithException") {
  {
    spawn(fixture::test_5_catched(&fixture::test_1_sync_with_exception));
  }
  CHECK(co_go::continuation_promise_count == 0);
}

TEST_CASE("AsynchronWithException") {
  {
    spawn(fixture::test_5_catched(&fixture::test_1_async_with_exception));
    fixture::a_thread.join();
  }
  CHECK(co_go::continuation_promise_count == 0);
}

TEST_CASE("api_async_callback_no_called") {
  CHECK(co_go::continuation_promise_count == 0);
  {
    bool resumed = false;
    [[maybe_unused]] auto _ = [&] -> co_go::continuation<> {
      co_await co_go::callback_async<int>(
          fixture::api_async_callback_no_called);
      resumed = true;
    }();
    fixture::a_thread.join();
    CHECK(!resumed);
  }
  //CHECK(co_go::continuation_promise_count ==
  //      1);  // <- LEAKS, because callback not invoked!
  co_go::continuation_promise_count = 0;
}

namespace {
namespace fixture {
co_go::continuation<int>
api_async_callback_throws_in_background_thread_wrapped() {
  co_return co_await co_go::callback_async<int>(
      fixture::api_async_callback_throws_in_background_thread);
}
}  // namespace fixture
}  // namespace

TEST_CASE("api_async_callback_throws_in_background_thread [continuation]") {
  CHECK(co_go::continuation_promise_count == 0);
  {
    bool resumed = false;
    co_go::spawn([&] -> co_go::continuation<> {
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
  CHECK(co_go::continuation_promise_count == 0);
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
  CHECK(co_go::continuation_promise_count == 0);
}

TEST_CASE("MoveConstructContinuation") {
  auto original([]() -> co_go::continuation<> { co_return; }());
  CHECK(original.coroutine());
  co_go::continuation<> movedTo(std::move(original));
#pragma warning(push)
#pragma warning(disable : 26800)
  CHECK(!original.coroutine());  // moved from
#pragma warning(pop)
  CHECK(movedTo.coroutine());
}
