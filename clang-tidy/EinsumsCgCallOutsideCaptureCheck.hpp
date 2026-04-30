//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

#pragma once

#include <clang-tidy/ClangTidyCheck.h>

namespace einsums::tidy {

/// einsums-cg-call-outside-capture: flag ``cg::einsum`` / ``cg::permute``
/// calls that aren't lexically inside any ``CaptureGuard`` scope. The
/// compute-graph variants only do useful work when a guard is active —
/// without one they execute eagerly (or fail outright, depending on the
/// build), defeating the whole purpose of using ``cg::*`` over the
/// eager ``tensor_algebra::*`` API.
///
/// Example:
/// \code
///   void wrong() {
///       cg::einsum("ij <- ik;kj", &C, A, B);    // EXPECT: no CaptureGuard in scope
///   }
///
///   void right() {
///       cg::CaptureGuard g(graph);
///       cg::einsum("ij <- ik;kj", &C, A, B);    // OK
///   }
/// \endcode
///
/// Walks lexical ancestors only — guards held in callable objects, captured
/// closures, or stored on heap-allocated state aren't tracked. False
/// negatives in those cases are accepted as the cost of staying decidable.
class EinsumsCgCallOutsideCaptureCheck : public ::clang::tidy::ClangTidyCheck {
  public:
    EinsumsCgCallOutsideCaptureCheck(::llvm::StringRef name, ::clang::tidy::ClangTidyContext *context)
        : ::clang::tidy::ClangTidyCheck(name, context) {}
    void registerMatchers(::clang::ast_matchers::MatchFinder *finder) override;
    void check(::clang::ast_matchers::MatchFinder::MatchResult const &result) override;
};

} // namespace einsums::tidy
