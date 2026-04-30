//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

#include "EinsumsCatch2TensorFixtureLeakCheck.hpp"

#include <clang/AST/ASTContext.h>
#include <clang/AST/Decl.h>
#include <clang/AST/Expr.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/ASTMatchers/ASTMatchFinder.h>
#include <clang/ASTMatchers/ASTMatchers.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Lex/Lexer.h>
#include <vector>

#include "EinsumsTidyTypeUtils.hpp"

namespace einsums::tidy {

using namespace ::clang;
using namespace ::clang::ast_matchers;

namespace {

/// Macro names that "introduce" a Catch2 test body. A function declared
/// inside the expansion of one of these is a test fixture for our
/// purposes.
bool is_test_case_macro(::llvm::StringRef name) {
    return name == "TEST_CASE" || name == "EINSUMS_TEST_CASE" || name == "TEMPLATE_TEST_CASE" || name == "TEMPLATE_PRODUCT_TEST_CASE" ||
           name == "TEST_CASE_METHOD" || name == "SCENARIO";
}

/// Macros that count as "the test observed this value". A ``DeclRefExpr``
/// reached during the expansion of any of these is treated as an
/// assertion / capture and silences the check.
///
/// We accept INFO/CAPTURE/SECTION-flavour macros too: even when they don't
/// formally assert, they prove the test author did *something* with the
/// value. The intent of this check is to catch dead computations, not
/// to enforce a particular assertion style.
bool is_assertion_or_capture_macro(::llvm::StringRef name) {
    if (name.starts_with("REQUIRE") || name.starts_with("CHECK"))
        return true;
    return name == "INFO" || name == "CAPTURE" || name == "WARN" || name == "FAIL" || name == "SUCCEED" || name == "STATIC_REQUIRE" ||
           name == "STATIC_CHECK" || name == "SECTION" || name == "DYNAMIC_SECTION" || name == "GIVEN" || name == "WHEN" ||
           name == "THEN" || name == "AND_GIVEN" || name == "AND_WHEN" || name == "AND_THEN";
}

/// Walk a source location's macro expansion stack and ask: did any macro
/// in the chain match ``predicate``? Catch2's ``REQUIRE(close(C, E))``
/// shows up as the inner expression living inside a chain that includes
/// ``REQUIRE`` — we want to walk that chain, not just look at the
/// immediately-spelled name.
template <typename Pred>
bool any_expansion_macro_matches(SourceLocation loc, SourceManager const &sm, ::clang::LangOptions const &lo, Pred &&pred) {
    while (loc.isValid() && loc.isMacroID()) {
        auto const name = ::clang::Lexer::getImmediateMacroName(loc, sm, lo);
        if (!name.empty() && pred(name))
            return true;
        loc = sm.getImmediateMacroCallerLoc(loc);
    }
    return false;
}

/// True if the function decl was *generated* by a Catch2-style test-case
/// macro. We consult the function's source location and check whether
/// any macro in its expansion stack is one of the test-case introducers.
bool is_test_case_function(FunctionDecl const *fd, SourceManager const &sm, ::clang::LangOptions const &lo) {
    if (fd == nullptr)
        return false;
    return any_expansion_macro_matches(fd->getLocation(), sm, lo, [](::llvm::StringRef n) { return is_test_case_macro(n); });
}

/// Walk a function body and collect every reference to any of the
/// supplied ``VarDecl``s. For each reference, record whether it was
/// inside a Catch2 assertion / capture macro expansion.
class RefVisitor : public ::clang::RecursiveASTVisitor<RefVisitor> {
  public:
    RefVisitor(SourceManager const &sm, ::clang::LangOptions const &lo, llvm::DenseSet<VarDecl const *> &watching,
               llvm::DenseSet<VarDecl const *> &observed)
        : sm(sm), lo(lo), watching(watching), observed(observed) {}

    bool VisitDeclRefExpr(DeclRefExpr *ref) {
        auto *vd = ::clang::dyn_cast<VarDecl>(ref->getDecl());
        if (vd == nullptr)
            return true;
        if (!watching.contains(vd))
            return true;
        if (any_expansion_macro_matches(ref->getBeginLoc(), sm, lo, [](::llvm::StringRef n) { return is_assertion_or_capture_macro(n); })) {
            observed.insert(vd);
        }
        return true;
    }

  private:
    SourceManager const             &sm;
    ::clang::LangOptions const      &lo;
    llvm::DenseSet<VarDecl const *> &watching;
    llvm::DenseSet<VarDecl const *> &observed;
};

} // namespace

void EinsumsCatch2TensorFixtureLeakCheck::registerMatchers(MatchFinder *finder) {
    // The check is expressed as a single match on the enclosing test
    // function: collect its tensor-typed locals, walk the body to see
    // which ever land inside a Catch2 macro, and report the rest.
    // A per-VarDecl matcher would re-walk the body for every variable,
    // O(N^2) on large fixtures — one walk per function is enough.
    finder->addMatcher(functionDecl(isDefinition()).bind("fn"), this);
}

void EinsumsCatch2TensorFixtureLeakCheck::check(MatchFinder::MatchResult const &result) {
    auto const *fn = result.Nodes.getNodeAs<FunctionDecl>("fn");
    if (fn == nullptr || !fn->hasBody())
        return;
    auto const &sm = result.Context->getSourceManager();
    auto const &lo = result.Context->getLangOpts();
    if (!is_test_case_function(fn, sm, lo))
        return;

    // Gather candidate VarDecls: tensor-like, declared in the body.
    llvm::DenseSet<VarDecl const *> watching;
    std::vector<VarDecl const *>    in_order;
    for (auto const *decl : fn->decls()) {
        // ``decls()`` here only returns the function's parameters; locals
        // live inside the body and need their own walk. We use a small
        // visitor below so we get both vars and the assertion-macro
        // introspection in one pass.
        (void)decl;
    }

    struct Collector : public ::clang::RecursiveASTVisitor<Collector> {
        llvm::DenseSet<VarDecl const *> &watching;
        std::vector<VarDecl const *>    &in_order;

        Collector(llvm::DenseSet<VarDecl const *> &w, std::vector<VarDecl const *> &v) : watching(w), in_order(v) {}

        bool VisitVarDecl(VarDecl *vd) {
            if (vd == nullptr || vd->isImplicit())
                return true;
            if (vd->getKind() != Decl::Var) // skip parameters / template-params
                return true;
            QualType const qt = vd->getType();
            if (!is_tensor_like(qt) || is_tensor_view(qt))
                return true;
            if (watching.insert(vd).second)
                in_order.push_back(vd);
            return true;
        }
    };
    Collector collector(watching, in_order);
    collector.TraverseStmt(fn->getBody());

    if (watching.empty())
        return;

    // Walk the body once, recording which watched vars appear inside an
    // assertion/capture macro expansion.
    llvm::DenseSet<VarDecl const *> observed;
    RefVisitor                      visitor(sm, lo, watching, observed);
    visitor.TraverseStmt(fn->getBody());

    for (auto const *vd : in_order) {
        if (observed.contains(vd))
            continue;
        diag(vd->getLocation(), "tensor %0 is constructed in this test case but never reaches a Catch2 "
                                "assertion (REQUIRE/CHECK/INFO/CAPTURE/SECTION). The test runs without "
                                "observing the tensor — a regression in the computation will pass silently.")
            << vd;
    }
}

} // namespace einsums::tidy
