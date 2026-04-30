//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

#include "EinsumsRankMismatchCheck.hpp"

#include <clang/AST/ASTContext.h>
#include <clang/AST/Expr.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <string_view>
#include <vector>

#include "EinsumSpecParser.hpp"
#include "EinsumsTidyMatchers.hpp"
#include "EinsumsTidyTypeUtils.hpp"

namespace einsums::tidy {

using namespace ::clang;
using namespace ::clang::ast_matchers;

namespace {

/// One operand binding: which call argument index carries which spec
/// operand and a human-friendly label for diagnostics. ``output``,
/// ``A``, ``B`` are the conventional names from EinsumSpec docs; we
/// use them verbatim so users can map a diagnostic back to the spec
/// they wrote.
struct OperandBinding {
    int         arg_index;
    int         expected_rank;
    char const *label;
};

/// Build the (arg_index, expected_rank, label) bindings from a parsed
/// einsum spec and the call's arity. The arity check filters out the
/// rare overload mismatch where someone wrote a custom ``einsum`` with
/// the wrong number of args — we just don't fire.
std::vector<OperandBinding> einsum_bindings(int arity, EinsumSpec const &spec) {
    std::vector<OperandBinding> out;
    int                         out_arg = -1;
    int                         a_arg   = -1;
    int                         b_arg   = -1;
    if (arity == 4) {
        out_arg = 1;
        a_arg   = 2;
        b_arg   = 3;
    } else if (arity == 6) {
        out_arg = 2;
        a_arg   = 4;
        b_arg   = 5;
    } else {
        return out;
    }
    out.push_back({out_arg, static_cast<int>(spec.c.size()), "output"});
    out.push_back({a_arg, static_cast<int>(spec.a.size()), "input A"});
    out.push_back({b_arg, static_cast<int>(spec.b.size()), "input B"});
    return out;
}

std::vector<OperandBinding> permute_bindings(int arity, PermuteSpec const &spec) {
    std::vector<OperandBinding> out;
    int                         out_arg = -1;
    int                         a_arg   = -1;
    if (arity == 3) {
        out_arg = 1;
        a_arg   = 2;
    } else if (arity == 5) {
        out_arg = 2;
        a_arg   = 4;
    } else {
        return out;
    }
    out.push_back({out_arg, static_cast<int>(spec.c.size()), "output"});
    out.push_back({a_arg, static_cast<int>(spec.a.size()), "input"});
    return out;
}

} // namespace

void EinsumsRankMismatchCheck::registerMatchers(MatchFinder *finder) {
    finder->addMatcher(einsum_call_with_literal_spec(), this);
    finder->addMatcher(permute_call_with_literal_spec(), this);
}

void EinsumsRankMismatchCheck::check(MatchFinder::MatchResult const &result) {
    auto const *call = result.Nodes.getNodeAs<CallExpr>("call");
    auto const *spec = result.Nodes.getNodeAs<StringLiteral>("spec");
    if (call == nullptr || spec == nullptr)
        return;
    if (!spec->isOrdinary())
        return;

    auto const             text = spec->getString();
    std::string_view const view(text.data(), text.size());

    // Try einsum-shaped parse first; if it fails, fall back to permute.
    // The two specs differ structurally (presence of ``;``) so the wrong
    // parser will return false rather than mis-tokenize.
    std::vector<OperandBinding> bindings;
    EinsumSpec                  einsum;
    PermuteSpec                 permute;
    if (parse_einsum_spec(view, einsum)) {
        bindings = einsum_bindings(static_cast<int>(call->getNumArgs()), einsum);
    } else if (parse_permute_spec(view, permute)) {
        bindings = permute_bindings(static_cast<int>(call->getNumArgs()), permute);
    } else {
        return; // malformed; another check warns about that
    }
    if (bindings.empty())
        return;

    for (auto const &b : bindings) {
        auto const *arg      = call->getArg(b.arg_index);
        auto const  arg_type = arg->getType();
        auto const  rank_opt = tensor_rank(arg_type);
        if (!rank_opt.has_value())
            continue; // template-dependent or non-tensor — let other checks handle
        if (*rank_opt == b.expected_rank)
            continue;
        diag(arg->getBeginLoc(), "rank mismatch: spec lists %0 indices for %1, but the tensor here has rank %2.")
            << b.expected_rank << b.label << *rank_opt;
        return; // one report per call — the user fixes the spec or the type, not both
    }
}

} // namespace einsums::tidy
