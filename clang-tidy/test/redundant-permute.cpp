//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

// Smoke test for einsums-redundant-permute. We declare a stub ``permute``
// that mirrors the real ComputeGraph signature (close enough for the
// matcher) so this file compiles standalone — no Einsums include path
// required for the check to fire.
//
// Run via the test/run_test.sh wrapper, which invokes:
//   clang-tidy --load=<EinsumsTidy.dylib> --checks='-*,einsums-*' <this-file>
// and greps stdout for the expected diagnostic.

namespace cg {

template <typename T, int N>
struct Tensor {};

template <typename CType, typename AType>
void permute(char const *spec, CType *C, AType const &A);

} // namespace cg

void identity_should_warn() {
    cg::Tensor<double, 2> A;
    cg::Tensor<double, 2> C;
    cg::permute("ij <- ij", &C, A); // EXPECT: redundant permute
}

void real_transpose_should_not_warn() {
    cg::Tensor<double, 2> A;
    cg::Tensor<double, 2> C;
    cg::permute("ji <- ij", &C, A); // OK
}

void identity_numpy_notation_should_warn() {
    cg::Tensor<double, 3> A;
    cg::Tensor<double, 3> C;
    cg::permute("ijk -> ijk", &C, A); // EXPECT: redundant permute
}

void identity_multichar_should_warn() {
    cg::Tensor<double, 2> A;
    cg::Tensor<double, 2> C;
    cg::permute("mu,nu <- mu,nu", &C, A); // EXPECT: redundant permute
}
