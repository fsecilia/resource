# Resource Library Design Ledger

## Status

This is the design ledger for the standalone resource-ownership library being developed from first principles.

No production commits have been made yet. The library/repository name is `resource`, the root namespace is `resource`, and `Resource` is the public owner type. Production C++ lives under `src/resource/`; the primary public header is `resource/resource.hpp`.

The attached `resource.tar.gz` is archaeological reference material from Axiom, not a specification. It also contains the current standards submodule. Before production code is written, re-read the C++ standards documents in that archive and keep the implementation aligned with them.

Prototype spikes in this thread were exercised with GCC 14.2 and Clang 17, with sanitizer passes where noted. The design is moving quickly, so code in this ledger records semantics and useful implementation shapes rather than claiming final source layout.

## Core goal

Build the analogue of `std::unique_ptr` for arbitrary external resource identities, without pointer ownership semantics.

A better mental model emerged during design:

> `Resource<T>` is approximately an owning `std::optional<T>` whose disengagement destroys the external resource represented by `T`.

This is only an analogy. The API should remain smaller than `std::optional` where optional-specific facilities do not help.

Primary requirements:

- unique ownership: `Resource` is noncopyable;
- substitutable cleanup behavior;
- optimize identities with an in-band disengaged value;
- fall back to `std::optional` storage when there is no useful sentinel;
- support handles as well as compound identities;
- keep the owned identity immutable through ordinary observation;
- make move operations unconditionally `noexcept` through explicit template constraints;
- express type validity with `requires`/concepts on the relevant declarations rather than requirement-enforcing `static_assert`s inside implementations;
- permit capturing lambdas as deleters;
- avoid hidden allocation or indirection;
- later consider a collection facade that shares destruction context across many resource identities.

## Naming

Current public owner name:

```cpp
Resource<Value, Deleter, Disengagement>
```

Use `Resource` while unique ownership is the only ownership model.

Possible reasons to rename it later:

- a legitimate `SharedResource` appears, making `UniqueResource` useful;
- the design deliberately converges on the published `unique_resource` vocabulary.

The library/repository name is `resource`.

Current vocabulary/layout:

```text
repository      resource
namespace       resource
source root     src/resource/
public include  <resource/resource.hpp>
primary type    resource::Resource
```

Do not add another `resource/` layer beneath `src/resource/`.

Do not introduce pattern-specific terminology for the Vulkan bookkeeping object merely because it resembles a known pattern. In particular, avoid using “memento” as project vocabulary or in examples. “Capability” describes part of the semantics but is also not currently a chosen name.

## What the Axiom code taught us

The old design had a useful two-dimensional split:

```text
ownership/lifetime
+
representation of disengagement
```

That split survives.

The old adapter mixed too many responsibilities: storage, engagement, mutation, and access. The new design separates public disengagement policy from private storage implementation.

The old API/context pairing was not arbitrary. It existed so runtime operations and destruction could share one copy of expensive API bookkeeping. The exact old type should not be rebuilt, but compound identities and projected sentinels preserve the legitimate use case.

The old `Resource<void>`-style reuse should not return. A scope guard is a separate abstraction.

## Resource identity

`Value` represents the identity and bookkeeping required to operate on and release the external resource.

It need not be a scalar handle. These are all legitimate shapes:

```cpp
using FileIdentity = int;

struct MappingIdentity {
    void* address;
    std::size_t size;
};

struct ImageIdentity {
    VkDevice device;
    VkImage handle;
};

struct ImageIdentityWithDispatch {
    VkDevice device;
    PFN_vkDestroyImage destroy;
    VkImage handle;
};
```

The resource value is identity, not mutable application state.

Ordinary observation is therefore const-only:

```cpp
auto get() const noexcept -> Value const&;
auto operator*() const noexcept -> Value const&;
auto operator->() const noexcept -> Value const*;
```

`operator*` and `operator->` follow `std::optional` semantics rather than `std::unique_ptr` semantics.

For `Value = Widget*`, `operator->()` therefore returns `Widget* const*`. It refers to the contained value; it does not recursively adopt smart-pointer semantics.

Do not provide mutable `get()`, mutable `operator*`, or mutable `operator->`.

Changing the stored identity through an ordinary reference could leak the old resource or silently turn an engaged sentinel-backed resource into a disengaged one.

## Deleter

There is no default deleter.

A pointer-shaped identity does not imply `delete`. Vulkan handles, `FILE*`, library handles, and externally allocated pointers demonstrate why a generic pointer default would be semantically unsafe.

The deleter is explicit:

```cpp
auto file = Resource{
    rawFile,
    [](FILE* const& file) noexcept {
        std::fclose(file);
    },
};
```

A capturing lambda is a first-class use case:

```cpp
auto image = Resource{
    rawImage,
    [destroyImage, device](VkImage const& image) noexcept {
        destroyImage(device, image, nullptr);
    },
};
```

A named deleter is preferred when cleanup behavior deserves a reusable type or the `Resource` type must be spelled as a member.

Current intended deleter contract:

```cpp
template <typename Deleter, typename Value>
concept ResourceDeleter =
    std::is_object_v<Deleter> &&
    std::is_nothrow_move_constructible_v<Deleter> &&
    requires(Deleter& deleter, Value const& value) {
        { std::invoke(deleter, value) } noexcept -> std::same_as<void>;
    };
```

The exact-`void` return is intentional unless a later use case disproves it. A cleanup function that reports failure should not have that result silently discarded merely because it was supplied directly as a deleter. A wrapper lambda can make the policy explicit.

Do not expose `deleter()` unless a real use case appears. Tests can observe cleanup behavior externally.

## Replacing a deleter during move assignment

Capturing lambda closure types are commonly move-constructible but not move-assignable.

`Resource` therefore must not require a move-assignable deleter.

When replacing an existing deleter, prefer move assignment when it is available and nothrow; otherwise destroy and reconstruct:

```cpp
template <typename T>
requires std::is_nothrow_move_constructible_v<T>
constexpr auto replaceFromMove(T& target, T&& source) noexcept -> void
{
    if constexpr (std::is_nothrow_move_assignable_v<T>) {
        target = std::move(source);
    } else {
        std::destroy_at(std::addressof(target));
        std::construct_at(
            std::addressof(target),
            std::move(source));
    }
}
```

The fundamental `Resource` requirement remains nothrow move construction because the `Resource` move constructor has to construct a fresh deleter.

Assignment is an optimization, not a semantic requirement.

## Disengagement and sentinel policy

The public third template parameter describes disengagement semantics, not a storage implementation.

Target shape:

```cpp
Resource<Value, Deleter, Disengagement>
```

Likely policies:

```cpp
Sentinel<sentinelValue>
ProjectedSentinel<projection, sentinelValue>
```

The default disengagement representation is selected from what the library can know about `Value`.

Pointers automatically use `nullptr` as a sentinel and do not consult a customization point.

Distinct class and enum identities may specialize:

```cpp
template <typename Value>
struct DefaultSentinel;
```

with a `static constexpr value`. Fundamental and pointer types are constrained out of this customization point so C typedef aliases cannot globally change the meaning of an underlying integer or pointer type. Such handles use an explicit `Sentinel<...>` at the use site.

A class or enum with no `DefaultSentinel` uses private optional-backed storage. A malformed `DefaultSentinel` customization must make the default `Resource<Value, Deleter>` form ill-formed rather than silently falling back to optional storage.

`ProjectedSentinel` is the accepted name. The first implementation accepts member-object pointers only. The public `auto` NTTP shape deliberately permits that constraint to be relaxed later without changing existing member-pointer spellings.

Examples:

```cpp
Resource<VkImage, DestroyImage, Sentinel<VK_NULL_HANDLE>>
```

and for a compound identity:

```cpp
struct ImageIdentity {
    VkDevice device;
    VkImage handle;
};

Resource<
    ImageIdentity,
    DestroyImage,
    ProjectedSentinel<&ImageIdentity::handle, VK_NULL_HANDLE>>
```

For projected engagement, only the projected identity component determines engagement and is explicitly restored to the sentinel when storage disengages.

Do not promise that every non-projected field remains bit-for-bit or value-for-value unchanged across `release()` or a move. Transferring the full `Value` may move those fields. The important contract is that the source becomes disengaged by restoring the projected component to its sentinel, while the destination receives the transferred bookkeeping. Trivially movable Vulkan bookkeeping such as raw handles and function pointers will naturally retain ordinary value semantics.

## Sentinel syntax and CTAD

The easy local case should use CTAD:

```cpp
auto resource = Resource{value, deleter};
```

An explicit sentinel should also work without a factory:

```cpp
auto resource = Resource{
    handle,
    deleter,
    sentinel<VK_NULL_HANDLE>,
};
```

The policy type can be represented by a tag:

```cpp
template <auto sentinelValue>
struct Sentinel {};

template <auto sentinelValue>
inline constexpr Sentinel<sentinelValue> sentinel{};
```

The production constructors provide sufficient implicit deduction guides; no explicit CTAD guides are currently needed to turn the third tag argument into the third `Resource` template argument.

Typed sentinels remain available when useful:

```cpp
Sentinel<VkImage{VK_NULL_HANDLE}>
```

but should not be required when the sentinel expression is already compatible with the deduced `Value`.

For aliased C handle types, explicit sentinel syntax is semantically important. If an integer-like value is constructed without a sentinel policy, optional-backed storage considers every integer value engaged.

## Sentinel values are disengagement, not errors

Constructing or resetting sentinel-backed storage with the sentinel itself means disengaged.

```cpp
auto image = Resource{
    VK_NULL_HANDLE,
    destroyImage,
    sentinel<VK_NULL_HANDLE>,
};

assert(!image.owns());
```

Likewise:

```cpp
image.reset(VK_NULL_HANDLE);
```

destroys the old resource if one is owned, stores the sentinel, and leaves the resource disengaged.

This is analogous to constructing or resetting a `std::unique_ptr` with `nullptr`.

## Private storage implementations

Expected private storage implementations are conceptually:

```text
OptionalStorage<Value>
SentinelStorage<Value, sentinel>
ProjectedSentinelStorage<Value, projection, sentinel>
```

Exact names and file layout are not settled.

Storage is responsible only for zero-or-one identity representation and transfer.

It does not know about the deleter.

It must support observation/release and real ownership-transferring move operations.

Storage move construction and move assignment must explicitly disengage the source.

Storage move assignment is only valid into a disengaged destination because storage has no deleter and therefore cannot destroy an existing external resource. This precondition should be asserted internally.

## Do not force difficult identities through OptionalStorage

A sentinel-backed identity must not be forced to optional storage merely because it is not move-assignable or copy-assignable.

Assignment is an implementation optimization.

For an owned value, moving should prefer move assignment when possible and otherwise reconstruct the contained `Value`:

```cpp
template <typename T>
requires std::is_nothrow_move_constructible_v<T>
constexpr auto replaceFromMove(T& target, T&& source) noexcept -> void
{
    if constexpr (std::is_nothrow_move_assignable_v<T>) {
        target = std::move(source);
    } else {
        std::destroy_at(std::addressof(target));
        std::construct_at(
            std::addressof(target),
            std::move(source));
    }
}
```

Restoring a sentinel should similarly assign when possible and otherwise reconstruct from the sentinel.

The owned identity is moved.

The NTTP sentinel is copied because it is a reusable disengaged value and cannot be consumed.

Do not introduce a sentinel factory unless a real resource has a useful sentinel that cannot be copied. Current resource identities and handles are expected to have ordinary value semantics.

## Storage move semantics

Storage should genuinely move rather than exposing `adopt()`/`release()` as a substitute for moving the storage object.

The transfer operation must distinguish an engaged source from an already-disengaged source. Do not assume that moving a sentinel value produces another value that still compares equal to the sentinel for every possible `Value`. An already-disengaged scalar-sentinel source should produce the canonical sentinel in the destination.

`std::exchange` is the preferred fast path when restoring the sentinel can use nothrow assignment:

```cpp
return std::exchange(value_, sentinelValue);
```

This moves the owned identity out and copies the reusable NTTP sentinel back in. If sentinel assignment is unavailable but sentinel construction is valid and nothrow, reconstruct the contained `Value` from the sentinel instead. Do not force the type through optional storage merely because assignment is unavailable.

A scalar sentinel storage move is therefore conceptually:

```cpp
constexpr SentinelStorage(SentinelStorage&& source) noexcept
    : value_{sentinelValue}
{
    if (source.owns()) {
        replaceFromMove(value_, std::move(source.value_));
        disengage(source.value_);
    }
}

constexpr auto operator=(SentinelStorage&& source) noexcept
    -> SentinelStorage&
{
    if (this == std::addressof(source)) {
        return *this;
    }

    assert(!owns());

    if (source.owns()) {
        replaceFromMove(value_, std::move(source.value_));
        disengage(source.value_);
    }

    return *this;
}
```

The exact implementation may factor this through a `take()` helper so the move constructor can directly initialize `value_` without first constructing a sentinel. The semantic requirements matter more than this sketch: transfer an engaged identity by move, canonicalize the source as disengaged, and keep an already-disengaged transfer disengaged.

Projected-sentinel storage is slightly different because a disengaged `Value` may still contain non-projected bookkeeping. Its move operation should transfer the full `Value` according to normal move semantics and then restore the source projection to the sentinel. The policy controls engagement, not a promise that auxiliary fields survive a move unchanged.

Optional-backed storage should provide the same ownership-transfer guarantee. Defaulted `std::optional` move assignment is not enough because moving an optional does not necessarily disengage the source.

A suitable conceptual implementation is:

```cpp
constexpr OptionalStorage(OptionalStorage&& source) noexcept
{
    if (source.owns()) {
        value_.emplace(source.release());
    }
}

constexpr auto operator=(OptionalStorage&& source) noexcept
    -> OptionalStorage&
{
    if (this == std::addressof(source)) {
        return *this;
    }

    assert(!owns());

    if (source.owns()) {
        value_.emplace(source.release());
    }

    return *this;
}
```

The exact code may change, but the source-disengagement invariant must not.

## Resource move semantics

`Resource` has unique ownership and is not copyable:

```cpp
Resource(Resource const&) = delete;
auto operator=(Resource const&) -> Resource& = delete;
```

Move construction transfers deleter state and storage ownership.

Conceptual shape:

```cpp
constexpr Resource(Resource&& source) noexcept
    : deleter_{std::move(source.deleter_)},
      storage_{std::move(source.storage_)}
{}
```

Move assignment must preserve cleanup ordering:

```cpp
constexpr auto operator=(Resource&& source) noexcept -> Resource&
{
    if (this == std::addressof(source)) {
        return *this;
    }

    reset();

    replaceFromMove(deleter_, std::move(source.deleter_));
    storage_ = std::move(source.storage_);

    return *this;
}
```

The ordering is significant:

```text
destroy destination identity with destination deleter
replace destination deleter with source deleter
transfer source storage into the now-disengaged destination
```

After the operation, the source must be disengaged.

All custom move operations are `noexcept`.

## Construction and empty state

Primary construction preserves the engagement state before any ownership-transfer move:

```cpp
Resource(Value const& value, Deleter deleter); // when Value is copy-constructible
Resource(Value&& value, Deleter deleter);
```

The same overload pair exists for the explicit third policy tag. A by-value `Value` parameter is intentionally avoided: constructing that parameter could move a sentinel before storage has a chance to observe whether the incoming identity is disengaged.

An explicitly typed resource may also begin empty **when the selected storage can itself construct a disengaged state**. This is automatic for optional-backed storage and ordinary scalar sentinels. A projected-sentinel policy over a compound `Value` may not have enough information to construct the non-projected fields, so empty construction must be constrained rather than silently adding optional storage or inventing placeholder state.

When the deleter is nothrow default-constructible, is neither a pointer nor a member pointer, and the storage is disengaged-constructible:

```cpp
Resource<Value, Deleter> resource{};
```

Pointer and member-pointer deleters are excluded from zero-argument construction because their value-initialized state is null. A supplied deleter remains valid through the deleter-only constructor.

For a stateful deleter, subject to the same storage constraint:

```cpp
Resource<Value, Deleter> resource{deleter};
```

These forms can later acquire an identity through `reset(value)`. The exact constructor constraints remain part of the cohesion pass; do not manufacture an empty compound identity merely to make these constructors universally available.

CTAD cannot infer `Value` from the deleter-only form, and that is acceptable. Delayed acquisition is most useful for members and other contexts where the resource type is already spelled.

## Ownership operations

Current target surface:

```cpp
explicit operator bool() const noexcept;

auto owns() const noexcept -> bool;

auto get() const noexcept -> Value const&;
auto operator*() const noexcept -> Value const&;
auto operator->() const noexcept -> Value const*;

[[nodiscard]]
auto release() noexcept -> Value;

auto reset() noexcept -> void;
auto reset(Value const& value) -> void;
auto reset(Value&& value) noexcept -> void;
```

`release()` requires engagement, returns the identity, and disengages without invoking the deleter.

`reset()` destroys the owned resource when engaged and leaves the object disengaged. The intended ordering is to transfer the identity out of storage first, making `*this` disengaged, and only then invoke the deleter on that transferred identity. This keeps the ownership state coherent even if cleanup re-enters code that can observe the `Resource`.

`reset(value)` prepares a temporary selected storage from the incoming identity before the old resource is destroyed. The lvalue overload copies into that temporary; the rvalue overload observes engagement before moving. It then follows the same disengage-before-delete rule for the previous identity before moving the prepared storage into the now-disengaged destination. Passing the sentinel to a sentinel-backed resource leaves it disengaged.

This storage-temporary shape is intentional. A by-value public parameter could move a sentinel before `Resource` can observe it, contradicting the rule that moved sentinel values need not remain sentinel-equal.

`get()`, `release()`, `operator*`, and `operator->` have an engagement precondition and should assert it in debug builds.

No throwing `value()` API is currently justified.

No explicit `operator!` is needed because contextual conversion through `explicit operator bool()` already supports `!resource`.

## Self-identity reset

This is a programming error for copyable handles:

```cpp
resource.reset(resource.get());
```

The old identity would be destroyed and then its now-dead identity adopted again.

Current leaning: add an opportunistic debug assertion only when `Value` already supports nothrow equality comparison. Do not add equality comparability as a requirement of `Resource`.

Conceptual shape:

```cpp
if constexpr (requires(Value const& left, Value const& right) {
                  { left == right } noexcept
                      -> std::convertible_to<bool>;
              }) {
    if (owns()) {
        assert(value != get());
    }
}
```

This remains a design point to confirm during the cohesion pass.

## Deliberately omitted surface

Do not add these without a concrete use case:

```text
mutable identity access
deleter()
storage()
value()
converting Resource moves
default deleter
Resource<void>
```

Converting moves in particular entangle value conversion, engagement/storage conversion, and stateful deleter conversion for little demonstrated benefit.

Explicit release and reconstruction at the call site is clearer when changing identity types.

## Size objective

For a pointer identity with an empty deleter, the nullable representation should collapse to one pointer:

```cpp
struct EmptyDelete {
    auto operator()(int* const&) const noexcept -> void {}
};

static_assert(
    sizeof(Resource<int*, EmptyDelete>) == sizeof(int*));
```

`[[no_unique_address]]` should be used for empty deleters and similar policy state where appropriate.

The optional-backed representation pays its engagement-state cost only for values that need it.

## Vulkan: generic conclusion

`Resource` now appears able to support all Vulkan representation choices identified so far.

The Vulkan library, and sometimes each individual Vulkan wrapper, can choose locally among:

```text
loader-dispatched core/WSI call
cached device/instance function pointer
narrow copied dispatch/bookkeeping state
larger copied API object
pointer/reference to stable external API state
proc-address lookup during cold teardown
compound identity containing parent handle + child handle
projected-sentinel engagement for compound identity
```

The generic resource library should not choose among these.

## Vulkan loader-dispatched calls

For core commands and loader-exported WSI commands, direct Vulkan calls can be used:

```cpp
vkDestroyImage(device, image, nullptr);
```

This pays the loader dispatch/trampoline rather than storing and calling a device-specific function pointer.

For cold teardown, the trampoline overhead is likely much less important than adding another pointer to every long-lived resource.

Extension-only commands may still require `vkGetInstanceProcAddr` or `vkGetDeviceProcAddr`.

For a cold extension-only destroy operation, resolving the function during teardown is a legitimate space/time trade rather than retaining a PFN in every resource.

For hot operations, retaining resolved function pointers or a dispatch table may still be preferable.

A pointer returned by `vkGetDeviceProcAddr` is associated with the device dispatch path used to obtain it and is valid for that device and its children; do not assume one resolved device command pointer is interchangeable across arbitrary devices. This is one reason copied per-resource bookkeeping may need both the parent raw handle and a resolved function pointer when the Vulkan layer chooses direct dispatch.

This policy belongs to the Vulkan layer.

## Vulkan parent/child relocation

Children will inherently escape their originators. The planned top-level Vulkan graph is also produced by a factory, so its public value types are expected to remain genuinely movable rather than merely relying on guaranteed copy elision plus post-construction seating restrictions.

Therefore a design where children retain pointers or references into a freely movable parent does not scale. A closed aggregate can repair internal references during a custom move, but an originator cannot reasonably track and repair arbitrary escaped children. “Movable until children exist” or “movable only before seating” is a materially different contract and is not the preferred general Vulkan model.

Do not solve this with heap allocation merely to seat the parent API table.

If a child must remain freely movable, it should retain stable bookkeeping by value: parent raw handle, necessary function pointers, allocator information, or another narrow copied object selected by the parent.

The parent may construct children directly:

```cpp
auto image = device.createImage(...);
```

A separate image factory should exist only if there is a genuine reason to pass image-creation authority independently of the whole device interface.

The parent can deliberately package private bookkeeping for a child without exposing those internals publicly. This preserves encapsulation even though raw handles or selected function pointers cross the internal boundary.

## Vulkan compound identities

The destruction dependency graph is not necessarily identical to the construction graph. For example, destroying a Vulkan device does not require retaining its creating instance solely for `vkDestroyDevice`, while destroying an image does require the logical device. Preserve only the state actually required by each wrapper.

A simple core image may need only:

```cpp
struct ImageIdentity {
    VkDevice device;
    VkImage handle;
};
```

with:

```cpp
struct DestroyImage {
    auto operator()(ImageIdentity const& image) const noexcept -> void
    {
        vkDestroyImage(
            image.device,
            image.handle,
            nullptr);
    }
};
```

A more specialized child may carry selected resolved functions:

```cpp
struct ImageIdentity {
    VkDevice device;
    PFN_vkDestroyImage destroy;
    VkImage handle;
};
```

or a private narrow bookkeeping type supplied by `Device`.

`ProjectedSentinel<&ImageIdentity::handle, VK_NULL_HANDLE>` allows such payloads to remain sentinel-optimized while preserving non-handle bookkeeping when disengaged.

## Vulkan custom allocators

A Vulkan object created with non-null `VkAllocationCallbacks` must be destroyed with a compatible allocator. If no allocator was supplied at creation, destruction must receive `nullptr`.

Therefore custom allocator bookkeeping may also be part of a Vulkan resource's retained destruction context.

The generic `Resource` model already permits this through the identity or deleter. No new generic mechanism is currently needed.

The Vulkan wrapper must also respect the lifetime requirements of callback functions and any callback user data.

## Vulkan resources that are not ordinary individually-owned handles

Not every Vulkan handle should automatically become a `Resource`.

Examples to keep in mind:

- descriptor sets can be implicitly freed when their descriptor pool is destroyed;
- command buffers are freed when their command pool is destroyed;
- some pool configurations permit or forbid individual freeing;
- swapchain images are retrieved from the swapchain and must not be passed to `vkDestroyImage`;
- some parent destruction operations require all explicitly destroyable children to have already been destroyed;
- destruction often has synchronization/liveness preconditions.

These are Vulkan-layer ownership rules, not reasons to complicate generic `Resource`.

Some cases may motivate the later collection/batch abstraction.

Borrowed or parent-owned handles should remain non-owning rather than being forced into `Resource`.

## Aggregate move-assignment ordering

An owning object graph can have correct destruction order but incorrect default move-assignment order.

For members such as:

```cpp
Instance instance_;
Device device_;
Image image_;
```

destruction runs in the desired reverse order:

```text
image
device
instance
```

but default memberwise move assignment runs forward and can destroy destination resources in the wrong dependency order.

A graph type that is move-assignable must implement graph-aware move assignment in safe dependency order.

This is a higher-level ownership-graph concern, not something `Resource` can infer.

## Collection / batch stretch goal

A later facade should support many independently removable resources sharing one deleter/context.

Motivation:

```text
one parent/API/destruction context
many child handles
```

Do not store the same parent handle/API bookkeeping once per child when the resources naturally live as a batch.

The likely design is separate from singular storage because the container itself represents zero-or-many engagement. Do not assume this implies a shared stateful ownership base between singular and collection forms; their commonality may remain only the deleter contract and small helper utilities.

Working direction:

```cpp
template <
    typename Container,
    typename Deleter,
    typename Projection = std::identity>
class ResourceCollection;
```

The projection identifies the owned resource inside `Container::value_type`.

This could support both:

```cpp
std::vector<VkImage>
```

and:

```cpp
std::unordered_map<ImageId, VkImage>
```

without separate sequence and associative wrapper hierarchies.

The wrapper should intercept ownership-changing operations such as insertion, erasure, extraction, clearing, and release.

For rich lookup APIs, expose a const view/reference to the underlying container rather than duplicating every associative-container observer.

Do not build this until the singular `Resource` is settled.

## Current likely architecture

```text
Resource<Value, Deleter, Disengagement = detail-selected default>
    |
    +-- Deleter
    |     nothrow move-constructible
    |     nothrow invocable
    |     move assignment optional
    |
    +-- Disengagement policy
    |     Sentinel<V>
    |     ProjectedSentinel<P, V>
    |     default optional-backed policy
    |
    +-- private storage selected from policy
    |     OptionalStorage<Value>
    |     SentinelStorage<...>
    |     projected-sentinel storage
    |
    +-- immutable Value identity
          scalar handle or compound bookkeeping
```

The public policy describes semantics.

The private storage describes representation.

`Resource` owns destruction.

Storage owns engagement and ownership-transfer mechanics.

The Vulkan layer decides what bookkeeping belongs in each resource identity.

## Cohesion pass: next work

The next spike should stop expanding the architecture and assemble the singular design into its likely real source organization.

The cohesion pass resolved the singular production shape:

- library/repository/root namespace: `resource`;
- public header: `src/resource/resource.hpp` / `<resource/resource.hpp>`;
- public sentinel policies: `Sentinel` and member-pointer-only `ProjectedSentinel`;
- default strong-type customization: `DefaultSentinel<Value>` for class/enum types only;
- no public out-of-band policy; unknown sentinels select private optional-backed storage;
- private storage types remain distinct: `OptionalStorage`, `SentinelStorage`, and `ProjectedSentinelStorage`;
- scalar sentinel storage keeps its optimized whole-value path rather than being forced through projected storage;
- the opportunistic self-reset assertion survives and adds no equality requirement;
- deleter invocation remains exact-`void` and `noexcept`;
- zero-argument construction excludes pointer and member-pointer deleters;
- projected empty construction exists only when its `Value` can itself be default-constructed without throwing;
- production stays in one self-contained public template header for now;
- implicit CTAD guides are sufficient;
- empty deleter + pointer `Resource` is verified to have pointer size;
- public value construction/reset use `const&` and `&&` overloads instead of by-value parameters so sentinel state is observed before a move;
- projected storage does not return a sentinel-restored local by value; doing so could re-move the sentinel when NRVO is not performed;
- sentinel reconstruction constructs directly from the reusable NTTP sentinel rather than constructing a local sentinel and moving it into place.

The focused suite currently uses compile-time assertions for structural contracts and GTest for runtime behavior. It contains 41 runtime tests. GCC 14.2 and Clang 17 warning-as-error builds, Release builds, and ASan/UBSan runs are green.

After this singular production pass:

- integrate the header/test with the real project build metadata;
- keep the ledger synchronized with any review corrections;
- only then investigate `ResourceCollection`.

## Testing expectations

Keep tests focused and small.

Important cases include:

```text
pointer chooses nullptr sentinel storage automatically
ordinary value chooses optional storage automatically
explicit Sentinel works through CTAD
typed Sentinel works when needed
ProjectedSentinel tests only the projected member
ProjectedSentinel move restores the source projection after transferring the full payload
construction observes projected engagement before moving the incoming payload
moving an already-disengaged projected payload canonicalizes both source and destination projections even when a member move changes the sentinel value
constructing with sentinel starts disengaged
resetting with sentinel destroys old identity and disengages
release returns identity and suppresses cleanup
destruction invokes deleter exactly once
move construction transfers ownership and disengages source
move construction from an already-disengaged sentinel storage remains disengaged
constructing/resetting a sentinel resource remains disengaged even when moving the sentinel value would change it
move assignment destroys destination with old deleter first
move assignment transfers source deleter state
capturing lambda deleter works despite deleted move assignment
named move-assignable deleter takes the ordinary assignment path
sentinel Value can be move-constructible but non-move-assignable
optional Value can be move-constructible but non-move-assignable
empty deleter + pointer Resource has pointer size
self-move assignment is harmless
empty typed Resource can later reset into engagement
custom sentinel policy does not accidentally treat unrelated integer values as empty
strong class/enum handle type can customize `DefaultSentinel` without affecting an aliased fundamental handle type
```

Use GTest for runtime behavior where appropriate, with static assertions and compile-fail fixtures for language and concept contracts.

## Constraint style, style, and toolchain

Library requirements belong in concepts and `requires` clauses at the declaration boundary. Do not accept an invalid instantiation and then reject it later with a requirement-enforcing `static_assert`. `static_assert` remains appropriate for tests and ordinary compile-time invariants such as representation-size checks.

Before production implementation, read the C++ standards documents included in the archived standards submodule.

Known relevant project conventions include:

```text
C++26
GCC 14 minimum
Clang 17 cross-check
east const
brace initialization where appropriate
trailing return types
constexpr where reasonable
// comments
/// Doxygen comments
small cohesive types
parameterize on collaborators
no silent fallback behavior
```

Do not let exploratory snippets in this ledger override the actual standards documents if there is a conflict.

## Current confidence

The singular abstraction is close to settled.

The most important architectural decisions are now stable:

```text
Resource owns cleanup but not nullability mechanics.
Value is immutable resource identity/bookkeeping.
Deleter is explicit and may be a capturing lambda.
Public disengagement policy is separate from private storage.
Sentinel storage remains compact without requiring assignable identities.
Storage move operations transfer ownership and explicitly disengage the source.
Pointers optimize automatically.
Distinct class/enum identities can declare a `DefaultSentinel`; fundamentals cannot acquire an implicit global sentinel.
Compound identities are first-class.
Projected sentinels support compound identities without optional overhead.
Public construction and reset observe sentinel state before performing ownership-transfer moves.
Vulkan-specific dispatch strategy remains a Vulkan-layer decision.
Escaping movable Vulkan children should not depend on addresses inside movable parents.
No heap allocation is introduced merely to stabilize resource bookkeeping.
```
