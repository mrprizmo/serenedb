## C++ codestyle

Based on common sense and something from google codestyle, abseil best practice, etc.
(structure of document from google codestyle)

Most important one, everything that isn't detected automatically is ok to change later if really needs to merge PR earlier.

These rules for serenedb, iresearch, vpack

Please avoid codestyle disscussion in PR when possible.

This document should be updated according to our knowledge and issues

These rules tries but doesn't fit to all code.
So if something impossible to write other or really bad it can be ok to ignore them.

It's ok to not rewrite all legacy in single day.
But in some day we hope most of our code will follow this style.

### C++ version, tools, etc

We support single pack of tools: latest stable clang, our own libraries, but system glibc for now, cmake, vscode
Everything else is not really supported, but devs or community can support something else if it's not intrusive changes.
We use latest C++ standard, to allow us to write simpler/faster code.

### Naming

TODO(mbkkt) will be .clang-tidy file (only readability (naming convention) settings)
1. enum kMember
2. enum class Type::Member

### Formatting

TODO(mbkkt) adjust clang-format for newer better options

.clang-format and some extensions (which is not supported yet)

1. last line in the file should be empty (needed for git), exists settings in vscode
2. namespaces -- empty line between namespace and not namespace is needed, empty line between namespaces isn't (just to increase readability, TODO(mbkkt) will try to setup this in clang-format).
```cpp
namespace c {

void foo();

}  // namespace c
namespace a::b {

class B;

}  // namespace a::b
```
3. prefer break outside of switch scope

### Headers

https://google.github.io/styleguide/cppguide.html#Header_Files -- kind of same except forward declaration

1. #pragma once instead of ifdef guard
2. We like to use forward declaration, but only in separate file (otherwise forbidden!).
   Shouldn't be more than single Fwd.h file per directory.
   TODO(mbkkt) what to do with current forward declaration
3. .hpp -- headers (TODO(mbkkt) needs to rename all our headers from .h to .hpp)
4. .tpp -- header implementations
5. .cpp -- sources, implementations
6. Prefer to avoid pimpl, exception is when multiple different libraries planned to be used
7. tests and sources separated in root level, tests are tries to copy sources struct
8. Avoid duplicate directory name in file name
9. TODO(mbkkt) some way to some setup include what you use (for now don't case)
10. Avoid manual template instantiation in .cpp, but switch-like functions are ok
11. inline should be used only in terms of linkage, otherwise use force inline
12. But everything is template/inline is bad, binary size is matter (because it affects performance, devs and users usability)
13. TODO(mbkkt) include order will be fixed by clang-format

### Scoping

https://google.github.io/styleguide/cppguide.html#Scoping -- namespaces/usings/globals are very similar

1. Avoid using namespace (forbidden in headers)
2. Prefer write code with real namespace inside same namespace
3. Use inline namespace for multiple versions only
4. Use namespace alias/using enum/class/struct (forbidden in headers)
5. instead of static global variables/functions prefer single anonymous namespace
6. instead of const prefer constexpr
7. "static" better than just global
8. constinit/magic static/inline static to avoid issue with static order init
9. Avoid code in global namespace!

### Initialization

https://google.github.io/styleguide/cppguide.html#Scoping -- no good rules for init

1. For std::pair/tuple/etc prefer braced init over make function (because it faster compiles and less error-prone)
2. Avoid raw new/delete call, use make_pointer functions (to easy find really specific code)
3. Prefer braced init over parenthesis ctor
4. For classes with only getter, setters, trivial ctor, compare, prefer just structs. Also we recommend using `{.foo = 1, .bar=2,}`
5. Use default operator==/<=>/= and ctor
6. TODO(mbkkt) discuss: "fundamental" types (numbers, string literals, raw pointers, auto): `int var = 1`, `std::string_view var = "..."`, `auto bar = makeBar()`
7. TODO(mbkkt) discuss: structs: `FooBar{.foo = 1, .bar=2,}`, it's required for multiline ctors, and recommended for other, exception is `emplace`-like functions
8. TODO(mbkkt) discuss: classes: `FooBar{1, 2,}`
9. trailing comma for init is optional, but required for list of something, or if parameters doesn't fit in single line
10. We forbid `Type var{};` and `Type var = {};` syntax because it's too difficult, for default ctor just write nothing
11. Prefer `auto fooBar = makeFooBar()` instead of `FooBar fooBar = makeFooBar()`, but if type is short any option is ok, please don't nitpick
12. `const` for variables is optional, but const for methods, references, data behind pointer is strongly recommended
13. Prefer to use `emplace`-like functions (it's possible even for just structs)
14. For nullptrs https://google.github.io/styleguide/cppguide.html#0_and_nullptr/NULL, for smart pointer is default ctor is ok (as for any non trivial class)
15. Prefer explicit pointer/reference over implicit `const auto*` instead of `auto`

### Classes

https://google.github.io/styleguide/cppguide.html#Classes -- kind of same?

1. enum and enum classes should have trailing comma
2. Prefer free functions over member functions for structs
3. Prefer structs over std::pair/std::tuple
4. TODO(mbkkt) Inheritence
5. TODO(mbkkt) Operator overloading
6. structs everything public, classes private members, except static/etc
7. TODO(mbkkt) declaration order
8. Prefer to avoid friends
9. Avoid public init functions: https://google.github.io/styleguide/cppguide.html#Doing_Work_in_Constructors
10. Prefer explicit ctors


### Functions

https://google.github.io/styleguide/cppguide.html#Functions -- same

1. trailing return https://google.github.io/styleguide/cppguide.html#trailing_return
2. lambda without args and noexcept should be written such way `[] {}` (clang-format will make it in future)
3. overloads, multiple ctors, etc is ok, but don't do same overloads like `const T&` and `std::shared_ptr<const T>&`
4. default args banned for virtual functions, if doubt use overloads

### Comments

https://google.github.io/styleguide/cppguide.html#Comments -- TODO

1. Avoid license in code
2. Avoid comments `/******/` and `///////////////////////////`
3. Use simple `//` without extra spaces/etc
4. We don't use doxygen for now
5. Comments only for explain logic/algorithm
6. Instead of comments to assert prefer print it in assert

### Extra

https://google.github.io/styleguide/cppguide.html#Other_C++_Features -- same + our rules

1. TODO(mbkkt) implicit or explicit conversion to bool from pointer/integer/number?
2. implicit conv, so `if (auto x = something())` instead of `if (auto x = something(); x)`, same for switch
3. Don't use `std::hash`, use `absl::Hash`, and custom hashing provided by it
4. Prefer to use `absl::*_hash_*` instead of `std::unordered_`
5. ~~Prefer to use `absl::Mutex`~~ Avoid writing sync code, sync code is for deep implementation details, database logic should be mostly async
6. TODO(mbkkt) compare `std::set`/`map` with `absl::btree_*` and `absl::*_hash_*`
7. TODO(mbkkt) choice between `absl::InlinedVector` and `boost::container::small_vector`
8. Avoid using of `std::initializer_list<T>`, use `std::span<const T>` in parameters, `std::array` in variables
9. use `magic_enum` for enum names, but consider using `userver` `bimap`
7. Use SDB/IRS_ASSERT -- for debug only checks
8. Use SDB/IRS_ENSURE -- for crash in debug and throw in release
9. USE SDB/IRS_VERIFY -- for crash in debug and release
10. Use absl::c_any_of/etc over std::any_of(begin, end), if not possible use std::ranges, use iterators overload only when they're really necessary
11. Prefer imperative code with cycles over std::ranges
12. Use algorithms for strings such as absl::StrCat, absl::Substitute, absl::StrJoin, absl::StrSplit
13. Don't use fmt-lib or printf if they're not necessary, use absl::SPrintf if needed, and if it's required std::format (mostly velox code)
14. Prefer not to use streams api (`operator<<`/`>>`), but there's a lot of existing code, so it's ok to use them sometime in legacy code. Avoid using it in new code. Also check `absl::StreamFormat`
15. https://google.github.io/styleguide/cppguide.html#Preincrement_and_Predecrement
16. https://google.github.io/styleguide/cppguide.html#Casting
17. Avoid rtti
18. TODO(mbkkt) do we thing std::bad_alloc possible? I suggest no, we should have different mechanics for this
19. Write noexcept only if function is really noexcept or it's required for correctness
20. Prefer not to use `&&` reference for trivially copyable classes
21. Don't missuse std::forward<Arg> and std::move
22. Use std::string_view almost everythere except c-api
23. Use reference instead of raw/smart pointers if ownership doesn't matter here
24. TODO(mbkkt) sizeof(varname) vs sizeof(typename) vs sizeof varname vs sizeof typename
