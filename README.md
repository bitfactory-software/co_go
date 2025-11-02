# co_go: A c++ [first-class continuation](https://en.wikipedia.org/wiki/Continuation#First-class_continuations) coroutine  

[![ci](https://github.com/cpp-best-practices/cmake_template/actions/workflows/ci.yml/badge.svg)](https://github.com/cpp-best-practices/cmake_template/actions/workflows/ci.yml)
[![codecov](https://codecov.io/gh/cpp-best-practices/cmake_template/branch/main/graph/badge.svg)](https://codecov.io/gh/cpp-best-practices/cmake_template)
[![CodeQL](https://github.com/cpp-best-practices/cmake_template/actions/workflows/codeql-analysis.yml/badge.svg)](https://github.com/cpp-best-practices/cmake_template/actions/workflows/codeql-analysis.yml)

## Why "co_go::continuation"?

Our product had its origin as a classic Windows application. In that envirionmet you can write great parts of the user interface in a sequetial manner via 'modal dialogs'. That looks like this:

'''
...
if (AfxMessageBox("Continue?", MB_YESNO) != MB_YES)
  return;
... // continue(!) with further processing
'''



