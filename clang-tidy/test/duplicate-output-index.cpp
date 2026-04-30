//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

// Smoke test for einsums-duplicate-output-index.

namespace cg {
template <typename T, int N>
struct Tensor {};

template <typename CType, typename AType, typename BType>
void einsum(char const *spec, CType *C, AType const &A, BType const &B);
} // namespace cg

void self_trace_in_output_should_warn() {
    cg::Tensor<double, 2> A;
    cg::Tensor<double, 2> B;
    cg::Tensor<double, 2> C;
    cg::einsum("ii <- ij;jk", &C, A, B); // EXPECT: appears more than once in C
}

void distinct_output_indices_should_not_warn() {
    cg::Tensor<double, 2> A;
    cg::Tensor<double, 2> B;
    cg::Tensor<double, 2> C;
    cg::einsum("ij <- ik;kj", &C, A, B); // OK
}
