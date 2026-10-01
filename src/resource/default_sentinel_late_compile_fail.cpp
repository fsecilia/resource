// SPDX-License-Identifier: MIT

/// \file
/// \brief Verifies that a default sentinel cannot be specialized after first use
/// \copyright Copyright (C) 2026 Frank Secilia

#include "resource.hpp"

enum class Fd : int {
    invalid = -1,
};

struct CloseFd final {
    auto operator()(Fd const&) const noexcept -> void {}
};

using File = resource::Resource<Fd, CloseFd>;

template <>
struct resource::DefaultSentinel<Fd> final {
    static constexpr auto value = Fd::invalid;
};

auto file = File{Fd::invalid, CloseFd{}};
