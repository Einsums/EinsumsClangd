//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

#include "EinsumsOutputNotInInputsCheck.hpp"

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

void EinsumsOutputNotInInputsCheck::registerMatchers(MatchFinder *finder) {
    finder->addMatcher(einsum_call_with_literal_spec(), this);
}

void EinsumsOutputNotInInputsCheck::check(MatchFinder::MatchResult const &result) {
    auto const *call = result.Nodes.getNodeAs<CallExpr>("call");
    auto const *spec = result.Nodes.getNodeAs<StringLiteral>("spec");
    if (call == nullptr || spec == nullptr)
        return;
    if (!spec->isOrdinary())
        return;

    auto const             text = spec->getString();
    std::string_view const view(text.data(), text.size());

    EinsumSpec parsed;
    if (!parse_einsum_spec(view, parsed))
        return;

    // Walk every output index. A "valid" output index appears in at least
    // one input; anything else is flagged. We don't dedupe — if the same
    // index appears in C twice, both occurrences are spurious if missing
    // from the inputs, and reporting each fits the user's mental model
    // ("this character is wrong, that character is wrong").
    auto in = [](std::vector<std::string> const &g, std::string const &name) { return std::find(g.begin(), g.end(), name) != g.end(); };
    for (auto const &name : parsed.c) {
        if (!in(parsed.a, name) && !in(parsed.b, name)) {
            diag(call->getBeginLoc(), "output index '%0' does not appear in either input — likely a typo or "
                                      "missing operand. einsum requires every output index to be present in at "
                                      "least one input tensor.")
                << name;
            // Only fire once per call. Multiple bad indices in the same
            // spec are usually the same root cause; the user fixes one
            // and the rest follow.
            return;
        }
    }
}

} // namespace einsums::tidy
