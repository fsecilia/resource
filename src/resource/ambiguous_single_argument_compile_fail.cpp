// SPDX-License-Identifier: MIT

/// \file
/// \brief Verifies ambiguous one-argument Resource construction is rejected
/// \copyright Copyright (C) 2026 Frank Secilia

#include "resource.hpp"

namespace {

struct AmbiguousArgument final {
    constexpr operator int() const noexcept { return 7; }
};

struct DeleteInt final {
    constexpr DeleteInt() noexcept = default;
    constexpr DeleteInt(AmbiguousArgument const&) noexcept {}

    constexpr auto operator()(int const&) const noexcept -> void {}
};

[[maybe_unused]] auto resource = resource::Resource<int, DeleteInt>{AmbiguousArgument{}};

} // namespace
