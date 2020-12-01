Platform Replacement {#dev_guide_platform_replacement}
============================
This section covers various methods that allow the developer to relace GTS's plaform layer
with their own. This is highly useful for expanding the number for plaforms and platform
versions that GTS supports.

# GTS Architechure
GTS is divided into the layers diagramed below. The **User Configuration** region represents
the location where the developer can insert their own platform later and replace portions
or all of GTS's platform implementation.

![](GTS_Layers.svg)

# User Configuration
User configureation is handled through a user_config.h header. It allows the developer to replace:
* [CPU Intrinsics](@ref CpuIntrinsicsWrapper)
* [C Memory Wrappers](@ref CMemoryWrapper)
* [OS Memory Wrappers](@ref OsMemoryWrapper)
* [OS Thread Wrappers](@ref OsThreadWrapper)
* [OS Event Wrappers](@ref OsEventWrapper)
* [Atomics Wrappers](@ref AtomicsWrapper)
* [Assert Wrappers](@ref AssertWrapper)
* [Tracing Wrappers](@ref TracingWrapper)

## Configuration File Location
user_config.h is consumed in machine.h. The developer can supply a location to thier own configuration file by defining
GTS_USER_CONFIG and adding an include.

## Methods

### Modular Library
The static library method requires the developer to separate their engine's platform layer into a static
library. The static library is added to GTS and the user_config.h is implmented by the static library.

![](PlatformReplaceModularLib.svg)

### Build GTS in Engine
The build-in-engine method move GTS directly into engines source project. The user_config.h is
implmented by the engines source.

### Build GTS with Engine and Supply Implemenation Files
The supply-implementation method is the most invasive. The developer will replacing the implementation files of:
* Thread.h
* Memory.h
* Atomic.h
* Machine.h
* Trace.h

Asserts can be hooked with the interface in assert.h.

<br>
<br>
<br>
<br>
