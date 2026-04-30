//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

// Smoke test for einsums-allocating-in-loop.

namespace einsums {
template <typename T, int N>
struct Tensor {};
template <typename T, int N>
struct TensorView {};
} // namespace einsums

void for_loop_should_warn() {
    for (int i = 0; i < 10; ++i) {
        einsums::Tensor<double, 2> tmp; // EXPECT: declared inside a loop
    }
}

void while_loop_should_warn() {
    int i = 0;
    while (i < 10) {
        einsums::Tensor<double, 3> tmp; // EXPECT: declared inside a loop
        ++i;
    }
}

void range_for_should_warn() {
    int xs[] = {1, 2, 3};
    for (auto x : xs) {
        einsums::Tensor<double, 2> tmp; // EXPECT: declared inside a loop
        (void)x;
    }
}

void view_in_loop_should_not_warn() {
    for (int i = 0; i < 10; ++i) {
        einsums::TensorView<double, 2> v; // OK — non-owning
    }
}

void declared_outside_loop_should_not_warn() {
    einsums::Tensor<double, 2> tmp;
    for (int i = 0; i < 10; ++i) {
        // tmp lives outside the loop — no allocation per iteration
        (void)tmp;
    }
}
