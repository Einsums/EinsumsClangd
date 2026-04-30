//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

// Smoke test for einsums-catch2-tensor-fixture-leak.
//
// Stub macros mimicking the surface of Catch2 + the Einsums variant. The
// check walks macro expansion stacks, so the fixture must use real
// macros — straight functions wouldn't trigger.
//
// Each test case here uses a distinct identifier (rather than relying on
// ``__LINE__`` token pasting) to keep the stub macros simple.

#define TEST_CASE(id)         static void test_case_##id()
#define EINSUMS_TEST_CASE(id) static void einsums_test_case_##id()
#define REQUIRE(x)            ((void)(x))
#define CHECK(x)              ((void)(x))
#define INFO(x)               ((void)(x))
#define CAPTURE(x)            ((void)(x))

namespace einsums {
template <typename T, int N>
struct Tensor {};
template <typename T, int N>
struct TensorView {};
} // namespace einsums

TEST_CASE(leaked) {
    einsums::Tensor<double, 2> A; // EXPECT: never reaches a Catch2 assertion
    einsums::Tensor<double, 2> C; // EXPECT: never reaches a Catch2 assertion
    (void)A;
    (void)C;
}

TEST_CASE(required) {
    einsums::Tensor<double, 2> A;
    REQUIRE(&A != nullptr); // OK — A is in a REQUIRE
}

EINSUMS_TEST_CASE(captured) {
    einsums::Tensor<double, 2> B;
    INFO(&B); // OK — INFO counts as capture
}

EINSUMS_TEST_CASE(checked) {
    einsums::Tensor<double, 2> D;
    CHECK(&D != nullptr); // OK
}

void not_a_test_should_not_warn() {
    einsums::Tensor<double, 2> E; // OK — outside any TEST_CASE macro
    (void)E;
}
