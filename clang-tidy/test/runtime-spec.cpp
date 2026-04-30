//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

// Smoke test for einsums-runtime-spec.

namespace cg {
template <typename T, int N>
struct Tensor {};

template <typename CType, typename AType, typename BType>
void einsum(char const *spec, CType *C, AType const &A, BType const &B);

template <typename CType, typename AType>
void permute(char const *spec, CType *C, AType const &A);
} // namespace cg

extern char const *get_spec();
extern char const *some_spec_var;

void einsum_with_variable_should_warn() {
    cg::Tensor<double, 2> A;
    cg::Tensor<double, 2> B;
    cg::Tensor<double, 2> C;
    cg::einsum(some_spec_var, &C, A, B); // EXPECT: not a string literal
}

void einsum_with_function_call_should_warn() {
    cg::Tensor<double, 2> A;
    cg::Tensor<double, 2> B;
    cg::Tensor<double, 2> C;
    cg::einsum(get_spec(), &C, A, B); // EXPECT: not a string literal
}

void permute_with_variable_should_warn() {
    cg::Tensor<double, 2> A;
    cg::Tensor<double, 2> C;
    cg::permute(some_spec_var, &C, A); // EXPECT: not a string literal
}

void einsum_with_literal_should_not_warn() {
    cg::Tensor<double, 2> A;
    cg::Tensor<double, 2> B;
    cg::Tensor<double, 2> C;
    cg::einsum("ij <- ik;kj", &C, A, B); // OK
}
