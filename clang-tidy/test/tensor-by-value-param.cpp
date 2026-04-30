//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

// Smoke test for einsums-tensor-by-value-param.

namespace einsums {
template <typename T, int N>
struct Tensor {};
template <typename T, int N>
struct TensorView {};
template <typename T, int N>
struct BlockTensor {};
} // namespace einsums

void by_value_should_warn(einsums::Tensor<double, 2> A) {
} // EXPECT: by value

void by_const_ref_should_not_warn(einsums::Tensor<double, 2> const &A) {
} // OK

void by_pointer_should_not_warn(einsums::Tensor<double, 2> *A) {
} // OK

void view_by_value_should_not_warn(einsums::TensorView<double, 2> A) {
} // OK — non-owning

void block_by_value_should_warn(einsums::BlockTensor<double, 3> A) {
} // EXPECT: by value
