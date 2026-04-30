//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

#include "EinsumsRedundantPermuteCheck.hpp"

#include <algorithm>
#include <clang/AST/ASTContext.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <string_view>

#include "EinsumSpecParser.hpp"
#include "EinsumsTidyMatchers.hpp"

namespace einsums::tidy {

using namespace ::clang;
using namespace ::clang::ast_matchers;

void EinsumsRedundantPermuteCheck::registerMatchers(MatchFinder *finder) {
    finder->addMatcher(permute_call_with_literal_spec(), this);
}

void EinsumsRedundantPermuteCheck::check(MatchFinder::MatchResult const &result) {
    auto const *call = result.Nodes.getNodeAs<CallExpr>("call");
    auto const *spec = result.Nodes.getNodeAs<StringLiteral>("spec");
    if (call == nullptr || spec == nullptr)
        return;

    // ``getString()`` returns the raw bytes (escapes already processed by
    // the parser). For our purposes — looking for ASCII index characters,
    // ``;``, ``->``, ``<-`` — wide-char or non-UTF-8 strings are nonsensical
    // and we just bail.
    if (!spec->isOrdinary())
        return;
    auto const             text = spec->getString();
    std::string_view const view(text.data(), text.size());

    PermuteSpec parsed;
    if (!parse_permute_spec(view, parsed))
        return;
    if (parsed.c.size() != parsed.a.size())
        return; // structurally inconsistent — don't second-guess
    if (!std::equal(parsed.c.begin(), parsed.c.end(), parsed.a.begin()))
        return; // genuine permutation — leave alone

    // Identity permute. Flag the call site; point the user at the spec
    // literal so the highlight lands on the offending string.
    diag(call->getBeginLoc(), "redundant permute: spec %0 is the identity permutation — the call allocates and "
                              "copies element-by-element to produce an unchanged result. Remove the call, or use "
                              "Tensor copy construction if you wanted a separate buffer.")
        << view;
    diag(spec->getBeginLoc(), "spec is here", DiagnosticIDs::Note);
}

} // namespace einsums::tidy
