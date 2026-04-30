//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

#include "EinsumsDuplicateOutputIndexCheck.hpp"

#include <clang/AST/ASTContext.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <string_view>
#include <unordered_set>

#include "EinsumSpecParser.hpp"
#include "EinsumsTidyMatchers.hpp"

namespace einsums::tidy {

using namespace ::clang;
using namespace ::clang::ast_matchers;

void EinsumsDuplicateOutputIndexCheck::registerMatchers(MatchFinder *finder) {
    finder->addMatcher(einsum_call_with_literal_spec(), this);
}

void EinsumsDuplicateOutputIndexCheck::check(MatchFinder::MatchResult const &result) {
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

    // O(n^2) over the output's index list (n is small — typically ≤ 4),
    // so a linear-scan dedupe is plenty. We could go via unordered_set
    // but the overhead isn't worth it at this size.
    std::unordered_set<std::string> seen;
    for (auto const &name : parsed.c) {
        if (!seen.insert(name).second) {
            diag(call->getBeginLoc(), "output index '%0' appears more than once in C — einsum can't express a "
                                      "self-trace on the output side. Move the duplicate to an input or use "
                                      "``cg::trace`` for a literal trace.")
                << name;
            return;
        }
    }
}

} // namespace einsums::tidy
