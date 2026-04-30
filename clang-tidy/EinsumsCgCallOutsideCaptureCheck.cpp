//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

#include "EinsumsCgCallOutsideCaptureCheck.hpp"

#include <clang/AST/ASTContext.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <string>

#include "EinsumsTidyMatchers.hpp"
#include "EinsumsTidyTypeUtils.hpp"

namespace einsums::tidy {

using namespace ::clang;
using namespace ::clang::ast_matchers;

namespace {

/// True when the call's resolved function lives inside the
/// ``compute_graph`` namespace. We can't filter at the matcher layer
/// without rewriting the shared matchers — so we recheck here. The
/// existing matchers are name-based (``einsum`` / ``permute`` in any
/// namespace) and we add the namespace test in the body.
bool callee_in_compute_graph(CallExpr const *call) {
    auto const *fd = call->getDirectCallee();
    if (fd == nullptr)
        return false;
    auto const qualified = fd->getQualifiedNameAsString();
    return qualified.find("compute_graph::") != std::string::npos;
}

} // namespace

void EinsumsCgCallOutsideCaptureCheck::registerMatchers(MatchFinder *finder) {
    finder->addMatcher(einsum_call_with_literal_spec(), this);
    finder->addMatcher(permute_call_with_literal_spec(), this);
}

void EinsumsCgCallOutsideCaptureCheck::check(MatchFinder::MatchResult const &result) {
    auto const *call = result.Nodes.getNodeAs<CallExpr>("call");
    if (call == nullptr)
        return;
    if (!callee_in_compute_graph(call))
        return; // ``tensor_algebra::einsum`` is the eager path — different check
    if (is_inside_capture_guard_scope(call, *result.Context))
        return;

    diag(call->getBeginLoc(), "compute_graph call has no ``CaptureGuard`` in any enclosing scope — "
                              "without an active guard the call runs eagerly (or fails), defeating the "
                              "purpose of using cg::* in the first place. Either declare a "
                              "``cg::CaptureGuard`` first or switch to ``tensor_algebra::einsum`` "
                              "for the eager path.");
}

} // namespace einsums::tidy
