//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

#pragma once

#include <clang-tidy/ClangTidyCheck.h>

namespace einsums::tidy {

/// einsums-mismatched-value-type: flag ``cg::einsum`` / ``cg::permute``
/// where the operand tensors have different value types — for example,
/// ``einsum(spec, &C_double, A_float, B_double)``. Mixing precisions in a
/// single contraction either produces a compile error (when
/// ``Tensor::ValueType`` constraints kick in) or silently up/down-converts
/// at every BLAS dispatch, both of which are usually bugs.
///
/// We compare the first template argument of each tensor type
/// (``T`` in ``Tensor<T, N>`` and friends). All operands must agree;
/// the first mismatch is reported with the offending types.
class EinsumsMismatchedValueTypeCheck : public ::clang::tidy::ClangTidyCheck {
  public:
    EinsumsMismatchedValueTypeCheck(::llvm::StringRef name, ::clang::tidy::ClangTidyContext *context)
        : ::clang::tidy::ClangTidyCheck(name, context) {}
    void registerMatchers(::clang::ast_matchers::MatchFinder *finder) override;
    void check(::clang::ast_matchers::MatchFinder::MatchResult const &result) override;
};

} // namespace einsums::tidy
