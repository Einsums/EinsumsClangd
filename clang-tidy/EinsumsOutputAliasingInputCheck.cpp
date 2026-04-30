//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

#include "EinsumsOutputAliasingInputCheck.hpp"

#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/Expr.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>

#include "EinsumsTidyMatchers.hpp"
#include "EinsumsTidyTypeUtils.hpp"

namespace einsums::tidy {

using namespace ::clang;
using namespace ::clang::ast_matchers;

namespace {

/// Identify the ``(output_arg_index, input_arg_indices…)`` mapping from the
/// einsum / permute call's arity. The two documented overloads differ in
/// whether prefactors are present:
///   - ``einsum(spec, &C, A, B)``                    arity 4 → out=1, in={2,3}
///   - ``einsum(spec, c_pf, &C, ab_pf, A, B)``       arity 6 → out=2, in={4,5}
///   - ``permute(spec, &C, A)``                      arity 3 → out=1, in={2}
///   - ``permute(spec, beta, &C, alpha, A)``         arity 5 → out=2, in={4}
struct ArgLayout {
    int              output;
    std::vector<int> inputs;
};

std::optional<ArgLayout> layout_for(CallExpr const *call) {
    auto const n = call->getNumArgs();
    switch (n) {
    case 4:
        return ArgLayout{1, {2, 3}};
    case 6:
        return ArgLayout{2, {4, 5}};
    case 3:
        return ArgLayout{1, {2}};
    case 5:
        return ArgLayout{2, {4}};
    default:
        return std::nullopt;
    }
}

} // namespace

void EinsumsOutputAliasingInputCheck::registerMatchers(MatchFinder *finder) {
    finder->addMatcher(einsum_call_with_literal_spec(), this);
    finder->addMatcher(permute_call_with_literal_spec(), this);
}

void EinsumsOutputAliasingInputCheck::check(MatchFinder::MatchResult const &result) {
    auto const *call = result.Nodes.getNodeAs<CallExpr>("call");
    if (call == nullptr)
        return;
    auto layout = layout_for(call);
    if (!layout)
        return;

    auto const *out_ref = peel_to_declref(call->getArg(layout->output));
    if (out_ref == nullptr)
        return;
    auto const *out_decl = out_ref->getDecl();
    if (out_decl == nullptr)
        return;

    for (int idx : layout->inputs) {
        auto const *in_ref = peel_to_declref(call->getArg(idx));
        if (in_ref == nullptr)
            continue;
        if (in_ref->getDecl() != out_decl)
            continue;
        diag(call->getBeginLoc(), "output tensor %0 is also passed as an input — aliasing across the "
                                  "output and input arguments is supported but easy to write by accident. "
                                  "If the in-place update is intentional, silence with NOLINT; otherwise "
                                  "use a separate output tensor.")
            << out_decl;
        return; // one diagnostic per call is enough
    }
}

} // namespace einsums::tidy
