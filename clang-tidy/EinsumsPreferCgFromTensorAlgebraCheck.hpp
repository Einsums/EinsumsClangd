//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

#pragma once

#include <clang-tidy/ClangTidyCheck.h>

namespace einsums::tidy {

/// einsums-prefer-cg-from-tensoralgebra: flag ``tensor_algebra::einsum``
/// (or ``permute``) calls that occur inside a ``CaptureGuard`` scope. The
/// eager ``tensor_algebra::*`` API runs immediately and doesn't
/// participate in the captured graph, so the user almost certainly meant
/// to call ``cg::einsum`` instead — that's what the surrounding guard
/// implies they want.
///
/// Example:
/// \code
///   {
///       cg::CaptureGuard g(graph);
///       tensor_algebra::einsum(Indices{...}, &C, ...);     // EXPECT: prefer cg::einsum
///   }
/// \endcode
///
/// Mirror of einsums-cg-call-outside-capture: same scope-walk, opposite
/// answer triggers the diagnostic.
class EinsumsPreferCgFromTensorAlgebraCheck : public ::clang::tidy::ClangTidyCheck {
  public:
    EinsumsPreferCgFromTensorAlgebraCheck(::llvm::StringRef name, ::clang::tidy::ClangTidyContext *context)
        : ::clang::tidy::ClangTidyCheck(name, context) {}
    void registerMatchers(::clang::ast_matchers::MatchFinder *finder) override;
    void check(::clang::ast_matchers::MatchFinder::MatchResult const &result) override;
};

} // namespace einsums::tidy
