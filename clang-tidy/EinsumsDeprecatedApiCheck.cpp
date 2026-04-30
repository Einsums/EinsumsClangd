//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

#include "EinsumsDeprecatedApiCheck.hpp"

#include <clang/AST/ASTContext.h>
#include <clang/AST/Attr.h>
#include <clang/AST/Decl.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <string>

namespace einsums::tidy {

using namespace ::clang;
using namespace ::clang::ast_matchers;

namespace {

/// True when ``decl``'s fully-qualified name lives under ``einsums::``.
/// Catches ``einsums::Foo``, ``einsums::tensor_algebra::bar``, etc.
/// Excludes stdlib / system deprecations (``std::``, anonymous namespaces,
/// global-scope) so this check stays a project-specific lens — Clang's
/// own ``-Wdeprecated-declarations`` continues to handle those.
bool is_einsums_symbol(NamedDecl const *decl) {
    if (decl == nullptr)
        return false;
    auto const qualified = decl->getQualifiedNameAsString();
    // Match top-level ``einsums::`` plus the convention of nested namespaces;
    // a substring search is enough since we only care about the prefix.
    return qualified.rfind("einsums::", 0) == 0;
}

/// Pull the deprecation message off a declaration's ``[[deprecated(...)]]``
/// attribute, if present. Returns the empty string for "deprecated without
/// message" (still a valid attribute spelling).
std::string deprecation_message(NamedDecl const *decl) {
    if (decl == nullptr)
        return {};
    if (auto const *attr = decl->getAttr<DeprecatedAttr>())
        return attr->getMessage().str();
    return {};
}

/// Most declarations carry their deprecation on themselves; a few (e.g. an
/// instantiated function template) inherit it from the primary template or
/// a redeclaration. Walk those alternates so we still detect the attribute.
NamedDecl const *find_deprecated_origin(NamedDecl const *decl) {
    if (decl == nullptr)
        return nullptr;
    if (decl->hasAttr<DeprecatedAttr>())
        return decl;
    // Function-template instantiations don't always carry the attribute on
    // the instantiation; check the primary template too.
    if (auto const *fd = dyn_cast<FunctionDecl>(decl)) {
        if (auto const *primary = fd->getPrimaryTemplate()) {
            if (auto const *templated = primary->getTemplatedDecl(); templated != nullptr && templated->hasAttr<DeprecatedAttr>())
                return templated;
        }
        // Walk redeclarations — sometimes the attribute lives on a forward decl.
        for (auto const *redecl : fd->redecls()) {
            if (redecl->hasAttr<DeprecatedAttr>())
                return redecl;
        }
    }
    return nullptr;
}

} // namespace

void EinsumsDeprecatedApiCheck::registerMatchers(MatchFinder *finder) {
    // Match every call. Filtering happens in the body — the matcher API
    // has ``isDeprecated()`` for some node kinds but it's inconsistent
    // across decl types, and we want to handle template-instantiated
    // forms that need the alternate-decl walk.
    finder->addMatcher(callExpr().bind("call"), this);
    // Member-access deprecations: ``obj.deprecated_field``. Same pattern.
    finder->addMatcher(memberExpr().bind("member"), this);
    // Type-name deprecations land here: ``DeprecatedType x;`` shows up as
    // a ``typeLoc`` referencing the deprecated decl. Skipping those for
    // now keeps the implementation small; calls + member access cover the
    // vast majority of Einsums deprecations in practice.
}

void EinsumsDeprecatedApiCheck::check(MatchFinder::MatchResult const &result) {
    NamedDecl const *referenced = nullptr;
    SourceLocation   loc;

    if (auto const *call = result.Nodes.getNodeAs<CallExpr>("call")) {
        referenced = call->getDirectCallee();
        loc        = call->getBeginLoc();
    } else if (auto const *member = result.Nodes.getNodeAs<MemberExpr>("member")) {
        referenced = dyn_cast<NamedDecl>(member->getMemberDecl());
        loc        = member->getMemberLoc();
    }
    if (referenced == nullptr)
        return;
    if (!is_einsums_symbol(referenced))
        return;

    auto const *origin = find_deprecated_origin(referenced);
    if (origin == nullptr)
        return;

    auto const message = deprecation_message(origin);
    if (message.empty()) {
        diag(loc, "%0 is deprecated.") << referenced;
    } else {
        diag(loc, "%0 is deprecated: %1") << referenced << message;
    }
}

} // namespace einsums::tidy
