Building & Linking GTS {#dev_guide_gts_build}
============================

# Building GTS

## Download the Source Code

Clone [the repository](https://github.com/GameTechDev/GTS-GamesTaskScheduler.git).

~~~sh
git clone https://github.com/GameTechDev/GTS-GamesTaskScheduler.git
~~~

## Build the Library

- GTS uses [premake](https://premake.github.io/download.html) to generate IDE projects and make files.
GTS project generation and build options can be altered in
~~~bat
premake/_scripts_/gts_sln.lua.
~~~

- See the [premake wiki](https://github.com/premake/premake-core/wiki) for documentation on how to
alter the lua files or make you own project generator.

### Windows

- Generate a Microsoft Visual Studio solutions:
~~~bat
premake/windows/gts.bat
~~~
 - This will create the directory "_build", which contains several solutions.

- Open a solution:
~~~bat
_build/gts/<vs*>/msvc/gts.sln
~~~

- Select your target platform and build the solution. The build products will be located at:
~~~bat
_build/gts/<vs*>/msvc/<build_flavor>
~~~

### Linux

- Generate a gnu make file:
~~~sh
premake/linux/gts.bat
~~~

 - This will create a directory that contains the make file.
 ~~~
 _build/gts/gmake
 ~~~

- Build using make and select the desired configuration. (Open the Makefile to see all the configs.)
~~~sh
make -f "Makefile" config=<config_type>
~~~

 - Build products are located in:
 ~~~
 _build/gts/gmake/<config>
 ~~~

# Linking GTS

The paths below are relative to the root location where GTS was cloned.

## Header Files

| Files                                                       | Description
| :---                                                        | :---
| source/gts/include/                                         | Public header files.

## Libraries

### Windows

| File                                                        | Description
| :---                                                        | :---
| \_build/gts/<vs\*>/<compiler_flavor>/<build_flavor>/gts.lib | GTS static library

### Linux

| File                                                        | Description
| :---                                                        | :---
| \_build/gts/gmake/<build_flavor>/gts.a                      | GTS static library

@note GTS uses thread priorities, which on linux requires
~~~sh
sudo setcap 'cap_sys_nice=eip' <application>
~~~

## Linking to GTS

### Windows
* Refer to the [Microsoft Visual Studio documentation](https://docs.microsoft.com/en-us/cpp/build/walkthrough-creating-and-using-a-static-library-cpp?view=vs-2017) on linking the application using MSVS solutions.

# GTS Build Options

GTS supports the following build-time options.

| Option                                  | Supported values (defaults in bold) | Description
| :---                                    | :---                                | :---
| GTS_ENABLE_ASSERTS                      | ***not defined***, *defined*        | Enables asserts in NDEBUG builds.
| GTS_ENABLE_INTERNAL_ASSERTS             | ***not defined***, *defined*        | Enables internal asserts in NDEBUG builds.
| GTS_TRACE_CONCURRENT_USE_LOGGER         | **0**, 1                            | Enables tracing with the ConcurrentLogger.
| GTS_TRACE_USE_TRACY                     | **0**, 1                            | Enables tracing with the [Tracy](https://github.com/wolfpld/tracy).
| GTS_TRACE_USE_RAD_TELEMETRY             | **0**, 1                            | Enables tracing with the [RAD Telemetry](http://www.radgametools.com/telemetry.htm).
| GTS_TRACE_USE_ITT                       | **0**, 1                            | Enables tracing with [Intel's ITT](https://github.com/intel/ittapi).
| GTS_ENABLE_COUNTER                      | ***not defined***, *defined*        | Enables statistics counters.
| GTS_USE_GTS_MALLOC                      | ***not defined***, *defined*        | All internal GTS dynamic memory allocations will be done through GTS malloc.
| GTS_HAS_CUSTOM_CPU_INTRINSICS_WRAPPERS  | ***not defined***, *defined*        | Indicates that the CPU intrinsic wrappers will be user provided in user_config.h.
| GTS_HAS_CUSTOM_DYNAMIC_MEMORY_WRAPPERS  | ***not defined***, *defined*        | Indicates that the dynamic memory wrappers will be user provided in user_config.h.
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

## Dynamic Memory
Memory overrides folowing precedence ordering:
1. GTS_HAS_CUSTOM_DYNAMIC_MEMORY_WRAPPERS
2. GTS_USE_GTS_MALLOC

@warning
When using gts_malloc as an external library, be sure **not** to define GTS_HAS_CUSTOM_DYNAMIC_MEMORY_WRAPPERS and GTS_USE_GTS_MALLOC.
See @ref dev_guide_gts_malloc_override for details.

## Platform Overrides
If the user defines any of the GTS_HAS_CUSTOM_\*; they must implement the corresponding marco in their user_config.h.

See @ref dev_guide_platform_replacement for details.

<br>
<br>
<br>
<br>
