//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

#pragma once

/// \file
/// AST-matcher fragments shared across the EinsumsTidy checks. Pulling these
/// up here means each new check is mostly its own ``check()`` body — the
/// boilerplate of "find a ``cg::einsum(...)`` call with a string-literal
/// spec, capture the call and the literal" lives once.
///
/// We can't easily share ``StatementMatcher`` *values* between translation
/// units (their internal types are heavy), so the helpers below are inline
/// functions that each check calls from its own ``registerMatchers`` body.
/// That gives a single source of truth for the matcher shape without forcing
/// global initializer ordering games.

#include <clang/ASTMatchers/ASTMatchers.h>

namespace einsums::tidy {

/// Match ``einsum(spec, ...)`` callsites whose first argument is a
/// ``stringLiteral`` (possibly wrapped in a ``CXXConstructExpr`` for
/// ``EinsumFormatString``). Captures:
///   - ``"call"`` → the ``CallExpr``
///   - ``"spec"`` → the ``StringLiteral`` interior
///
/// Arity bound to 4 (``einsum(spec, &C, A, B)``) or 6
/// (``einsum(spec, c_pf, &C, ab_pf, A, B)``) — the documented overloads —
/// to keep us from misfiring on unrelated user-defined ``einsum`` helpers.
inline ::clang::ast_matchers::StatementMatcher einsum_call_with_literal_spec() {
    using namespace ::clang::ast_matchers;
    return callExpr(callee(functionDecl(hasName("einsum"))),
                    hasArgument(
                        0, anyOf(ignoringParenImpCasts(stringLiteral().bind("spec")), expr(hasDescendant(stringLiteral().bind("spec"))))),
                    anyOf(argumentCountIs(4), argumentCountIs(6)))
        .bind("call");
}

/// Same shape for ``permute``; arity 3 (``permute(spec, &C, A)``) or 5
/// (``permute(spec, beta, &C, alpha, A)``).
inline ::clang::ast_matchers::StatementMatcher permute_call_with_literal_spec() {
    using namespace ::clang::ast_matchers;
    return callExpr(callee(functionDecl(hasName("permute"))),
                    hasArgument(
                        0, anyOf(ignoringParenImpCasts(stringLiteral().bind("spec")), expr(hasDescendant(stringLiteral().bind("spec"))))),
                    anyOf(argumentCountIs(3), argumentCountIs(5)))
        .bind("call");
}

/// Match ``einsum(spec, ...)`` callsites whose first argument is *not* a
/// string literal anywhere in its subtree — e.g. ``cg::einsum(my_string, &C, A, B)``.
/// Used by einsums-runtime-spec to nudge users toward the literal form
/// that gets ``EinsumFormatString``'s consteval validation.
inline ::clang::ast_matchers::StatementMatcher einsum_call_with_nonliteral_spec() {
    using namespace ::clang::ast_matchers;
    return callExpr(callee(functionDecl(hasName("einsum"))),
                    hasArgument(0, expr(unless(hasDescendant(stringLiteral())), unless(stringLiteral())).bind("arg0")),
                    anyOf(argumentCountIs(4), argumentCountIs(6)))
        .bind("call");
}

inline ::clang::ast_matchers::StatementMatcher permute_call_with_nonliteral_spec() {
    using namespace ::clang::ast_matchers;
    return callExpr(callee(functionDecl(hasName("permute"))),
                    hasArgument(0, expr(unless(hasDescendant(stringLiteral())), unless(stringLiteral())).bind("arg0")),
                    anyOf(argumentCountIs(3), argumentCountIs(5)))
        .bind("call");
}

} // namespace einsums::tidy
