// SPDX-License-Identifier: MIT

/// \file
/// \brief Verifies same-type value and deleter arguments are rejected as ambiguous
/// \copyright Copyright (C) 2026 Frank Secilia

#include "resource.hpp"

namespace {

struct SelfDeletingValue final {
    constexpr auto operator()(SelfDeletingValue const&) const noexcept -> void {}
};

[[maybe_unused]] auto resource = resource::Resource<SelfDeletingValue, SelfDeletingValue>{SelfDeletingValue{}};

} // namespace
