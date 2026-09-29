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

struct NoDefaultSentinel final {};

} // namespace detail

/// Describes an in-band value that represents a disengaged resource.
template <auto sentinelValue>
struct Sentinel final {};

/// Tag object for selecting an explicit in-band sentinel through CTAD.
template <auto sentinelValue>
inline constexpr Sentinel<sentinelValue> sentinel{};

/// Describes an in-band sentinel in one member of a compound resource identity.
template <auto projection, auto sentinelValue>
    requires std::is_member_object_pointer_v<decltype(projection)>
struct ProjectedSentinel final {};

/// Tag object for selecting an explicit projected sentinel through CTAD.
template <auto projection, auto sentinelValue>
    requires std::is_member_object_pointer_v<decltype(projection)>
inline constexpr ProjectedSentinel<projection, sentinelValue> projectedSentinel{};

/// Customizes the intrinsic sentinel for a distinct class or enum resource identity.
///
/// Specialize this type and provide a static constexpr `value` member. Fundamental
/// and pointer types cannot specialize this customization point.
template <typename Value>
    requires(std::is_class_v<Value> || std::is_enum_v<Value>)
struct DefaultSentinel {
    static constexpr detail::NoDefaultSentinel value{};
};

namespace detail {

template <typename Type>
concept NothrowMovableObject =
    std::is_object_v<Type> && !std::is_array_v<Type> && std::same_as<Type, std::remove_cv_t<Type>> &&
    std::is_nothrow_move_constructible_v<Type> && std::is_nothrow_destructible_v<Type>;

template <typename Value>
concept ResourceValue = NothrowMovableObject<Value>;

template <typename Value, auto sentinelValue>
concept SentinelCompatible = ResourceValue<Value> && requires(Value* location, Value const& value) {
    { Value{sentinelValue} } noexcept -> std::same_as<Value>;
    { std::construct_at(location, sentinelValue) } noexcept -> std::same_as<Value*>;
    { value == sentinelValue } noexcept -> std::convertible_to<bool>;
};

template <typename Value>
concept DefaultSentinelCustomizable = std::is_class_v<Value> || std::is_enum_v<Value>;

template <typename Value>
concept DefaultSentinelReadable = DefaultSentinelCustomizable<Value> && requires { DefaultSentinel<Value>::value; };

template <typename Value>
concept DefaultSentinelAbsent = DefaultSentinelReadable<Value> &&
    std::same_as<std::remove_cv_t<decltype(DefaultSentinel<Value>::value)>, NoDefaultSentinel>;

template <typename Value>
concept DefaultSentinelPresent = DefaultSentinelReadable<Value> && !DefaultSentinelAbsent<Value> &&
    SentinelCompatible<Value, DefaultSentinel<Value>::value>;

template <typename Value>
struct DefaultPolicySelector;

template <typename Value>
    requires std::is_pointer_v<Value>
struct DefaultPolicySelector<Value> final {
    using Type = Sentinel<nullptr>;
};

template <typename Value>
    requires(!std::is_pointer_v<Value> && !DefaultSentinelCustomizable<Value>)
struct DefaultPolicySelector<Value> final {
    using Type = NoDefaultSentinel;
};

template <typename Value>
    requires(!std::is_pointer_v<Value> && DefaultSentinelAbsent<Value>)
struct DefaultPolicySelector<Value> final {
    using Type = NoDefaultSentinel;
};

template <typename Value>
    requires(!std::is_pointer_v<Value> && DefaultSentinelPresent<Value>)
struct DefaultPolicySelector<Value> final {
    using Type = Sentinel<DefaultSentinel<Value>::value>;
};

template <typename Value>
using DefaultPolicy = DefaultPolicySelector<Value>::Type;

template <NothrowMovableObject Type>
constexpr auto replaceFromMove(Type& target, Type&& source) noexcept -> void {
    if constexpr (std::is_nothrow_move_assignable_v<Type>) {
        target = std::move(source);
    } else {
        std::destroy_at(std::addressof(target));
        std::construct_at(std::addressof(target), std::move(source));
    }
}

template <typename Value, auto sentinelValue>
    requires SentinelCompatible<Value, sentinelValue>
constexpr auto replaceWithSentinel(Value& target) noexcept -> void {
    if constexpr (requires {
                      { target = sentinelValue } noexcept -> std::same_as<Value&>;
                  }) {
        target = sentinelValue;
    } else {
        std::destroy_at(std::addressof(target));
        std::construct_at(std::addressof(target), sentinelValue);
    }
}

template <typename Value>
    requires ResourceValue<Value>
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

        Value value{std::move(*value_)};
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

template <typename Value, auto sentinelValue>
    requires SentinelCompatible<Value, sentinelValue>
class SentinelStorage final {
public:
    constexpr SentinelStorage() noexcept
        : value_{sentinelValue} {}

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
                          { std::exchange(value, sentinelValue) } noexcept -> std::same_as<Value>;
                      }) {
            return std::exchange(value_, sentinelValue);
        } else {
            Value value{std::move(value_)};
            replaceWithSentinel<Value, sentinelValue>(value_);
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
        return static_cast<bool>(value == sentinelValue);
    }

    static constexpr auto emptyValue() noexcept -> Value { return Value{sentinelValue}; }

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

template <typename Value, auto projection, auto sentinelValue>
concept ProjectedSentinelCompatible = ResourceValue<Value> && std::is_member_object_pointer_v<decltype(projection)> &&
    requires(Value& value, Value const& constValue) {
        requires std::is_lvalue_reference_v<decltype(std::invoke(projection, value))>;
        requires(!std::is_const_v<std::remove_reference_t<decltype(std::invoke(projection, value))>>);
        requires(!std::is_volatile_v<std::remove_reference_t<decltype(std::invoke(projection, value))>>);
        requires SentinelCompatible<std::remove_cvref_t<decltype(std::invoke(projection, value))>, sentinelValue>;
        { std::invoke(projection, constValue) == sentinelValue } noexcept -> std::convertible_to<bool>;
    };

template <typename Value, auto projection, auto sentinelValue>
    requires ProjectedSentinelCompatible<Value, projection, sentinelValue>
class ProjectedSentinelStorage final {
private:
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

        Value value{std::move(value_)};
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
        return static_cast<bool>(projected(value) == sentinelValue);
    }

    static constexpr auto disengage(Value& value) noexcept -> void {
        replaceWithSentinel<ProjectedValue, sentinelValue>(projected(value));
    }

    Value value_;
};

template <typename Value, typename Disengagement>
struct StorageFor;

template <typename Value>
    requires ResourceValue<Value>
struct StorageFor<Value, NoDefaultSentinel> final {
    using Type = OptionalStorage<Value>;
};

template <typename Value, auto sentinelValue>
    requires SentinelCompatible<Value, sentinelValue>
struct StorageFor<Value, Sentinel<sentinelValue>> final {
    using Type = SentinelStorage<Value, sentinelValue>;
};

template <typename Value, auto projection, auto sentinelValue>
    requires ProjectedSentinelCompatible<Value, projection, sentinelValue>
struct StorageFor<Value, ProjectedSentinel<projection, sentinelValue>> final {
    using Type = ProjectedSentinelStorage<Value, projection, sentinelValue>;
};

template <typename Value, typename Disengagement>
concept StoragePolicy = requires { typename StorageFor<Value, Disengagement>::Type; };

template <typename Value, typename Disengagement>
    requires StoragePolicy<Value, Disengagement>
using StorageForT = StorageFor<Value, Disengagement>::Type;

template <typename Deleter, typename Value>
concept ResourceDeleter = NothrowMovableObject<Deleter> && requires(Deleter& deleter, Value const& value) {
    { std::invoke(deleter, value) } noexcept -> std::same_as<void>;
};

template <typename Value>
concept NothrowEqualityComparable = requires(Value const& left, Value const& right) {
    { left == right } noexcept -> std::convertible_to<bool>;
};

} // namespace detail

/// Owns one external resource identity and destroys it with an explicit deleter.
template <typename Value, typename Deleter, typename Disengagement = detail::DefaultPolicy<Value>>
    requires detail::ResourceValue<Value> && detail::ResourceDeleter<Deleter, Value> &&
    detail::StoragePolicy<Value, Disengagement>
class Resource final {
private:
    using Storage = detail::StorageForT<Value, Disengagement>;

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

    constexpr Resource(Value const& value, Deleter deleter, Disengagement) noexcept(
        std::is_nothrow_copy_constructible_v<Value>)
        requires std::is_copy_constructible_v<Value>
        : Resource{value, std::move(deleter)} {}

    constexpr Resource(Value&& value, Deleter deleter, Disengagement) noexcept
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

        Value value{storage_.release()};
        std::invoke(deleter_, value);
    }

    /// Replaces the owned identity with a copy of `value`.
    ///
    /// The replacement is established before the old resource is destroyed.
    constexpr auto reset(Value const& value) noexcept(std::is_nothrow_copy_constructible_v<Value>) -> void
        requires std::is_copy_constructible_v<Value>
    {
        assertNotSelfReset(value);

        Storage incoming{value};
        reset();
        storage_ = std::move(incoming);
    }

    /// Replaces the owned identity with `value`.
    ///
    /// The replacement is established before the old resource is destroyed.
    constexpr auto reset(Value&& value) noexcept -> void {
        assertNotSelfReset(value);

        Storage incoming{std::move(value)};
        reset();
        storage_ = std::move(incoming);
    }

private:
    constexpr auto assertNotSelfReset([[maybe_unused]] Value const& value) const noexcept -> void {
        if constexpr (detail::NothrowEqualityComparable<Value>) {
            if (owns()) {
                assert(!static_cast<bool>(value == get()));
            }
        }
    }

    [[no_unique_address]] Deleter deleter_;
    Storage storage_;
};

template <typename Value, typename Deleter>
Resource(Value, Deleter) -> Resource<Value, Deleter>;

template <typename Value, typename Deleter, typename Disengagement>
Resource(Value, Deleter, Disengagement) -> Resource<Value, Deleter, Disengagement>;

} // namespace resource
