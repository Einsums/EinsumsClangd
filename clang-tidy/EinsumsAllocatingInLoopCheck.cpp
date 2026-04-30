//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

#include "EinsumsAllocatingInLoopCheck.hpp"

#include <clang/AST/ASTContext.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>

#include "EinsumsTidyTypeUtils.hpp"

namespace einsums::tidy {

using namespace ::clang;
using namespace ::clang::ast_matchers;

void EinsumsAllocatingInLoopCheck::registerMatchers(MatchFinder *finder) {
    // Match every var declared inside any loop body. Type-filter happens in
    // ``check``: the matcher API for "owning tensor (anything in our family
    // except ``TensorView``)" is awkward enough to write that filtering in
    // C++ is clearer.
    //
    // The four loop kinds: ``for``, ``while``, range-based ``for``, and
    // ``do``. ``cxxForRangeStmt`` is the C++11+ range-based form; classic
    // ``forStmt`` covers the index-based one.
    finder->addMatcher(varDecl(hasAncestor(stmt(anyOf(forStmt(), whileStmt(), cxxForRangeStmt(), doStmt())).bind("loop"))).bind("var"),
                       this);
}

void EinsumsAllocatingInLoopCheck::check(MatchFinder::MatchResult const &result) {
    auto const *var  = result.Nodes.getNodeAs<VarDecl>("var");
    auto const *loop = result.Nodes.getNodeAs<Stmt>("loop");
    if (var == nullptr || loop == nullptr)
        return;

    // Owning tensor types only — TensorView is a non-owning handle and
    // re-creating one per iteration is cheap.
    QualType const qt = var->getType();
    if (qt->isReferenceType() || qt->isPointerType())
        return; // a reference / pointer rebinds, doesn't allocate
    if (!is_tensor_like(qt) || is_tensor_view(qt))
        return;
    // Skip template-dependent declarations — we can't reason about the
    // cost without knowing the concrete type. The check fires on each
    // instantiation that uses an owning tensor.
    if (qt->isDependentType())
        return;

    // The matcher's ``hasAncestor`` walks all the way up, which means a
    // var declared *outside* a loop but in the same function still
    // matches when the loop comes later in the same compound statement —
    // because ``hasAncestor`` finds the function body's ``CompoundStmt``,
    // and the loop is a descendant of that. We need the var to actually
    // be inside the loop body. Verify by walking parents until we hit
    // the loop's own body.
    // ``ASTContext::getParents`` is non-const, so we need a non-const
    // reference here. ``getParents`` also requires a ``DynTypedNode``
    // for ``Decl`` (only ``Stmt`` has a direct templated overload).
    ::clang::ASTContext &ctx       = *result.Context;
    auto const           is_inside = [&]() {
        for (auto p = ctx.getParents(::clang::DynTypedNode::create(*var)); !p.empty(); p = ctx.getParents(p[0])) {
            if (auto const *s = p[0].get<Stmt>(); s == loop)
                return true;
        }
        return false;
    }();
    if (!is_inside)
        return;

    diag(var->getLocation(), "owning tensor %0 declared inside a loop — every iteration re-runs the "
                             "constructor, allocating a new buffer and freeing the old one. Hoist the "
                             "declaration out of the loop and reset (e.g. ``.zero()``) at the top of "
                             "each iteration.")
        << var;
}

} // namespace einsums::tidy
