#----------------------------------------------------------------------------------------------
# Copyright (c) The Einsums Developers. All rights reserved.
# Licensed under the MIT License. See LICENSE.txt in the project root for license information.
#----------------------------------------------------------------------------------------------

# EinsumsTidy — Einsums-specific clang-tidy checks.
#
# Built as a shared library (a "clang-tidy plugin") loaded via
# ``clang-tidy --load=<path-to-EinsumsTidy>``. The module's static
# initializer registers a ``ClangTidyModuleRegistry::Add<EinsumsTidyModule>``
# at load time, which makes the new ``einsums-*`` checks available to any
# clang-tidy invocation.
#
# Requires Clang development headers. The conda ``einsums-dev`` env ships
# these already, so for that env configuring is a one-liner:
#
#     cmake -S clang-tidy -B build-einsums-tidy
#     cmake --build build-einsums-tidy
#
# The module is built as a standalone .dylib/.so/.dll so it can version-skew
# freely from whatever clangd version is in use, and developers can test
# checks via the standalone ``clang-tidy`` binary on the command line. (The
# sibling ``../clangd-build/`` target embeds the same checks directly into
# a custom clangd binary; same checks, different deployment.)

# Allow this directory to act as a standalone CMake project (the common case
# now that the EinsumsClangd repo is split out) while still being tolerant of
# being pulled in via ``add_subdirectory`` from a parent.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.20)
    project(einsums_tidy LANGUAGES CXX)
    set(_EINSUMS_TIDY_STANDALONE TRUE)
else()
    set(_EINSUMS_TIDY_STANDALONE FALSE)
endif()

# Default ON when building this directory directly; OFF when used as a
# subdirectory of a larger project (parent opts in explicitly).
option(EINSUMS_BUILD_TIDY_MODULE
    "Build the EinsumsTidy clang-tidy plugin module"
    ${_EINSUMS_TIDY_STANDALONE})

# The list of check source files is shared between two targets:
#   * The standalone .dylib plugin (this directory) — built when
#     EINSUMS_BUILD_TIDY_MODULE=ON (default ON when configured standalone).
#   * The custom clangd binary (../clangd-build/) — that subdir's own CMake
#     project reads ``EINSUMS_TIDY_CHECK_SOURCES`` out of this list and
#     compiles the same .cpp files into clangd's link line.
# Both pick up the same EinsumsTidyModule registration and the same checks;
# only the deployment differs (load via --load= vs. statically linked).
# Define the file list once, in both relative and absolute form, so the
# clangd-build target can reach across without copying.
set(EINSUMS_TIDY_CHECK_SOURCES_RELATIVE
    EinsumsTidyModule.cpp
    EinsumsRedundantPermuteCheck.cpp
    EinsumsOutputNotInInputsCheck.cpp
    EinsumsNoLinkIndexCheck.cpp
    EinsumsDuplicateOutputIndexCheck.cpp
    EinsumsMixedIndexModeCheck.cpp
    EinsumsRuntimeSpecCheck.cpp
    EinsumsTensorByValueParamCheck.cpp
    EinsumsTensorViewFromTemporaryCheck.cpp
    EinsumsOutputAliasingInputCheck.cpp
    EinsumsMismatchedValueTypeCheck.cpp
    EinsumsRankMismatchCheck.cpp
    EinsumsAllocatingInLoopCheck.cpp
    EinsumsCgCallOutsideCaptureCheck.cpp
    EinsumsPreferCgFromTensorAlgebraCheck.cpp
    EinsumsDeprecatedApiCheck.cpp
    EinsumsCatch2TensorFixtureLeakCheck.cpp
    CACHE INTERNAL "Source files for EinsumsTidy checks (relative to clang-tidy/)"
)
set(_einsums_tidy_dir "${CMAKE_CURRENT_SOURCE_DIR}")
set(EINSUMS_TIDY_CHECK_SOURCES "")
foreach(_src IN LISTS EINSUMS_TIDY_CHECK_SOURCES_RELATIVE)
    list(APPEND EINSUMS_TIDY_CHECK_SOURCES "${_einsums_tidy_dir}/${_src}")
endforeach()
set(EINSUMS_TIDY_CHECK_SOURCES ${EINSUMS_TIDY_CHECK_SOURCES}
    CACHE INTERNAL "Absolute paths to EinsumsTidy check source files"
)
set(EINSUMS_TIDY_CHECK_DIR "${_einsums_tidy_dir}"
    CACHE INTERNAL "Directory containing EinsumsTidy check headers"
)

if(NOT EINSUMS_BUILD_TIDY_MODULE)
    return()
endif()

# --- Locate Clang/LLVM development packages -------------------------------
# ``find_package(Clang)`` pulls in the AST + AST matchers + ClangTidy headers.
# It also defines the imported targets we link against (``clangAST``,
# ``clangASTMatchers``, ``clangTidy``, ``clangTidyUtils`` …).
find_package(Clang REQUIRED CONFIG)
find_package(LLVM REQUIRED CONFIG)

message(STATUS "EinsumsTidy: using LLVM ${LLVM_VERSION} from ${LLVM_DIR}")
message(STATUS "EinsumsTidy: using Clang from ${Clang_DIR}")

# clang-tidy's headers are tucked under ``<llvm-prefix>/include/clang-tidy/``
# rather than ``<llvm-prefix>/include/clang/``. The Clang package's include
# dirs cover the latter; we add the former explicitly.
set(_CLANG_TIDY_INCLUDE_DIR "${LLVM_INCLUDE_DIRS}")

# --- Module library --------------------------------------------------------
add_library(EinsumsTidy MODULE ${EINSUMS_TIDY_CHECK_SOURCES_RELATIVE})

target_include_directories(EinsumsTidy PRIVATE
    ${LLVM_INCLUDE_DIRS}
    ${CLANG_INCLUDE_DIRS}
)

# Link only clang-tidy framework + AST matchers. The host clang-tidy process
# already has the rest of the LLVM/Clang symbols; on macOS we let the loader
# resolve at load time via dynamic_lookup, on Linux the same is implicit
# unless --no-undefined is set.
target_link_libraries(EinsumsTidy PRIVATE
    clangTidy
    clangTidyUtils
    clangAST
    clangASTMatchers
    clangBasic
    clangLex
)

# Plugins use C++17 minimum (matching clang-tidy's own dialect); the rest of
# Studio is C++23, which is fine for headers we author but the LLVM headers
# only commit to 17.
set_target_properties(EinsumsTidy PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    PREFIX ""        # keep ``EinsumsTidy.dylib`` / ``EinsumsTidy.so`` / ``EinsumsTidy.dll``
)

# Macro defines parity with clang-tidy's own modules. Mostly cosmetic but
# guards against accidental visibility changes if we ever pick up
# ``CLANG_TIDY_EXPORT`` macros.
target_compile_definitions(EinsumsTidy PRIVATE
    CLANG_TIDY_PLUGIN
)

# Per-platform load-time symbol resolution. macOS needs explicit
# dynamic_lookup; Linux is permissive by default.
if(APPLE)
    set_target_properties(EinsumsTidy PROPERTIES
        SUFFIX ".dylib"
        LINK_FLAGS "-undefined dynamic_lookup -flat_namespace"
    )
elseif(UNIX)
    set_target_properties(EinsumsTidy PROPERTIES
        SUFFIX ".so"
    )
endif()

# Disable RTTI to match how the in-tree clang-tidy modules are built. LLVM's
# ABI compatibility expects this — mismatched RTTI between the host and the
# plugin causes silent symbol resolution failures.
target_compile_options(EinsumsTidy PRIVATE -fno-rtti)

# --- Optional: integration tests via lit (the clang-tidy convention). The
# in-tree tests use ``check_clang_tidy.py``; we shell out to the existing
# clang-tidy binary in the conda env and grep stdout. Wired here as a
# best-effort target — won't run by default, just available via ctest.
# Default ON when standalone, otherwise honor the parent ``EINSUMS_WITH_TESTS``.
option(EINSUMS_TIDY_BUILD_TESTS
    "Register ctest entries for the EinsumsTidy fixtures"
    ${_EINSUMS_TIDY_STANDALONE})
if(EINSUMS_TIDY_BUILD_TESTS OR EINSUMS_WITH_TESTS)
    enable_testing()
    # Each entry: (test name, fixture, expected-message-substring). The
    # runner spawns ``clang-tidy --load=...`` and asserts every line tagged
    # ``// EXPECT:`` produces a diagnostic whose message contains the substring.
    set(_tidy_cases
        "einsums_tidy_redundant_permute|redundant-permute.cpp|redundant permute"
        "einsums_tidy_output_not_in_inputs|output-not-in-inputs.cpp|does not appear in either input"
        "einsums_tidy_no_link_index|no-link-index.cpp|silent outer product"
        "einsums_tidy_duplicate_output_index|duplicate-output-index.cpp|appears more than once"
        "einsums_tidy_mixed_index_mode|mixed-index-mode.cpp|mixes comma-separated and single-char"
        "einsums_tidy_runtime_spec|runtime-spec.cpp|not a string literal"
        "einsums_tidy_tensor_by_value_param|tensor-by-value-param.cpp|tensor type taken by value"
        "einsums_tidy_tensor_view_from_temporary|tensor-view-from-temporary.cpp|initialized from a temporary"
        "einsums_tidy_output_aliasing_input|output-aliasing-input.cpp|aliasing across the"
        "einsums_tidy_mismatched_value_type|mismatched-value-type.cpp|mismatched value types"
        "einsums_tidy_rank_mismatch|rank-mismatch.cpp|rank mismatch"
        "einsums_tidy_allocating_in_loop|allocating-in-loop.cpp|declared inside a loop"
        "einsums_tidy_cg_call_outside_capture|cg-call-outside-capture.cpp|no ``CaptureGuard`` in any enclosing scope"
        "einsums_tidy_prefer_cg_from_tensoralgebra|prefer-cg-from-tensoralgebra.cpp|inside a ``CaptureGuard`` scope"
        "einsums_tidy_deprecated_api|deprecated-api.cpp|deprecated"
        "einsums_tidy_catch2_tensor_fixture_leak|catch2-tensor-fixture-leak.cpp|never reaches a Catch2 assertion"
    )
    foreach(_case IN LISTS _tidy_cases)
        string(REPLACE "|" ";" _parts "${_case}")
        list(GET _parts 0 _name)
        list(GET _parts 1 _fixture)
        list(GET _parts 2 _expected)
        add_test(NAME ${_name}
            COMMAND ${CMAKE_COMMAND} -E env
                CLANG_TIDY=$ENV{CONDA_PREFIX}/bin/clang-tidy
                "${CMAKE_CURRENT_SOURCE_DIR}/test/run_test.sh"
                "$<TARGET_FILE:EinsumsTidy>"
                "${CMAKE_CURRENT_SOURCE_DIR}/test/${_fixture}"
                "${_expected}"
        )
    endforeach()
endif()
