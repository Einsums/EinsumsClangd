//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

#pragma once

#include <clang-tidy/ClangTidyCheck.h>

namespace einsums::tidy {

/// einsums-deprecated-api: surface calls to ``[[deprecated]]``-attributed
/// Einsums declarations through the ``einsums-*`` check group. Clang's
/// own ``-Wdeprecated-declarations`` already warns on these, but routing
/// them through clang-tidy means they show up in the same diagnostic
/// stream as the rest of the Einsums checks, can be enabled / silenced
/// per-file via ``.clang-tidy``, and carry the human-friendly message
/// the deprecation attribute already attached.
///
/// Scope: only fires for declarations whose qualified name lives under
/// the ``einsums::`` namespace (or a sub-namespace thereof). System or
/// stdlib deprecation warnings stay on the standard Clang path; this
/// check is a project-specific lens.
///
/// Example:
/// \code
///   namespace einsums {
///   [[deprecated("use Foo::compute() instead")]]
///   void old_compute();
///   }
///
///   einsums::old_compute();   // EXPECT: deprecated
/// \endcode
class EinsumsDeprecatedApiCheck : public ::clang::tidy::ClangTidyCheck {
  public:
    EinsumsDeprecatedApiCheck(::llvm::StringRef name, ::clang::tidy::ClangTidyContext *context)
        : ::clang::tidy::ClangTidyCheck(name, context) {}
    void registerMatchers(::clang::ast_matchers::MatchFinder *finder) override;
    void check(::clang::ast_matchers::MatchFinder::MatchResult const &result) override;
};

} // namespace einsums::tidy
