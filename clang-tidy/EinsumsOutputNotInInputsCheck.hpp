//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

#pragma once

#include <clang-tidy/ClangTidyCheck.h>

namespace einsums::tidy {

/// einsums-output-not-in-inputs: flag ``cg::einsum`` calls whose output
/// spec contains an index that doesn't appear in either input. Almost
/// always a typo (``"ij <- ik;kl"`` should be ``"ij <- ik;kj"``).
///
/// Examples:
/// \code
///   cg::einsum("ij <- ik;kl", &C, A, B);    // 'j' missing from inputs — typo
///   cg::einsum("ij <- ik;kj", &C, A, B);    // OK
/// \endcode
class EinsumsOutputNotInInputsCheck : public ::clang::tidy::ClangTidyCheck {
  public:
    EinsumsOutputNotInInputsCheck(::llvm::StringRef name, ::clang::tidy::ClangTidyContext *context)
        : ::clang::tidy::ClangTidyCheck(name, context) {}
    void registerMatchers(::clang::ast_matchers::MatchFinder *finder) override;
    void check(::clang::ast_matchers::MatchFinder::MatchResult const &result) override;
};

} // namespace einsums::tidy
