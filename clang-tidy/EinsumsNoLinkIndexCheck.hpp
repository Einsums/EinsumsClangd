//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

#pragma once

#include <clang-tidy/ClangTidyCheck.h>

namespace einsums::tidy {

/// einsums-no-link-index: flag ``cg::einsum`` where A and B share no
/// contracted (link) index. With nothing to contract over, the call is a
/// silent outer product across distinct index sets — almost always a typo
/// where the user meant the same letter in both operands.
///
/// "Link index" = an index present in both A and B but absent from C.
/// Examples:
/// \code
///   cg::einsum("ij <- i;j", &C, A, B);      // no shared input index — flag
///   cg::einsum("ij <- ik;kj", &C, A, B);    // 'k' contracted — OK
///   cg::einsum("ijk <- i;j;k", ...);        // arity mismatch, separate parser path
/// \endcode
///
/// Note: an einsum where every input index appears in C is technically a
/// "Hadamard-style" contraction, which is valid (and not what this check
/// flags). The trigger is specifically: there exists no index in both A
/// and B.
class EinsumsNoLinkIndexCheck : public ::clang::tidy::ClangTidyCheck {
  public:
    EinsumsNoLinkIndexCheck(::llvm::StringRef name, ::clang::tidy::ClangTidyContext *context)
        : ::clang::tidy::ClangTidyCheck(name, context) {}
    void registerMatchers(::clang::ast_matchers::MatchFinder *finder) override;
    void check(::clang::ast_matchers::MatchFinder::MatchResult const &result) override;
};

} // namespace einsums::tidy
