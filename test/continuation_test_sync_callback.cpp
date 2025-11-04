#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <co_go/continuation.hpp>
#include <functional>
#include <future>
#include <print>

namespace {
int step = 1;

// + lib callback style
void int_callback_api(std::function<void(int)> const& callback) noexcept {
  std::println("before callback");
  callback(42);
  std::println("after callback");
};
// - lib callback style
// + lib wrapped for coro style
auto int_recieve_coro() {
  return co_go::await_callback<int>(int_callback_api);
};
// - lib wrapped for coro style
co_go::continuation<int> int_recieve_coro_indirect() {
  auto x = co_await co_go::await_callback<int>(int_callback_api);
  std::println("int_recieve_coro_indirect after callback {}", x);
  co_return x + 1;
};
// - lib wrapped for coro style

//// + lib callback style
//auto void_callback_api(std::function<void(void)> const& callback) {
//  std::println("before callback");
//  CHECK(step++ == 1);
//  callback();
//  std::println("after callback");
//  CHECK(step++ == 3);
//};

}  // namespace

TEST_CASE("int [continuation]") {
  // + call callback style
  step = 1;
  CHECK(step == 1);
  int_callback_api([&](int _42) {
    std::println("recieving 42");
    CHECK(42 == _42);
  });
  // - app callback style

  [&] -> co_go::continuation<void> {
    // call coro style must exist inside a coro
    auto _42 = co_await int_recieve_coro();
    std::println("recieving 42");
    CHECK(42 == _42);
  }();

  [&] -> co_go::continuation<void> {
    // call coro style must exist inside a coro
    auto _43 = co_await int_recieve_coro_indirect();
    std::println("recieving 43");
    CHECK(43 == _43);
  }();
}

namespace {
void async_api(std::function<void(int)> const& continuation) noexcept {
  auto unused = std::async(std::launch::async, [=] {
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(10ms);
    std::println("sleep on thread {}", std::this_thread::get_id());
    continuation(42);
    std::println("after call to continuation async_api");
  });
}
co_go::continuation<int> async_api_coro() {
  co_return co_await co_go::await_callback<int>(async_api);
};
co_go::continuation<int> async_api_coro_indirect() {
  auto x = co_await async_api_coro();
  CHECK(x == 42);
  co_return x + 1;
};
}  // namespace

TEST_CASE("int async [continuation]") {
  auto id_start = std::this_thread::get_id();
  auto called = false;
  [&] -> co_go::continuation<void> {
    // call coro style must exist inside a coro
    auto _42 = co_await async_api_coro();  // blocks!
    std::println("recieving 42");
    called = true;
    CHECK(42 == _42);
    CHECK(id_start == std::this_thread::get_id());
  }();
  CHECK(called);
}

TEST_CASE("int async indirect [continuation]") {
  auto id_start = std::this_thread::get_id();
  auto called = false;
  [&] -> co_go::continuation<void> {
    // call coro style must exist inside a coro
    auto _43 = co_await async_api_coro_indirect();
    std::println("recieving 43");
    called = true;
    CHECK(43 == _43);
    CHECK(id_start == std::this_thread::get_id());
  }();
  CHECK(called);
}
