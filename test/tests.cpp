#include <catch2/catch_test_macros.hpp>
#include <co_go/callback.hpp>
#include <functional>
#include <print>

namespace {
int step = 1;

// + lib callback style
void int_callback_api(std::function<void(int)> callback)
{
  std::println("before callback");
  CHECK(step++ == 1);
  callback(42);
  std::println("after callback");
  CHECK(step++ == 3);
};
// - lib callback style
// + lib wrapped for coro style
auto int_recieve_coro()
{
  return co_go::wrap<int>(int_callback_api);
};
// - lib wrapped for coro style

// + lib callback style
auto void_callback_api(std::function<void(void)> callback)
{
  std::println("before callback");
  CHECK(step++ == 1);
  callback();
  std::println("after callback");
  CHECK(step++ == 3);
};
// - lib callback style
// + lib wrapped for coro style
auto void_recieve_coro() { return co_go::wrap<void>(void_callback_api); };
// - lib wrapped for coro style

}// namespace

TEST_CASE("int [callback]")
{
  // + call callback style
  step = 1;
  CHECK(step == 1);
  int_callback_api([&](int _42) {
    std::println("recieving 42");
    CHECK(step++ == 2);
    CHECK(42 == _42);
  });
  CHECK(step == 4);
  // - app callback style

  step = 1;
  CHECK(step == 1);
  [&] -> co_go::callback<void> {
    // call coro style must exist inside a coro
    auto _42 = co_await int_recieve_coro();
    std::println("recieving 42");
    CHECK(step++ == 2);
    CHECK(42 == _42);
  }();
  CHECK(step == 4);
}

TEST_CASE("void [callback]")
{
  // + app callback style
  step = 1;
  CHECK(step == 1);
  void_callback_api([&] {
    std::println("recieving");
    CHECK(step++ == 2);
  });
  CHECK(step == 4);
  // - app callback style

  step = 1;
  [&] -> co_go::callback<void> {
    // + app coro style must exist inside a coro
    co_await void_recieve_coro();
    std::println("recieving void");
    CHECK(step++ == 2);
    // - app coro style
  }();
  CHECK(step == 4);
}
