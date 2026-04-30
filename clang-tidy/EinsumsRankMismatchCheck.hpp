//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

#pragma once

#include <clang-tidy/ClangTidyCheck.h>

namespace einsums::tidy {

/// einsums-rank-mismatch: verify that each tensor argument's
/// ``Tensor<T, N>`` rank matches the spec's index count for the operand
/// it's bound to. The spec is the source of truth at the call site —
/// the user wrote ``"ij <- ik;kj"``, intending two-index operands; if
/// any argument is actually a ``Tensor<T, 3>``, the call is broken.
///
/// Examples:
/// \code
///   cg::Tensor<double, 3> A;
///   cg::einsum("ij <- ik;kj", &C, A, B);   // EXPECT: rank mismatch on A
///
///   cg::Tensor<double, 2> A;
///   cg::einsum("ij <- ik;kj", &C, A, B);   // OK
/// \endcode
///
/// Skips operands whose rank is template-dependent (uninstantiated
/// templates) so we don't false-positive in generic code; once the
/// template gets instantiated with concrete tensors, the check fires
/// against each instantiation.
class EinsumsRankMismatchCheck : public ::clang::tidy::ClangTidyCheck {
  public:
    EinsumsRankMismatchCheck(::llvm::StringRef name, ::clang::tidy::ClangTidyContext *context)
        : ::clang::tidy::ClangTidyCheck(name, context) {}
    void registerMatchers(::clang::ast_matchers::MatchFinder *finder) override;
    void check(::clang::ast_matchers::MatchFinder::MatchResult const &result) override;
};

} // namespace einsums::tidy
