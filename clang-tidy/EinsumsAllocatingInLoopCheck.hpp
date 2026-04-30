//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

#pragma once

#include <clang-tidy/ClangTidyCheck.h>

namespace einsums::tidy {

/// einsums-allocating-in-loop: flag owning-tensor variables declared
/// inside a loop body. Each iteration re-runs the constructor, allocating
/// a fresh buffer and freeing the previous one — usually a real
/// performance pitfall in code where the loop is hot. The fix is almost
/// always to hoist the variable out of the loop and ``zero()`` (or
/// otherwise reset) it at the top of each iteration.
///
/// Examples:
/// \code
///   for (int i = 0; i < N; ++i) {
///       Tensor<double, 2> tmp({M, K});      // EXPECT: allocating in loop
///       cg::einsum(...);
///   }
///
///   Tensor<double, 2> tmp({M, K});
///   for (int i = 0; i < N; ++i) {
///       tmp.zero();
///       cg::einsum(...);                    // OK
///   }
/// \endcode
///
/// Exempts ``TensorView`` (a non-owning handle, ~24 bytes — fine to
/// re-create per iteration) and tensors of template-dependent rank
/// (in generic code we can't tell whether the cost is real).
class EinsumsAllocatingInLoopCheck : public ::clang::tidy::ClangTidyCheck {
  public:
    EinsumsAllocatingInLoopCheck(::llvm::StringRef name, ::clang::tidy::ClangTidyContext *context)
        : ::clang::tidy::ClangTidyCheck(name, context) {}
    void registerMatchers(::clang::ast_matchers::MatchFinder *finder) override;
    void check(::clang::ast_matchers::MatchFinder::MatchResult const &result) override;
};

} // namespace einsums::tidy
