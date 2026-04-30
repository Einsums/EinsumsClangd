//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

// Smoke test for einsums-prefer-cg-from-tensoralgebra.

namespace einsums {
template <typename T, int N>
struct Tensor {};

namespace compute_graph {
struct CaptureGuard {
    CaptureGuard();
    ~CaptureGuard();
};
} // namespace compute_graph

namespace tensor_algebra {
template <typename CType, typename AType, typename BType>
void einsum(char const *spec, CType *C, AType const &A, BType const &B);
}
} // namespace einsums

void eager_inside_capture_should_warn() {
    einsums::Tensor<double, 2>           A;
    einsums::Tensor<double, 2>           B;
    einsums::Tensor<double, 2>           C;
    einsums::compute_graph::CaptureGuard g;
    einsums::tensor_algebra::einsum("ij <- ik;kj", &C, A, B); // EXPECT: prefer cg
}

void eager_outside_capture_should_not_warn() {
    einsums::Tensor<double, 2> A;
    einsums::Tensor<double, 2> B;
    einsums::Tensor<double, 2> C;
    einsums::tensor_algebra::einsum("ij <- ik;kj", &C, A, B); // OK — eager is the right call here
}
