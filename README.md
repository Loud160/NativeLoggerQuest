# Native Logger Quest

Native Logger Quest is a small asynchronous native logger for Meta Quest
Beat Saber mods. It was extracted from Big Screen so multiple mods can compile
the same tested logger implementation into their own native library without
installing or depending directly on Paper2.

It is a **build-time static library**, not a QMOD and not a separately
installed shared library. Each consuming mod owns an isolated logger instance,
tag, output path, bounded queue, writer thread, and rotation lifecycle. It does
not replace or intercept Paper2 used by other mods.

## Behavior

- immediate Android logcat output;
- asynchronous file output through one owned writer thread;
- 1 MiB/2,048-record bounded ordinary queue by default;
- reserved queue capacity for warnings and errors;
- 5 MiB active log plus one previous-log rotation by default;
- completed-batch flushing for useful crash-tail retention;
- bounded explicit flush and orderly shutdown;
- file failures fall open to logcat; and
- no exception escapes a producer logging call.

## CMake use

Fetch or vendor an immutable Native Logger Quest source release, then add it
before defining the consuming mod's final link behavior:

```cmake
set(NATIVE_LOGGER_QUEST_TAG "MyMod" CACHE STRING "" FORCE)
set(NATIVE_LOGGER_QUEST_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(path/to/NativeLoggerQuest)

target_link_libraries(MyMod PRIVATE
    NativeLoggerQuest::NativeLoggerQuest)
```

The caller supplies its own active and previous file paths through
`NativeLoggerOptions`, then initializes the logger before ordinary mod systems
or dependency APIs are used.

## Optional beatsaber-hook abort bridge

Some beatsaber-hook inline abort helpers refer to Paper2 C-ABI functions even
when a mod does not use Paper2 for logging. A mod that intentionally removes
its own Paper2 linkage can attach Native Logger Quest's private bridge:

```cmake
native_logger_quest_enable_paper2_abort_bridge(MyMod)
```

The bridge redirects only unresolved references linked into that one mod. Its
symbols are hidden, so it cannot satisfy, replace, or intercept Paper2 calls
from BSML, SongCore, or another shared object.

## Dependency diagnostics

Because this logger is compiled into the consuming mod and has no Paper2 ABI
dependency, it can start before that mod initializes normal dependency APIs.
The mod can then record an installed-version mismatch in plain language and
show a UI warning later if the UI stack is safe. A native dependency that
prevents the consuming mod's own ELF from loading remains outside the reach of
all in-process code, so consumers must avoid hard Paper2 ABI references when
using the logger for that bootstrap purpose.

## License and origin

Native Logger Quest was extracted from Big Screen. The source remains under
GPL-3.0-only with Big Screen's preserved GPLv3 section 7 attribution, origin,
and interoperability terms. See [LICENSE](LICENSE),
[LICENSE-ADDITIONAL-TERMS.md](LICENSE-ADDITIONAL-TERMS.md), and
[NOTICE](NOTICE).
