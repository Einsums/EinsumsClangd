//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

#pragma once

#include <clang-tidy/ClangTidyCheck.h>

namespace einsums::tidy {

/// einsums-output-aliasing-input: flag ``cg::einsum(spec, &A, A, B)`` (or
/// any variant where the output-arg points at the same variable as one of
/// the input-args). Sometimes intentional — in-place updates do exist —
/// but more often it's a typo where the user meant a different variable
/// for the output. The aliasing is also the easy way to confuse the
/// underlying BLAS dispatch about which buffer can be safely written.
///
/// Examples:
/// \code
///   cg::einsum("ij <- ik;kj", &A, A, B);    // EXPECT: output aliases input A
///   cg::einsum("ij <- ik;kj", &C, A, B);    // OK
/// \endcode
///
/// We only flag when the output and the input refer to the same
/// ``VarDecl`` — different references through pointer arithmetic or
/// view construction don't trip it.
class EinsumsOutputAliasingInputCheck : public ::clang::tidy::ClangTidyCheck {
  public:
    EinsumsOutputAliasingInputCheck(::llvm::StringRef name, ::clang::tidy::ClangTidyContext *context)
        : ::clang::tidy::ClangTidyCheck(name, context) {}
    void registerMatchers(::clang::ast_matchers::MatchFinder *finder) override;
    void check(::clang::ast_matchers::MatchFinder::MatchResult const &result) override;
};

} // namespace einsums::tidy
