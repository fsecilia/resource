// SPDX-License-Identifier: MIT

/// \file
/// \brief Verifies that fundamental identities cannot customize a default sentinel
/// \copyright Copyright (C) 2026 Frank Secilia

#include "resource.hpp"

template <>
struct resource::DefaultSentinel<int> final {
    static constexpr auto value = -1;
};
