# EinsumsClangd

Einsums-specific C++ tooling that augments [clang-tidy] and [clangd] with
**16 semantic checks** that understand the [Einsums] tensor-algebra library:
`Tensor<T, N>` types, `cg::einsum` spec strings, `CaptureGuard` scopes,
Catch2 test fixtures. The same check sources ship in two complementary
deliveries.

[clang-tidy]: https://clang.llvm.org/extra/clang-tidy/
[clangd]: https://clangd.llvm.org/
[Einsums]: https://github.com/Einsums/Einsums

## Layout

```
.
├── clang-tidy/    # Standalone EinsumsTidy.dylib plugin (loaded via clang-tidy --load=)
├── clangd-build/  # Custom clangd binary with the same checks statically linked in
└── LICENSE.txt
```

The two subdirs are **independent CMake projects** because LLVM's CMakeLists
forces `CMAKE_CXX_STANDARD=17` at directory scope inside `clangd-build/`,
which would clash with anything wanting a newer dialect at the same configure
pass. Configure each separately.

| Subdir | What it produces | Build cost | Used by |
|---|---|---|---|
| `clang-tidy/` | `EinsumsTidy.dylib` (.so/.dll) loaded with `clang-tidy --load=` | Seconds | CLI lints, CI runs |
| `clangd-build/` | Drop-in `clangd` binary with checks statically linked | 30–60 min cold (vendors LLVM) | Editor LSP (Einsums Studio auto-detects) |

Both register the same `einsums-*` check names; only the deployment differs.
Per-subdir READMEs cover the details:

- [`clang-tidy/README.md`](clang-tidy/README.md) — full check inventory,
  per-check rationale, fixtures, instructions for adding new checks
- [`clangd-build/README.md`](clangd-build/README.md) — LLVM vendor pinning,
  resource-dir symlink rationale, distribution / code-signing notes

## Downloads

Pre-built `clangd` binaries with EinsumsTidy checks embedded are published as
GitHub Releases. The asset filenames are **stable across releases** so the
`latest` redirect always works:

| Platform | URL |
|---|---|
| macOS, Apple Silicon | `https://github.com/Einsums/EinsumsClangd/releases/latest/download/einsums-clangd-darwin-arm64.tar.gz` |
| macOS, Intel | `https://github.com/Einsums/EinsumsClangd/releases/latest/download/einsums-clangd-darwin-x86_64.tar.gz` |
| Linux, x86_64 (glibc ≥ 2.35) | `https://github.com/Einsums/EinsumsClangd/releases/latest/download/einsums-clangd-linux-x86_64.tar.gz` |

A SHA-256 file is published alongside each tarball
(`...tar.gz.sha256`). Each tarball contains:

```
einsums-clangd/
├── bin/clangd
├── lib/clang/<major>/include/...
├── LICENSE.txt
└── VERSION                # release tag, LLVM tag, target, build timestamp
```

To install manually:

```bash
curl -fL -o einsums-clangd.tar.gz \
    https://github.com/Einsums/EinsumsClangd/releases/latest/download/einsums-clangd-darwin-arm64.tar.gz
tar -xzf einsums-clangd.tar.gz
./einsums-clangd/bin/clangd --version
```

Point your editor / LSP client at `<extracted-path>/bin/clangd`. The
sibling `lib/clang/<major>/` is auto-discovered relative to the binary
(via `bin/../lib/clang/<major>`), so don't move them apart.

### Compiler version compatibility

The bundled clangd's frontend must be **≥ your compiler's frontend**.
Newer clangd parses older libstdc++/libc++ headers fine; the reverse can
break. The `VERSION` file inside the tarball lists the embedded LLVM tag
(currently `llvmorg-22.1.4`). Match scenarios:

- `einsums-dev` conda env (LLVM 22) → exact match, ideal
- macOS Apple Clang (Xcode 16.x ≈ LLVM 19) → newer parser, fine
- Ubuntu system GCC (libstdc++ 11–13) → fine
- Bleeding-edge GCC/Clang ahead of our pin → may fail; build from source
  or request a workflow_dispatch build (see *Custom builds* below)

### Custom builds

If you need a different LLVM frontend version, trigger the workflow
manually with a custom `llvm_tag`:

1. Fork or clone EinsumsClangd
2. Actions → "Build and release einsums-clangd" → "Run workflow"
3. Set `llvm_tag` to e.g. `llvmorg-19.1.7` and run

Output is uploaded as workflow artifacts (not a release). Or build
locally — see the `clangd-build/` quickstart below.

### Studio integration

[Einsums Studio]'s toolchain auto-detects a `bin/clangd` reachable on
PATH or in `<project>/.einsums-studio/clangd/`. The expected URL for
Studio's auto-download is the stable name above; the `VERSION` file
gives Studio enough to warn the user if their compiler outpaces the
bundled frontend.

## Quickstart

### Build the standalone plugin (fast path)

```bash
cmake -S clang-tidy -B build-einsums-tidy -GNinja
cmake --build build-einsums-tidy
ctest --test-dir build-einsums-tidy   # runs the fixture suite
```

Requires Clang development headers (`find_package(Clang)` / `find_package(LLVM)`).
The conda `einsums-dev` env ships these. Output: `build-einsums-tidy/EinsumsTidy.dylib`.

Use it from any clang-tidy invocation:

```bash
clang-tidy --load=build-einsums-tidy/EinsumsTidy.dylib --checks='einsums-*' \
    -p path/to/compile_commands.json some_file.cpp
```

### Build the embedded clangd (slow path, full LSP integration)

```bash
cmake -S clangd-build -B build-einsums-clangd -GNinja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-einsums-clangd --target clangd
ctest --test-dir build-einsums-clangd -R einsums_clangd
```

First build: 30–60 min cold (one-time ~3 GB LLVM clone via `FetchContent`).
Output: `build-einsums-clangd/bin/clangd` — drop-in replacement for stock
clangd. [Einsums Studio]'s LSP path auto-detects this binary if a built
copy is reachable.

[Einsums Studio]: https://github.com/Einsums/Einsums

### Enable the checks in a project

Drop a `.clangd` file at the project root. The repo's own `.clangd` is a
working template:

```yaml
Diagnostics:
  ClangTidy:
    Add: 'einsums-*'
    FastCheckFilter: None
```

`FastCheckFilter: None` is required for clangd 19+ — without it clangd
silently skips any tidy check it doesn't recognize as part of its built-in
allowlist.

## Contributing

Adding a new check is mostly mechanical — see `clang-tidy/README.md` for
the recipe. New checks need to be added to `EINSUMS_TIDY_CHECK_SOURCES_RELATIVE`
in `clang-tidy/CMakeLists.txt` (the standalone plugin) **and** to
`EINSUMS_TIDY_CHECK_SOURCES` in `clangd-build/CMakeLists.txt` (the embedded
clangd). The two lists are intentionally manually-synced rather than
generated, so adding a check is a single PR that touches both.

Fixture conventions: every check has a corresponding `clang-tidy/test/<check>.cpp`
fixture with `// EXPECT:` comments marking expected diagnostic message
substrings. The fixture is run by both:

- `clang-tidy/test/run_test.sh` — drives the standalone `.dylib` against
  the system `clang-tidy`.
- `clangd-build/test/lsp_smoke.py` — drives the freshly-built embedded
  clangd over LSP, asserting end-to-end diagnostic delivery.

## License

MIT. See [`LICENSE.txt`](LICENSE.txt).
