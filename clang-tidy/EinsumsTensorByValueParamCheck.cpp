//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

#include "EinsumsTensorByValueParamCheck.hpp"

#include <clang/AST/ASTContext.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>

#include "EinsumsTidyTypeUtils.hpp"

namespace einsums::tidy {

using namespace ::clang;
using namespace ::clang::ast_matchers;

void EinsumsTensorByValueParamCheck::registerMatchers(MatchFinder *finder) {
    // Bind every parameter and filter in the body — the AST-matcher API
    // for "this type's class template is one of {Tensor, BlockTensor,
    // TiledTensor} but not TensorView" gets verbose quickly. The check()
    // body has the helpers we need anyway.
    finder->addMatcher(parmVarDecl().bind("param"), this);
}

void EinsumsTensorByValueParamCheck::check(MatchFinder::MatchResult const &result) {
    auto const *param = result.Nodes.getNodeAs<ParmVarDecl>("param");
    if (param == nullptr)
        return;

    QualType const qt = param->getType();
    // By-value means the parameter type itself is the tensor — not a
    // pointer, not a reference. Skip both before classifying.
    if (qt->isReferenceType() || qt->isPointerType())
        return;
    if (!is_tensor_like(qt))
        return;
    // ``TensorView`` is the explicit non-owning handle: passing it by
    // value is the right call. Only the owning containers earn the warning.
    if (is_tensor_view(qt))
        return;

    // Skip parameters of compiler-generated functions (defaulted ctors,
    // ABI helpers) so we don't fire on noise the user can't fix.
    if (auto const *fd = ::clang::dyn_cast<FunctionDecl>(param->getDeclContext()); fd != nullptr) {
        if (fd->isImplicit() || fd->isDefaulted())
            return;
    }

    diag(param->getLocation(), "parameter %0 of tensor type taken by value — every call copies the entire "
                               "underlying buffer. Take it by ``const &`` (read-only) or by ``TensorView`` "
                               "(if the callee only needs a non-owning view).")
        << param;
}

} // namespace einsums::tidy
