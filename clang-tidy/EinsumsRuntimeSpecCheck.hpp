//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

#pragma once

#include <clang-tidy/ClangTidyCheck.h>

namespace einsums::tidy {

/// einsums-runtime-spec: flag ``cg::einsum`` / ``cg::permute`` calls whose
/// first argument is not a string literal. The literal-arg path goes
/// through ``EinsumFormatString``'s ``consteval`` constructor, which
/// statically validates the spec at compile time. The runtime path
/// (``std::string`` or any expression that isn't a literal) defers
/// validation until the call actually executes — typos surface at the
/// first invocation rather than at the point the spec is written.
///
/// Examples:
/// \code
///   std::string s = "ij <- ik;kj";
///   cg::einsum(s, &C, A, B);                  // EXPECT: prefer a literal
///   cg::einsum(get_spec(), &C, A, B);         // EXPECT: prefer a literal
///   cg::einsum("ij <- ik;kj", &C, A, B);      // OK — literal
/// \endcode
///
/// Most often the right fix is to inline the literal at the call site;
/// when the spec genuinely is computed (e.g. driven by Python bindings),
/// silence with ``// NOLINT``.
class EinsumsRuntimeSpecCheck : public ::clang::tidy::ClangTidyCheck {
  public:
    EinsumsRuntimeSpecCheck(::llvm::StringRef name, ::clang::tidy::ClangTidyContext *context)
        : ::clang::tidy::ClangTidyCheck(name, context) {}
    void registerMatchers(::clang::ast_matchers::MatchFinder *finder) override;
    void check(::clang::ast_matchers::MatchFinder::MatchResult const &result) override;
};

} // namespace einsums::tidy
