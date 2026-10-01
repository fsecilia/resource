// SPDX-License-Identifier: MIT

/// \file
/// \brief Tests for unique external resource ownership
/// \copyright Copyright (C) 2026 Frank Secilia

#include "resource.hpp"
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdlib>
#include <gtest/gtest.h>
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

struct NonNegativeEngagement final {
    static constexpr auto engaged(int value) noexcept -> bool { return value >= 0; }

    static constexpr auto disengage(int& value) noexcept -> void { value = -1; }
};

static_assert(EngagementFor<NonNegativeEngagement, int>);

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

struct OpaqueCompound final {
    int parent;
    OpaqueHandle handle;
};

struct DeleteOpaqueCompound final {
    constexpr auto operator()(OpaqueCompound const&) const noexcept -> void {}
};

struct PlainHandle final {
    int value;
};

struct DeletePlain final {
    constexpr auto operator()(PlainHandle const&) const noexcept -> void {}
};

struct CoarselyEqualHandle final {
    int value;

    friend constexpr auto operator==(CoarselyEqualHandle const&, CoarselyEqualHandle const&) noexcept -> bool {
        return true;
    }
};

static_assert(CoarselyEqualHandle{7} == CoarselyEqualHandle{9});

struct RecordCoarselyEqualDelete final {
    int* deletedValue;

    constexpr auto operator()(CoarselyEqualHandle const& value) const noexcept -> void { *deletedValue = value.value; }
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

struct MissingSentinelValueHandle final {
    int value;
};

struct DeleteMissingSentinelValue final {
    constexpr auto operator()(MissingSentinelValueHandle const&) const noexcept -> void {}
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

struct ConstIntCompound final {
    int parent;
    int const handle;
};

struct VolatileIntCompound final {
    int parent;
    int volatile handle;
};

struct UnrelatedIntCompound final {
    int handle;
};

template <typename Value>
struct DeleteAny final {
    constexpr auto operator()(Value const&) const noexcept -> void {}
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

struct NonAssignableCompound final {
    int parent;
    NonAssignable handle;
};

struct DeleteNonAssignableCompound final {
    constexpr auto operator()(NonAssignableCompound const&) const noexcept -> void {}
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

struct PotentiallyThrowingCopyDelete final {
    PotentiallyThrowingCopyDelete() = default;
    PotentiallyThrowingCopyDelete(PotentiallyThrowingCopyDelete const&) noexcept(false) {}
    PotentiallyThrowingCopyDelete(PotentiallyThrowingCopyDelete&&) noexcept = default;

    constexpr auto operator()(int const&) const noexcept -> void {}
};

using PotentiallyThrowingCopyOwner = Resource<int, PotentiallyThrowingCopyDelete>;
using PotentiallyThrowingCopySentinelOwner = Resource<int, PotentiallyThrowingCopyDelete, Sentinel<-1>>;

static_assert(ResourceDeleter<PotentiallyThrowingCopyDelete, int>);
static_assert(!std::is_constructible_v<PotentiallyThrowingCopyOwner, PotentiallyThrowingCopyDelete&>);
static_assert(std::is_nothrow_constructible_v<PotentiallyThrowingCopyOwner, PotentiallyThrowingCopyDelete&&>);
static_assert(!std::is_constructible_v<PotentiallyThrowingCopyOwner, int, PotentiallyThrowingCopyDelete&>);
static_assert(std::is_nothrow_constructible_v<PotentiallyThrowingCopyOwner, int, PotentiallyThrowingCopyDelete&&>);
static_assert(
    !std::is_constructible_v<PotentiallyThrowingCopySentinelOwner, int, PotentiallyThrowingCopyDelete&, Sentinel<-1>>);
static_assert(std::is_nothrow_constructible_v<PotentiallyThrowingCopySentinelOwner, int,
    PotentiallyThrowingCopyDelete&&, Sentinel<-1>>);

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
    std::array<int, 3> entries{};
    std::size_t count{};
};

struct LoggingDelete final {
    int id;
    DeleteLog* log;

    auto operator()(int const&) const noexcept -> void {
        log->entries[log->count] = id;
        ++log->count;
    }
};

enum class DeleterMoveOperation {
    none,
    construction,
    assignment,
};

struct NothrowMoveAssignableDelete final {
    DeleterMoveOperation* operation;

    constexpr explicit NothrowMoveAssignableDelete(DeleterMoveOperation* observedOperation) noexcept
        : operation{observedOperation} {}

    constexpr NothrowMoveAssignableDelete(NothrowMoveAssignableDelete&& source) noexcept
        : operation{source.operation} {
        *operation = DeleterMoveOperation::construction;
    }

    constexpr auto operator=(NothrowMoveAssignableDelete&& source) noexcept -> NothrowMoveAssignableDelete& {
        operation = source.operation;
        *operation = DeleterMoveOperation::assignment;
        return *this;
    }

    constexpr auto operator()(int const&) const noexcept -> void {}
};

struct NonMoveAssignableDelete final {
    DeleterMoveOperation* operation;

    constexpr explicit NonMoveAssignableDelete(DeleterMoveOperation* observedOperation) noexcept
        : operation{observedOperation} {}

    constexpr NonMoveAssignableDelete(NonMoveAssignableDelete&& source) noexcept
        : operation{source.operation} {
        *operation = DeleterMoveOperation::construction;
    }

    constexpr auto operator=(NonMoveAssignableDelete&&) -> NonMoveAssignableDelete& = delete;

    constexpr auto operator()(int const&) const noexcept -> void {}
};

struct ThrowingMoveAssignableDelete final {
    DeleterMoveOperation* operation;

    constexpr explicit ThrowingMoveAssignableDelete(DeleterMoveOperation* observedOperation) noexcept
        : operation{observedOperation} {}

    constexpr ThrowingMoveAssignableDelete(ThrowingMoveAssignableDelete&& source) noexcept
        : operation{source.operation} {
        *operation = DeleterMoveOperation::construction;
    }

    constexpr auto operator=(ThrowingMoveAssignableDelete&& source) noexcept(false) -> ThrowingMoveAssignableDelete& {
        operation = source.operation;
        *operation = DeleterMoveOperation::assignment;
        return *this;
    }

    constexpr auto operator()(int const&) const noexcept -> void {}
};

static_assert(std::is_nothrow_move_assignable_v<NothrowMoveAssignableDelete>);
static_assert(!std::is_move_assignable_v<NonMoveAssignableDelete>);
static_assert(std::is_move_assignable_v<ThrowingMoveAssignableDelete>);
static_assert(!std::is_nothrow_move_assignable_v<ThrowingMoveAssignableDelete>);

struct OwnershipProbeDelete final {
    void const* context;
    auto (*owns)(void const*) noexcept -> bool;
    bool* observedDisengaged;

    auto operator()(int const&) const noexcept -> void { *observedDisengaged = !owns(context); }
};

template <typename Value, typename Deleter>
concept DefaultResourceFormable = requires { typename Resource<Value, Deleter>; };

template <typename Value, typename Deleter, typename Engagement>
concept ResourceFormable = requires { typename Resource<Value, Deleter, Engagement>; };

template <typename Value>
concept DefaultSentinelNameable = requires { typename DefaultSentinel<Value>; };

template <auto projection, typename SentinelType>
concept ProjectedEngagementNameable = requires { typename ProjectedEngagement<projection, SentinelType>; };

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

template <>
struct DefaultSentinel<MissingSentinelValueHandle> final {};

namespace {

using PointerResource = Resource<int*, EmptyPointerDelete>;
using OpaqueResource = Resource<OpaqueHandle, DeleteOpaque>;
using OpaqueCompoundEngagement = ProjectedEngagement<&OpaqueCompound::handle>;
using OpaqueCompoundResource = Resource<OpaqueCompound, DeleteOpaqueCompound, OpaqueCompoundEngagement>;
using FunctionDelete = void (*)(int const&) noexcept;
using FunctionDeleteResource = Resource<int, FunctionDelete>;
using MemberDelete = void (MemberDeletedValue::*)() const noexcept;
using MemberDeleteResource = Resource<MemberDeletedValue, MemberDelete>;
using CompoundEngagement = ProjectedEngagement<&Compound::handle, Sentinel<StrongHandle{-1}>>;
using CompoundResource = Resource<Compound, DestroyCompound, CompoundEngagement>;
using NonNegativeCompoundEngagement = ProjectedEngagement<&IntCompound::handle, NonNegativeEngagement>;
using NonNegativeCompoundResource = Resource<IntCompound, DestroyIntCompound, NonNegativeCompoundEngagement>;
using NonDefaultCompoundEngagement = ProjectedEngagement<&NonDefaultCompound::handle, Sentinel<-1>>;
using NonDefaultCompoundResource =
    Resource<NonDefaultCompound, DestroyNonDefaultCompound, NonDefaultCompoundEngagement>;
using MoveChangingEngagement = ProjectedEngagement<&MoveChangingCompound::handle, Sentinel<MoveChangingHandle{-1}>>;
using MoveChangingResource = Resource<MoveChangingCompound, DestroyMoveChangingCompound, MoveChangingEngagement>;
using MoveChangingScalarResource =
    Resource<MoveChangingHandle, DestroyMoveChangingHandle, Sentinel<MoveChangingHandle{-1}>>;

static_assert(sizeof(PointerResource) == sizeof(int*));
static_assert(sizeof(OpaqueResource) == sizeof(OpaqueHandle));
static_assert(sizeof(OpaqueCompoundResource) == sizeof(OpaqueCompound));
static_assert(!std::copy_constructible<PointerResource>);
static_assert(!std::is_copy_assignable_v<PointerResource>);
static_assert(std::is_nothrow_move_constructible_v<PointerResource>);
static_assert(std::is_nothrow_move_assignable_v<PointerResource>);
static_assert(std::is_default_constructible_v<PointerResource>);
static_assert(std::is_default_constructible_v<OpaqueResource>);
static_assert(!std::is_default_constructible_v<Resource<int, NoopDelete, NonNegativeEngagement>>);
static_assert(noexcept(std::declval<PointerResource&>().reset()));
static_assert(noexcept(std::declval<PointerResource&>().release()));
static_assert(std::same_as<decltype(std::declval<PointerResource const&>().get()), int* const&>);
static_assert(std::same_as<decltype(*std::declval<PointerResource const&>()), int* const&>);
static_assert(std::same_as<decltype(std::declval<PointerResource const&>().operator->()), int* const*>);

static_assert(std::same_as<decltype(Resource{static_cast<int*>(nullptr), EmptyPointerDelete{}}), PointerResource>);
static_assert(std::same_as<decltype(Resource{7, CountDelete{nullptr}}), Resource<int, CountDelete>>);
static_assert(
    std::same_as<decltype(Resource{7, CountDelete{nullptr}, sentinel<-1>}), Resource<int, CountDelete, Sentinel<-1>>>);
static_assert(std::same_as<decltype(Resource{7, CountDelete{nullptr}, NonNegativeEngagement{}}),
    Resource<int, CountDelete, NonNegativeEngagement>>);
static_assert(std::same_as<decltype(Resource{StrongHandle{7}, CountStrongDelete{nullptr}}),
    Resource<StrongHandle, CountStrongDelete>>);
static_assert(std::same_as<decltype(Resource{EnumHandle::valid, DeleteEnum{}}), Resource<EnumHandle, DeleteEnum>>);
static_assert(std::same_as<decltype(Resource{OpaqueHandle{7}, DeleteOpaque{}}), OpaqueResource>);
static_assert(std::same_as<decltype(Resource{Compound{3, StrongHandle{7}}, DestroyCompound{nullptr},
                               projectedSentinel<&Compound::handle, StrongHandle{-1}>}),
    CompoundResource>);
static_assert(std::same_as<decltype(Resource{IntCompound{3, 7}, DestroyIntCompound{},
                               projectedEngagement<&IntCompound::handle, NonNegativeEngagement>}),
    NonNegativeCompoundResource>);
static_assert(std::same_as<decltype(Resource{OpaqueCompound{3, OpaqueHandle{7}}, DeleteOpaqueCompound{},
                               projectedEngagement<&OpaqueCompound::handle>}),
    OpaqueCompoundResource>);

static_assert(!DefaultSentinelNameable<int>);
static_assert(!DefaultSentinelNameable<int*>);
static_assert(DefaultSentinelNameable<StrongHandle>);
static_assert(DefaultSentinelNameable<EnumHandle>);
static_assert(DefaultSentinelNameable<OpaqueHandle>);
static_assert(ProjectedEngagementNameable<&IntCompound::handle, Sentinel<-1>>);
static_assert(!ProjectedEngagementNameable<nullptr, Sentinel<-1>>);

static_assert(!ResourceFormable<ConstIntCompound, DeleteAny<ConstIntCompound>,
    ProjectedEngagement<&ConstIntCompound::handle, Sentinel<-1>>>);
static_assert(!ResourceFormable<VolatileIntCompound, DeleteAny<VolatileIntCompound>,
    ProjectedEngagement<&VolatileIntCompound::handle, Sentinel<-1>>>);
static_assert(!ResourceFormable<IntCompound, DeleteAny<IntCompound>,
    ProjectedEngagement<&UnrelatedIntCompound::handle, Sentinel<-1>>>);
static_assert(!ResourceFormable<IntCompound, DeleteAny<IntCompound>,
    ProjectedEngagement<&IntCompound::handle, Sentinel<nullptr>>>);
static_assert(
    ResourceFormable<IntCompound, DeleteAny<IntCompound>, ProjectedEngagement<&IntCompound::handle, Sentinel<-1>>>);
static_assert(ResourceFormable<IntCompound, DeleteAny<IntCompound>, NonNegativeCompoundEngagement>);
static_assert(EngagementFor<OpaqueCompoundEngagement, OpaqueCompound>);

static_assert(!DefaultResourceFormable<int, ReturningDelete>);
static_assert(!DefaultResourceFormable<int, ThrowingDelete>);
static_assert(!DefaultResourceFormable<ThrowingMoveValue, DeleteThrowingMoveValue>);
static_assert(MalformedHandle{-1} == MalformedHandle{-1});
static_assert(!DefaultResourceFormable<MalformedHandle, DeleteMalformed>);
static_assert(!DefaultResourceFormable<MissingSentinelValueHandle, DeleteMissingSentinelValue>);
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

static_assert(!ResourceFormable<NonAssignable, DeleteNonAssignable, Sentinel<NonAssignable{-1}>>);
static_assert(!ResourceFormable<NonAssignableCompound, DeleteNonAssignableCompound,
    ProjectedEngagement<&NonAssignableCompound::handle, Sentinel<NonAssignable{-1}>>>);
static_assert(std::is_move_assignable_v<Resource<OptionalNonAssignable, DeleteOptionalNonAssignable>>);
static_assert(std::is_move_assignable_v<Resource<int, NothrowMoveAssignableDelete, Sentinel<-1>>>);
static_assert(std::is_nothrow_move_constructible_v<Resource<int, NonMoveAssignableDelete, Sentinel<-1>>>);
static_assert(!std::is_move_assignable_v<Resource<int, NonMoveAssignableDelete, Sentinel<-1>>>);
static_assert(std::is_nothrow_move_constructible_v<Resource<int, ThrowingMoveAssignableDelete, Sentinel<-1>>>);
static_assert(!std::is_move_assignable_v<Resource<int, ThrowingMoveAssignableDelete, Sentinel<-1>>>);

constexpr auto constantEvaluationScenario() -> bool {
    auto count = 0;
    auto first = 7;
    auto source = Resource<int, CountDelete, Sentinel<-1>>{first, CountDelete{&count}};
    auto moved = std::move(source);

    auto replacement = 9;
    moved.reset(replacement);

    auto destination = Resource<int, CountDelete, Sentinel<-1>>{11, CountDelete{&count}};
    destination = std::move(moved);
    auto const released = destination.release();

    return count == 2 && !source && !moved && !destination && released == 9;
}

static_assert(constantEvaluationScenario());

TEST(ResourceTest, PointerNullIsDisengagedByDefault) {
    auto resource = Resource{static_cast<int*>(nullptr), EmptyPointerDelete{}};

    EXPECT_FALSE(resource);
}

TEST(ResourceTest, ProjectedDefaultSentinelSupportsNonStructuralHandle) {
    auto resource = OpaqueCompoundResource{OpaqueCompound{31, OpaqueHandle{-1}}, DeleteOpaqueCompound{}};

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

TEST(ResourceTest, PredicateEngagementCanRecognizeMultipleDisengagedValues) {
    auto count = 0;
    auto resource = Resource<int, CountDelete, NonNegativeEngagement>{-7, CountDelete{&count}};

    EXPECT_FALSE(resource);
}

TEST(ResourceTest, PredicateEngagementUsesItsCanonicalDisengagementTransition) {
    auto count = 0;
    auto resource = Resource<int, CountDelete, NonNegativeEngagement>{7, CountDelete{&count}};

    static_cast<void>(resource.release());

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

TEST(ResourceTest, LvalueConstructionCopiesIdentity) {
    auto count = 0;
    auto value = 7;
    auto resource = Resource{value, CountDelete{&count}};
    value = 9;

    EXPECT_EQ(resource.get(), 7);
}

TEST(ResourceTest, LvalueResetCopiesIdentity) {
    auto count = 0;
    auto resource = Resource{7, CountDelete{&count}};
    auto replacement = 9;

    resource.reset(replacement);
    replacement = 11;

    EXPECT_EQ(resource.get(), 9);
}

TEST(ResourceTest, DefaultConstructedResourceIsDisengaged) {
    auto resource = Resource<int, NoopDelete>{};

    EXPECT_FALSE(resource);
}

TEST(ResourceTest, DereferenceObservesIdentity) {
    auto count = 0;
    auto resource = Resource{7, CountDelete{&count}};

    EXPECT_EQ(*resource, 7);
}

TEST(ResourceTest, ResetDoesNotRequireEqualityComparison) {
    auto resource = Resource{PlainHandle{7}, DeletePlain{}};

    resource.reset(PlainHandle{9});

    EXPECT_EQ(resource.get().value, 9);
}

TEST(ResourceTest, ResetDoesNotUseValueEqualityAsResourceIdentity) {
    auto deletedValue = 0;
    auto resource = Resource{CoarselyEqualHandle{7}, RecordCoarselyEqualDelete{&deletedValue}};

    resource.reset(CoarselyEqualHandle{9});

    EXPECT_EQ(deletedValue, 7);
    EXPECT_EQ(resource.get().value, 9);
}

TEST(ResourceTest, ResetRejectsOwnedValueReference) {
#ifdef NDEBUG
    GTEST_SKIP() << "internal assertions are compiled out (NDEBUG)";
#else
    auto count = 0;
    auto resource = Resource{7, CountDelete{&count}};

    EXPECT_DEBUG_DEATH(resource.reset(resource.get()), "");
#endif
}

TEST(ResourceTest, DestructionInvokesDeleterExactlyOnce) {
    auto count = 0;

    {
        auto resource = Resource{7, CountDelete{&count}};
    }

    EXPECT_EQ(count, 1);
}

TEST(ResourceTest, DestructionPassesOwnedIdentityToDeleter) {
    auto deletedValue = 0;

    {
        auto resource = Resource{CoarselyEqualHandle{7}, RecordCoarselyEqualDelete{&deletedValue}};
    }

    EXPECT_EQ(deletedValue, 7);
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
    auto resourceAddress = static_cast<ObservedResource*>(nullptr);
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

TEST(ResourceTest, SentinelMoveConstructedPairDeletesOnce) {
    auto count = 0;

    {
        auto source = Resource{7, CountDelete{&count}, sentinel<-1>};
        auto destination = std::move(source);
        static_cast<void>(destination);
    }

    EXPECT_EQ(count, 1);
}

TEST(ResourceTest, MoveAssignmentDestroysWithDestinationDeleter) {
    auto log = DeleteLog{};
    auto destination = Resource{5, LoggingDelete{11, &log}, sentinel<-1>};
    auto source = Resource{7, LoggingDelete{17, &log}, sentinel<-1>};

    destination = std::move(source);

    EXPECT_EQ(log.entries[0], 11);
}

TEST(ResourceTest, MoveAssignmentTransfersSourceDeleter) {
    auto log = DeleteLog{};
    auto destination = Resource{5, LoggingDelete{11, &log}, sentinel<-1>};
    auto source = Resource{7, LoggingDelete{17, &log}, sentinel<-1>};

    destination = std::move(source);
    destination.reset();

    EXPECT_EQ(log.entries[1], 17);
}

TEST(ResourceTest, MoveAssignmentDisengagesSource) {
    auto log = DeleteLog{};
    auto destination = Resource{5, LoggingDelete{11, &log}, sentinel<-1>};
    auto source = Resource{7, LoggingDelete{17, &log}, sentinel<-1>};

    destination = std::move(source);

    EXPECT_FALSE(source);
}

TEST(ResourceTest, MoveAssignmentUsesNothrowDeleterAssignment) {
    auto operation = DeleterMoveOperation::none;
    auto destination = Resource{5, NothrowMoveAssignableDelete{&operation}, sentinel<-1>};
    auto source = Resource{7, NothrowMoveAssignableDelete{&operation}, sentinel<-1>};
    operation = DeleterMoveOperation::none;

    destination = std::move(source);

    EXPECT_EQ(operation, DeleterMoveOperation::assignment);
}

TEST(ResourceTest, SelfMoveAssignmentPreservesIdentity) {
    auto count = 0;
    auto resource = Resource{7, CountDelete{&count}};
    auto& alias = resource;

    resource = std::move(alias);

    EXPECT_EQ(resource.get(), 7);
}

TEST(ResourceTest, OptionalStorageSupportsNonAssignableValue) {
    using NonAssignableResource = Resource<OptionalNonAssignable, DeleteOptionalNonAssignable>;

    auto source = NonAssignableResource{OptionalNonAssignable{7}, DeleteOptionalNonAssignable{}};
    auto destination = NonAssignableResource{DeleteOptionalNonAssignable{}};

    destination = std::move(source);

    EXPECT_EQ(destination.get().value, 7);
}

TEST(ResourceTest, OptionalStorageCanResetNonAssignableValue) {
    auto resource = Resource<OptionalNonAssignable, DeleteOptionalNonAssignable>{DeleteOptionalNonAssignable{}};

    resource.reset(OptionalNonAssignable{7});

    EXPECT_EQ(resource.get().value, 7);
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

TEST(ResourceTest, ProjectedEngagementUsesOnlyProjectedMemberForEngagement) {
    auto count = 0;
    auto resource = CompoundResource{Compound{31, StrongHandle{-1}}, DestroyCompound{&count}};

    EXPECT_FALSE(resource);
}

TEST(ResourceTest, ProjectedEngagementIgnoresAuxiliaryValuesForEngagement) {
    auto count = 0;
    auto resource = CompoundResource{Compound{-1, StrongHandle{7}}, DestroyCompound{&count}};

    EXPECT_TRUE(resource);
}

TEST(ResourceTest, ProjectedEngagementCanProjectPredicateEngagement) {
    auto resource = NonNegativeCompoundResource{IntCompound{31, -7}, DestroyIntCompound{}};

    EXPECT_FALSE(resource);
}

TEST(ResourceTest, ProjectedPredicateEngagementDisengagesMovedFromSource) {
    auto source = NonNegativeCompoundResource{IntCompound{31, 7}, DestroyIntCompound{}};

    auto destination = std::move(source);
    static_cast<void>(destination);

    EXPECT_FALSE(source);
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

TEST(ResourceTest, ProjectedMoveAssignmentDisengagesSource) {
    auto count = 0;
    auto destination = CompoundResource{Compound{31, StrongHandle{5}}, DestroyCompound{&count}};
    auto source = CompoundResource{Compound{53, StrongHandle{7}}, DestroyCompound{&count}};

    destination = std::move(source);

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

TEST(ResourceTest, GetOnDisengagedResourceDies) {
#ifdef NDEBUG
    GTEST_SKIP() << "internal assertions are compiled out (NDEBUG)";
#else
    auto resource = Resource<int, NoopDelete>{};

    EXPECT_DEBUG_DEATH(static_cast<void>(resource.get()), "");
#endif
}

TEST(ResourceTest, DereferenceOnDisengagedResourceDies) {
#ifdef NDEBUG
    GTEST_SKIP() << "internal assertions are compiled out (NDEBUG)";
#else
    auto resource = Resource<int, NoopDelete>{};

    EXPECT_DEBUG_DEATH(static_cast<void>(*resource), "");
#endif
}

TEST(ResourceTest, ArrowOnDisengagedResourceDies) {
#ifdef NDEBUG
    GTEST_SKIP() << "internal assertions are compiled out (NDEBUG)";
#else
    auto resource = Resource<int, NoopDelete>{};

    EXPECT_DEBUG_DEATH(static_cast<void>(resource.operator->()), "");
#endif
}

TEST(ResourceTest, ReleaseOnDisengagedResourceDies) {
#ifdef NDEBUG
    GTEST_SKIP() << "internal assertions are compiled out (NDEBUG)";
#else
    auto resource = Resource<int, NoopDelete>{};

    EXPECT_DEBUG_DEATH(static_cast<void>(resource.release()), "");
#endif
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
