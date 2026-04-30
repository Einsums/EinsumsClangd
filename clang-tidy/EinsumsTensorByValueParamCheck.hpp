//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

#pragma once

#include <clang-tidy/ClangTidyCheck.h>

namespace einsums::tidy {

/// einsums-tensor-by-value-param: flag function parameters typed as a
/// concrete tensor (``Tensor<T, N>``, ``BlockTensor<...>``, ``TiledTensor<...>``).
/// These types are owning containers — passing one by value forces a copy
/// of the entire underlying buffer at every call. The fix is to take it
/// by ``const &`` (read-only) or by ``TensorView<T, N>`` (when the callee
/// genuinely just wants a non-owning handle).
///
/// We deliberately exempt ``TensorView`` itself: it's already a non-owning
/// handle, ~24 bytes, and passing it by value is correct.
///
/// Example:
/// \code
///   void heavy(Tensor<double, 2> A);              // EXPECT: pass by const &
///   void readonly(Tensor<double, 2> const &A);    // OK
///   void view_arg(TensorView<double, 2> A);       // OK (non-owning)
/// \endcode
class EinsumsTensorByValueParamCheck : public ::clang::tidy::ClangTidyCheck {
  public:
    EinsumsTensorByValueParamCheck(::llvm::StringRef name, ::clang::tidy::ClangTidyContext *context)
        : ::clang::tidy::ClangTidyCheck(name, context) {}
    void registerMatchers(::clang::ast_matchers::MatchFinder *finder) override;
    void check(::clang::ast_matchers::MatchFinder::MatchResult const &result) override;
};

} // namespace einsums::tidy
