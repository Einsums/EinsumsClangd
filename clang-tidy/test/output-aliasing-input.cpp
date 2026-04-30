//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

// Smoke test for einsums-output-aliasing-input.

namespace cg {
template <typename T, int N>
struct Tensor {};

template <typename CType, typename AType, typename BType>
void einsum(char const *spec, CType *C, AType const &A, BType const &B);

template <typename CType, typename AType>
void permute(char const *spec, CType *C, AType const &A);
} // namespace cg

void output_aliases_a_should_warn() {
    cg::Tensor<double, 2> A;
    cg::Tensor<double, 2> B;
    cg::einsum("ij <- ik;kj", &A, A, B); // EXPECT: aliases input
}

void output_aliases_b_should_warn() {
    cg::Tensor<double, 2> A;
    cg::Tensor<double, 2> B;
    cg::einsum("ij <- ik;kj", &B, A, B); // EXPECT: aliases input
}

void permute_aliases_should_warn() {
    cg::Tensor<double, 2> A;
    cg::permute("ji <- ij", &A, A); // EXPECT: aliases input
}

void distinct_should_not_warn() {
    cg::Tensor<double, 2> A;
    cg::Tensor<double, 2> B;
    cg::Tensor<double, 2> C;
    cg::einsum("ij <- ik;kj", &C, A, B); // OK
}
