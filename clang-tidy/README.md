# EinsumsTidy

Einsums-specific clang-tidy checks. **16 checks** that understand the
library's semantic model — `Tensor<T, N>` types, `cg::einsum` spec
strings, `CaptureGuard` scopes, Catch2 test fixtures — and surface bugs
or anti-patterns that no off-the-shelf clang-tidy module knows about.

## Two delivery paths

The same check sources ship in two ways:

1. **Standalone plugin** — built into `EinsumsTidy.dylib` and loaded by
   the system `clang-tidy` via `--load=`. Used for command-line linting,
   CI runs, and headless batch-style invocations. Built from this
   directory (`clang-tidy/` of the EinsumsClangd repo).

2. **Embedded in a custom clangd** — same check sources statically
   linked into a project-private clangd binary. Used by Einsums Studio's
   editor diagnostics over LSP. Built from `../clangd-build/` — see the
   sibling README there for build / install / distribution details.

Both paths register the same `einsums-*` check names. A `.clangd` file
at the repository root enables them; consumers don't need to do
anything per-file.

## Check inventory

Grouped by complexity tier. Click through to the source for full
implementation details — every check has a docstring on its class
declaration explaining intent, examples, and known limitations.

### Tier 1 — string-spec analysis (cheap, no AST type info)

| Check | Detects |
| --- | --- |
| [`einsums-redundant-permute`](EinsumsRedundantPermuteCheck.hpp) | identity permute (`"ij <- ij"`) — allocates and copies for nothing |
| [`einsums-output-not-in-inputs`](EinsumsOutputNotInInputsCheck.hpp) | output index missing from both A and B (typo / missing operand) |
| [`einsums-no-link-index`](EinsumsNoLinkIndexCheck.hpp) | A and B share no contracted index — silent outer product |
| [`einsums-duplicate-output-index`](EinsumsDuplicateOutputIndexCheck.hpp) | same index twice in C — semantically ambiguous |
| [`einsums-mixed-index-mode`](EinsumsMixedIndexModeCheck.hpp) | comma and single-char modes mixed in one spec |
| [`einsums-runtime-spec`](EinsumsRuntimeSpecCheck.hpp) | first arg isn't a string literal — skips `consteval` validation |

### Tier 2 — AST type-aware

| Check | Detects |
| --- | --- |
| [`einsums-tensor-by-value-param`](EinsumsTensorByValueParamCheck.hpp) | function parameter typed `Tensor<T, N>` — should be `const &` or `TensorView` |
| [`einsums-tensor-view-from-temporary`](EinsumsTensorViewFromTemporaryCheck.hpp) | `TensorView v = make_tensor()` — view dangles after the rvalue is destroyed |
| [`einsums-output-aliasing-input`](EinsumsOutputAliasingInputCheck.hpp) | `einsum(spec, &A, A, B)` — output is also one of the inputs |
| [`einsums-mismatched-value-type`](EinsumsMismatchedValueTypeCheck.hpp) | operands have different `T` in `Tensor<T, N>` — mixing `float`/`double` |
| [`einsums-rank-mismatch`](EinsumsRankMismatchCheck.hpp) | tensor argument's rank `N` doesn't match the spec's index count |

### Tier 3 — scope / context analysis

| Check | Detects |
| --- | --- |
| [`einsums-allocating-in-loop`](EinsumsAllocatingInLoopCheck.hpp) | owning tensor declared inside a loop body |
| [`einsums-cg-call-outside-capture`](EinsumsCgCallOutsideCaptureCheck.hpp) | `cg::einsum` / `cg::permute` with no `CaptureGuard` in any enclosing scope |
| [`einsums-prefer-cg-from-tensoralgebra`](EinsumsPreferCgFromTensorAlgebraCheck.hpp) | `tensor_algebra::einsum` *inside* a `CaptureGuard` (eager call wasted on graph capture) |

### Tier 5 — modernization & correctness

| Check | Detects |
| --- | --- |
| [`einsums-deprecated-api`](EinsumsDeprecatedApiCheck.hpp) | calls to `[[deprecated]]`-attributed `einsums::` symbols, surfaced through the einsums-* group |
| [`einsums-catch2-tensor-fixture-leak`](EinsumsCatch2TensorFixtureLeakCheck.hpp) | tensor declared in a `TEST_CASE` / `EINSUMS_TEST_CASE` body never reaches a Catch2 assertion macro |

## Per-check rationale and examples

### `einsums-redundant-permute`

`cg::permute("ij <- ij", &C, A)` runs the entire permute machinery to
produce `C == A`. Allocates a buffer for `C`, walks `A`'s indices,
writes them in identical order. Pure waste.

```cpp
cg::permute("ij <- ij", &A_T, A);    // EXPECT: redundant
cg::permute("ji <- ij", &A_T, A);    // OK — actual transpose
```

Fix: remove the call entirely. If the user wanted a separate buffer,
use `Tensor` copy construction.

### `einsums-output-not-in-inputs`

Every output index must appear in at least one input. An index in C
that doesn't appear in A or B is a typo or missing operand.

```cpp
cg::einsum("ij <- ik;kl", &C, A, B);   // EXPECT: 'j' not in inputs
cg::einsum("ij <- ik;kj", &C, A, B);   // OK
```

### `einsums-no-link-index`

Einsum needs at least one index shared between A and B to have
something to contract over. Without it, the call is a silent outer
product — almost always a typo.

```cpp
cg::einsum("ij <- i;j", &C, A, B);     // EXPECT: silent outer product
cg::einsum("ij <- ik;kj", &C, A, B);   // OK — 'k' is the link
```

If the outer product is intentional, leave a `// NOLINT(einsums-no-link-index)`
comment with a note explaining why.

### `einsums-duplicate-output-index`

`einsum(C, A, B)` has no clean meaning for "same index twice in C." A
self-trace belongs on the input side or in a separate `trace` call.

```cpp
cg::einsum("ii <- ij;jk", &C, A, B);   // EXPECT: duplicate in C
cg::einsum("ij <- ik;kj", &C, A, B);   // OK
```

### `einsums-mixed-index-mode`

The parser auto-detects mode from the presence of *any* comma. Mix
both styles in one spec and operands like `ij` get parsed as one
multi-char index named `"ij"` — almost never the user's intent.

```cpp
cg::einsum("ij,k <- ik;kj", &C, A, B);     // EXPECT: mixed mode
cg::einsum("i,j <- i,k;k,j", &C, A, B);    // OK
cg::einsum("ij <- ik;kj", &C, A, B);       // OK
```

### `einsums-runtime-spec`

A literal first argument goes through `EinsumFormatString`'s
`consteval` constructor and validates the spec at compile time. A
non-literal (variable, function call, computed string) defers
validation to runtime — typos surface only on the first call.

```cpp
std::string s = "ij <- ik;kj";
cg::einsum(s, &C, A, B);                   // EXPECT: not a literal
cg::einsum("ij <- ik;kj", &C, A, B);       // OK
```

When the spec is genuinely computed (Python bindings, codegen),
silence with `// NOLINT`.

### `einsums-tensor-by-value-param`

Owning tensor types passed by value copy the entire underlying buffer
on every call. `TensorView` is exempt — it's the explicit non-owning
handle.

```cpp
void heavy(Tensor<double, 2> A);              // EXPECT: by value
void readonly(Tensor<double, 2> const &A);    // OK
void view_arg(TensorView<double, 2> A);       // OK (non-owning)
```

### `einsums-tensor-view-from-temporary`

A `TensorView` stores a pointer into the tensor's buffer. If that
buffer comes from a temporary (a function call that returns by value,
an inline `Tensor{...}`), the buffer is freed at end-of-statement and
the view dangles.

```cpp
TensorView<double, 2> v = make_tensor();   // EXPECT: dangling view
Tensor<double, 2>     t = make_tensor();
TensorView<double, 2> v = t;               // OK — t outlives v
```

### `einsums-output-aliasing-input`

In-place updates (`einsum(spec, &A, A, B)`) are valid but easy to write
by accident. Aliasing also confuses the BLAS dispatch about which
buffer can be safely written.

```cpp
cg::einsum("ij <- ik;kj", &A, A, B);       // EXPECT: A is also input
cg::einsum("ij <- ik;kj", &C, A, B);       // OK
```

If the in-place is intentional, silence with `// NOLINT`.

### `einsums-mismatched-value-type`

All tensor operands in a contraction must have the same value type.
Mixing precisions either fails to compile or silently up/down-converts
at every BLAS dispatch.

```cpp
cg::einsum("ij <- ik;kj", &C_double, A_float, B_double); // EXPECT
cg::einsum("ij <- ik;kj", &C_double, A_double, B_double);// OK
```

### `einsums-rank-mismatch`

Spec's index count for an operand must equal the tensor's `N`. The
spec is the source of truth at the call site; if `Tensor<T, 3>` is
bound to a 2-index spec position, the call is broken.

```cpp
Tensor<double, 3> A;
cg::einsum("ij <- ik;kj", &C, A, B);       // EXPECT: A is rank 3, spec wants 2
```

Skips template-dependent declarations to avoid false positives in
generic code; checks each instantiation as it appears.

### `einsums-allocating-in-loop`

Owning tensors declared inside `for` / `while` / `do` / range-`for`
bodies allocate a fresh buffer every iteration. Hoist out and `.zero()`
at the top of each iteration instead. `TensorView` is exempt.

```cpp
for (int i = 0; i < N; ++i) {
    Tensor<double, 2> tmp({M, K});         // EXPECT: allocating
    // ...
}

Tensor<double, 2> tmp({M, K});
for (int i = 0; i < N; ++i) {
    tmp.zero();
    // ...                                  // OK
}
```

### `einsums-cg-call-outside-capture`

`cg::einsum` / `cg::permute` only do useful work when a `CaptureGuard`
is active. Without one they execute eagerly (or fail), defeating the
point of `cg::*`.

```cpp
void wrong() {
    cg::einsum("ij <- ik;kj", &C, A, B);   // EXPECT: no CaptureGuard
}

void right() {
    cg::CaptureGuard g(graph);
    cg::einsum("ij <- ik;kj", &C, A, B);   // OK
}
```

Walks lexical ancestors only — guards held in callable objects or
captured closures aren't tracked.

### `einsums-prefer-cg-from-tensoralgebra`

Mirror of the above: `tensor_algebra::einsum` (the eager API) inside a
`CaptureGuard` scope runs immediately and doesn't become part of the
captured graph. The user almost certainly wanted `cg::einsum`.

```cpp
{
    cg::CaptureGuard g(graph);
    tensor_algebra::einsum(...);           // EXPECT: prefer cg::einsum
}
```

### `einsums-deprecated-api`

Re-routes Clang's `[[deprecated]]` warnings on `einsums::*` symbols
through the `einsums-*` check group, so they show up alongside the
rest of the project-specific diagnostics rather than in the generic
`-Wdeprecated-declarations` stream. Handles function calls and member
access. Walks template-instantiation alternates so attribute on the
primary template still triggers.

```cpp
namespace einsums {
[[deprecated("use new_compute()")]] void old_compute();
}
einsums::old_compute();                    // EXPECT: deprecated
```

System / stdlib deprecations stay on the standard Clang path.

### `einsums-catch2-tensor-fixture-leak`

Tensors constructed in a `TEST_CASE` / `EINSUMS_TEST_CASE` /
`TEMPLATE_TEST_CASE` / `SCENARIO` body but never reaching any Catch2
assertion macro (`REQUIRE`, `CHECK`, `INFO`, `CAPTURE`, `WARN`,
`FAIL`, `SECTION`, `GIVEN`/`WHEN`/`THEN`, etc.) — a sign of a
broken-but-passing test. Generous on what counts as "use" — even
`CAPTURE(x)` for diagnostic logging suffices.

```cpp
TEST_CASE("contraction") {
    Tensor<double, 2> C({3, 3});
    cg::einsum(...);
    // EXPECT: C is never asserted
}

TEST_CASE("contraction-fixed") {
    Tensor<double, 2> C({3, 3});
    cg::einsum(...);
    REQUIRE(close_to(C, expected));        // OK
}
```

## Configuration

A project-root `.clangd` enables every `einsums-*` check. The
`FastCheckFilter: None` line is **required** — clangd 19+ silently
skips checks that aren't on its built-in fast-path allowlist, no
matter what the user's `.clang-tidy` says.

```yaml
Diagnostics:
  ClangTidy:
    Add: 'einsums-*'
    FastCheckFilter: None
```

A separate `.clang-tidy` controls per-check enable/disable for
non-clangd invocations:

```yaml
Checks: '-*,einsums-*'
```

To silence a specific check on a specific line:

```cpp
cg::einsum("ij <- ik;kj", &A, A, B);  // NOLINT(einsums-output-aliasing-input)
```

To silence the next line:

```cpp
// NOLINTNEXTLINE(einsums-allocating-in-loop)
Tensor<double, 2> tmp({M, K});
```

## Build (standalone plugin)

The conda `einsums-dev` env ships every dependency. From a fresh
checkout:

```bash
cmake -S . -B build -GNinja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DEINSUMS_BUILD_TIDY_MODULE=ON
cmake --build build --target EinsumsTidy
```

The plugin lands at:

* macOS: `build/lib/EinsumsTidy.dylib`
* Linux: `build/lib/EinsumsTidy.so`

## Use from the command line

```bash
clang-tidy \
    --load=build/lib/EinsumsTidy.dylib \
    --checks='-*,einsums-*' \
    path/to/file.cpp -- -std=c++23
```

For a project with a compile_commands.json:

```bash
clang-tidy --load=build/lib/EinsumsTidy.dylib path/to/file.cpp -p build
```

## Tests

Each check has a `test/<name>.cpp` fixture with `// EXPECT: …` comments
on lines that should produce a diagnostic. The runner under `test/`
invokes `clang-tidy --load=...` and verifies every tagged line emits a
matching warning.

```bash
ctest --test-dir build -R einsums_tidy
# 16 tests, ~1 s total
```

Add a fixture for any new check before adding the ctest entry — the
runner is small enough that catching false positives is largely the
fixture's job.

## Adding a check

1. **Skeleton** — copy an existing `EinsumsXxxCheck.{hpp,cpp}` pair as
   a template. The header is small (class declaration + docstring);
   the .cpp wires up `registerMatchers` and `check`.

2. **Reuse helpers** — three shared headers cover most needs:
   - `EinsumsTidyMatchers.hpp` — pre-built AST matchers for
     einsum/permute callsites with literal-spec or non-literal-spec
     first arguments.
   - `EinsumSpecParser.hpp` — header-only parser for ComputeGraph
     einsum/permute spec strings (handles arrow notation, NumPy
     notation, single-char and multi-char modes).
   - `EinsumsTidyTypeUtils.hpp` — type introspection
     (`is_tensor_like`, `is_tensor_view`, `tensor_value_type`,
     `tensor_rank`) and the `is_inside_capture_guard_scope` walker.

3. **Register** — add a `factories.registerCheck<...>("einsums-...")`
   line in `EinsumsTidyModule::addCheckFactories`.

4. **Source list (×2)** — add the new `.cpp` to BOTH:
   - `clang-tidy/CMakeLists.txt` → `EINSUMS_TIDY_CHECK_SOURCES_RELATIVE`
   - `clangd-build/CMakeLists.txt` → `EINSUMS_TIDY_CHECK_SOURCES`

   The custom clangd build maintains a separate hardcoded source list
   because it's a standalone CMake project that can't read the
   plugin's CACHE variables.

5. **Fixture** — drop `test/<check-name>.cpp` with `// EXPECT: <substring>`
   on each line that should trigger.

6. **ctest entry** — add to the `_tidy_cases` list in
   `clang-tidy/CMakeLists.txt` (format:
   `"name|fixture.cpp|expected-substring"`).

7. **Document** — add a row to the inventory table above plus a
   per-check section in this README (rationale, example, fix). The
   class docstring in the header is the canonical source; the README
   summarizes for browsing.

## Architecture notes

### Why a plugin and a custom clangd both ship

Stock clangd doesn't load custom tidy modules at runtime
(`--load=` is a `clang-tidy` flag, not a clangd flag). The two paths
exist for different consumers:

- **Plugin** — fast iteration during development (a check change is a
  ~5 second rebuild of the .dylib). CI / script use.
- **Custom clangd** — editor-time diagnostics with no per-save
  subprocess. ~2-minute relink after a check change.

Both bake the same `EinsumsTidyModule` registration; the plugin
loads it via `dlopen` at clang-tidy startup, the custom clangd has
it statically linked.

### Why we don't link against the ComputeGraph library

The plugin runs inside the host process (`clang-tidy` or `clangd`) —
pulling all of Einsums into LLVM's address space would balloon the
binary and create header-include conflicts. The spec parser and type
helpers are re-implemented locally (~150 lines of code shared across
checks). When the format evolves, both copies need updating.

### Diagnostic placement

Each check emits at the most user-actionable location:

- Spec-content checks (output-not-in-inputs, no-link-index, etc.) →
  the call's begin loc, so the squiggle covers the offending function
  name.
- Type-content checks (rank-mismatch, mismatched-value-type) → the
  argument's begin loc, so the user sees which operand is wrong.
- Scope checks (cg-call-outside-capture, allocating-in-loop) → the
  callsite or var declaration as appropriate.

Notes (`diag(... DiagnosticIDs::Note)`) attach additional context
where useful (e.g. pointing at the spec literal's source range from
a call-site warning).
