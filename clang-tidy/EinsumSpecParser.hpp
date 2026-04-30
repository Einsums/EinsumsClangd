//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

#pragma once

/// \file
/// Header-only parser for ComputeGraph einsum/permute spec strings.
///
/// Shared across the EinsumsTidy checks so each one doesn't re-implement the
/// ~50-line tokenizer. We deliberately don't link the ComputeGraph library
/// into the clang-tidy plugin (would pull all of Einsums into the LLVM
/// process); the spec format is small enough to re-implement here.
///
/// Format recap (from ``Einsums/ComputeGraph/EinsumSpec.hpp``):
///
///   - ``"ij <- ik;kj"`` — arrow notation, output on the left
///   - ``"ik;kj -> ij"`` — NumPy notation, output on the right
///   - ``"mu,nu <- mu,rho;rho,nu"`` — comma mode for multi-char indices
///
/// Permute is the same minus the ``;`` (single input), e.g. ``"ji <- ij"``.

#include <string>
#include <string_view>
#include <vector>

namespace einsums::tidy {

/// Two-input einsum spec. ``c`` is the output, ``a`` and ``b`` the inputs.
struct EinsumSpec {
    std::vector<std::string> c;
    std::vector<std::string> a;
    std::vector<std::string> b;
};

/// Single-input permute spec.
struct PermuteSpec {
    std::vector<std::string> c;
    std::vector<std::string> a;
};

namespace detail {

/// Tokenize one operand's index list into a vector of index names.
/// ``raw`` is one side of the arrow / one operand of the semicolon;
/// ``multi_char`` is selected globally based on whether *any* comma
/// appears anywhere in the spec (matching EinsumSpec.hpp's rule).
inline std::vector<std::string> tokenize(std::string_view raw, bool multi_char) {
    std::vector<std::string> out;
    std::string              current;
    for (char c : raw) {
        bool const is_space = (c == ' ' || c == '\t' || c == '\n' || c == '\r');
        if (multi_char) {
            if (c == ',' || is_space) {
                if (!current.empty()) {
                    out.push_back(std::move(current));
                    current.clear();
                }
                continue;
            }
            current.push_back(c);
        } else {
            if (is_space)
                continue;
            out.emplace_back(1, c);
        }
    }
    if (!current.empty())
        out.push_back(std::move(current));
    return out;
}

} // namespace detail

/// Parse an einsum spec ``"<C> <- <A> ; <B>"`` (or ``"<A> ; <B> -> <C>"``).
/// Returns ``true`` on a structurally valid spec, ``false`` otherwise (no
/// arrow, more than one ``;``, missing operand, malformed).
inline bool parse_einsum_spec(std::string_view spec, EinsumSpec &out) {
    auto const left      = spec.find("<-");
    auto const right     = spec.find("->");
    bool const has_left  = (left != std::string_view::npos);
    bool const has_right = (right != std::string_view::npos);
    if (has_left == has_right) // both or neither
        return false;

    auto const arrow_pos = has_left ? left : right;
    auto const c_raw     = has_left ? spec.substr(0, arrow_pos) : spec.substr(arrow_pos + 2);
    auto const ab_raw    = has_left ? spec.substr(arrow_pos + 2) : spec.substr(0, arrow_pos);

    auto const semi = ab_raw.find(';');
    if (semi == std::string_view::npos)
        return false;
    if (ab_raw.find(';', semi + 1) != std::string_view::npos)
        return false;

    auto const a_raw = ab_raw.substr(0, semi);
    auto const b_raw = ab_raw.substr(semi + 1);

    bool const multi_char = (spec.find(',') != std::string_view::npos);
    out.c                 = detail::tokenize(c_raw, multi_char);
    out.a                 = detail::tokenize(a_raw, multi_char);
    out.b                 = detail::tokenize(b_raw, multi_char);
    return !out.c.empty() && !out.a.empty() && !out.b.empty();
}

/// Parse a permute spec ``"<C> <- <A>"`` (or ``"<A> -> <C>"``).
inline bool parse_permute_spec(std::string_view spec, PermuteSpec &out) {
    auto const left      = spec.find("<-");
    auto const right     = spec.find("->");
    bool const has_left  = (left != std::string_view::npos);
    bool const has_right = (right != std::string_view::npos);
    if (has_left == has_right)
        return false;
    if (spec.find(';') != std::string_view::npos)
        return false;

    auto const arrow_pos = has_left ? left : right;
    auto const c_raw     = has_left ? spec.substr(0, arrow_pos) : spec.substr(arrow_pos + 2);
    auto const a_raw     = has_left ? spec.substr(arrow_pos + 2) : spec.substr(0, arrow_pos);

    bool const multi_char = (spec.find(',') != std::string_view::npos);
    out.c                 = detail::tokenize(c_raw, multi_char);
    out.a                 = detail::tokenize(a_raw, multi_char);
    return !out.c.empty() && !out.a.empty();
}

} // namespace einsums::tidy
