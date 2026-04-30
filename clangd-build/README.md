# einsums-clangd

A custom **clangd** binary with the EinsumsTidy checks statically linked
in. This is the path Einsums Studio's editor uses for `einsums-*`
diagnostics over LSP — stock clangd doesn't support runtime loading of
clang-tidy modules (`--load=` is a `clang-tidy`-only flag), so we vendor
LLVM source and link our checks into clangd's `tool/` link line.

The check sources live in `../clang-tidy/`. This directory is purely
build infrastructure: it wires LLVM's CMakeLists into a FetchContent
build and injects our `.cpp` files into clangd's executable target. Same
`einsums-*` check names, same fixtures, same diagnostics — the only
difference from the standalone `EinsumsTidy.dylib` plugin is the
deployment.

## Standalone CMake project

Unlike the rest of Einsums Studio, this directory does NOT participate
in the parent build. It's a separate `project()` invocation because
LLVM's CMakeLists forces `CMAKE_CXX_STANDARD=17` at directory scope,
which would clash with Studio's C++23 settings if pulled into the same
configure pass.

You configure and build it as its own thing:

```bash
cmake -S clangd-build -B build-einsums-clangd \
    -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-einsums-clangd --target clangd
```

That produces `build-einsums-clangd/bin/clangd`. Studio's
`ProjectContext` auto-detects this binary on project open and uses it
in preference to a stock clangd from PATH.

## What the build does

1. **Fetches LLVM source** via `FetchContent` at a pinned tag
   (`EINSUMS_LLVM_GIT_TAG`, currently `llvmorg-22.1.4` to match the
   conda env's clang). Shallow clone, ~3 GB on disk.

2. **Configures LLVM** with `LLVM_ENABLE_PROJECTS=clang;clang-tools-extra`
   and a curated set of `OFF` flags (no docs, no tests, no benchmarks,
   no static analyzer, no ARC migration tool) to keep the build small.
   `CLANG_BUILD_TOOLS=ON` stays — turning it off skips the
   `clang-resource-headers` target, which leaves `<__stddef.h>` and
   friends missing and every parse fails with "unknown type name
   '__int64_t'".

3. **Injects our checks** via `target_sources(clangd PRIVATE ...)` after
   `FetchContent_MakeAvailable` has created the `clangd` target. Each
   of the `EinsumsXxxCheck.cpp` files compiles into clangd's `tool/`
   directory; the static initializer in `EinsumsTidyModule.cpp`
   registers the module with clang-tidy's `ModuleRegistry` at
   clangd startup.

4. **Stages outputs** at predictable paths:
   - The clangd binary at `build-einsums-clangd/bin/clangd`
     (via `RUNTIME_OUTPUT_DIRECTORY`).
   - A symlink `build-einsums-clangd/lib/clang/<ver>` → the resource
     headers under `_deps/llvm-project-build/lib/clang/<ver>` so
     clangd's runtime resource-dir lookup succeeds.

## Build cost

- **First build** (cold): 30–60 minutes on a recent Mac/Linux box,
  depending on parallelism. Mostly LLVM core + Clang frontend
  compilation.
- **Incremental rebuild after editing a check** (`.cpp` in
  `../clang-tidy/`): ~minutes — recompiles the changed file plus
  relinks clangd. The relink is the long step (clangd is ~150 MB of
  static libraries).
- **Reconfigure**: ~10 seconds. CMake doesn't re-fetch LLVM; only the
  source list and target wiring changes propagate.

## Running on a file

The same flags work as for stock clangd:

```bash
build-einsums-clangd/bin/clangd \
    --check=path/to/file.cpp \
    --enable-config \
    -p build
```

`--check` is a one-shot mode that returns a non-zero exit code on any
errors. For diagnostics-as-warnings (which is what the `einsums-*`
checks are), use the LSP path — see the `test/lsp_smoke.py` runner for
a minimal example of driving clangd over stdin/stdout JSON-RPC.

## Tests

```bash
ctest --test-dir build-einsums-clangd -R einsums_clangd
```

The `einsums_clangd_lsp_redundant_permute` ctest entry uses the python
LSP smoke runner (`test/lsp_smoke.py`) to drive the freshly-built
clangd against the canonical `redundant-permute.cpp` fixture. It
catches the entire pipeline: registry registration into clangd's static
link, `.clangd` config parse, `FastCheckFilter: None`, and the
`Add: einsums-*` line.

The full inventory of check fixtures lives in `../clang-tidy/test/`
and runs through the standalone `clang-tidy --load=` path; we don't
duplicate every fixture here since the per-check correctness is
already covered there. This ctest verifies the **embedded** path is
plumbed correctly end-to-end.

## Bumping LLVM

Pinned via `EINSUMS_LLVM_GIT_TAG`. Bump it when:

- The conda env's clang version moves (so the embedded clangd's
  parser stays compatible with the libc++ headers conda ships).
- A new C++ standard feature lands that Einsums starts using and the
  current LLVM doesn't fully support.
- A real clangd bug gets fixed upstream and you want it in your
  editor.

After bumping:

1. Edit `EINSUMS_LLVM_GIT_TAG` at the top of `CMakeLists.txt`.
2. Wipe the build dir (CMake won't reliably re-fetch otherwise):
   `rm -rf build-einsums-clangd`.
3. Reconfigure and rebuild — full LLVM clone + 30–60 min build.
4. Re-run `ctest -R einsums_clangd` to confirm nothing broke.

The check sources are version-portable — they target Clang's stable
AST API (`AST/*`, `ASTMatchers/*`, `Lex/*`) and don't reach into clangd
internals. Bumping LLVM has historically been a no-op at our layer.

## Distribution (for shipping with Einsums Studio)

Currently outside the scope of this build script — distributing
clangd in a Studio bundle requires:

- **Bundling**: copy `bin/clangd` and the `lib/clang/<ver>/include/`
  resource headers into Studio's app bundle in the layout clangd
  expects (`<bin>/../lib/clang/<ver>`).
- **Code signing** (macOS) with the same Developer ID that signs the
  rest of Studio. The hardened runtime needs an entitlement allowing
  `dlopen` of project libraries.
- **Universal binaries**: separate arm64 and x86_64 builds combined
  with `lipo`. Each arch is its own ~30-60 minute build.

When that work is undertaken, it lives in Studio's `app/` bundle
script, not here. This directory's job is to produce a working clangd
binary; downstream packaging is the bundler's job.

## Architecture pointers

For implementation details:

- `../clang-tidy/README.md` — full check inventory, per-check
  rationale, configuration via `.clangd`, instructions for adding new
  checks.
- `../clang-tidy/EinsumsTidyModule.cpp` — module registration; the
  static initializer here is what makes the `einsums-*` checks
  visible to whichever clang-tidy host (this clangd or standalone)
  loaded the binary.
- The shared header trio (`EinsumsTidyMatchers.hpp`,
  `EinsumSpecParser.hpp`, `EinsumsTidyTypeUtils.hpp`) — most of the
  reusable infrastructure lives there; check sources are mostly
  thin wrappers that compose those helpers.
