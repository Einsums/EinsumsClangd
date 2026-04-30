//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

#include "EinsumsRuntimeSpecCheck.hpp"

#include <clang/AST/ASTContext.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>

#include "EinsumsTidyMatchers.hpp"

namespace einsums::tidy {

using namespace ::clang;
using namespace ::clang::ast_matchers;

void EinsumsRuntimeSpecCheck::registerMatchers(MatchFinder *finder) {
    finder->addMatcher(einsum_call_with_nonliteral_spec(), this);
    finder->addMatcher(permute_call_with_nonliteral_spec(), this);
}

void EinsumsRuntimeSpecCheck::check(MatchFinder::MatchResult const &result) {
    auto const *call = result.Nodes.getNodeAs<CallExpr>("call");
    if (call == nullptr)
        return;

    // The matcher already filtered to "no string literal in the first arg's
    // subtree". We have nothing more to verify — emit straight to the call
    // site so the squiggle lands on ``einsum(``, where the user will see it.
    diag(call->getBeginLoc(), "first argument is not a string literal — runtime specs skip "
                              "EinsumFormatString's consteval validation. Errors that would "
                              "have been caught at compile time only surface when the call "
                              "runs. Inline the literal here, or silence with NOLINT if the "
                              "spec genuinely needs to be computed.");
}

} // namespace einsums::tidy
