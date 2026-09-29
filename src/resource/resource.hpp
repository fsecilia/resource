// SPDX-License-Identifier: MIT

/// \file
/// \brief Unique ownership for arbitrary external resource identities
/// \copyright Copyright (C) 2026 Frank Secilia

#pragma once

#include <cassert>
#include <concepts>
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace resource {

namespace detail {

struct NoSentinel final {};

template <typename Type>
concept NothrowMovableObject =
    std::is_object_v<Type> && !std::is_array_v<Type> && std::same_as<Type, std::remove_cv_t<Type>> &&
    std::is_nothrow_move_constructible_v<Type> && std::is_nothrow_destructible_v<Type>;

} // namespace detail

/// Describes a type that can represent an owned external resource identity.
///
/// Moving a value, by construction or assignment when supported, must make the
/// destination identify the same external resource that the source identified
/// before the move. The moved-from representation may change.
template <typename Value>
concept ResourceValue = detail::NothrowMovableObject<Value>;

/// Describes a deleter that destroys the external resource identified by a value
/// without throwing.
///
/// Moving a deleter, by construction or assignment when supported, must preserve
/// that cleanup behavior in the destination.
template <typename Deleter, typename Value>
concept ResourceDeleter =
    ResourceValue<Value> && detail::NothrowMovableObject<Deleter> && requires(Deleter& deleter, Value const& value) {
        { std::invoke(deleter, value) } noexcept -> std::same_as<void>;
    };

/// Describes an in-band value that represents a disengaged resource.
template <auto sentinelValue>
struct Sentinel final {
    static constexpr auto value = sentinelValue;
};

/// Tag object for selecting an explicit in-band sentinel through CTAD.
template <auto sentinelValue>
inline constexpr Sentinel<sentinelValue> sentinel{};

/// Describes an in-band sentinel in one member of a compound resource identity.
template <auto projection, typename SentinelType>
    requires std::is_member_object_pointer_v<decltype(projection)>
struct ProjectedSentinel final {};

/// Tag object for selecting an explicit projected sentinel through CTAD.
template <auto projection, auto sentinelValue>
    requires std::is_member_object_pointer_v<decltype(projection)>
inline constexpr ProjectedSentinel<projection, Sentinel<sentinelValue>> projectedSentinel{};

/// Customizes the intrinsic sentinel for a distinct class or enum resource identity.
///
/// Specialize this type and provide a static constexpr `value` member. Fundamental
/// and pointer types cannot specialize this customization point.
template <typename Value>
    requires(std::is_class_v<Value> || std::is_enum_v<Value>)
struct DefaultSentinel;

namespace detail {

template <typename Type>
struct IsExplicitSentinel final : std::false_type {};

template <auto sentinelValue>
struct IsExplicitSentinel<Sentinel<sentinelValue>> final : std::true_type {};

template <typename Type>
concept ExplicitSentinel = IsExplicitSentinel<Type>::value;

template <typename Value, typename SentinelType>
concept DirectSentinelCompatible = ResourceValue<Value> && requires(Value* location, Value const& value) {
    { Value{SentinelType::value} } noexcept -> std::same_as<Value>;
    { std::construct_at(location, SentinelType::value) } noexcept -> std::same_as<Value*>;
    { value == SentinelType::value } noexcept -> std::convertible_to<bool>;
};

template <typename Value>
concept DefaultSentinelCustomizable = std::is_class_v<Value> || std::is_enum_v<Value>;

template <typename Value>
concept DefaultSentinelSpecialized = DefaultSentinelCustomizable<Value> && requires { sizeof(DefaultSentinel<Value>); };

template <typename SentinelType, typename Value>
concept DefaultSentinelForValue =
    DefaultSentinelSpecialized<Value> && std::same_as<SentinelType, DefaultSentinel<Value>>;

template <typename SentinelType, typename Value>
concept DirectSentinelFor = (ExplicitSentinel<SentinelType> || DefaultSentinelForValue<SentinelType, Value>) &&
    DirectSentinelCompatible<Value, SentinelType>;

template <typename Value, auto projection, typename SentinelType>
concept ProjectedSentinelCompatible = ResourceValue<Value> && std::is_member_object_pointer_v<decltype(projection)> &&
    requires(Value& value, Value const& constValue) {
        requires std::is_lvalue_reference_v<decltype(std::invoke(projection, value))>;
        requires(!std::is_const_v<std::remove_reference_t<decltype(std::invoke(projection, value))>>);
        requires(!std::is_volatile_v<std::remove_reference_t<decltype(std::invoke(projection, value))>>);
        requires DirectSentinelFor<SentinelType, std::remove_cvref_t<decltype(std::invoke(projection, value))>>;
        { std::invoke(projection, constValue) == SentinelType::value } noexcept -> std::convertible_to<bool>;
    };

template <typename SentinelType, typename Value>
struct IsProjectedSentinelFor final : std::false_type {};

template <auto projection, typename ProjectedSentinelType, typename Value>
struct IsProjectedSentinelFor<ProjectedSentinel<projection, ProjectedSentinelType>, Value> final
    : std::bool_constant<ProjectedSentinelCompatible<Value, projection, ProjectedSentinelType>> {};

template <typename SentinelType, typename Value>
concept ProjectedSentinelFor = IsProjectedSentinelFor<SentinelType, Value>::value;

template <ResourceValue Value>
struct DefaultSentinelSelector {
    using Type = NoSentinel;
};

template <ResourceValue Value>
    requires std::is_pointer_v<Value>
struct DefaultSentinelSelector<Value> final {
    using Type = Sentinel<nullptr>;
};

template <ResourceValue Value>
    requires(!std::is_pointer_v<Value> && DefaultSentinelSpecialized<Value>)
struct DefaultSentinelSelector<Value> final {
    using Type = DefaultSentinel<Value>;
};

template <ResourceValue Value>
using DefaultSentinelFor = DefaultSentinelSelector<Value>::Type;

} // namespace detail

/// Describes a sentinel representation supported for a resource identity.
template <typename SentinelType, typename Value>
concept SentinelFor = ResourceValue<Value> &&
    (std::same_as<SentinelType, detail::NoSentinel> || detail::DirectSentinelFor<SentinelType, Value> ||
        detail::ProjectedSentinelFor<SentinelType, Value>);

namespace detail {

template <NothrowMovableObject Type>
constexpr auto replaceFromMove(Type& target, Type&& source) noexcept -> void {
    if constexpr (std::is_nothrow_move_assignable_v<Type>) {
        target = std::move(source);
    } else {
        std::destroy_at(std::addressof(target));
        std::construct_at(std::addressof(target), std::move(source));
    }
}

template <ResourceValue Value, DirectSentinelFor<Value> SentinelType>
constexpr auto replaceWithSentinel(Value& target) noexcept -> void {
    if constexpr (requires {
                      { target = SentinelType::value } noexcept -> std::same_as<Value&>;
                  }) {
        target = SentinelType::value;
    } else {
        std::destroy_at(std::addressof(target));
        std::construct_at(std::addressof(target), SentinelType::value);
    }
}

template <ResourceValue Value>
class OptionalStorage final {
public:
    constexpr OptionalStorage() noexcept = default;

    explicit constexpr OptionalStorage(Value const& value)
        requires std::is_copy_constructible_v<Value>
        : value_{std::in_place, value} {}

    explicit constexpr OptionalStorage(Value&& value) noexcept
        : value_{std::in_place, std::move(value)} {}

    constexpr OptionalStorage(OptionalStorage const&) = delete;
    constexpr auto operator=(OptionalStorage const&) -> OptionalStorage& = delete;

    constexpr OptionalStorage(OptionalStorage&& source) noexcept {
        if (source.owns()) {
            value_.emplace(source.release());
        }
    }

    constexpr auto operator=(OptionalStorage&& source) noexcept -> OptionalStorage& {
        if (this == std::addressof(source)) {
            return *this;
        }

        assert(!owns());

        if (source.owns()) {
            value_.emplace(source.release());
        }

        return *this;
    }

    constexpr auto owns() const noexcept -> bool { return value_.has_value(); }

    constexpr auto get() const noexcept -> Value const& {
        assert(owns());
        return *value_;
    }

    constexpr auto release() noexcept -> Value {
        assert(owns());

        auto value = Value{std::move(*value_)};
        value_.reset();
        return value;
    }

    constexpr auto adopt(Value&& value) noexcept -> void {
        assert(!owns());
        value_.emplace(std::move(value));
    }

private:
    std::optional<Value> value_;
};

template <ResourceValue Value, DirectSentinelFor<Value> SentinelType>
class SentinelStorage final {
public:
    constexpr SentinelStorage() noexcept
        : value_{SentinelType::value} {}

    explicit constexpr SentinelStorage(Value const& value)
        requires std::is_copy_constructible_v<Value>
        : value_{canonicalize(value)} {}

    explicit constexpr SentinelStorage(Value&& value) noexcept
        : value_{canonicalize(std::move(value))} {}

    constexpr SentinelStorage(SentinelStorage const&) = delete;
    constexpr auto operator=(SentinelStorage const&) -> SentinelStorage& = delete;

    constexpr SentinelStorage(SentinelStorage&& source) noexcept
        : value_{takeForMove(source)} {}

    constexpr auto operator=(SentinelStorage&& source) noexcept -> SentinelStorage& {
        if (this == std::addressof(source)) {
            return *this;
        }

        assert(!owns());

        if (source.owns()) {
            replaceFromMove(value_, source.release());
        }

        return *this;
    }

    constexpr auto owns() const noexcept -> bool { return !isSentinel(value_); }

    constexpr auto get() const noexcept -> Value const& {
        assert(owns());
        return value_;
    }

    constexpr auto release() noexcept -> Value {
        assert(owns());

        if constexpr (requires(Value& value) {
                          { std::exchange(value, SentinelType::value) } noexcept -> std::same_as<Value>;
                      }) {
            return std::exchange(value_, SentinelType::value);
        } else {
            auto value = Value{std::move(value_)};
            replaceWithSentinel<Value, SentinelType>(value_);
            return value;
        }
    }

    constexpr auto adopt(Value&& value) noexcept -> void {
        assert(!owns());

        if (!isSentinel(value)) {
            replaceFromMove(value_, std::move(value));
        }
    }

private:
    static constexpr auto isSentinel(Value const& value) noexcept -> bool {
        return static_cast<bool>(value == SentinelType::value);
    }

    static constexpr auto emptyValue() noexcept -> Value { return Value{SentinelType::value}; }

    static constexpr auto canonicalize(Value const& value) -> Value
        requires std::is_copy_constructible_v<Value>
    {
        if (isSentinel(value)) {
            return emptyValue();
        }

        return Value{value};
    }

    static constexpr auto canonicalize(Value&& value) noexcept -> Value {
        if (isSentinel(value)) {
            return emptyValue();
        }

        return Value{std::move(value)};
    }

    static constexpr auto takeForMove(SentinelStorage& source) noexcept -> Value {
        if (!source.owns()) {
            return emptyValue();
        }

        return source.release();
    }

    Value value_;
};

template <ResourceValue Value, auto projection, typename SentinelType>
    requires ProjectedSentinelCompatible<Value, projection, SentinelType>
class ProjectedSentinelStorage final {
    using ProjectedValue = std::remove_cvref_t<decltype(std::invoke(projection, std::declval<Value&>()))>;

public:
    constexpr ProjectedSentinelStorage() noexcept
        requires std::is_nothrow_default_constructible_v<Value>
        : value_{} {
        disengage(value_);
    }

    explicit constexpr ProjectedSentinelStorage(Value const& value)
        requires std::is_copy_constructible_v<Value>
        : ProjectedSentinelStorage{value, !isSentinel(value)} {}

    explicit constexpr ProjectedSentinelStorage(Value&& value) noexcept
        : ProjectedSentinelStorage{std::move(value), !isSentinel(value)} {}

    constexpr ProjectedSentinelStorage(ProjectedSentinelStorage const&) = delete;
    constexpr auto operator=(ProjectedSentinelStorage const&) -> ProjectedSentinelStorage& = delete;

    constexpr ProjectedSentinelStorage(ProjectedSentinelStorage&& source) noexcept
        : ProjectedSentinelStorage{std::move(source), source.owns()} {}

    constexpr auto operator=(ProjectedSentinelStorage&& source) noexcept -> ProjectedSentinelStorage& {
        if (this == std::addressof(source)) {
            return *this;
        }

        assert(!owns());

        auto const sourceOwns = source.owns();
        replaceFromMove(value_, std::move(source.value_));
        disengage(source.value_);

        if (!sourceOwns) {
            disengage(value_);
        }

        return *this;
    }

    constexpr auto owns() const noexcept -> bool { return !isSentinel(value_); }

    constexpr auto get() const noexcept -> Value const& {
        assert(owns());
        return value_;
    }

    constexpr auto release() noexcept -> Value {
        assert(owns());

        auto value = Value{std::move(value_)};
        disengage(value_);
        return value;
    }

    constexpr auto adopt(Value&& value) noexcept -> void {
        assert(!owns());

        auto const incomingOwns = !isSentinel(value);
        replaceFromMove(value_, std::move(value));

        if (!incomingOwns) {
            disengage(value_);
        }
    }

private:
    constexpr ProjectedSentinelStorage(Value const& value, bool incomingOwns)
        requires std::is_copy_constructible_v<Value>
        : value_{value} {
        if (!incomingOwns) {
            disengage(value_);
        }
    }

    constexpr ProjectedSentinelStorage(Value&& value, bool incomingOwns) noexcept
        : value_{std::move(value)} {
        if (!incomingOwns) {
            disengage(value_);
        }
    }

    constexpr ProjectedSentinelStorage(ProjectedSentinelStorage&& source, bool sourceOwns) noexcept
        : value_{std::move(source.value_)} {
        disengage(source.value_);

        if (!sourceOwns) {
            disengage(value_);
        }
    }

    static constexpr auto projected(Value& value) noexcept -> ProjectedValue& { return std::invoke(projection, value); }

    static constexpr auto projected(Value const& value) noexcept -> ProjectedValue const& {
        return std::invoke(projection, value);
    }

    static constexpr auto isSentinel(Value const& value) noexcept -> bool {
        return static_cast<bool>(projected(value) == SentinelType::value);
    }

    static constexpr auto disengage(Value& value) noexcept -> void {
        replaceWithSentinel<ProjectedValue, SentinelType>(projected(value));
    }

    Value value_;
};

template <ResourceValue Value, typename SentinelType>
struct StorageFor;

template <ResourceValue Value>
struct StorageFor<Value, NoSentinel> final {
    using Type = OptionalStorage<Value>;
};

template <ResourceValue Value, DirectSentinelFor<Value> SentinelType>
struct StorageFor<Value, SentinelType> final {
    using Type = SentinelStorage<Value, SentinelType>;
};

template <ResourceValue Value, auto projection, typename ProjectedSentinelType>
    requires ProjectedSentinelCompatible<Value, projection, ProjectedSentinelType>
struct StorageFor<Value, ProjectedSentinel<projection, ProjectedSentinelType>> final {
    using Type = ProjectedSentinelStorage<Value, projection, ProjectedSentinelType>;
};

template <ResourceValue Value, SentinelFor<Value> SentinelType>
using StorageForT = StorageFor<Value, SentinelType>::Type;

} // namespace detail

/// Owns one external resource identity and destroys it with an explicit deleter.
template <ResourceValue Value, ResourceDeleter<Value> Deleter,
    SentinelFor<Value> SentinelType = detail::DefaultSentinelFor<Value>>
class Resource final {
private:
    using Storage = detail::StorageForT<Value, SentinelType>;

public:
    constexpr Resource() noexcept
        requires std::is_nothrow_default_constructible_v<Deleter> && (!std::is_pointer_v<Deleter>) &&
                     (!std::is_member_pointer_v<Deleter>) && std::is_nothrow_default_constructible_v<Storage>
        : deleter_{},
          storage_{} {}

    explicit constexpr Resource(Deleter deleter) noexcept
        requires std::is_nothrow_default_constructible_v<Storage>
        : deleter_{std::move(deleter)},
          storage_{} {}

    constexpr Resource(Value const& value, Deleter deleter) noexcept(std::is_nothrow_copy_constructible_v<Value>)
        requires std::is_copy_constructible_v<Value>
        : deleter_{std::move(deleter)},
          storage_{value} {}

    constexpr Resource(Value&& value, Deleter deleter) noexcept
        : deleter_{std::move(deleter)},
          storage_{std::move(value)} {}

    constexpr Resource(Value const& value, Deleter deleter, SentinelType) noexcept(
        std::is_nothrow_copy_constructible_v<Value>)
        requires std::is_copy_constructible_v<Value>
        : Resource{value, std::move(deleter)} {}

    constexpr Resource(Value&& value, Deleter deleter, SentinelType) noexcept
        : Resource{std::move(value), std::move(deleter)} {}

    constexpr Resource(Resource const&) = delete;
    constexpr auto operator=(Resource const&) -> Resource& = delete;

    constexpr Resource(Resource&& source) noexcept
        : deleter_{std::move(source.deleter_)},
          storage_{std::move(source.storage_)} {}

    constexpr auto operator=(Resource&& source) noexcept -> Resource& {
        if (this == std::addressof(source)) {
            return *this;
        }

        reset();
        detail::replaceFromMove(deleter_, std::move(source.deleter_));
        storage_ = std::move(source.storage_);
        return *this;
    }

    constexpr ~Resource() noexcept { reset(); }

    /// Returns whether this object owns a resource identity.
    constexpr auto owns() const noexcept -> bool { return storage_.owns(); }

    /// Tests whether this object owns a resource identity.
    explicit constexpr operator bool() const noexcept { return owns(); }

    /// Returns the owned identity.
    ///
    /// \pre `owns()` is true.
    constexpr auto get() const noexcept -> Value const& { return storage_.get(); }

    /// Returns the owned identity.
    ///
    /// \pre `owns()` is true.
    constexpr auto operator*() const noexcept -> Value const& { return get(); }

    /// Returns a pointer to the owned identity.
    ///
    /// \pre `owns()` is true.
    constexpr auto operator->() const noexcept -> Value const* { return std::addressof(get()); }

    /// Releases ownership without invoking the deleter.
    ///
    /// \pre `owns()` is true.
    /// \returns The transferred resource identity.
    [[nodiscard]] constexpr auto release() noexcept -> Value { return storage_.release(); }

    /// Destroys the owned resource when engaged and leaves this object disengaged.
    constexpr auto reset() noexcept -> void {
        if (!owns()) {
            return;
        }

        auto value = storage_.release();
        std::invoke(deleter_, value);
    }

    /// Replaces the owned identity with a copy of `value`.
    ///
    /// The replacement is established before the old resource is destroyed.
    ///
    /// \pre If this object owns a resource, `value` does not identify that same
    /// external resource.
    constexpr auto reset(Value const& value) noexcept(std::is_nothrow_copy_constructible_v<Value>) -> void
        requires std::is_copy_constructible_v<Value>
    {
        auto incoming = Storage{value};
        reset();
        storage_ = std::move(incoming);
    }

    /// Replaces the owned identity with `value`.
    ///
    /// The replacement is established before the old resource is destroyed.
    ///
    /// \pre If this object owns a resource, `value` does not identify that same
    /// external resource.
    constexpr auto reset(Value&& value) noexcept -> void {
        auto incoming = Storage{std::move(value)};
        reset();
        storage_ = std::move(incoming);
    }

private:
    [[no_unique_address]] Deleter deleter_;
    Storage storage_;
};

template <ResourceValue Value, ResourceDeleter<Value> Deleter>
Resource(Value, Deleter) -> Resource<Value, Deleter>;

template <ResourceValue Value, ResourceDeleter<Value> Deleter, SentinelFor<Value> SentinelType>
Resource(Value, Deleter, SentinelType) -> Resource<Value, Deleter, SentinelType>;

} // namespace resource
