//----------------------------------------------------------------------------------------------
// Copyright (c) The Einsums Developers. All rights reserved.
// Licensed under the MIT License. See LICENSE.txt in the project root for license information.
//----------------------------------------------------------------------------------------------

// Smoke test for einsums-deprecated-api.

namespace einsums {
[[deprecated("use new_compute() instead")]] void old_compute();
void                                             new_compute();
} // namespace einsums

namespace std {
[[deprecated("don't call me")]] void some_old_stdlib_func();
}

void deprecated_call_should_warn() {
    einsums::old_compute(); // EXPECT: deprecated
}

void normal_call_should_not_warn() {
    einsums::new_compute(); // OK
}

void external_deprecated_should_not_warn() {
    std::some_old_stdlib_func(); // OK — outside einsums::, Clang's standard warning handles it
}
