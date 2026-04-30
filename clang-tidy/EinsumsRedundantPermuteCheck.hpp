//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

#pragma once

#include <clang-tidy/ClangTidyCheck.h>

namespace einsums::tidy {

/// einsums-redundant-permute: flag ``cg::permute`` calls whose spec is the
/// identity (output indices match input indices in the same order). These
/// are no-ops at runtime — they allocate, copy element-by-element, and
/// produce a tensor identical to the input. The fix is to either remove the
/// call entirely or, if the user wanted a separate buffer, use ``Tensor``
/// copy construction explicitly so it's obvious that's the intent.
///
/// Examples that fire:
/// \code
///   cg::permute("ij <- ij", &C, A);          // identity, no-op
///   cg::permute("ij -> ij", &C, A);          // identity (NumPy notation)
///   cg::permute("mu,nu <- mu,nu", &C, A);    // identity (multi-char mode)
/// \endcode
///
/// Examples that don't fire:
/// \code
///   cg::permute("ji <- ij", &C, A);          // actual transpose
///   cg::permute("ikj <- ijk", &C, A);        // actual permutation
/// \endcode
///
/// String-only analysis — no AST type info needed. Picked as the first
/// check to validate the plugin pipeline end-to-end before adding more
/// AST-heavy diagnostics.
class EinsumsRedundantPermuteCheck : public ::clang::tidy::ClangTidyCheck {
  public:
    EinsumsRedundantPermuteCheck(::llvm::StringRef name, ::clang::tidy::ClangTidyContext *context)
        : ::clang::tidy::ClangTidyCheck(name, context) {}

    void registerMatchers(::clang::ast_matchers::MatchFinder *finder) override;
    void check(::clang::ast_matchers::MatchFinder::MatchResult const &result) override;
};

} // namespace einsums::tidy
