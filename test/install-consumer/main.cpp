// SPDX-License-Identifier: MIT

/// \file
/// \brief Installed-package consumer for Resource
/// \copyright Copyright (C) 2026 Frank Secilia

#include <resource/resource.hpp>

struct DeleteInt final {
    constexpr auto operator()(int const&) const noexcept -> void {}
};

int main() {
    auto owned = resource::Resource{17, DeleteInt{}};
    return owned.get() == 17 ? 0 : 1;
}
