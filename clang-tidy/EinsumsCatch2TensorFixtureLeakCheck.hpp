//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

#pragma once

#include <clang-tidy/ClangTidyCheck.h>

namespace einsums::tidy {

/// einsums-catch2-tensor-fixture-leak: flag tensors declared inside a
/// ``TEST_CASE`` (or Einsums' ``EINSUMS_TEST_CASE``) body whose value
/// never reaches an assertion macro (``REQUIRE``, ``CHECK``,
/// ``REQUIRE_THAT``, ``CHECK_THAT``, ``INFO``, ``CAPTURE``, etc.). Such
/// tensors are usually a sign the test is broken-but-passing — the
/// computation runs but no observation is made, so a regression that
/// changes the result silently goes unnoticed.
///
/// Examples:
/// \code
///   TEST_CASE("contraction") {
///       Tensor<double, 2> A({3, 3});
///       random_fill(A);
///       Tensor<double, 2> C({3, 3});
///       cg::einsum("ij <- ik;kj", &C, A, A);
///       // EXPECT: C is never asserted
///   }
///
///   TEST_CASE("contraction-fixed") {
///       Tensor<double, 2> A({3, 3});
///       random_fill(A);
///       Tensor<double, 2> C({3, 3});
///       cg::einsum("ij <- ik;kj", &C, A, A);
///       REQUIRE(close_to(C, expected));   // OK — C is asserted
///   }
/// \endcode
///
/// Doesn't fire on TensorViews (typically aliases of an asserted tensor)
/// or on tensors used as operands of a *different* tensor that does
/// reach an assertion (transitive use is enough).
class EinsumsCatch2TensorFixtureLeakCheck : public ::clang::tidy::ClangTidyCheck {
  public:
    EinsumsCatch2TensorFixtureLeakCheck(::llvm::StringRef name, ::clang::tidy::ClangTidyContext *context)
        : ::clang::tidy::ClangTidyCheck(name, context) {}
    void registerMatchers(::clang::ast_matchers::MatchFinder *finder) override;
    void check(::clang::ast_matchers::MatchFinder::MatchResult const &result) override;
};

} // namespace einsums::tidy
