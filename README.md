# Resource

Resource is a small C++26 RAII owner for external resource identities. It fills a role similar to `std::unique_ptr`, but the owned identity does not need to be a pointer or refer to C++ object storage.

A resource can be a `FILE*`, an integer handle, an enum, or a compound value that contains the bookkeeping required for cleanup. Resource owns that identity uniquely, invokes an explicit deleter when ownership ends, and does not allocate memory or add indirection of its own.

A POSIX file descriptor shows the basic model:

```cpp
#include <fcntl.h>
#include <unistd.h>

#include <resource/resource.hpp>

auto file = resource::Resource{
    ::open("settings.txt", O_RDONLY),
    &::close,
    resource::sentinel<-1>,
};
```

Resource uses optional-backed engagement by default because it makes no assumption about which values mean no resource. The explicit `sentinel<-1>` opts this file descriptor into an in-band representation where `-1` means disengaged.

Resource is header-only. Its public header is `<resource/resource.hpp>`.

## Ownership model

`Resource<Value, Deleter, Engagement>` has three jobs:

- `Value` stores the identity and bookkeeping needed to operate on or destroy the external resource.
- `Deleter` destroys the external resource identified by a `Value`.
- `Engagement`, when present, describes how a `Value` represents whether Resource owns a resource.

The third parameter normally does not need to be written. Without an Engagement, Resource stores engagement state separately from `Value` using optional-backed storage. A supplied `Value` is therefore always treated as an owned identity, regardless of its bit pattern.

A `Value` is identity, not mutable application state. It must be an unqualified non-array object type that is nothrow move-constructible and nothrow destructible. `get()` always observes that identity as `Value const&`. For non-pointer identities, `operator*` and `operator->` provide the same const-only contained-value access.

For pointer identities, `operator*` and `operator->` instead follow owning-pointer semantics: they access the object or function identified by the stored pointer. A const Resource does not add constness to the pointee. For example, `Resource<Widget*, Deleter>::operator->()` returns `Widget*`, while `get()` still returns `Widget* const&`. `operator*` is unavailable when the pointer cannot be dereferenced, such as `void*`.

Resource is noncopyable. Moving transfers ownership and leaves the source disengaged.

## Engagement and storage

The default representation uses `std::optional<Value>` internally. Engagement state then lives outside `Value`, so every contained value is considered engaged. This is the canonical representation because it does not assume that any particular `Value` means invalid.

When a value has a known in-band disengaged representation, Resource can avoid that extra engagement state by opting into an Engagement. An Engagement supplies two static operations:

```cpp
Engagement::engaged(value);
Engagement::disengage(value);
```

Both operations must be nonthrowing. `engaged` reports whether the value identifies an owned resource. `disengage` changes it to a representation that does not own a resource. Engagement is compile-time behavior; Resource does not store an Engagement object.

In-band Engagement is an optimization, not a requirement. Resource never infers one from the C++ type. A pointer does not imply `nullptr`, an integer does not imply `0` or `-1`, and a class or enum does not imply a distinguished invalid value. If no explicit Engagement is supplied, Resource uses the default optional-backed representation.

### Explicit sentinels

When an API defines one value as invalid, `Sentinel<V>` makes that rule explicit and allows Resource to store engagement in-band.

On POSIX, a file descriptor can make the `-1` rule explicit:

```cpp
#include <fcntl.h>
#include <unistd.h>

#include <resource/resource.hpp>

using resource::Resource;
using resource::sentinel;

auto fd = Resource{
    ::open("settings.txt", O_RDONLY),
    &::close,
    sentinel<-1>,
};
```

Without `sentinel<-1>`, the file descriptor uses optional-backed storage and `-1` is treated as an engaged value like any other integer. The same rule applies to pointers: an API where `nullptr` means invalid can opt into `sentinel<nullptr>`.

The corresponding Engagement type is `Sentinel<-1>` when the type must be spelled explicitly. The sentinel must brace-construct the resource `Value` without narrowing. That construction, comparison with the canonical sentinel value, and assignment back to the canonical sentinel value must all be nonthrowing.

### Predicate Engagements

An Engagement does not need to compare against one sentinel. It can recognize any ownership rule that can also make the value safely disengaged.

For example, this Engagement treats every nonnegative integer as owned and every negative integer as disengaged:

```cpp
struct NonNegativeEngagement {
    static constexpr auto engaged(int value) noexcept -> bool {
        return value >= 0;
    }

    static constexpr auto disengage(int& value) noexcept -> void {
        value = -1;
    }
};
```

It can be selected explicitly:

```cpp
struct CloseHandle {
    auto operator()(int const& handle) const noexcept -> void;
};

using OwnedHandle = resource::Resource<int, CloseHandle, NonNegativeEngagement>;
```

`disengage` chooses one canonical representation even when `engaged` recognizes several values as disengaged.

### Projected Engagement

Compound identities are useful when cleanup needs more than the child handle itself. Resource can keep the complete identity while deciding engagement from one member.

```cpp
struct ImageIdentity {
    int device;
    int handle;
};

auto destroyImage(int device, int handle) noexcept -> void;

struct DestroyImage {
    auto operator()(ImageIdentity const& image) const noexcept -> void {
        destroyImage(image.device, image.handle);
    }
};

using Image = resource::Resource<
    ImageIdentity,
    DestroyImage,
    resource::ProjectedEngagement<&ImageIdentity::handle, resource::Sentinel<-1>>>;
```

Only `handle` determines whether the Resource is engaged, but moving, releasing, and deleting still use the complete `ImageIdentity`. The sentinel-specific CTAD helper expresses the same policy without spelling the Resource type:

```cpp
auto image = resource::Resource{
    ImageIdentity{device, handle},
    DestroyImage{},
    resource::projectedSentinel<&ImageIdentity::handle, -1>,
};
```

`ProjectedEngagement` can delegate to any explicit Engagement, including a predicate Engagement. There is no implicit projected policy because the default Resource representation stores engagement independently of `Value`; there is nothing value-derived to project. The `projectedEngagement` and `projectedSentinel` tag objects provide the corresponding CTAD forms.

## Empty Resource objects

Resource can begin empty only when it can construct a known-safe disengaged state.

Optional-backed storage can always begin empty because it does not need to construct a `Value`. A whole-value `Sentinel` can also begin empty because Resource knows the complete disengaged value.

Arbitrary predicate Engagements and projected Engagements do not automatically gain empty construction. Resource will not default-construct a `Value` and then call `disengage`, because that default-constructed value might already identify a live external resource. A projected Engagement also does not define safe values for the other members of a compound identity.

When empty construction is supported and the deleter is nothrow default-constructible, an explicitly typed Resource can start empty:

```cpp
resource::Resource<Handle, DestroyHandle> handle{};
```

A stateful deleter can be supplied without a value:

```cpp
resource::Resource<Handle, DestroyHandle> handle{destroyHandle};
```

An explicitly typed Resource can also accept only a value when its deleter can be safely default-constructed. A one-argument call chooses an exact `Value` or `Deleter` type first. Otherwise, the argument must identify only one role: it must brace-construct the `Value` without narrowing, or brace-construct the `Deleter`. If both roles match, Resource rejects the construction rather than guessing. When `Value` and `Deleter` are the same type, one-argument construction is unavailable.

Pointer and member-pointer deleters are excluded from forms that default-construct the deleter because value-initializing them would produce a null callable. They can still be supplied explicitly through the deleter-only constructor when the selected storage can begin empty.

## Ownership operations

`owns()` and explicit conversion to `bool` report whether Resource owns an identity.

`get()`, `operator*`, and `operator->` observe the owned identity. They require the Resource to be engaged and assert that precondition in assertion-enabled builds.

`release()` transfers the identity out without invoking the deleter and leaves the Resource disengaged:

```cpp
auto const raw = resource.release();
```

`reset()` destroys the current resource when engaged and leaves the Resource disengaged. `reset(value)` prepares the incoming identity before destroying the old one, then adopts the prepared identity.

Passing a sentinel to a sentinel-backed Resource leaves it disengaged, just as resetting a `std::unique_ptr` with `nullptr` leaves that pointer empty.

Resetting from the Resource's own stored object is a programming error:

```cpp
resource.reset(resource.get());
```

Resource detects that literal aliasing case with an address-based assertion. It deliberately does not use `operator==` to infer identity. Two separate `Value` objects may compare equal while representing different ownership situations, and two distinct values may still identify the same external resource. Callers are responsible for avoiding the latter case.

## Deleters

A deleter is an object that can be invoked with `Value const&`. Its return type is unrestricted because Resource has no return channel for cleanup performed by destruction or `reset()`.

A named deleter can wrap an ordinary C cleanup function:

```cpp
#include <cstdio>
#include <resource/resource.hpp>

struct CloseFile final {
    auto operator()(std::FILE* const& file) const noexcept -> int {
        return std::fclose(file);
    }
};

auto file = resource::Resource{
    std::fopen("settings.txt", "r"),
    CloseFile{},
    resource::sentinel<nullptr>,
};
```

A lambda works equally well when a separate deleter type would add no useful meaning:

```cpp
auto file = resource::Resource{
    std::fopen("settings.txt", "r"),
    [](std::FILE* const& stream) noexcept {
        return std::fclose(stream);
    },
    resource::sentinel<nullptr>,
};
```

These examples call `std::fclose` through a wrapper instead of taking its address. C++ does not generally guarantee that the address of a standard-library function may be taken unless that function is specifically designated addressable. Calling the function normally is unaffected.

Resource discards the deleter's return value during automatic cleanup. If that result matters to program behavior, release the identity and invoke the stored deleter explicitly:

```cpp
auto const raw = file.release();
auto const result = file.deleter()(raw);
```

`deleter()` returns the stored deleter by reference. The non-const overload permits stateful deleters to perform manual cleanup after `release()` without requiring the caller to duplicate the deleter's state.

An exception must not escape when Resource invokes the deleter. Cleanup runs from nonthrowing Resource operations, including destruction, so an escaping exception terminates the program. The deleter does not need a `noexcept` annotation in its type; this is a semantic contract on its behavior when Resource calls it.

The stored deleter must be an unqualified non-array object type that is nothrow move-constructible and nothrow destructible. Construction from a supplied deleter argument is available only when that particular construction is nonthrowing. An lvalue deleter therefore works when copying it cannot throw; otherwise, move it into Resource.

Resource move assignment additionally requires the deleter to be nothrow move-assignable. A capturing lambda can still be a valid deleter even when its type is not move-assignable; the resulting Resource remains move-constructible but is not move-assignable.

Resource has no default deleter. A pointer-shaped identity does not imply `delete`: the same shape can represent a C stream, library handle, mapped object, Vulkan handle, or some other external resource with unrelated cleanup rules.

## Move requirements

Moving a `Value` must preserve the external resource identity in the destination. The moved-from representation may change.

After a non-self Resource move, the source is disengaged. Reusing that moved-from Resource with `reset(value)` also requires its moved-from deleter state to remain usable for cleanup; Resource cannot strengthen the ordinary moved-from guarantees of an arbitrary deleter type.

Move assignment exists only when both the deleter and selected storage support ordinary nonthrowing move assignment. Resource does not destroy and reconstruct live members merely to manufacture assignment.

Optional-backed storage can still support some move-constructible but non-move-assignable `Value` types because `std::optional` provides a real lifetime boundary for its contained object. In-band Engagement instead relies on ordinary nonthrowing mutation to restore disengagement.

For a pointer identity with an empty deleter, an explicit in-band sentinel can reduce the representation to one pointer:

```cpp
struct EmptyDelete {
    auto operator()(int* const&) const noexcept -> void {}
};

using PointerResource = resource::Resource<
    int*,
    EmptyDelete,
    resource::Sentinel<nullptr>>;

static_assert(sizeof(PointerResource) == sizeof(int*));
```

The default `Resource<int*, EmptyDelete>` instead uses optional-backed engagement. Its representation may be larger because Resource does not assume that `nullptr` means disengaged.

## Using Resource with CMake

An installed package exports `Resource::Resource`:

```cmake
find_package(Resource 0.1 CONFIG REQUIRED)
target_link_libraries(my_target PRIVATE Resource::Resource)
```

Then include the public header normally:

```cpp
#include <resource/resource.hpp>
```

The repository can also be used directly with `add_subdirectory`; its build uses the vendored Canon project when available and otherwise looks for an installed Canon package.

Resource itself is header-only. Building the repository is mainly useful for its validation suite and package installation.

## Development

The project uses C++26. The supported compiler floor is GCC 14.2, with Clang 17 as a cross-check. CMake 3.31.6 is required for the project build.

After cloning, populate Canon and its nested GoogleTest dependency before building the validation suite:

```sh
git submodule update --init --recursive
```

The shared developer presets provide compiler-qualified configure, build, and test workflows. GCC is the primary profile:

```sh
cmake --workflow --preset gcc-debug
cmake --workflow --preset gcc-release
cmake --workflow --preset gcc-asan
cmake --workflow --preset gcc-tidy
cmake --workflow --preset gcc-coverage
```

Corresponding `clang-*` workflows are available for the Clang cross-check. The tidy workflows require clang-tidy 21.1.6 or newer. Coverage workflows require gcovr and a compiler-matched coverage backend, then generate the coverage report after tests complete.

The validation suite combines focused GoogleTest cases, compile-time assertions, and an installed-package integration test.

Formatting uses clang-format 21.1.8 exactly.

## License

Resource is licensed under the MIT License. See `LICENSE`.
