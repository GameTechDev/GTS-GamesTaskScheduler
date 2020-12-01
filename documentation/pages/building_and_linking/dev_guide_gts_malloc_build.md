Building & Linking GTS Malloc {#dev_guide_gts_malloc_build}
============================

# Building GTS Malloc

## Download the Source Code

Clone [the repository](https://github.com/GameTechDev/GTS-GamesTaskScheduler.git).

~~~sh
git clone https://github.com/GameTechDev/GTS-GamesTaskScheduler.git
~~~

## Build the Library

- GTS Malloc uses [premake](https://premake.github.io/download.html) to generate IDE projects and make files.
GTS project generation and build options can be altered in
~~~bat
premake/_scripts_/gts_malloc_sln.lua.
~~~

- See the [premake wiki](https://github.com/premake/premake-core/wiki) for documentation on how to
alter the lua files or make you own project generator.

### Windows

- Generate a Microsoft Visual Studio solutions:
~~~bat
premake/gts_malloc.bat
~~~
 - This will create the directory "_build", which contains several solutions.

- Open a solution:
~~~bat
_build/gts_malloc/<vs*>/msvc/gts_malloc.sln
~~~

- Select your target platform and build the solution. The build products will be located at:
~~~bat
_build/gts_malloc/<vs*>/msvc/<build_flavor>
~~~
 - GTS Malloc can be built as both static and dynamic libraries.
 
 ### Linux

- Generate a gnu make file:
~~~sh
premake/linux/gts_malloc.bat
~~~

 - This will create a directory that contains the make file.
 ~~~
 _build/gts_malloc/gmake
 ~~~

- Build using make and select the desired configuration. (Open the Makefile to see all the configs.)
~~~sh
make -f "Makefile" config=<config_type>
~~~

 - Build products are located in:
 ~~~
 _build/gts_malloc/gmake/<config>
 ~~~

# Linking to GTS Malloc

The paths below are relative to the root location where GTS was cloned.

## Header Files

| Files                                                       | Description
| :---                                                        | :---
| source/gts/include/                                         | Public header files.

## Libraries

### Static ### {#dev_guide_gts_malloc_static_link}

#### Windows

| File                                                                             | Description
| :---                                                                             | :---
| \_build/gts_malloc/<vs\*>/<compiler_flavor>/<build_flavor>/gts_malloc_static.lib | GTS Malloc static library

Refer to the
[Microsoft Visual Studio documentation](https://docs.microsoft.com/en-us/cpp/build/walkthrough-creating-and-using-a-static-library-cpp?view=vs-2017)
on linking the application using MSVS solutions.

#### Linux

| File                                            | Description
| :---                                            | :---
| \_build/gts_malloc/gmake/<build_flavor>/gts_malloc.a  | GTS Malloc static library

### Shared

#### Windows

| File                                                                               | Description
| :---                                                                               | :---
| \_build/gts_malloc/<vs\*>/<compiler_flavor>/<build_flavor>/gts_malloc_shared.lib   | GTS Malloc import library
| \_build/gts_malloc/<vs\*>/<compiler_flavor>/<build_flavor>/gts_malloc_shared.dll   | GTS Malloc shared libaray
| \_build/gts_malloc/<vs\*>/<compiler_flavor>/<build_flavor>/gts_malloc_redirect.lib | GTS Malloc redirection import library
| \_build/gts_malloc/<vs\*>/<compiler_flavor>/<build_flavor>/gts_malloc_redirect.dll | GTS Malloc redirection shared library

Refer to the
[Microsoft Visual Studio documentation](https://docs.microsoft.com/en-us/cpp/build/walkthrough-creating-and-using-a-dynamic-link-library-cpp?view=vs-2017)
on linking the application using MSVS solutions.

#### Linux

| File                                                         | Description
| :---                                                         | :---
| \_build/gts_malloc/gmake/<build_flavor>/gts_malloc_shared.so | GTS Malloc shared libaray

# Overriding Malloc

## Static Override

[Statically link](@ref dev_guide_gts_malloc_static_link) gts_malloc. Include GtsMallocCOverride.h in each translation
unit that will override malloc.

@warning
Care must be taken when using this method as one may mistakenly mix pointers from external code.

## Dynamic Override

### Windows

Dynamically link gts_malloc and gts_malloc_redirect. gts_malloc_redirect will
automatically redirect calls to malloc.

### Linux

[Preload](https://www.man7.org/linux/man-pages/man8/ld.so.8.html) gts_malloc:
~~~sh
LD_PRELOAD=/path/to/gts_malloc.so myprogram
~~~

## Overriding new/delete

To further override new/delete, include GtsMallocCppOverride.h.

@note
To do this add the header to **single** source file of the project.

# GTS Malloc Build Options

GTS Malloc supports the following build-time options.

| Option                                  | Supported values (defaults in bold) | Description
| :---                                    | :---                                | :---
| GTS_ENABLE_ASSERTS                      | ***not defined***, *defined*        | Enables asserts in NDEBUG builds.
| GTS_ENABLE_INTERNAL_ASSERTS             | ***not defined***, *defined*        | Enables internal asserts in NDEBUG builds.
| GTS_TRACE_CONCURRENT_USE_LOGGER         | **0**, 1                            | Enables tracing with the ConcurrentLogger.
| GTS_TRACE_USE_TRACY                     | **0**, 1                            | Enables tracing with the [Tracy](https://github.com/wolfpld/tracy).
| GTS_TRACE_USE_RAD_TELEMETRY             | **0**, 1                            | Enables tracing with the [RAD Telemetry](http://www.radgametools.com/telemetry.htm). *not tested*
| GTS_TRACE_USE_ITT                       | **0**, 1                            | Enables tracing with [Intel's ITT](https://github.com/intel/ittapi).
| GTS_ENABLE_COUNTER                      | ***not defined***, *defined*        | Enables statistics counters.
| GTS_HAS_CUSTOM_CPU_INTRINSICS_WRAPPERS  | ***not defined***, *defined*        | Indicates that the CPU intrinsic wrappers will be user provided in user_config.h.
| GTS_HAS_CUSTOM_OS_MEMORY_WRAPPERS       | ***not defined***, *defined*        | Indicates that the OS memory wrappers will be user provided in user_config.h.
| GTS_HAS_CUSTOM_THREAD_WRAPPERS          | ***not defined***, *defined*        | Indicates that the OS thread wrappers will be user provided in user_config.h.
| GTS_HAS_CUSTOM_EVENT_WRAPPERS           | ***not defined***, *defined*        | Indicates that the OS event wrappers will be user provided in user_config.h.
| GTS_HAS_CUSTOM_ATOMICS_WRAPPERS         | ***not defined***, *defined*        | Indicates that the atomics wrappers will be user provided in user_config.h.
| GTS_HAS_CUSTOM_ASSERT_WRAPPER           | ***not defined***, *defined*        | Indicates that assert handling will be user provided in user_config.h.
| GTS_HAS_CUSTOM_TRACE_WRAPPER            | ***not defined***, *defined*        | Indicates that the trace wrappers will be user provided in user_config.h.

@warning
All other build options that can be found in source files are dedicated for
the development/debug purposes and are subject to change without any notice.
Please avoid using them.

## Tracing
Tracing overrides have the folowing precedence ordering:
1. GTS_HAS_CUSTOM_TRACE_WRAPPER
2. GTS_TRACE_USE_RAD_TELEMETRY
3. GTS_TRACE_USE_TRACY
4. GTS_TRACE_CONCURRENT_USE_LOGGER
5. GTS_TRACE_USE_ITT

See @ref dev_guide_tracing for details.

## Platform Overrides
If the user defines any of the GTS_HAS_CUSTOM_\*; they must implement the corresponding macro in their user_config.h.

See @ref dev_guide_platform_replacement for details.

<br>
<br>
<br>
<br>