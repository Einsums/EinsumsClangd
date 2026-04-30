//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

#include "EinsumsMixedIndexModeCheck.hpp"

#include <clang/AST/ASTContext.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <string_view>
#include <vector>

#include "EinsumsTidyMatchers.hpp"

namespace einsums::tidy {

using namespace ::clang;
using namespace ::clang::ast_matchers;

namespace {

/// Strip whitespace from a string_view in place. Easier than rolling out a
/// trim helper — the operands we look at are short.
std::string strip_spaces(std::string_view in) {
    std::string out;
    out.reserve(in.size());
    for (char c : in)
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r')
            out.push_back(c);
    return out;
}

/// Split the spec into operand strings. Doesn't tokenize indices — just
/// returns the raw text on each side of arrows / semicolons. We need raw
/// because the *presence* of a comma per operand is what we're checking,
/// and the parser would have normalized that away.
std::vector<std::string> split_operands(std::string_view spec) {
    auto const left      = spec.find("<-");
    auto const right     = spec.find("->");
    bool const has_left  = (left != std::string_view::npos);
    bool const has_right = (right != std::string_view::npos);
    if (has_left == has_right)
        return {}; // malformed; not our problem

    auto const arrow_pos = has_left ? left : right;
    auto const c_raw     = has_left ? spec.substr(0, arrow_pos) : spec.substr(arrow_pos + 2);
    auto const ab_raw    = has_left ? spec.substr(arrow_pos + 2) : spec.substr(0, arrow_pos);

    std::vector<std::string> out;
    out.push_back(strip_spaces(c_raw));

    auto const semi = ab_raw.find(';');
    if (semi == std::string_view::npos) {
        // permute case — single input
        out.push_back(strip_spaces(ab_raw));
    } else {
        // einsum case — A then B, each may itself contain another semi
        // (rejected by the spec validator anyway, but cheap to handle).
        out.push_back(strip_spaces(ab_raw.substr(0, semi)));
        out.push_back(strip_spaces(ab_raw.substr(semi + 1)));
    }
    return out;
}

/// True when ``operand`` has at least one comma — using comma mode.
bool has_comma(std::string const &operand) {
    return operand.find(',') != std::string::npos;
}

/// True when ``operand`` is *ambiguous* under comma mode: two or more
/// non-comma characters with no comma to disambiguate them. A single
/// non-comma char (e.g. ``"i"``) is fine in either mode.
bool is_ambiguous_under_comma_mode(std::string const &operand) {
    if (operand.find(',') != std::string::npos)
        return false;
    return operand.size() > 1;
}

} // namespace

void EinsumsMixedIndexModeCheck::registerMatchers(MatchFinder *finder) {
    // One matcher per call kind. Both bind the same node names (``call`` /
    // ``spec``) so the ``check`` body is shared.
    finder->addMatcher(einsum_call_with_literal_spec(), this);
    finder->addMatcher(permute_call_with_literal_spec(), this);
}

void EinsumsMixedIndexModeCheck::check(MatchFinder::MatchResult const &result) {
    auto const *call = result.Nodes.getNodeAs<CallExpr>("call");
    auto const *spec = result.Nodes.getNodeAs<StringLiteral>("spec");
    if (call == nullptr || spec == nullptr)
        return;
    if (!spec->isOrdinary())
        return;

    auto const             text = spec->getString();
    std::string_view const view(text.data(), text.size());

    auto const operands = split_operands(view);
    if (operands.empty())
        return; // malformed — different check's territory

    // Detection: at least one operand uses commas AND at least one *other*
    // operand is ambiguous under comma mode. The asymmetric pair is what
    // makes the spec read like a typo. A spec that's all-single-char or
    // all-comma is fine and we stay quiet.
    std::string comma_operand;
    bool        any_ambiguous = false;
    for (auto const &op : operands) {
        if (has_comma(op) && comma_operand.empty())
            comma_operand = op;
        if (is_ambiguous_under_comma_mode(op))
            any_ambiguous = true;
    }
    if (comma_operand.empty() || !any_ambiguous)
        return;

    diag(call->getBeginLoc(), "spec mixes comma-separated and single-char index modes — operand %0 contains "
                              "commas while another operand has multiple letters with no separator. The parser "
                              "will silently pick comma-mode and treat ``ij`` as one index named ``\"ij\"``. "
                              "Use commas in every operand or in none.")
        << comma_operand;
}

} // namespace einsums::tidy
