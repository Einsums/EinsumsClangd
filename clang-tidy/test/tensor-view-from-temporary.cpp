//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

// Smoke test for einsums-tensor-view-from-temporary.

namespace einsums {
template <typename T, int N>
struct Tensor {};
template <typename T, int N>
struct TensorView {
    TensorView() = default;
    TensorView(Tensor<T, N> const &);
};
} // namespace einsums

einsums::Tensor<double, 2> make_tensor();

void view_from_function_call_should_warn() {
    einsums::TensorView<double, 2> v = make_tensor(); // EXPECT: dangling view
}

void view_from_named_tensor_should_not_warn() {
    einsums::Tensor<double, 2>     t = make_tensor();
    einsums::TensorView<double, 2> v = t; // OK — t outlives v
}
