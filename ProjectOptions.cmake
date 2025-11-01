include(cmake/SystemLink.cmake)
include(cmake/LibFuzzer.cmake)
include(CMakeDependentOption)
include(CheckCXXCompilerFlag)


include(CheckCXXSourceCompiles)


macro(cogoproject_supports_sanitizers)
  if((CMAKE_CXX_COMPILER_ID MATCHES ".*Clang.*" OR CMAKE_CXX_COMPILER_ID MATCHES ".*GNU.*") AND NOT WIN32)

    message(STATUS "Sanity checking UndefinedBehaviorSanitizer, it should be supported on this platform")
    set(TEST_PROGRAM "int main() { return 0; }")

    # Check if UndefinedBehaviorSanitizer works at link time
    set(CMAKE_REQUIRED_FLAGS "-fsanitize=undefined")
    set(CMAKE_REQUIRED_LINK_OPTIONS "-fsanitize=undefined")
    check_cxx_source_compiles("${TEST_PROGRAM}" HAS_UBSAN_LINK_SUPPORT)

    if(HAS_UBSAN_LINK_SUPPORT)
      message(STATUS "UndefinedBehaviorSanitizer is supported at both compile and link time.")
      set(SUPPORTS_UBSAN ON)
    else()
      message(WARNING "UndefinedBehaviorSanitizer is NOT supported at link time.")
      set(SUPPORTS_UBSAN OFF)
    endif()
  else()
    set(SUPPORTS_UBSAN OFF)
  endif()

  if((CMAKE_CXX_COMPILER_ID MATCHES ".*Clang.*" OR CMAKE_CXX_COMPILER_ID MATCHES ".*GNU.*") AND WIN32)
    set(SUPPORTS_ASAN OFF)
  else()
    if (NOT WIN32)
      message(STATUS "Sanity checking AddressSanitizer, it should be supported on this platform")
      set(TEST_PROGRAM "int main() { return 0; }")

      # Check if AddressSanitizer works at link time
      set(CMAKE_REQUIRED_FLAGS "-fsanitize=address")
      set(CMAKE_REQUIRED_LINK_OPTIONS "-fsanitize=address")
      check_cxx_source_compiles("${TEST_PROGRAM}" HAS_ASAN_LINK_SUPPORT)

      if(HAS_ASAN_LINK_SUPPORT)
        message(STATUS "AddressSanitizer is supported at both compile and link time.")
        set(SUPPORTS_ASAN ON)
      else()
        message(WARNING "AddressSanitizer is NOT supported at link time.")
        set(SUPPORTS_ASAN OFF)
      endif()
    else()
      set(SUPPORTS_ASAN ON)
    endif()
  endif()
endmacro()

macro(cogoproject_setup_options)
  option(cogoproject_ENABLE_HARDENING "Enable hardening" ON)
  option(cogoproject_ENABLE_COVERAGE "Enable coverage reporting" OFF)
  cmake_dependent_option(
    cogoproject_ENABLE_GLOBAL_HARDENING
    "Attempt to push hardening options to built dependencies"
    ON
    cogoproject_ENABLE_HARDENING
    OFF)

  cogoproject_supports_sanitizers()

  if(NOT PROJECT_IS_TOP_LEVEL OR cogoproject_PACKAGING_MAINTAINER_MODE)
    option(cogoproject_ENABLE_IPO "Enable IPO/LTO" OFF)
    option(cogoproject_WARNINGS_AS_ERRORS "Treat Warnings As Errors" OFF)
    option(cogoproject_ENABLE_USER_LINKER "Enable user-selected linker" OFF)
    option(cogoproject_ENABLE_SANITIZER_ADDRESS "Enable address sanitizer" OFF)
    option(cogoproject_ENABLE_SANITIZER_LEAK "Enable leak sanitizer" OFF)
    option(cogoproject_ENABLE_SANITIZER_UNDEFINED "Enable undefined sanitizer" OFF)
    option(cogoproject_ENABLE_SANITIZER_THREAD "Enable thread sanitizer" OFF)
    option(cogoproject_ENABLE_SANITIZER_MEMORY "Enable memory sanitizer" OFF)
    option(cogoproject_ENABLE_UNITY_BUILD "Enable unity builds" OFF)
    option(cogoproject_ENABLE_CLANG_TIDY "Enable clang-tidy" OFF)
    option(cogoproject_ENABLE_CPPCHECK "Enable cpp-check analysis" OFF)
    option(cogoproject_ENABLE_PCH "Enable precompiled headers" OFF)
    option(cogoproject_ENABLE_CACHE "Enable ccache" OFF)
  else()
    option(cogoproject_ENABLE_IPO "Enable IPO/LTO" ON)
    option(cogoproject_WARNINGS_AS_ERRORS "Treat Warnings As Errors" ON)
    option(cogoproject_ENABLE_USER_LINKER "Enable user-selected linker" OFF)
    option(cogoproject_ENABLE_SANITIZER_ADDRESS "Enable address sanitizer" ${SUPPORTS_ASAN})
    option(cogoproject_ENABLE_SANITIZER_LEAK "Enable leak sanitizer" OFF)
    option(cogoproject_ENABLE_SANITIZER_UNDEFINED "Enable undefined sanitizer" ${SUPPORTS_UBSAN})
    option(cogoproject_ENABLE_SANITIZER_THREAD "Enable thread sanitizer" OFF)
    option(cogoproject_ENABLE_SANITIZER_MEMORY "Enable memory sanitizer" OFF)
    option(cogoproject_ENABLE_UNITY_BUILD "Enable unity builds" OFF)
    option(cogoproject_ENABLE_CLANG_TIDY "Enable clang-tidy" ON)
    option(cogoproject_ENABLE_CPPCHECK "Enable cpp-check analysis" ON)
    option(cogoproject_ENABLE_PCH "Enable precompiled headers" OFF)
    option(cogoproject_ENABLE_CACHE "Enable ccache" ON)
  endif()

  if(NOT PROJECT_IS_TOP_LEVEL)
    mark_as_advanced(
      cogoproject_ENABLE_IPO
      cogoproject_WARNINGS_AS_ERRORS
      cogoproject_ENABLE_USER_LINKER
      cogoproject_ENABLE_SANITIZER_ADDRESS
      cogoproject_ENABLE_SANITIZER_LEAK
      cogoproject_ENABLE_SANITIZER_UNDEFINED
      cogoproject_ENABLE_SANITIZER_THREAD
      cogoproject_ENABLE_SANITIZER_MEMORY
      cogoproject_ENABLE_UNITY_BUILD
      cogoproject_ENABLE_CLANG_TIDY
      cogoproject_ENABLE_CPPCHECK
      cogoproject_ENABLE_COVERAGE
      cogoproject_ENABLE_PCH
      cogoproject_ENABLE_CACHE)
  endif()

  cogoproject_check_libfuzzer_support(LIBFUZZER_SUPPORTED)
  if(LIBFUZZER_SUPPORTED AND (cogoproject_ENABLE_SANITIZER_ADDRESS OR cogoproject_ENABLE_SANITIZER_THREAD OR cogoproject_ENABLE_SANITIZER_UNDEFINED))
    set(DEFAULT_FUZZER ON)
  else()
    set(DEFAULT_FUZZER OFF)
  endif()

  option(cogoproject_BUILD_FUZZ_TESTS "Enable fuzz testing executable" ${DEFAULT_FUZZER})

endmacro()

macro(cogoproject_global_options)
  if(cogoproject_ENABLE_IPO)
    include(cmake/InterproceduralOptimization.cmake)
    cogoproject_enable_ipo()
  endif()

  cogoproject_supports_sanitizers()

  if(cogoproject_ENABLE_HARDENING AND cogoproject_ENABLE_GLOBAL_HARDENING)
    include(cmake/Hardening.cmake)
    if(NOT SUPPORTS_UBSAN 
       OR cogoproject_ENABLE_SANITIZER_UNDEFINED
       OR cogoproject_ENABLE_SANITIZER_ADDRESS
       OR cogoproject_ENABLE_SANITIZER_THREAD
       OR cogoproject_ENABLE_SANITIZER_LEAK)
      set(ENABLE_UBSAN_MINIMAL_RUNTIME FALSE)
    else()
      set(ENABLE_UBSAN_MINIMAL_RUNTIME TRUE)
    endif()
    message("${cogoproject_ENABLE_HARDENING} ${ENABLE_UBSAN_MINIMAL_RUNTIME} ${cogoproject_ENABLE_SANITIZER_UNDEFINED}")
    cogoproject_enable_hardening(cogoproject_options ON ${ENABLE_UBSAN_MINIMAL_RUNTIME})
  endif()
endmacro()

macro(cogoproject_local_options)
  if(PROJECT_IS_TOP_LEVEL)
    include(cmake/StandardProjectSettings.cmake)
  endif()

  add_library(cogoproject_warnings INTERFACE)
  add_library(cogoproject_options INTERFACE)

  include(cmake/CompilerWarnings.cmake)
  cogoproject_set_project_warnings(
    cogoproject_warnings
    ${cogoproject_WARNINGS_AS_ERRORS}
    ""
    ""
    ""
    "")

  if(cogoproject_ENABLE_USER_LINKER)
    include(cmake/Linker.cmake)
    cogoproject_configure_linker(cogoproject_options)
  endif()

  include(cmake/Sanitizers.cmake)
  cogoproject_enable_sanitizers(
    cogoproject_options
    ${cogoproject_ENABLE_SANITIZER_ADDRESS}
    ${cogoproject_ENABLE_SANITIZER_LEAK}
    ${cogoproject_ENABLE_SANITIZER_UNDEFINED}
    ${cogoproject_ENABLE_SANITIZER_THREAD}
    ${cogoproject_ENABLE_SANITIZER_MEMORY})

  set_target_properties(cogoproject_options PROPERTIES UNITY_BUILD ${cogoproject_ENABLE_UNITY_BUILD})

  if(cogoproject_ENABLE_PCH)
    target_precompile_headers(
      cogoproject_options
      INTERFACE
      <vector>
      <string>
      <utility>)
  endif()

  if(cogoproject_ENABLE_CACHE)
    include(cmake/Cache.cmake)
    cogoproject_enable_cache()
  endif()

  include(cmake/StaticAnalyzers.cmake)
  if(cogoproject_ENABLE_CLANG_TIDY)
    cogoproject_enable_clang_tidy(cogoproject_options ${cogoproject_WARNINGS_AS_ERRORS})
  endif()

  if(cogoproject_ENABLE_CPPCHECK)
    cogoproject_enable_cppcheck(${cogoproject_WARNINGS_AS_ERRORS} "" # override cppcheck options
    )
  endif()

  if(cogoproject_ENABLE_COVERAGE)
    include(cmake/Tests.cmake)
    cogoproject_enable_coverage(cogoproject_options)
  endif()

  if(cogoproject_WARNINGS_AS_ERRORS)
    check_cxx_compiler_flag("-Wl,--fatal-warnings" LINKER_FATAL_WARNINGS)
    if(LINKER_FATAL_WARNINGS)
      # This is not working consistently, so disabling for now
      # target_link_options(cogoproject_options INTERFACE -Wl,--fatal-warnings)
    endif()
  endif()

  if(cogoproject_ENABLE_HARDENING AND NOT cogoproject_ENABLE_GLOBAL_HARDENING)
    include(cmake/Hardening.cmake)
    if(NOT SUPPORTS_UBSAN 
       OR cogoproject_ENABLE_SANITIZER_UNDEFINED
       OR cogoproject_ENABLE_SANITIZER_ADDRESS
       OR cogoproject_ENABLE_SANITIZER_THREAD
       OR cogoproject_ENABLE_SANITIZER_LEAK)
      set(ENABLE_UBSAN_MINIMAL_RUNTIME FALSE)
    else()
      set(ENABLE_UBSAN_MINIMAL_RUNTIME TRUE)
    endif()
    cogoproject_enable_hardening(cogoproject_options OFF ${ENABLE_UBSAN_MINIMAL_RUNTIME})
  endif()

endmacro()
