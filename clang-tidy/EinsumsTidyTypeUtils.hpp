//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

#pragma once

/// \file
/// Type-introspection helpers shared by the Einsums type-aware checks.
///
/// Three things every such check ends up doing:
///   1. "Does this expression refer to a tensor?" — with `Tensor` /
///      `TensorView` / `BlockTensor` / `TiledTensor` all counting.
///   2. "What's its value type ``T``?" — the first template argument.
///   3. "What's its rank ``N``?" — the second template argument, an int.
///
/// We intentionally don't tie ourselves to ``einsums::`` namespace: users
/// commonly bring the types into scope with ``using``. Matching by
/// unqualified record name is sufficient because the check fires on
/// callsites of ``einsum``/``permute``, which already imply the
/// surrounding namespace.

#include <clang/AST/ASTContext.h>
#include <clang/AST/ASTTypeTraits.h> // clang::DynTypedNode
#include <clang/AST/DeclTemplate.h>
#include <clang/AST/Expr.h>
#include <clang/AST/ParentMapContext.h> // ASTContext::getParents
#include <clang/AST/Type.h>
#include <llvm/ADT/StringRef.h>
#include <optional>

namespace einsums::tidy {

/// Strip pointers, references, and cv-qualifiers; return the underlying
/// (value-type-shaped) ``QualType``. ``Tensor<T> *``, ``Tensor<T> const &``,
/// and ``Tensor<T>`` all collapse to ``Tensor<T>``.
inline ::clang::QualType strip_indirection(::clang::QualType qt) {
    while (true) {
        if (auto const *pt = qt->getAs<::clang::PointerType>()) {
            qt = pt->getPointeeType();
            continue;
        }
        if (qt->isReferenceType()) {
            qt = qt.getNonReferenceType();
            continue;
        }
        break;
    }
    return qt.getUnqualifiedType();
}

/// True when ``qt`` (after ``strip_indirection``) names one of the Einsums
/// tensor templates. Conservative on the name list — extending requires
/// adding the new family member here.
inline bool is_tensor_like(::clang::QualType qt) {
    qt             = strip_indirection(qt);
    auto const *rd = qt->getAsCXXRecordDecl();
    if (rd == nullptr)
        return false;
    auto const name = rd->getName();
    return name == "Tensor" || name == "TensorView" || name == "BlockTensor" || name == "TiledTensor";
}

/// True specifically when the type is a non-owning ``TensorView``. Used by
/// the dangling-view check, which is more permissive about the "owning"
/// tensors (those keep their data alive).
inline bool is_tensor_view(::clang::QualType qt) {
    qt             = strip_indirection(qt);
    auto const *rd = qt->getAsCXXRecordDecl();
    return rd != nullptr && rd->getName() == "TensorView";
}

/// Pull the first template type argument out of a tensor type — e.g.
/// ``T`` from ``Tensor<T, 2>``. Returns nullopt if ``qt`` isn't a class
/// template specialization or doesn't have a leading type arg (rare).
inline std::optional<::clang::QualType> tensor_value_type(::clang::QualType qt) {
    qt             = strip_indirection(qt);
    auto const *rd = qt->getAsCXXRecordDecl();
    if (rd == nullptr)
        return std::nullopt;
    auto const *spec = ::clang::dyn_cast<::clang::ClassTemplateSpecializationDecl>(rd);
    if (spec == nullptr)
        return std::nullopt;
    auto const &args = spec->getTemplateArgs();
    if (args.size() < 1 || args[0].getKind() != ::clang::TemplateArgument::Type)
        return std::nullopt;
    return args[0].getAsType().getUnqualifiedType();
}

/// Pull the rank ``N`` out of ``Tensor<T, N>``. Returns nullopt when the
/// rank is template-dependent (uninstantiated template) or not an integral
/// constant — both of which mean we can't reliably compare against a spec.
inline std::optional<int> tensor_rank(::clang::QualType qt) {
    qt             = strip_indirection(qt);
    auto const *rd = qt->getAsCXXRecordDecl();
    if (rd == nullptr)
        return std::nullopt;
    auto const *spec = ::clang::dyn_cast<::clang::ClassTemplateSpecializationDecl>(rd);
    if (spec == nullptr)
        return std::nullopt;
    auto const &args = spec->getTemplateArgs();
    if (args.size() < 2 || args[1].getKind() != ::clang::TemplateArgument::Integral)
        return std::nullopt;
    auto const &ap = args[1].getAsIntegral();
    if (ap.getActiveBits() > 31)
        return std::nullopt;
    return static_cast<int>(ap.getSExtValue());
}

/// Walk through unary ``&`` / parens / implicit casts to find the
/// underlying ``DeclRefExpr``. Used by aliasing checks that need to ask
/// "are these two arguments referring to the same variable?".
inline ::clang::DeclRefExpr const *peel_to_declref(::clang::Expr const *e) {
    if (e == nullptr)
        return nullptr;
    e = e->IgnoreParenImpCasts();
    if (auto const *uo = ::clang::dyn_cast<::clang::UnaryOperator>(e); uo != nullptr && uo->getOpcode() == ::clang::UO_AddrOf) {
        e = uo->getSubExpr()->IgnoreParenImpCasts();
    }
    return ::clang::dyn_cast<::clang::DeclRefExpr>(e);
}

/// Walk ``target``'s AST ancestors and return ``true`` if some enclosing
/// scope contains a ``CaptureGuard`` variable declaration that lexically
/// precedes ``target``. Used by the capture-scope checks to ask
/// "is this call participating in a graph capture?". The walk stops when
/// a ``FunctionDecl`` is reached — capture scope doesn't transcend
/// function boundaries.
///
/// Caveat: this handles direct ``CaptureGuard g(...)`` declarations only,
/// not aliased ones (``auto &alias = make_guard()``) or ones held in a
/// captured ``std::function`` object. Both are rare in practice; if they
/// crop up, switch the offending call to a sibling style or silence with
/// ``// NOLINT``.
inline bool is_inside_capture_guard_scope(::clang::Stmt const *target, ::clang::ASTContext &ctx) {
    if (target == nullptr)
        return false;
    ::clang::Stmt const  *prev    = target; ///< most recent Stmt we walked through
    ::clang::DynTypedNode current = ::clang::DynTypedNode::create(*target);
    while (true) {
        auto const parents = ctx.getParents(current);
        if (parents.empty())
            return false;
        ::clang::DynTypedNode const &parent = parents[0];

        // Function boundary: capture scopes don't cross into outer functions
        // (a guard declared in main() doesn't affect a free function it
        // calls). Stop the walk.
        if (parent.get<::clang::FunctionDecl>() != nullptr)
            return false;

        if (auto const *cs = parent.get<::clang::CompoundStmt>(); cs != nullptr) {
            // Iterate the compound's children up to (but not including) the
            // child we came from. Anything declared in those siblings is
            // lexically in scope at ``target``'s point.
            for (auto const *stmt : cs->body()) {
                if (stmt == prev)
                    break;
                auto const *ds = ::clang::dyn_cast<::clang::DeclStmt>(stmt);
                if (ds == nullptr)
                    continue;
                for (auto const *decl : ds->decls()) {
                    auto const *vd = ::clang::dyn_cast<::clang::VarDecl>(decl);
                    if (vd == nullptr)
                        continue;
                    auto qt        = vd->getType();
                    qt             = strip_indirection(qt);
                    auto const *rd = qt->getAsCXXRecordDecl();
                    if (rd != nullptr && rd->getName() == "CaptureGuard")
                        return true;
                }
            }
        }

        // Track the most recent Stmt we passed through so we can identify
        // "the child we came from" at the next CompoundStmt level.
        if (auto const *s = parent.get<::clang::Stmt>(); s != nullptr)
            prev = s;
        current = parent;
    }
}

} // namespace einsums::tidy
