//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

#include "EinsumsPreferCgFromTensorAlgebraCheck.hpp"

#include <clang/AST/ASTContext.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <string>

#include "EinsumsTidyTypeUtils.hpp"

namespace einsums::tidy {

using namespace ::clang;
using namespace ::clang::ast_matchers;

namespace {

/// True when ``call``'s resolved function lives in the eager
/// ``tensor_algebra`` namespace (or its ``detail`` subnamespace, where
/// most concrete overloads actually live in the codebase). We don't gate
/// on the matcher because we need to admit BOTH ``einsum`` and
/// ``permute`` callsites with whatever signature they happen to have —
/// not just the literal-spec ones.
bool callee_in_tensor_algebra(CallExpr const *call) {
    auto const *fd = call->getDirectCallee();
    if (fd == nullptr)
        return false;
    auto const qualified = fd->getQualifiedNameAsString();
    return qualified.find("tensor_algebra::") != std::string::npos;
}

} // namespace

void EinsumsPreferCgFromTensorAlgebraCheck::registerMatchers(MatchFinder *finder) {
    // Match every ``einsum`` and ``permute`` call by name (any namespace);
    // namespace filtering happens in the body.
    finder->addMatcher(callExpr(callee(functionDecl(anyOf(hasName("einsum"), hasName("permute"))))).bind("call"), this);
}

void EinsumsPreferCgFromTensorAlgebraCheck::check(MatchFinder::MatchResult const &result) {
    auto const *call = result.Nodes.getNodeAs<CallExpr>("call");
    if (call == nullptr)
        return;
    if (!callee_in_tensor_algebra(call))
        return;
    if (!is_inside_capture_guard_scope(call, *result.Context))
        return;

    diag(call->getBeginLoc(), "eager ``tensor_algebra::*`` call inside a ``CaptureGuard`` scope — this "
                              "executes immediately and does not become part of the captured graph. "
                              "Use ``cg::einsum`` / ``cg::permute`` here so the operation participates "
                              "in the surrounding capture.");
}

} // namespace einsums::tidy
