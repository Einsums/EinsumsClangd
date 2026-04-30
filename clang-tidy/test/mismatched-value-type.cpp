//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

// Smoke test for einsums-mismatched-value-type.

namespace cg {
template <typename T, int N>
struct Tensor {};

template <typename CType, typename AType, typename BType>
void einsum(char const *spec, CType *C, AType const &A, BType const &B);
} // namespace cg

void mixed_precision_should_warn() {
    cg::Tensor<float, 2>  A;
    cg::Tensor<double, 2> B;
    cg::Tensor<double, 2> C;
    cg::einsum("ij <- ik;kj", &C, A, B); // EXPECT: mismatched value types
}

void all_double_should_not_warn() {
    cg::Tensor<double, 2> A;
    cg::Tensor<double, 2> B;
    cg::Tensor<double, 2> C;
    cg::einsum("ij <- ik;kj", &C, A, B); // OK
}
