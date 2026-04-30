//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

// Smoke test for einsums-cg-call-outside-capture.

namespace einsums {
namespace compute_graph {
template <typename T, int N>
struct Tensor {};

struct CaptureGuard {
    CaptureGuard();
    ~CaptureGuard();
};

template <typename CType, typename AType, typename BType>
void einsum(char const *spec, CType *C, AType const &A, BType const &B);
} // namespace compute_graph
} // namespace einsums

void no_guard_should_warn() {
    einsums::compute_graph::Tensor<double, 2> A;
    einsums::compute_graph::Tensor<double, 2> B;
    einsums::compute_graph::Tensor<double, 2> C;
    einsums::compute_graph::einsum("ij <- ik;kj", &C, A, B); // EXPECT: no CaptureGuard in scope
}

void with_guard_should_not_warn() {
    einsums::compute_graph::Tensor<double, 2> A;
    einsums::compute_graph::Tensor<double, 2> B;
    einsums::compute_graph::Tensor<double, 2> C;
    einsums::compute_graph::CaptureGuard      g;
    einsums::compute_graph::einsum("ij <- ik;kj", &C, A, B); // OK
}

void with_outer_guard_should_not_warn() {
    einsums::compute_graph::CaptureGuard      g;
    einsums::compute_graph::Tensor<double, 2> A;
    einsums::compute_graph::Tensor<double, 2> B;
    einsums::compute_graph::Tensor<double, 2> C;
    {
        einsums::compute_graph::einsum("ij <- ik;kj", &C, A, B); // OK — guard from outer scope
    }
}
