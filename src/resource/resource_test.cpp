// SPDX-License-Identifier: MIT

/// \file
/// \brief Tests for unique external resource ownership
/// \copyright Copyright (C) 2026 Frank Secilia

#include "resource.hpp"

#include <gtest/gtest.h>

#include <concepts>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace resource {
namespace {

struct EmptyPointerDelete final {
    constexpr auto operator()(int* const&) const noexcept -> void {}
};

struct CountDelete final {
    int* count;

    constexpr auto operator()(int const&) const noexcept -> void { ++*count; }
};

struct NoopDelete final {
    constexpr auto operator()(int const&) const noexcept -> void {}
};

struct StrongHandle final {
    int value;

    friend constexpr auto operator==(StrongHandle const&, StrongHandle const&) noexcept -> bool = default;
};

struct CountStrongDelete final {
    int* count;

    constexpr auto operator()(StrongHandle const&) const noexcept -> void { ++*count; }
};

class OpaqueHandle final {
public:
    constexpr explicit OpaqueHandle(int value) noexcept
        : value_{value} {}

    friend constexpr auto operator==(OpaqueHandle const&, OpaqueHandle const&) noexcept -> bool = default;

private:
    int value_;
};

struct DeleteOpaque final {
    constexpr auto operator()(OpaqueHandle const&) const noexcept -> void {}
};

struct PlainHandle final {
    int value;
};

struct DeletePlain final {
    constexpr auto operator()(PlainHandle const&) const noexcept -> void {}
};

enum class EnumHandle : int {
    invalid = -1,
    valid = 7,
};

struct DeleteEnum final {
    constexpr auto operator()(EnumHandle const&) const noexcept -> void {}
};

struct MalformedHandle final {
    int value;

    friend constexpr auto operator==(MalformedHandle const&, MalformedHandle const&) noexcept -> bool = default;
};

struct DeleteMalformed final {
    constexpr auto operator()(MalformedHandle const&) const noexcept -> void {}
};

struct Compound final {
    int parent;
    StrongHandle handle;
};

struct DestroyCompound final {
    int* count;

    constexpr auto operator()(Compound const&) const noexcept -> void { ++*count; }
};

struct IntCompound final {
    int parent;
    int handle;
};

struct DestroyIntCompound final {
    constexpr auto operator()(IntCompound const&) const noexcept -> void {}
};

struct NonDefaultCompound final {
    int parent;
    int handle;

    NonDefaultCompound() = delete;

    constexpr NonDefaultCompound(int parentValue, int handleValue) noexcept
        : parent{parentValue},
          handle{handleValue} {}
};

struct DestroyNonDefaultCompound final {
    constexpr auto operator()(NonDefaultCompound const&) const noexcept -> void {}
};

struct NonAssignable final {
    int value;

    constexpr explicit NonAssignable(int initialValue) noexcept
        : value{initialValue} {}

    constexpr NonAssignable(NonAssignable const&) noexcept = default;
    constexpr NonAssignable(NonAssignable&&) noexcept = default;
    constexpr auto operator=(NonAssignable const&) -> NonAssignable& = delete;
    constexpr auto operator=(NonAssignable&&) -> NonAssignable& = delete;

    friend constexpr auto operator==(NonAssignable const&, NonAssignable const&) noexcept -> bool = default;
};

struct DeleteNonAssignable final {
    constexpr auto operator()(NonAssignable const&) const noexcept -> void {}
};

struct NonAssignableMoveChangingHandle final {
    int value;

    constexpr explicit NonAssignableMoveChangingHandle(int initialValue) noexcept
        : value{initialValue} {}

    constexpr NonAssignableMoveChangingHandle(NonAssignableMoveChangingHandle const&) noexcept = default;

    constexpr NonAssignableMoveChangingHandle(NonAssignableMoveChangingHandle&& source) noexcept
        : value{source.value == -1 ? 99 : source.value} {
        source.value = 77;
    }

    constexpr auto operator=(NonAssignableMoveChangingHandle const&) -> NonAssignableMoveChangingHandle& = delete;
    constexpr auto operator=(NonAssignableMoveChangingHandle&&) -> NonAssignableMoveChangingHandle& = delete;

    friend constexpr auto operator==(
        NonAssignableMoveChangingHandle const&, NonAssignableMoveChangingHandle const&) noexcept -> bool = default;
};

struct DeleteNonAssignableMoveChangingHandle final {
    constexpr auto operator()(NonAssignableMoveChangingHandle const&) const noexcept -> void {}
};

struct OptionalNonAssignable final {
    int value;

    constexpr explicit OptionalNonAssignable(int initialValue) noexcept
        : value{initialValue} {}

    constexpr OptionalNonAssignable(OptionalNonAssignable const&) noexcept = default;
    constexpr OptionalNonAssignable(OptionalNonAssignable&&) noexcept = default;
    constexpr auto operator=(OptionalNonAssignable const&) -> OptionalNonAssignable& = delete;
    constexpr auto operator=(OptionalNonAssignable&&) -> OptionalNonAssignable& = delete;
};

struct DeleteOptionalNonAssignable final {
    constexpr auto operator()(OptionalNonAssignable const&) const noexcept -> void {}
};

struct MoveChangingHandle final {
    int value;

    constexpr explicit MoveChangingHandle(int initialValue) noexcept
        : value{initialValue} {}

    constexpr MoveChangingHandle(MoveChangingHandle const&) noexcept = default;

    constexpr MoveChangingHandle(MoveChangingHandle&& source) noexcept
        : value{source.value == -1 ? 99 : source.value} {
        source.value = 77;
    }

    constexpr auto operator=(MoveChangingHandle const&) -> MoveChangingHandle& = default;
    constexpr auto operator=(MoveChangingHandle&&) -> MoveChangingHandle& = default;

    friend constexpr auto operator==(MoveChangingHandle const&, MoveChangingHandle const&) noexcept -> bool = default;
};

struct MoveChangingCompound final {
    int parent;
    MoveChangingHandle handle;
};

struct DestroyMoveChangingCompound final {
    constexpr auto operator()(MoveChangingCompound const&) const noexcept -> void {}
};

struct DestroyMoveChangingHandle final {
    constexpr auto operator()(MoveChangingHandle const&) const noexcept -> void {}
};

struct ReturningDelete final {
    constexpr auto operator()(int const&) const noexcept -> int { return 0; }
};

struct ThrowingDelete final {
    auto operator()(int const&) const -> void {}
};

struct ThrowingMoveValue final {
    ThrowingMoveValue() = default;
    ThrowingMoveValue(ThrowingMoveValue const&) = default;
    ThrowingMoveValue(ThrowingMoveValue&&) noexcept(false) {}
};

struct DeleteThrowingMoveValue final {
    auto operator()(ThrowingMoveValue const&) const noexcept -> void {}
};

struct MemberDeletedValue final {
    int value;

    constexpr auto close() const noexcept -> void {}
};

struct DeleteLog final {
    int entries[3]{};
    int count{};
};

struct LoggingDelete final {
    int id;
    DeleteLog* log;

    auto operator()(int const&) const noexcept -> void {
        log->entries[log->count] = id;
        ++log->count;
    }
};

struct OwnershipProbeDelete final {
    void const* context;
    auto (*owns)(void const*) noexcept -> bool;
    bool* observedDisengaged;

    auto operator()(int const&) const noexcept -> void { *observedDisengaged = !owns(context); }
};

template <typename Value, typename Deleter>
concept DefaultResourceFormable = requires { typename Resource<Value, Deleter>; };

template <typename Value, typename Deleter, typename SentinelType>
concept ResourceFormable = requires { typename Resource<Value, Deleter, SentinelType>; };

template <typename Value>
concept DefaultSentinelNameable = requires { typename DefaultSentinel<Value>; };

template <auto projection, auto sentinelValue>
concept ProjectedSentinelNameable = requires { typename ProjectedSentinel<projection, sentinelValue>; };

constexpr auto deleteInt(int const&) noexcept -> void {
}

} // namespace

template <>
struct DefaultSentinel<StrongHandle> final {
    static constexpr StrongHandle value{-1};
};

template <>
struct DefaultSentinel<EnumHandle> final {
    static constexpr EnumHandle value{EnumHandle::invalid};
};

template <>
struct DefaultSentinel<OpaqueHandle> final {
    static constexpr OpaqueHandle value{-1};
};

template <>
struct DefaultSentinel<MalformedHandle> final {
    static constexpr auto value = nullptr;
};

namespace {

using PointerResource = Resource<int*, EmptyPointerDelete>;
using OpaqueResource = Resource<OpaqueHandle, DeleteOpaque>;
using FunctionDelete = void (*)(int const&) noexcept;
using FunctionDeleteResource = Resource<int, FunctionDelete>;
using MemberDelete = void (MemberDeletedValue::*)() const noexcept;
using MemberDeleteResource = Resource<MemberDeletedValue, MemberDelete>;
using CompoundPolicy = ProjectedSentinel<&Compound::handle, StrongHandle{-1}>;
using CompoundResource = Resource<Compound, DestroyCompound, CompoundPolicy>;
using NonDefaultCompoundPolicy = ProjectedSentinel<&NonDefaultCompound::handle, -1>;
using NonDefaultCompoundResource = Resource<NonDefaultCompound, DestroyNonDefaultCompound, NonDefaultCompoundPolicy>;
using MoveChangingPolicy = ProjectedSentinel<&MoveChangingCompound::handle, MoveChangingHandle{-1}>;
using MoveChangingResource = Resource<MoveChangingCompound, DestroyMoveChangingCompound, MoveChangingPolicy>;
using MoveChangingScalarResource =
    Resource<MoveChangingHandle, DestroyMoveChangingHandle, Sentinel<MoveChangingHandle{-1}>>;
using NonAssignableMoveChangingResource = Resource<NonAssignableMoveChangingHandle,
    DeleteNonAssignableMoveChangingHandle, Sentinel<NonAssignableMoveChangingHandle{-1}>>;

static_assert(sizeof(PointerResource) == sizeof(int*));
static_assert(sizeof(OpaqueResource) == sizeof(OpaqueHandle));
static_assert(!std::copy_constructible<PointerResource>);
static_assert(!std::is_copy_assignable_v<PointerResource>);
static_assert(std::is_nothrow_move_constructible_v<PointerResource>);
static_assert(std::is_nothrow_move_assignable_v<PointerResource>);
static_assert(noexcept(std::declval<PointerResource&>().reset()));
static_assert(noexcept(std::declval<PointerResource&>().release()));
static_assert(std::same_as<decltype(std::declval<PointerResource const&>().get()), int* const&>);
static_assert(std::same_as<decltype(*std::declval<PointerResource const&>()), int* const&>);
static_assert(std::same_as<decltype(std::declval<PointerResource const&>().operator->()), int* const*>);

static_assert(std::same_as<decltype(Resource{static_cast<int*>(nullptr), EmptyPointerDelete{}}), PointerResource>);
static_assert(std::same_as<decltype(Resource{7, CountDelete{nullptr}}), Resource<int, CountDelete>>);
static_assert(
    std::same_as<decltype(Resource{7, CountDelete{nullptr}, sentinel<-1>}), Resource<int, CountDelete, Sentinel<-1>>>);
static_assert(std::same_as<decltype(Resource{StrongHandle{7}, CountStrongDelete{nullptr}}),
    Resource<StrongHandle, CountStrongDelete>>);
static_assert(std::same_as<decltype(Resource{EnumHandle::valid, DeleteEnum{}}), Resource<EnumHandle, DeleteEnum>>);
static_assert(std::same_as<decltype(Resource{OpaqueHandle{7}, DeleteOpaque{}}), OpaqueResource>);
static_assert(std::same_as<decltype(Resource{Compound{3, StrongHandle{7}}, DestroyCompound{nullptr},
                               projectedSentinel<&Compound::handle, StrongHandle{-1}>}),
    CompoundResource>);

static_assert(!DefaultSentinelNameable<int>);
static_assert(!DefaultSentinelNameable<int*>);
static_assert(DefaultSentinelNameable<StrongHandle>);
static_assert(DefaultSentinelNameable<EnumHandle>);
static_assert(DefaultSentinelNameable<OpaqueHandle>);
static_assert(ProjectedSentinelNameable<&IntCompound::handle, -1>);
static_assert(!ProjectedSentinelNameable<nullptr, -1>);

static_assert(!DefaultResourceFormable<int, ReturningDelete>);
static_assert(!DefaultResourceFormable<int, ThrowingDelete>);
static_assert(!DefaultResourceFormable<ThrowingMoveValue, DeleteThrowingMoveValue>);
static_assert(MalformedHandle{-1} == MalformedHandle{-1});
static_assert(!DefaultResourceFormable<MalformedHandle, DeleteMalformed>);
static_assert(ResourceFormable<MalformedHandle, DeleteMalformed, Sentinel<MalformedHandle{-1}>>);

static_assert(std::is_default_constructible_v<Resource<int, NoopDelete>>);
static_assert(std::is_constructible_v<Resource<int, CountDelete>, CountDelete>);
static_assert(DefaultResourceFormable<PlainHandle, DeletePlain>);
static_assert(!std::is_default_constructible_v<FunctionDeleteResource>);
static_assert(std::is_constructible_v<FunctionDeleteResource, FunctionDelete>);
static_assert(!std::is_default_constructible_v<MemberDeleteResource>);
static_assert(std::is_constructible_v<MemberDeleteResource, MemberDelete>);
static_assert(!std::is_default_constructible_v<NonDefaultCompoundResource>);
static_assert(std::is_constructible_v<NonDefaultCompoundResource, NonDefaultCompound, DestroyNonDefaultCompound>);

static_assert(std::is_constructible_v<Resource<NonAssignable, DeleteNonAssignable, Sentinel<NonAssignable{-1}>>,
    NonAssignable, DeleteNonAssignable>);
static_assert(std::is_move_assignable_v<Resource<NonAssignable, DeleteNonAssignable, Sentinel<NonAssignable{-1}>>>);
static_assert(std::is_move_assignable_v<Resource<OptionalNonAssignable, DeleteOptionalNonAssignable>>);

TEST(ResourceTest, PointerNullIsDisengagedByDefault) {
    auto resource = Resource{static_cast<int*>(nullptr), EmptyPointerDelete{}};

    EXPECT_FALSE(resource);
}

TEST(ResourceTest, FundamentalValueHasNoImplicitSentinel) {
    auto count = 0;
    auto resource = Resource{-1, CountDelete{&count}};

    EXPECT_TRUE(resource);
}

TEST(ResourceTest, ExplicitSentinelIsDisengaged) {
    auto count = 0;
    auto resource = Resource{-1, CountDelete{&count}, sentinel<-1>};

    EXPECT_FALSE(resource);
}

TEST(ResourceTest, StrongHandleUsesDefaultSentinel) {
    auto count = 0;
    auto resource = Resource{StrongHandle{-1}, CountStrongDelete{&count}};

    EXPECT_FALSE(resource);
}

TEST(ResourceTest, EnumUsesDefaultSentinel) {
    auto resource = Resource{EnumHandle::invalid, DeleteEnum{}};

    EXPECT_FALSE(resource);
}

TEST(ResourceTest, NonStructuralHandleUsesDefaultSentinel) {
    auto resource = Resource{OpaqueHandle{-1}, DeleteOpaque{}};

    EXPECT_FALSE(resource);
}

TEST(ResourceTest, UnspecializedClassUsesOutOfBandEngagement) {
    auto resource = Resource{PlainHandle{-1}, DeletePlain{}};

    EXPECT_TRUE(resource);
}

TEST(ResourceTest, ResetDoesNotRequireEqualityComparison) {
    auto resource = Resource{PlainHandle{7}, DeletePlain{}};

    resource.reset(PlainHandle{9});

    EXPECT_EQ(resource.get().value, 9);
}

TEST(ResourceTest, DestructionInvokesDeleterExactlyOnce) {
    auto count = 0;

    {
        auto resource = Resource{7, CountDelete{&count}};
    }

    EXPECT_EQ(count, 1);
}

TEST(ResourceTest, ReleaseReturnsIdentity) {
    auto count = 0;
    auto resource = Resource{7, CountDelete{&count}};

    auto const value = resource.release();

    EXPECT_EQ(value, 7);
}

TEST(ResourceTest, ReleaseSuppressesCleanup) {
    auto count = 0;

    {
        auto resource = Resource{7, CountDelete{&count}};
        static_cast<void>(resource.release());
    }

    EXPECT_EQ(count, 0);
}

TEST(ResourceTest, ReleaseDisengagesResource) {
    auto count = 0;
    auto resource = Resource{7, CountDelete{&count}};

    static_cast<void>(resource.release());

    EXPECT_FALSE(resource);
}

TEST(ResourceTest, ResetDestroysOwnedIdentity) {
    auto count = 0;
    auto resource = Resource{7, CountDelete{&count}};

    resource.reset();

    EXPECT_EQ(count, 1);
}

TEST(ResourceTest, ResetLeavesResourceDisengaged) {
    auto count = 0;
    auto resource = Resource{7, CountDelete{&count}};

    resource.reset();

    EXPECT_FALSE(resource);
}

TEST(ResourceTest, ResetDisengagesBeforeCleanup) {
    using ObservedResource = Resource<int, OwnershipProbeDelete, Sentinel<-1>>;

    auto observedDisengaged = false;
    ObservedResource* resourceAddress = nullptr;
    auto const owns = [](void const* context) noexcept -> bool {
        auto const resourceSlot = static_cast<ObservedResource* const*>(context);
        return static_cast<bool>(**resourceSlot);
    };

    auto resource = ObservedResource{7, OwnershipProbeDelete{&resourceAddress, owns, &observedDisengaged}};
    resourceAddress = &resource;

    resource.reset();

    EXPECT_TRUE(observedDisengaged);
}

TEST(ResourceTest, ResetWithValueReplacesIdentity) {
    auto count = 0;
    auto resource = Resource{7, CountDelete{&count}};

    resource.reset(9);

    EXPECT_EQ(resource.get(), 9);
}

TEST(ResourceTest, ResetWithSentinelLeavesResourceDisengaged) {
    auto count = 0;
    auto resource = Resource{7, CountDelete{&count}, sentinel<-1>};

    resource.reset(-1);

    EXPECT_FALSE(resource);
}

TEST(ResourceTest, ResetWithSentinelDestroysPreviousIdentity) {
    auto count = 0;
    auto resource = Resource{7, CountDelete{&count}, sentinel<-1>};

    resource.reset(-1);

    EXPECT_EQ(count, 1);
}

TEST(ResourceTest, MoveConstructionTransfersIdentity) {
    auto count = 0;
    auto source = Resource{7, CountDelete{&count}};

    auto destination = std::move(source);

    EXPECT_EQ(destination.get(), 7);
}

TEST(ResourceTest, MoveConstructionDisengagesSource) {
    auto count = 0;
    auto source = Resource{7, CountDelete{&count}};

    auto destination = std::move(source);
    static_cast<void>(destination);

    EXPECT_FALSE(source);
}

TEST(ResourceTest, MoveAssignmentDestroysWithDestinationDeleter) {
    DeleteLog log{};
    auto destination = Resource{5, LoggingDelete{11, &log}, sentinel<-1>};
    auto source = Resource{7, LoggingDelete{17, &log}, sentinel<-1>};

    destination = std::move(source);

    EXPECT_EQ(log.entries[0], 11);
}

TEST(ResourceTest, MoveAssignmentTransfersSourceDeleter) {
    DeleteLog log{};
    auto destination = Resource{5, LoggingDelete{11, &log}, sentinel<-1>};
    auto source = Resource{7, LoggingDelete{17, &log}, sentinel<-1>};

    destination = std::move(source);
    destination.reset();

    EXPECT_EQ(log.entries[1], 17);
}

TEST(ResourceTest, MoveAssignmentDisengagesSource) {
    DeleteLog log{};
    auto destination = Resource{5, LoggingDelete{11, &log}, sentinel<-1>};
    auto source = Resource{7, LoggingDelete{17, &log}, sentinel<-1>};

    destination = std::move(source);

    EXPECT_FALSE(source);
}

TEST(ResourceTest, MoveAssignmentSupportsNonAssignableCapturingLambda) {
    auto count = 0;
    auto deleter = [&count](int const&) noexcept -> void { ++count; };
    static_assert(!std::is_move_assignable_v<decltype(deleter)>);

    auto destination = Resource{5, deleter, sentinel<-1>};
    auto source = Resource{7, deleter, sentinel<-1>};

    destination = std::move(source);

    EXPECT_EQ(count, 1);
}

TEST(ResourceTest, SelfMoveAssignmentPreservesIdentity) {
    auto count = 0;
    auto resource = Resource{7, CountDelete{&count}};
    auto& alias = resource;

    resource = std::move(alias);

    EXPECT_EQ(resource.get(), 7);
}

TEST(ResourceTest, ReleaseRestoresNonAssignableMoveChangingSentinel) {
    auto resource =
        NonAssignableMoveChangingResource{NonAssignableMoveChangingHandle{7}, DeleteNonAssignableMoveChangingHandle{}};

    static_cast<void>(resource.release());

    EXPECT_FALSE(resource);
}

TEST(ResourceTest, SentinelStorageSupportsNonAssignableValue) {
    using NonAssignableResource = Resource<NonAssignable, DeleteNonAssignable, Sentinel<NonAssignable{-1}>>;

    auto source = NonAssignableResource{NonAssignable{7}, DeleteNonAssignable{}};
    auto destination = NonAssignableResource{NonAssignable{-1}, DeleteNonAssignable{}};

    destination = std::move(source);

    EXPECT_EQ(destination.get().value, 7);
}

TEST(ResourceTest, OptionalStorageSupportsNonAssignableValue) {
    using NonAssignableResource = Resource<OptionalNonAssignable, DeleteOptionalNonAssignable>;

    auto source = NonAssignableResource{OptionalNonAssignable{7}, DeleteOptionalNonAssignable{}};
    auto destination = NonAssignableResource{DeleteOptionalNonAssignable{}};

    destination = std::move(source);

    EXPECT_EQ(destination.get().value, 7);
}

TEST(ResourceTest, ScalarSentinelConstructionCanonicalizesDisengagedValue) {
    auto resource = MoveChangingScalarResource{MoveChangingHandle{-1}, DestroyMoveChangingHandle{}};

    EXPECT_FALSE(resource);
}

TEST(ResourceTest, ScalarMoveFromDisengagedSourceKeepsDestinationDisengaged) {
    auto source = MoveChangingScalarResource{MoveChangingHandle{-1}, DestroyMoveChangingHandle{}};

    auto destination = std::move(source);

    EXPECT_FALSE(destination);
}

TEST(ResourceTest, ScalarResetWithDisengagedValueStaysDisengaged) {
    auto resource = MoveChangingScalarResource{MoveChangingHandle{7}, DestroyMoveChangingHandle{}};

    resource.reset(MoveChangingHandle{-1});

    EXPECT_FALSE(resource);
}

TEST(ResourceTest, ProjectedSentinelUsesOnlyProjectedMemberForEngagement) {
    auto count = 0;
    auto resource = CompoundResource{Compound{31, StrongHandle{-1}}, DestroyCompound{&count}};

    EXPECT_FALSE(resource);
}

TEST(ResourceTest, ProjectedSentinelIgnoresAuxiliaryValuesForEngagement) {
    auto count = 0;
    auto resource = CompoundResource{Compound{-1, StrongHandle{7}}, DestroyCompound{&count}};

    EXPECT_TRUE(resource);
}

TEST(ResourceTest, ProjectedMoveTransfersAuxiliaryBookkeeping) {
    auto count = 0;
    auto source = CompoundResource{Compound{31, StrongHandle{7}}, DestroyCompound{&count}};

    auto destination = std::move(source);

    EXPECT_EQ(destination->parent, 31);
}

TEST(ResourceTest, ProjectedMoveDisengagesSource) {
    auto count = 0;
    auto source = CompoundResource{Compound{31, StrongHandle{7}}, DestroyCompound{&count}};

    auto destination = std::move(source);
    static_cast<void>(destination);

    EXPECT_FALSE(source);
}

TEST(ResourceTest, ProjectedConstructionCanonicalizesDisengagedProjection) {
    auto resource =
        MoveChangingResource{MoveChangingCompound{31, MoveChangingHandle{-1}}, DestroyMoveChangingCompound{}};

    EXPECT_FALSE(resource);
}

TEST(ResourceTest, ProjectedMoveFromDisengagedSourceCanonicalizesDestinationProjection) {
    auto source = MoveChangingResource{MoveChangingCompound{31, MoveChangingHandle{-1}}, DestroyMoveChangingCompound{}};

    auto destination = std::move(source);

    EXPECT_FALSE(destination);
}

TEST(ResourceTest, ProjectedMoveFromDisengagedSourceRestoresSourceProjection) {
    auto source = MoveChangingResource{MoveChangingCompound{31, MoveChangingHandle{-1}}, DestroyMoveChangingCompound{}};

    auto destination = std::move(source);
    static_cast<void>(destination);

    EXPECT_FALSE(source);
}

TEST(ResourceTest, ProjectedResetWithDisengagedValueStaysDisengaged) {
    auto resource =
        MoveChangingResource{MoveChangingCompound{31, MoveChangingHandle{7}}, DestroyMoveChangingCompound{}};

    resource.reset(MoveChangingCompound{53, MoveChangingHandle{-1}});

    EXPECT_FALSE(resource);
}

TEST(ResourceTest, EmptyTypedResourceCanAcquireIdentity) {
    auto count = 0;
    auto resource = Resource<int, CountDelete>{CountDelete{&count}};

    resource.reset(7);

    EXPECT_EQ(resource.get(), 7);
}

TEST(ResourceTest, FunctionPointerDeleterCanBeSuppliedForEmptyResource) {
    auto resource = FunctionDeleteResource{&deleteInt};

    EXPECT_FALSE(resource);
}

TEST(ResourceTest, MemberPointerDeleterCanBeSuppliedForEmptyResource) {
    auto resource = MemberDeleteResource{&MemberDeletedValue::close};

    EXPECT_FALSE(resource);
}

} // namespace
} // namespace resource
