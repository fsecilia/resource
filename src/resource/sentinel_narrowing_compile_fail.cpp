// SPDX-License-Identifier: MIT

/// \file
/// \brief Compile-fail coverage for narrowing explicit sentinels
/// \copyright Copyright (C) 2026 Frank Secilia

#include "resource.hpp"

namespace {

struct DeleteUnsigned final {
    constexpr auto operator()(unsigned const&) const noexcept -> void {}
};

using InvalidResource = resource::Resource<unsigned, DeleteUnsigned, resource::Sentinel<-1>>;

} // namespace
