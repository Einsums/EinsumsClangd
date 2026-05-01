#----------------------------------------------------------------------------------------------
# Copyright (c) The Einsums Developers. All rights reserved.
# Licensed under the MIT License. See LICENSE.txt in the project root for license information.
#----------------------------------------------------------------------------------------------

# einsums-clangd — a custom clangd binary with the EinsumsTidy checks
# statically linked in.
#
# Standalone CMake project. Cannot be pulled into a larger build via
# ``add_subdirectory`` because LLVM's CMakeLists forces
# ``CMAKE_CXX_STANDARD=17`` at directory scope, which would clash with any
# host project using a newer dialect. Configure / build separately:
#
#     cmake -S clangd-build -B build-einsums-clangd
#     cmake --build build-einsums-clangd --target clangd
#
# The resulting ``build-einsums-clangd/bin/clangd`` is a drop-in
# replacement for stock clangd. Studio's Toolchain auto-detect picks it
# up and points the LSP at it. The first build takes 30-60 minutes;
# incremental rebuilds after editing one of our checks are a relink
# (~30 s).
#
# Why we do this instead of loading our checks at runtime: stock clangd
# does not support ``--load=`` for tidy modules. Running our checks
# through the standalone ``clang-tidy`` subprocess works and the
# ``../clang-tidy/`` target still produces a plugin .dylib for that path,
# but it costs a per-save process spawn + fresh AST. Embedding the same
# checks into clangd lets them piggyback on clangd's already-cached AST.

cmake_minimum_required(VERSION 3.20)
project(einsums_clangd LANGUAGES C CXX)

# --- Locate the EinsumsTidy check sources ---------------------------------
# The check .cpp / .hpp files live in the sibling clang-tidy/ directory.
# We compile them directly here (no separate library); the static
# initializer they carry registers with clang-tidy's module registry at
# clangd startup, so they "just work" once linked into the binary.
set(_TIDY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../clang-tidy")
if(NOT EXISTS "${_TIDY_DIR}/EinsumsTidyModule.cpp")
    message(FATAL_ERROR "Cannot find EinsumsTidy sources at ${_TIDY_DIR}. "
        "Expected to find them in the sibling 'clang-tidy/' directory.")
endif()
set(EINSUMS_TIDY_CHECK_SOURCES
    "${_TIDY_DIR}/EinsumsTidyModule.cpp"
    "${_TIDY_DIR}/EinsumsRedundantPermuteCheck.cpp"
    "${_TIDY_DIR}/EinsumsOutputNotInInputsCheck.cpp"
    "${_TIDY_DIR}/EinsumsNoLinkIndexCheck.cpp"
    "${_TIDY_DIR}/EinsumsDuplicateOutputIndexCheck.cpp"
    "${_TIDY_DIR}/EinsumsMixedIndexModeCheck.cpp"
    "${_TIDY_DIR}/EinsumsRuntimeSpecCheck.cpp"
    "${_TIDY_DIR}/EinsumsTensorByValueParamCheck.cpp"
    "${_TIDY_DIR}/EinsumsTensorViewFromTemporaryCheck.cpp"
    "${_TIDY_DIR}/EinsumsOutputAliasingInputCheck.cpp"
    "${_TIDY_DIR}/EinsumsMismatchedValueTypeCheck.cpp"
    "${_TIDY_DIR}/EinsumsRankMismatchCheck.cpp"
    "${_TIDY_DIR}/EinsumsAllocatingInLoopCheck.cpp"
    "${_TIDY_DIR}/EinsumsCgCallOutsideCaptureCheck.cpp"
    "${_TIDY_DIR}/EinsumsPreferCgFromTensorAlgebraCheck.cpp"
    "${_TIDY_DIR}/EinsumsDeprecatedApiCheck.cpp"
    "${_TIDY_DIR}/EinsumsCatch2TensorFixtureLeakCheck.cpp"
)

# --- LLVM source acquisition ----------------------------------------------
# Pin to a specific tag rather than tracking ``main``: clangd's tidy
# integration is not stable API and a rolling pull would force a re-test
# on every LLVM commit. Pick the closest stable release to the conda env's
# clang-tidy version (the einsums-dev env currently ships LLVM 22, hence
# llvmorg-22.1.4 — bump in lockstep when the env's clang moves).
set(EINSUMS_LLVM_GIT_TAG "llvmorg-22.1.4" CACHE STRING
    "LLVM tag to vendor when building the custom clangd")

# Extract the major version from the pinned tag (``llvmorg-22.1.4`` -> ``22``).
# We can't rely on ``${LLVM_VERSION_MAJOR}`` here: LLVM sets it inside the
# FetchContent subdirectory scope, where it does NOT propagate back up to our
# scope. If we used it directly, the symlink target below would silently expand
# the path component to "" and create a self-referential symlink at lib/clang/.
if(NOT EINSUMS_LLVM_GIT_TAG MATCHES "^llvmorg-([0-9]+)")
    message(FATAL_ERROR
        "EINSUMS_LLVM_GIT_TAG=${EINSUMS_LLVM_GIT_TAG} does not match the "
        "expected 'llvmorg-<major>.<minor>.<patch>' format; can't derive the "
        "resource-dir version for the einsums_clangd_resource_dir_symlink target.")
endif()
set(_EINSUMS_LLVM_VERSION_MAJOR "${CMAKE_MATCH_1}")

Set(FETCHCONTENT_QUIET FALSE)

include(FetchContent)
FetchContent_Declare(llvm-project
    GIT_REPOSITORY https://github.com/llvm/llvm-project.git
    GIT_TAG ${EINSUMS_LLVM_GIT_TAG}
    GIT_SHALLOW ON
    GIT_PROGRESS TRUE
    SOURCE_SUBDIR llvm
)

# --- LLVM build options ---------------------------------------------------
# Turn off everything we don't need. With these flags, a clean clangd
# build is 30-60 minutes on a recent Mac and ~3-5 GB of disk.
set(LLVM_ENABLE_PROJECTS "clang;clang-tools-extra"
    CACHE STRING "" FORCE)
set(LLVM_ENABLE_RUNTIMES "" CACHE STRING "" FORCE)
set(LLVM_TARGETS_TO_BUILD "AArch64;X86" CACHE STRING "" FORCE)
set(LLVM_ENABLE_ASSERTIONS OFF CACHE BOOL "" FORCE)
set(LLVM_ENABLE_LIBXML2 OFF CACHE BOOL "" FORCE)
set(LLVM_ENABLE_ZLIB OFF CACHE BOOL "" FORCE)
set(LLVM_ENABLE_ZSTD OFF CACHE BOOL "" FORCE)
set(LLVM_ENABLE_TERMINFO OFF CACHE BOOL "" FORCE)
set(LLVM_INCLUDE_TESTS OFF CACHE BOOL "" FORCE)
set(LLVM_INCLUDE_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(LLVM_INCLUDE_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LLVM_INCLUDE_DOCS OFF CACHE BOOL "" FORCE)
set(CLANG_INCLUDE_TESTS OFF CACHE BOOL "" FORCE)
set(CLANG_INCLUDE_DOCS OFF CACHE BOOL "" FORCE)
set(CLANG_TOOLS_EXTRA_INCLUDE_DOCS OFF CACHE BOOL "" FORCE)
# NOTE: ``CLANG_BUILD_TOOLS=OFF`` would skip not only ``clang`` itself but
# also the ``clang-resource-headers`` target that populates
# ``<prefix>/lib/clang/<ver>/include/`` with ``__stddef.h``, ``__int_t.h``,
# and the rest of the compiler intrinsic headers libc++ pulls in. Without
# those, every parse trips on "unknown type name '__int64_t'" or
# "modules ODR violation". Leaving the option ON adds a few minutes to
# the build (clang, clang-format, …) but gives us a working resource-dir.
set(CLANG_ENABLE_ARCMT OFF CACHE BOOL "" FORCE)
set(CLANG_ENABLE_STATIC_ANALYZER OFF CACHE BOOL "" FORCE)
set(CLANG_ENABLE_OBJC_REWRITER OFF CACHE BOOL "" FORCE)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

message(STATUS "EinsumsClangd: vendoring LLVM ${EINSUMS_LLVM_GIT_TAG} (one-time ~3 GB clone)")
FetchContent_MakeAvailable(llvm-project)

# --- Inject our checks into clangd's link line ----------------------------
# After ``FetchContent_MakeAvailable`` runs LLVM's build system, the
# ``clangd`` target exists. ``target_sources`` adds our check .cpp files so
# they get compiled and linked into the binary. The static initializer in
# ``EinsumsTidyModule.cpp`` registers the module with clang-tidy's registry
# at clangd startup, the same way LLVM's in-tree modules do.
if(NOT TARGET clangd)
    message(FATAL_ERROR
        "EinsumsClangd: 'clangd' target not present after FetchContent_MakeAvailable. "
        "The LLVM tag ${EINSUMS_LLVM_GIT_TAG} may not include clang-tools-extra.")
endif()

target_sources(clangd PRIVATE ${EINSUMS_TIDY_CHECK_SOURCES})
target_include_directories(clangd PRIVATE "${_TIDY_DIR}")

# ``EinsumsTidyModule.cpp`` does ``#include <clang-tidy/ClangTidyModule.h>``,
# which on a developer's machine resolves via the conda env's
# ``-isystem $CONDA_PREFIX/include`` (the env ships installed clang-tidy
# headers under ``include/clang-tidy/``). On the GHA runner there's no conda
# env, so we have to point the compiler at the vendored source tree
# explicitly: clang-tools-extra/clang-tidy/ holds the in-tree headers, so
# adding clang-tools-extra/ to the include path makes the bracket include
# resolve to ``clang-tools-extra/clang-tidy/ClangTidyModule.h``.
target_include_directories(clangd PRIVATE
    "${llvm-project_SOURCE_DIR}/clang-tools-extra"
)

# Force the clang resource-headers (``__stddef.h``, ``__int_t.h``, …) to be
# built as a prereq of clangd. Without this target, ``<build>/lib/clang/<v>/``
# stays empty and clangd can't find ``__int64_t`` / friends when parsing
# libc++ headers — the symptom is "unknown type name '__int64_t'" /
# "module ODR violation" errors on every C++ file.
if(TARGET clang-resource-headers)
    add_dependencies(clangd clang-resource-headers)
endif()

# clangd resolves its resource-dir relative to its own binary
# (``<bindir>/../lib/clang/<ver>``). LLVM's CMake installs the
# resource-headers under ``_deps/llvm-project-build/lib/clang/<ver>``,
# which is the wrong layout for our nested FetchContent build — clangd
# at ``build-einsums-clangd/bin/clangd`` looks at
# ``build-einsums-clangd/lib/clang/<ver>``, but that path doesn't
# exist. Materialize a symlink at the expected location so clangd's
# header search picks them up without needing ``--resource-dir`` on
# every invocation.
# ``add_custom_command(TARGET ...)`` is restricted to the directory where
# the target was created, and ``clangd`` lives down inside the LLVM
# subdirectory. A standalone ``add_custom_target`` that depends on clangd
# achieves the same effect across the directory boundary: it runs after
# clangd is built and is part of the default build through ALL.
add_custom_target(einsums_clangd_resource_dir_symlink ALL
    DEPENDS clangd
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/lib/clang"
    # Remove any stale symlink/dir at the destination so re-runs are idempotent
    # (``cmake -E create_symlink`` fails if the destination already exists).
    COMMAND ${CMAKE_COMMAND} -E rm -rf
        "${CMAKE_BINARY_DIR}/lib/clang/${_EINSUMS_LLVM_VERSION_MAJOR}"
    COMMAND ${CMAKE_COMMAND} -E create_symlink
        "${CMAKE_BINARY_DIR}/_deps/llvm-project-build/lib/clang/${_EINSUMS_LLVM_VERSION_MAJOR}"
        "${CMAKE_BINARY_DIR}/lib/clang/${_EINSUMS_LLVM_VERSION_MAJOR}"
    COMMENT "Linking clang resource-dir → bindir/../lib/clang/${_EINSUMS_LLVM_VERSION_MAJOR}"
    VERBATIM
)

# Drop the binary at a predictable location so Studio's auto-detect can find
# it without configuration. ``set_target_properties`` *does* work across
# directory boundaries for setting properties on existing targets — only
# ``add_custom_command(TARGET ...)`` is restricted that way.
set_target_properties(clangd PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

message(STATUS "EinsumsClangd: clangd target augmented with checks from ${_TIDY_DIR}")

# --- Tests --------------------------------------------------------------
# End-to-end LSP smoke test: drive the freshly built clangd over stdin/stdout,
# open the EinsumsTidy fixture, and assert ``redundant permute`` shows up in
# at least one diagnostic. Catches the entire pipeline that the standalone
# ``einsums_tidy_redundant_permute`` ctest in the sibling clang-tidy/ subdir
# can't: registry registration into clangd's static link, ``.clangd`` config
# parse, ``FastCheckFilter: None``, the Add: ``einsums-*`` line, and the
# fixture's resource-dir resolution.
#
# The fixture lives in the sibling ``clang-tidy/test/`` so we don't have to
# duplicate it here. Test only registers when both the binary and the
# fixture are reachable — fresh checkouts where clangd hasn't been built yet
# silently skip the test rather than fail it.
enable_testing()
set(_FIXTURE "${CMAKE_CURRENT_SOURCE_DIR}/../clang-tidy/test/redundant-permute.cpp")
if(EXISTS "${_FIXTURE}")
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME einsums_clangd_lsp_redundant_permute
            COMMAND ${Python3_EXECUTABLE}
                "${CMAKE_CURRENT_SOURCE_DIR}/test/lsp_smoke.py"
                "--clangd" "$<TARGET_FILE:clangd>"
                "--file"   "${_FIXTURE}"
                "--expect" "redundant permute"
                "--timeout" "30"
        )
        # Make sure the resource-dir post-build step + its symlink are in
        # place before the test runs; without ``__stddef.h`` &c. clangd
        # bails out before tidy ever runs and the test would mis-diagnose.
        set_tests_properties(einsums_clangd_lsp_redundant_permute PROPERTIES
            DEPENDS clangd
        )
    endif()
endif()
