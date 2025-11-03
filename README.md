# co_go: A c++ [first-class continuation](https://en.wikipedia.org/wiki/Continuation#First-class_continuations) coroutine  

[![ci](https://github.com/cpp-best-practices/cmake_template/actions/workflows/ci.yml/badge.svg)](https://github.com/cpp-best-practices/cmake_template/actions/workflows/ci.yml)
[![codecov](https://codecov.io/gh/cpp-best-practices/cmake_template/branch/main/graph/badge.svg)](https://codecov.io/gh/cpp-best-practices/cmake_template)
[![CodeQL](https://github.com/cpp-best-practices/cmake_template/actions/workflows/codeql-analysis.yml/badge.svg)](https://github.com/cpp-best-practices/cmake_template/actions/workflows/codeql-analysis.yml)

## Why co_go::continuation?

Our product had its origin as a classic Windows application. In that envirionmet you can write great parts of the user interface in a sequetial manner via 'modal dialogs'. That looks like this:

```cpp
...
if (AfxMessageBox("Continue?", MB_YESNO) != MB_YES)
  return;
... // continue(!) with further processing
```

In more modern programming models, e.g. 'QML' this pattern changes to a 'callback' style like this:

```cpp
show_message_box("Continue?", {"Yes","No" }, [&](std::string_view choice){
    // continue(!) with further processing
    });
} // usualy the end of the function
```

What to do, when you want to port the logic written for windows style to an architecture, that requires a callback style? You go for a continuation coroutine.
With that tool you can go like this:

```cpp
class abstract_message_box {
virtual co_go::continuation<std::string>
  show (std::string_view text, std::initializer_list<std::string_view> const& options) = 0; 
}
```









