//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

#pragma once

#include <clang-tidy/ClangTidyCheck.h>

namespace einsums::tidy {

/// einsums-duplicate-output-index: flag ``cg::einsum`` where the same
/// index appears more than once on the output side. ``einsum(C, A, B)``
/// has no clean meaning for "same index repeated in C" — a self-trace
/// belongs on the input side or in a separate ``trace`` call.
///
/// Examples:
/// \code
///   cg::einsum("ii <- ij;jk", &C, A, B);    // 'i' duplicated in C — flag
///   cg::einsum("ij <- ij;jk", &C, A, B);    // OK — distinct output indices
/// \endcode
class EinsumsDuplicateOutputIndexCheck : public ::clang::tidy::ClangTidyCheck {
  public:
    EinsumsDuplicateOutputIndexCheck(::llvm::StringRef name, ::clang::tidy::ClangTidyContext *context)
        : ::clang::tidy::ClangTidyCheck(name, context) {}
    void registerMatchers(::clang::ast_matchers::MatchFinder *finder) override;
    void check(::clang::ast_matchers::MatchFinder::MatchResult const &result) override;
};

} // namespace einsums::tidy
