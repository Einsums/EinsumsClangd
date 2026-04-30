//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

#pragma once

#include <clang-tidy/ClangTidyCheck.h>

namespace einsums::tidy {

/// einsums-mixed-index-mode: flag specs that combine comma-separated
/// multi-char indices with single-char indices in the same string. The
/// EinsumSpec parser auto-detects "multi-char mode" from the presence of
/// *any* comma anywhere in the spec — once that triggers, an operand
/// like ``ij`` becomes one index named ``"ij"``, almost never the user's
/// intent.
///
/// Examples that fire:
/// \code
///   cg::einsum("ij,k <- ik;kj", &C, A, B);     // output uses commas, inputs don't
///   cg::einsum("ij <- i,k;kj", &C, A, B);      // input A uses commas, output/B don't
///   cg::permute("ij <- i,j", &C, A);           // mixed in a permute
/// \endcode
///
/// Examples that don't fire (consistent on either side):
/// \code
///   cg::einsum("i,j <- i,k;k,j", &C, A, B);    // multi-char throughout
///   cg::einsum("ij <- ik;kj", &C, A, B);       // single-char throughout
/// \endcode
///
/// Triggers on either ``cg::einsum`` or ``cg::permute``; the matcher and
/// inspection logic are identical, only the operand list differs.
class EinsumsMixedIndexModeCheck : public ::clang::tidy::ClangTidyCheck {
  public:
    EinsumsMixedIndexModeCheck(::llvm::StringRef name, ::clang::tidy::ClangTidyContext *context)
        : ::clang::tidy::ClangTidyCheck(name, context) {}
    void registerMatchers(::clang::ast_matchers::MatchFinder *finder) override;
    void check(::clang::ast_matchers::MatchFinder::MatchResult const &result) override;
};

} // namespace einsums::tidy
