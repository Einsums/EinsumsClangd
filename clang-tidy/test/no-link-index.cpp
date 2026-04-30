//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

// Smoke test for einsums-no-link-index.

namespace cg {
template <typename T, int N>
struct Tensor {};

template <typename CType, typename AType, typename BType>
void einsum(char const *spec, CType *C, AType const &A, BType const &B);
} // namespace cg

void no_shared_index_should_warn() {
    cg::Tensor<double, 1> A;
    cg::Tensor<double, 1> B;
    cg::Tensor<double, 2> C;
    cg::einsum("ij <- i;j", &C, A, B); // EXPECT: silent outer product
}

void shared_index_should_not_warn() {
    cg::Tensor<double, 2> A;
    cg::Tensor<double, 2> B;
    cg::Tensor<double, 2> C;
    cg::einsum("ij <- ik;kj", &C, A, B); // OK — 'k' is contracted
}
