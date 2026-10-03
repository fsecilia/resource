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

struct NoEngagement final {};

struct UnspecifiedDefaultSentinel {};

template <typename Type>
concept NothrowMovableObject =
    std::is_object_v<Type> && !std::is_array_v<Type> && std::same_as<Type, std::remove_cv_t<Type>> &&
    std::is_nothrow_move_constructible_v<Type> && std::is_nothrow_destructible_v<Type>;

template <typename Type>
concept NothrowBooleanConvertible =
    std::convertible_to<Type, bool> && noexcept(static_cast<bool>(std::declval<Type>()));

} // namespace detail

/// Describes a type that can represent an owned external resource identity.
///
/// Moving a value, by construction or assignment when supported, must make the
/// destination identify the same external resource that the source identified
/// before the move. The moved-from representation may change.
template <typename Value>
concept ResourceValue = detail::NothrowMovableObject<Value>;

/// Describes a deleter that destroys the external resource identified by a value.
///
/// An exception must not escape when Resource invokes the deleter. Moving a deleter,
/// by construction or assignment when supported, must preserve that cleanup behavior
/// in the destination.
template <typename Deleter, typename Value>
concept ResourceDeleter = ResourceValue<Value> && detail::NothrowMovableObject<Deleter> &&
    requires(Deleter& deleter, Value const& value) { std::invoke(deleter, value); };

/// Describes engagement through equality with one in-band sentinel value.
template <auto sentinelValue>
struct Sentinel final {
    static constexpr auto value = sentinelValue;

    template <typename Value>
        requires requires(Value const& candidate) {
            { Value{sentinelValue} } noexcept -> std::same_as<Value>;
            { candidate == Value{sentinelValue} } noexcept -> detail::NothrowBooleanConvertible;
        }
    static constexpr auto engaged(Value const& candidate) noexcept -> bool {
        return !static_cast<bool>(candidate == Value{sentinelValue});
    }

    template <typename Value>
        requires requires(Value& candidate) {
            { Value{sentinelValue} } noexcept -> std::same_as<Value>;
            { candidate = Value{sentinelValue} } noexcept;
        }
    static constexpr auto disengage(Value& candidate) noexcept -> void {
        candidate = Value{sentinelValue};
    }
};

/// Tag object for selecting an explicit in-band sentinel through CTAD.
template <auto sentinelValue>
inline constexpr Sentinel<sentinelValue> sentinel{};

/// Customizes the intrinsic sentinel for a distinct class or enum resource identity.
///
/// Specialize this type before the first `Resource` use for `Value`, and provide a
/// static constexpr `value` member. The specialization must be reachable from every
/// use that depends on it. Fundamental and pointer types cannot specialize this
/// customization point.
template <typename Value>
    requires(std::is_class_v<Value> || std::is_enum_v<Value>)
struct DefaultSentinel : detail::UnspecifiedDefaultSentinel {};

/// Describes in-band engagement for a resource identity.
///
/// `engaged(value)` reports whether `value` identifies an owned resource.
/// `disengage(value)` changes `value` to a disengaged representation. Both
/// operations must be nonthrowing.
template <typename Engagement, typename Value>
concept EngagementFor = ResourceValue<Value> && requires(Value& value, Value const& constValue) {
    { Engagement::engaged(constValue) } noexcept -> std::same_as<bool>;
    { Engagement::disengage(value) } noexcept -> std::same_as<void>;
};

namespace detail {

template <typename Value>
concept DefaultSentinelCustomizable = std::is_class_v<Value> || std::is_enum_v<Value>;

template <typename Value>
concept DefaultSentinelSpecialized =
    DefaultSentinelCustomizable<Value> && !std::is_base_of_v<UnspecifiedDefaultSentinel, DefaultSentinel<Value>>;

template <ResourceValue Value>
struct DefaultSentinelEngagement final {
    static constexpr auto engaged(Value const& value) noexcept -> bool
        requires requires {
            { Value{DefaultSentinel<Value>::value} } noexcept -> std::same_as<Value>;
            { value == Value{DefaultSentinel<Value>::value} } noexcept -> NothrowBooleanConvertible;
        }
    {
        return !static_cast<bool>(value == Value{DefaultSentinel<Value>::value});
    }

    static constexpr auto disengage(Value& value) noexcept -> void
        requires requires {
            { Value{DefaultSentinel<Value>::value} } noexcept -> std::same_as<Value>;
            { value = Value{DefaultSentinel<Value>::value} } noexcept;
        }
    {
        value = Value{DefaultSentinel<Value>::value};
    }
};

template <ResourceValue Value>
struct DefaultEngagementSelector {
    using Type = NoEngagement;
};

template <ResourceValue Value>
    requires std::is_pointer_v<Value>
struct DefaultEngagementSelector<Value> final {
    using Type = Sentinel<nullptr>;
};

template <ResourceValue Value>
    requires(!std::is_pointer_v<Value> && DefaultSentinelSpecialized<Value>)
struct DefaultEngagementSelector<Value> final {
    using Type = DefaultSentinelEngagement<Value>;
};

template <ResourceValue Value>
using DefaultEngagementFor = DefaultEngagementSelector<Value>::Type;

template <typename MemberPointer>
struct MemberObjectValue;

template <typename Member, typename Owner>
struct MemberObjectValue<Member Owner::*> final {
    using Type = std::remove_cv_t<Member>;
};

template <typename MemberPointer>
using MemberObjectValueT = MemberObjectValue<MemberPointer>::Type;

} // namespace detail

/// Projects engagement behavior onto one member of a compound resource identity.
template <auto projection,
    typename Engagement = detail::DefaultEngagementFor<detail::MemberObjectValueT<decltype(projection)>>>
    requires std::is_member_object_pointer_v<decltype(projection)>
struct ProjectedEngagement final {
    template <typename Value>
        requires requires(Value const& value) {
            requires std::is_lvalue_reference_v<decltype(std::invoke(projection, value))>;
            requires EngagementFor<Engagement, std::remove_cvref_t<decltype(std::invoke(projection, value))>>;
            { Engagement::engaged(std::invoke(projection, value)) } noexcept -> std::same_as<bool>;
        }
    static constexpr auto engaged(Value const& value) noexcept -> bool {
        return Engagement::engaged(std::invoke(projection, value));
    }

    template <typename Value>
        requires requires(Value& value) {
            requires std::is_lvalue_reference_v<decltype(std::invoke(projection, value))>;
            requires(!std::is_const_v<std::remove_reference_t<decltype(std::invoke(projection, value))>>);
            requires(!std::is_volatile_v<std::remove_reference_t<decltype(std::invoke(projection, value))>>);
            requires EngagementFor<Engagement, std::remove_cvref_t<decltype(std::invoke(projection, value))>>;
            { Engagement::disengage(std::invoke(projection, value)) } noexcept -> std::same_as<void>;
        }
    static constexpr auto disengage(Value& value) noexcept -> void {
        Engagement::disengage(std::invoke(projection, value));
    }
};

/// Tag object for selecting projected engagement through CTAD.
template <auto projection,
    typename Engagement = detail::DefaultEngagementFor<detail::MemberObjectValueT<decltype(projection)>>>
    requires std::is_member_object_pointer_v<decltype(projection)>
inline constexpr ProjectedEngagement<projection, Engagement> projectedEngagement{};

/// Tag object for selecting an explicit projected sentinel through CTAD.
template <auto projection, auto sentinelValue>
    requires std::is_member_object_pointer_v<decltype(projection)>
inline constexpr ProjectedEngagement<projection, Sentinel<sentinelValue>> projectedSentinel{};

namespace detail {

template <typename Engagement, typename Value>
concept EngagementSelectionFor = std::same_as<Engagement, NoEngagement> || EngagementFor<Engagement, Value>;

template <typename Engagement, ResourceValue Value>
struct DisengagedValueFactory;

template <auto sentinelValue, ResourceValue Value>
    requires requires {
        { Value{sentinelValue} } noexcept -> std::same_as<Value>;
    }
struct DisengagedValueFactory<Sentinel<sentinelValue>, Value> final {
    static constexpr auto make() noexcept -> Value { return Value{sentinelValue}; }
};

template <ResourceValue Value>
    requires requires {
        { Value{DefaultSentinel<Value>::value} } noexcept -> std::same_as<Value>;
    }
struct DisengagedValueFactory<DefaultSentinelEngagement<Value>, Value> final {
    static constexpr auto make() noexcept -> Value { return Value{DefaultSentinel<Value>::value}; }
};

template <typename Engagement, typename Value>
concept DisengagedValueConstructible = requires {
    { DisengagedValueFactory<Engagement, Value>::make() } noexcept -> std::same_as<Value>;
};

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

private:
    std::optional<Value> value_;
};

template <ResourceValue Value, EngagementFor<Value> Engagement>
class EngagementStorage final {
public:
    constexpr EngagementStorage() noexcept
        requires DisengagedValueConstructible<Engagement, Value>
        : value_{DisengagedValueFactory<Engagement, Value>::make()} {}

    explicit constexpr EngagementStorage(Value const& value)
        requires std::is_copy_constructible_v<Value>
        : value_{value} {
        if (!Engagement::engaged(value)) {
            Engagement::disengage(value_);
        }
    }

    explicit constexpr EngagementStorage(Value&& value) noexcept
        : EngagementStorage{std::move(value), Engagement::engaged(value)} {}

    constexpr EngagementStorage(EngagementStorage const&) = delete;
    constexpr auto operator=(EngagementStorage const&) -> EngagementStorage& = delete;

    constexpr EngagementStorage(EngagementStorage&& source) noexcept
        : EngagementStorage{std::move(source.value_), source.owns()} {
        Engagement::disengage(source.value_);
    }

    constexpr auto operator=(EngagementStorage&& source) noexcept -> EngagementStorage&
        requires std::is_nothrow_move_assignable_v<Value>
    {
        assert(!owns());

        if (source.owns()) {
            value_ = source.release();
        }

        return *this;
    }

    constexpr auto owns() const noexcept -> bool { return Engagement::engaged(value_); }

    constexpr auto get() const noexcept -> Value const& {
        assert(owns());
        return value_;
    }

    constexpr auto release() noexcept -> Value {
        assert(owns());

        auto value = Value{std::move(value_)};
        Engagement::disengage(value_);
        return value;
    }

private:
    constexpr EngagementStorage(Value&& value, bool incomingOwns) noexcept
        : value_{std::move(value)} {
        if (!incomingOwns) {
            Engagement::disengage(value_);
        }
    }

    Value value_;
};

template <ResourceValue Value, typename Engagement>
struct StorageFor;

template <ResourceValue Value>
struct StorageFor<Value, NoEngagement> final {
    using Type = OptionalStorage<Value>;
};

template <ResourceValue Value, EngagementFor<Value> Engagement>
struct StorageFor<Value, Engagement> final {
    using Type = EngagementStorage<Value, Engagement>;
};

template <ResourceValue Value, typename Engagement>
    requires EngagementSelectionFor<Engagement, Value>
using StorageForT = StorageFor<Value, Engagement>::Type;

template <typename Target, typename Argument>
concept BraceConstructibleFrom = requires(Argument&& argument) { Target{std::forward<Argument>(argument)}; };

template <typename Argument, typename Target>
concept ExactArgument = std::same_as<std::remove_cvref_t<Argument>, Target>;

template <typename Argument, typename Value>
concept ValueShapedArgument = ExactArgument<Argument, Value> || BraceConstructibleFrom<Value, Argument>;

template <typename Argument, typename Deleter>
concept DeleterShapedArgument = ExactArgument<Argument, Deleter> || BraceConstructibleFrom<Deleter, Argument>;

template <typename Argument, typename Value, typename Deleter>
concept ValueArgument = (!std::same_as<Value, Deleter>) &&
    (ExactArgument<Argument, Value> ||
        (!ExactArgument<Argument, Deleter> && ValueShapedArgument<Argument, Value> &&
            !DeleterShapedArgument<Argument, Deleter>));

template <typename Argument, typename Value, typename Deleter>
concept DeleterArgument = (!std::same_as<Value, Deleter>) &&
    (ExactArgument<Argument, Deleter> ||
        (!ExactArgument<Argument, Value> && DeleterShapedArgument<Argument, Deleter> &&
            !ValueShapedArgument<Argument, Value>));

} // namespace detail

/// Owns one external resource identity and destroys it with an explicit deleter.
template <ResourceValue Value, ResourceDeleter<Value> Deleter,
    typename Engagement = detail::DefaultEngagementFor<Value>>
    requires detail::EngagementSelectionFor<Engagement, Value>
class Resource final {
private:
    using Storage = detail::StorageForT<Value, Engagement>;

public:
    constexpr Resource() noexcept
        requires std::is_nothrow_default_constructible_v<Deleter> && (!std::is_pointer_v<Deleter>) &&
                     (!std::is_member_pointer_v<Deleter>) && std::is_nothrow_default_constructible_v<Storage>
        : deleter_{},
          storage_{} {}

    template <typename ValueArg>
        requires detail::ValueArgument<ValueArg, Value, Deleter> && detail::BraceConstructibleFrom<Value, ValueArg> &&
                     std::is_nothrow_default_constructible_v<Deleter> && (!std::is_pointer_v<Deleter>) &&
                     (!std::is_member_pointer_v<Deleter>)
    explicit constexpr Resource(ValueArg&& value) noexcept(noexcept(Value{std::forward<ValueArg>(value)}))
        : deleter_{},
          storage_{Value{std::forward<ValueArg>(value)}} {}

    template <typename DeleterArg>
        requires detail::DeleterArgument<DeleterArg, Value, Deleter> &&
                     std::is_nothrow_constructible_v<Deleter, DeleterArg&&> &&
                     std::is_nothrow_default_constructible_v<Storage>
    explicit constexpr Resource(DeleterArg&& deleter) noexcept
        : deleter_{Deleter{std::forward<DeleterArg>(deleter)}},
          storage_{} {}

    template <typename DeleterArg>
        requires std::is_copy_constructible_v<Value> && std::is_nothrow_constructible_v<Deleter, DeleterArg&&>
    constexpr Resource(Value const& value, DeleterArg&& deleter) noexcept(std::is_nothrow_copy_constructible_v<Value>)
        : deleter_{std::forward<DeleterArg>(deleter)},
          storage_{value} {}

    template <typename DeleterArg>
        requires std::is_nothrow_constructible_v<Deleter, DeleterArg&&>
    constexpr Resource(Value&& value, DeleterArg&& deleter) noexcept
        : deleter_{std::forward<DeleterArg>(deleter)},
          storage_{std::move(value)} {}

    template <typename DeleterArg>
        requires std::is_copy_constructible_v<Value> && std::is_nothrow_constructible_v<Deleter, DeleterArg&&>
    constexpr Resource(Value const& value, DeleterArg&& deleter, Engagement) noexcept(
        std::is_nothrow_copy_constructible_v<Value>)
        : Resource{value, std::forward<DeleterArg>(deleter)} {}

    template <typename DeleterArg>
        requires std::is_nothrow_constructible_v<Deleter, DeleterArg&&>
    constexpr Resource(Value&& value, DeleterArg&& deleter, Engagement) noexcept
        : Resource{std::move(value), std::forward<DeleterArg>(deleter)} {}

    constexpr Resource(Resource const&) = delete;
    constexpr auto operator=(Resource const&) -> Resource& = delete;

    constexpr Resource(Resource&& source) noexcept
        : deleter_{std::move(source.deleter_)},
          storage_{std::move(source.storage_)} {}

    constexpr auto operator=(Resource&& source) noexcept -> Resource&
        requires std::is_nothrow_move_assignable_v<Deleter> && std::is_nothrow_move_assignable_v<Storage>
    {
        if (this == std::addressof(source)) {
            return *this;
        }

        auto incoming = Resource{std::move(source)};
        reset();
        deleter_ = std::move(incoming.deleter_);
        storage_ = std::move(incoming.storage_);
        return *this;
    }

    constexpr ~Resource() noexcept { reset(); }

    /// Returns whether this object owns a resource identity.
    constexpr auto owns() const noexcept -> bool { return storage_.owns(); }

    /// Tests whether this object owns a resource identity.
    explicit constexpr operator bool() const noexcept { return owns(); }

    /// Returns the deleter.
    constexpr auto deleter() noexcept -> Deleter& { return deleter_; }

    /// Returns the deleter.
    constexpr auto deleter() const noexcept -> Deleter const& { return deleter_; }

    /// Returns the owned identity.
    ///
    /// \pre `owns()` is true.
    constexpr auto get() const noexcept -> Value const& { return storage_.get(); }

    /// Returns the owned identity.
    ///
    /// \pre `owns()` is true.
    constexpr auto operator*() const noexcept -> Value const&
        requires(!std::is_pointer_v<Value>)
    {
        return get();
    }

    /// Returns the object or function identified by an owned pointer identity.
    ///
    /// \pre `owns()` is true.
    constexpr auto operator*() const noexcept -> std::add_lvalue_reference_t<std::remove_pointer_t<Value>>
        requires std::is_pointer_v<Value> && requires(Value const& value) { *value; }
    {
        return *get();
    }

    /// Returns a pointer to the owned identity.
    ///
    /// \pre `owns()` is true.
    constexpr auto operator->() const noexcept -> Value const*
        requires(!std::is_pointer_v<Value>)
    {
        return std::addressof(get());
    }

    /// Returns the owned pointer identity.
    ///
    /// \pre `owns()` is true.
    constexpr auto operator->() const noexcept -> Value
        requires std::is_pointer_v<Value>
    {
        return get();
    }

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
        requires std::is_copy_constructible_v<Value> && std::is_nothrow_move_assignable_v<Storage>
    {
        assert(!owns() || std::addressof(value) != std::addressof(get()));

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
    constexpr auto reset(Value&& value) noexcept -> void
        requires std::is_nothrow_move_assignable_v<Storage>
    {
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

template <ResourceValue Value, ResourceDeleter<Value> Deleter, EngagementFor<Value> Engagement>
Resource(Value, Deleter, Engagement) -> Resource<Value, Deleter, Engagement>;

} // namespace resource
