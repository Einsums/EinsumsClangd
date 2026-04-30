//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

#include "EinsumsTensorViewFromTemporaryCheck.hpp"

#include <clang/AST/ASTContext.h>
#include <clang/AST/Expr.h>
#include <clang/AST/ExprCXX.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>

#include "EinsumsTidyTypeUtils.hpp"

namespace einsums::tidy {

using namespace ::clang;
using namespace ::clang::ast_matchers;

namespace {

/// Walk the initializer of a ``TensorView`` variable and decide whether
/// it's binding to a temporary owning tensor. Conservative: returns
/// ``true`` only for clearly-temporary expressions (function calls
/// returning by value, inline ``Tensor{...}`` constructions). A bound
/// variable, a reference, or a dereferenced pointer all return ``false``
/// so we don't false-positive on legitimate aliases.
bool initializer_is_temporary_tensor(Expr const *init) {
    if (init == nullptr)
        return false;
    init = init->IgnoreParenImpCasts();

    // ``CXXConstructExpr`` for ``TensorView{Tensor{...}}`` — peel one
    // layer of construction and check the inner expression.
    if (auto const *ce = dyn_cast<CXXConstructExpr>(init); ce != nullptr) {
        if (ce->getNumArgs() == 0)
            return false;
        init = ce->getArg(0)->IgnoreParenImpCasts();
    }
    // Materialize-temporary nodes wrap rvalues that get a temporary
    // address. Their presence is the strongest signal we have for
    // "this expression is going to be destroyed at end-of-statement".
    if (isa<MaterializeTemporaryExpr>(init))
        return is_tensor_like(init->getType());
    // A direct function-call expression returning a ``Tensor`` by value
    // is the canonical case. Type check confirms the return is owning.
    if (auto const *call = dyn_cast<CallExpr>(init); call != nullptr) {
        QualType const rt = call->getType();
        if (rt->isReferenceType() || rt->isPointerType())
            return false;
        return is_tensor_like(rt) && !is_tensor_view(rt);
    }
    // ``Tensor{a, b}`` literal construction — also a temporary.
    if (auto const *cte = dyn_cast<CXXTemporaryObjectExpr>(init); cte != nullptr)
        return is_tensor_like(cte->getType()) && !is_tensor_view(cte->getType());
    return false;
}

} // namespace

void EinsumsTensorViewFromTemporaryCheck::registerMatchers(MatchFinder *finder) {
    // Match every ``varDecl`` and filter by type / initializer in the
    // body. Trying to express "var of TensorView whose initializer is a
    // CallExpr returning by-value Tensor" purely in matchers gets gnarly.
    finder->addMatcher(varDecl(hasInitializer(expr())).bind("var"), this);
}

void EinsumsTensorViewFromTemporaryCheck::check(MatchFinder::MatchResult const &result) {
    auto const *var = result.Nodes.getNodeAs<VarDecl>("var");
    if (var == nullptr)
        return;
    if (!is_tensor_view(var->getType()))
        return;

    Expr const *init = var->getInit();
    if (init == nullptr)
        return;

    if (!initializer_is_temporary_tensor(init))
        return;

    diag(var->getLocation(), "TensorView %0 is initialized from a temporary owning tensor — the temporary "
                             "is destroyed at the end of this statement, leaving the view dangling. Bind "
                             "the tensor to a named variable first, then construct the view from it.")
        << var;
}

} // namespace einsums::tidy
