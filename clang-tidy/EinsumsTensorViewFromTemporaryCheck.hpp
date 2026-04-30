//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

#pragma once

#include <clang-tidy/ClangTidyCheck.h>

namespace einsums::tidy {

/// einsums-tensor-view-from-temporary: flag ``TensorView v = expr;`` (or
/// the equivalent ``TensorView v(expr);``) where ``expr`` is a temporary
/// owning-tensor — typically a function call returning a ``Tensor`` by
/// value, or a ``Tensor{...}`` materialised inline. The view stores a
/// pointer into the temporary's data; that buffer is freed when the
/// temporary's full-expression ends, leaving ``v`` dangling.
///
/// Examples:
/// \code
///   Tensor<double, 2> make_tensor();
///
///   TensorView<double, 2> v = make_tensor();   // EXPECT: dangling view
///   TensorView<double, 2> v(make_tensor());    // EXPECT: dangling view
///
///   Tensor<double, 2> t = make_tensor();
///   TensorView<double, 2> v = t;               // OK — t outlives v
/// \endcode
///
/// Doesn't catch every dangling case (lifetime analysis in C++ is
/// undecidable in general), but the literal "view assigned a freshly-
/// returned tensor" pattern is common and unambiguously broken.
class EinsumsTensorViewFromTemporaryCheck : public ::clang::tidy::ClangTidyCheck {
  public:
    EinsumsTensorViewFromTemporaryCheck(::llvm::StringRef name, ::clang::tidy::ClangTidyContext *context)
        : ::clang::tidy::ClangTidyCheck(name, context) {}
    void registerMatchers(::clang::ast_matchers::MatchFinder *finder) override;
    void check(::clang::ast_matchers::MatchFinder::MatchResult const &result) override;
};

} // namespace einsums::tidy
