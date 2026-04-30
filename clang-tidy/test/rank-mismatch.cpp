//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

// Smoke test for einsums-rank-mismatch.

namespace cg {
template <typename T, int N>
struct Tensor {};

template <typename CType, typename AType, typename BType>
void einsum(char const *spec, CType *C, AType const &A, BType const &B);

template <typename CType, typename AType>
void permute(char const *spec, CType *C, AType const &A);
} // namespace cg

void rank3_passed_to_rank2_spec_should_warn() {
    cg::Tensor<double, 3> A; // wrong rank
    cg::Tensor<double, 2> B;
    cg::Tensor<double, 2> C;
    cg::einsum("ij <- ik;kj", &C, A, B); // EXPECT: rank mismatch
}

void permute_rank_mismatch_should_warn() {
    cg::Tensor<double, 3> A;
    cg::Tensor<double, 2> C;
    cg::permute("ij <- ij", &C, A); // EXPECT: rank mismatch
}

void matching_ranks_should_not_warn() {
    cg::Tensor<double, 2> A;
    cg::Tensor<double, 2> B;
    cg::Tensor<double, 2> C;
    cg::einsum("ij <- ik;kj", &C, A, B); // OK
}
