//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

// Smoke test for einsums-mixed-index-mode.

namespace cg {
template <typename T, int N>
struct Tensor {};

template <typename CType, typename AType, typename BType>
void einsum(char const *spec, CType *C, AType const &A, BType const &B);

template <typename CType, typename AType>
void permute(char const *spec, CType *C, AType const &A);
} // namespace cg

void output_has_commas_inputs_dont_should_warn() {
    cg::Tensor<double, 2> A;
    cg::Tensor<double, 2> B;
    cg::Tensor<double, 3> C;
    cg::einsum("ij,k <- ik;kj", &C, A, B); // EXPECT: mixes comma-separated and single-char
}

void input_has_commas_others_dont_should_warn() {
    cg::Tensor<double, 2> A;
    cg::Tensor<double, 2> B;
    cg::Tensor<double, 2> C;
    cg::einsum("ij <- i,k;kj", &C, A, B); // EXPECT: mixes comma-separated and single-char
}

void permute_mixed_should_warn() {
    cg::Tensor<double, 2> A;
    cg::Tensor<double, 2> C;
    cg::permute("ij <- i,j", &C, A); // EXPECT: mixes comma-separated and single-char
}

void all_comma_should_not_warn() {
    cg::Tensor<double, 2> A;
    cg::Tensor<double, 2> B;
    cg::Tensor<double, 2> C;
    cg::einsum("i,j <- i,k;k,j", &C, A, B); // OK
}

void all_single_char_should_not_warn() {
    cg::Tensor<double, 2> A;
    cg::Tensor<double, 2> B;
    cg::Tensor<double, 2> C;
    cg::einsum("ij <- ik;kj", &C, A, B); // OK
}
