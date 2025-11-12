#include <catch2/catch_test_macros.hpp>
#include <chrono>  // NOLINT(misc-include-cleaner)
#include <functional>
#include <optional>
#include <print>
#include <ranges>
#include <string_view>
#include <thread>
#include <tuple>

#define CA2CO_TEST
#include <ca2co/continuation.hpp>

using namespace std::chrono_literals;

namespace {
namespace fixture {

std::thread
    a_thread;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
constexpr auto short_break = 10ms;  // NOLINT(misc-include-cleaner)
constexpr auto answer_number = 42;

void async_api_string_view_int(
    std::function<void(std::string_view, int)> const& callback) noexcept {
  a_thread = std::thread{[=] {
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(short_break);
    std::println("sleep on thread {}", std::this_thread::get_id());
    callback("hello world", answer_number);
    std::println("after call to continuation async_api");
  }};
}
ca2co::continuation<std::string_view, int> co_async_api_string_view_int() {
  co_return co_await ca2co::callback_async<std::string_view, int>(
      fixture::async_api_string_view_int);
};
ca2co::continuation<std::string_view, int> co_sync_api_string_view_int() {
  co_return std::make_tuple<std::string_view, int>(std::string_view("xy"), 2);
}

void loop_callback_api(
    std::function<void(ca2co::iterator<int>)> const& callback) noexcept {
  for (auto i : std::ranges::iota_view(0, 4)) callback(std::optional{i});
  callback({});
}

ca2co::continuation<ca2co::iterator<int>> co_loop_api() {
  return ca2co::callback_sync<ca2co::iterator<int>>(loop_callback_api);
}

}  // namespace fixture
}  // namespace

TEST_CASE("sync_api_string_view_int") {
  static auto called = false;
  [&] -> ca2co::continuation<> {  // NOLINT
    auto [a_s, an_i] = co_await fixture::co_sync_api_string_view_int();
    CHECK(a_s == "xy");
    CHECK(an_i == 2);
    called = true;
  }();
  CHECK(called);
}

TEST_CASE("async_api_string_view_int direct") {
  static auto called = false;
  [&] -> ca2co::continuation<> {  // NOLINT
    auto [a_s, an_i] = co_await ca2co::callback_async<std::string_view, int>(
        fixture::async_api_string_view_int);
    CHECK(a_s == "hello world");
    CHECK(an_i == fixture::answer_number);
    called = true;
  }();
  fixture::a_thread.join();
  CHECK(called);
}

TEST_CASE("async_api_string_view_int indirect") {
  static auto called = false;
  [&] -> ca2co::continuation<> {  // NOLINT
    auto [a_s, an_i] = co_await fixture::co_async_api_string_view_int();
    CHECK(a_s == "hello world");
    CHECK(an_i == fixture::answer_number);
    called = true;
  }();
  fixture::a_thread.join();
  CHECK(called);
}

TEST_CASE("co_loop_api sync") {
  static auto sum = 0;
  static_assert(!ca2co::is_iterator<std::optional<int>>);
  static_assert(ca2co::is_iterator<ca2co::iterator<int>>);
  using callback_awaiter_t = decltype(fixture::co_loop_api());
  [&] -> ca2co::continuation<> {  // NOLINT
    for (auto __i = co_await fixture::co_loop_api(); __i; co_await __i)
      if (auto i = *__i; true) sum += i;
  }();
  CHECK(sum == 1 + 2 + 3);
}

TEST_CASE("CA2CO_for_co_await sync") {
  static auto sum = 0;
  static_assert(!ca2co::is_iterator<std::optional<int>>);
  static_assert(ca2co::is_iterator<ca2co::iterator<int>>);
  using callback_awaiter_t = decltype(fixture::co_loop_api());
  [&] -> ca2co::continuation<> {  // NOLINT
    CA2CO_for_co_await(auto i, fixture::co_loop_api()) sum += i;
  }();
  CHECK(sum == 1 + 2 + 3);
}
