//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

/// \file
/// EinsumsTidy module entry point.
///
/// Registers every Einsums-specific check under the ``einsums-*`` prefix.
/// The static ``ClangTidyModuleRegistry::Add<>`` instance at file scope is
/// what makes this module discoverable when ``clang-tidy --load=`` pulls it
/// in: dlopen'ing the plugin runs the static initializer, the module
/// registers itself with ``ClangTidyModuleRegistry``, and clang-tidy
/// thereafter treats our checks as if they were built in.

#include <clang-tidy/ClangTidyModule.h>

#include "EinsumsAllocatingInLoopCheck.hpp"
#include "EinsumsCatch2TensorFixtureLeakCheck.hpp"
#include "EinsumsCgCallOutsideCaptureCheck.hpp"
#include "EinsumsDeprecatedApiCheck.hpp"
#include "EinsumsDuplicateOutputIndexCheck.hpp"
#include "EinsumsMismatchedValueTypeCheck.hpp"
#include "EinsumsMixedIndexModeCheck.hpp"
#include "EinsumsNoLinkIndexCheck.hpp"
#include "EinsumsOutputAliasingInputCheck.hpp"
#include "EinsumsOutputNotInInputsCheck.hpp"
#include "EinsumsPreferCgFromTensorAlgebraCheck.hpp"
#include "EinsumsRankMismatchCheck.hpp"
#include "EinsumsRedundantPermuteCheck.hpp"
#include "EinsumsRuntimeSpecCheck.hpp"
#include "EinsumsTensorByValueParamCheck.hpp"
#include "EinsumsTensorViewFromTemporaryCheck.hpp"

namespace einsums::tidy {

class EinsumsTidyModule : public ::clang::tidy::ClangTidyModule {
  public:
    void addCheckFactories(::clang::tidy::ClangTidyCheckFactories &factories) override {
        factories.registerCheck<EinsumsRedundantPermuteCheck>("einsums-redundant-permute");
        factories.registerCheck<EinsumsOutputNotInInputsCheck>("einsums-output-not-in-inputs");
        factories.registerCheck<EinsumsNoLinkIndexCheck>("einsums-no-link-index");
        factories.registerCheck<EinsumsDuplicateOutputIndexCheck>("einsums-duplicate-output-index");
        factories.registerCheck<EinsumsMixedIndexModeCheck>("einsums-mixed-index-mode");
        factories.registerCheck<EinsumsRuntimeSpecCheck>("einsums-runtime-spec");
        factories.registerCheck<EinsumsTensorByValueParamCheck>("einsums-tensor-by-value-param");
        factories.registerCheck<EinsumsTensorViewFromTemporaryCheck>("einsums-tensor-view-from-temporary");
        factories.registerCheck<EinsumsOutputAliasingInputCheck>("einsums-output-aliasing-input");
        factories.registerCheck<EinsumsMismatchedValueTypeCheck>("einsums-mismatched-value-type");
        factories.registerCheck<EinsumsRankMismatchCheck>("einsums-rank-mismatch");
        factories.registerCheck<EinsumsAllocatingInLoopCheck>("einsums-allocating-in-loop");
        factories.registerCheck<EinsumsCgCallOutsideCaptureCheck>("einsums-cg-call-outside-capture");
        factories.registerCheck<EinsumsPreferCgFromTensorAlgebraCheck>("einsums-prefer-cg-from-tensoralgebra");
        factories.registerCheck<EinsumsDeprecatedApiCheck>("einsums-deprecated-api");
        factories.registerCheck<EinsumsCatch2TensorFixtureLeakCheck>("einsums-catch2-tensor-fixture-leak");
    }
};

} // namespace einsums::tidy

// Static registration — runs at .dylib/.so load time. The argument names
// here are the module name (used in clang-tidy's error messages when a
// module fails to load) and a one-line description.
static ::clang::tidy::ClangTidyModuleRegistry::Add<einsums::tidy::EinsumsTidyModule>
    einsums_tidy_module_registration("einsums-module", "Einsums-specific clang-tidy checks.");

// Force-link symbol so the module can't be dropped by --gc-sections or
// equivalent dead-stripping. Mirrors the pattern in clang-tidy's own
// in-tree modules (e.g. ``LLVMModule.cpp``'s ``LLVMModuleAnchorSource``).
int volatile EinsumsTidyModuleAnchorSource = 0;
