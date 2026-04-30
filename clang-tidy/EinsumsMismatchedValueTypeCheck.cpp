//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

#include "EinsumsMismatchedValueTypeCheck.hpp"

#include <clang/AST/ASTContext.h>
#include <clang/AST/Expr.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>

#include "EinsumsTidyMatchers.hpp"
#include "EinsumsTidyTypeUtils.hpp"

namespace einsums::tidy {

using namespace ::clang;
using namespace ::clang::ast_matchers;

namespace {

/// Indices of the tensor arguments in the call's argument list, by arity.
/// Same logic as the aliasing check; couldn't easily share without more
/// scaffolding so we reproduce it here.
std::vector<int> tensor_arg_indices(CallExpr const *call) {
    switch (call->getNumArgs()) {
    case 4:
        return {1, 2, 3};
    case 6:
        return {2, 4, 5};
    case 3:
        return {1, 2};
    case 5:
        return {2, 4};
    default:
        return {};
    }
}

} // namespace

void EinsumsMismatchedValueTypeCheck::registerMatchers(MatchFinder *finder) {
    finder->addMatcher(einsum_call_with_literal_spec(), this);
    finder->addMatcher(permute_call_with_literal_spec(), this);
}

void EinsumsMismatchedValueTypeCheck::check(MatchFinder::MatchResult const &result) {
    auto const *call = result.Nodes.getNodeAs<CallExpr>("call");
    if (call == nullptr)
        return;

    auto const indices = tensor_arg_indices(call);
    if (indices.empty())
        return;

    QualType                                               reference;
    int                                                    reference_idx = -1;
    ::clang::ast_matchers::MatchFinder::MatchResult const &r             = result;
    (void)r; // silence unused warning if compiled without optional checks

    for (int const idx : indices) {
        auto const *arg = call->getArg(idx);
        auto const  vt  = tensor_value_type(arg->getType());
        if (!vt.has_value())
            return; // some arg's value-type couldn't be deduced — bail rather than guess
        if (reference.isNull()) {
            reference     = *vt;
            reference_idx = idx;
            continue;
        }
        if (vt->getCanonicalType() != reference.getCanonicalType()) {
            // Same call site but two distinct ValueTypes — fire once with both
            // names so the user can see which two operands disagree.
            diag(call->getBeginLoc(), "tensor operands have mismatched value types — argument %0 is %1 while "
                                      "argument %2 is %3. Either convert one operand explicitly or fix the "
                                      "underlying type so the contraction stays in a single precision.")
                << reference_idx << reference.getAsString() << idx << vt->getAsString();
            return;
        }
    }
}

} // namespace einsums::tidy
