//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

#include "EinsumsNoLinkIndexCheck.hpp"

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

void EinsumsNoLinkIndexCheck::registerMatchers(MatchFinder *finder) {
    finder->addMatcher(einsum_call_with_literal_spec(), this);
}

void EinsumsNoLinkIndexCheck::check(MatchFinder::MatchResult const &result) {
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

    // A "link" index is present in both A and B. Equivalently: any element
    // of A that also lives in B. With even one such index the call has
    // something to contract over and we stay quiet.
    auto in = [](std::vector<std::string> const &g, std::string const &name) { return std::find(g.begin(), g.end(), name) != g.end(); };
    bool has_link = false;
    for (auto const &name : parsed.a) {
        if (in(parsed.b, name)) {
            has_link = true;
            break;
        }
    }
    if (has_link)
        return;

    diag(call->getBeginLoc(), "einsum has no contracted index between A and B — this is a silent outer product. "
                              "If that's the intent, leave a comment saying so; otherwise check the spec for a "
                              "typo (a letter that should match between the two operands).");
}

} // namespace einsums::tidy
