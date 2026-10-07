# GTA IV Grass & Procedural Objects Fix --- Safe Balanced Edition

## Overview

This edition is a conservative, stability-focused rework of the original
GTA IV Grass / Procedural Objects Fix.

The goal is **not** to remove the original mod's functionality. The goal
is to keep its useful procedural-object and grass streaming improvements
while reducing the amount of aggressive memory usage, runtime pressure,
diagnostics, and global game-state modification that can make a 32-bit
game more fragile.

The edition is designed around the following priorities:

1.  Increase procedural grass/object draw distance enough to produce a
    clearly visible improvement over vanilla GTA IV.
2.  Preserve more grass and procedural objects than vanilla when the
    configured limits are reached.
3.  Avoid the very large 32K/40K-style pools used by the original
    configuration.
4.  Reduce the probability of streaming stalls, hangs, and crashes
    during heavy streaming situations such as cutscenes.
5.  Avoid modifying unrelated model draw distances globally.
6.  Remove `.gfr` flight-recorder file generation completely.
7.  Keep the project suitable for Visual Studio 2026 / MSVC / Win32
    (x86).
8.  Keep the source tree simple: source, headers, includes, assembly
    bridge, solution, and project files are kept together in one
    directory.

------------------------------------------------------------------------

# 1. Why This Edition Exists

The original project is useful because GTA IV's procedural grass and
procedural objects can suffer from limited visibility and
streaming-related behavior.

However, the original configuration is intentionally aggressive. It can
maintain very large numbers of procedural and rendered objects and
contains extensive runtime diagnostics, hooks, tracking structures, and
generated bridge code.

During testing, the original version was observed to be capable of
problematic behavior under heavy streaming conditions, including:

-   cutscene freezes where audio continues while the image stops;
-   occasional crashes;
-   unusually heavy streaming situations;
-   large differences in grass/object density compared with vanilla even
    when the public density setting is `1.0`;
-   significant runtime overhead when very large object pools are
    allowed.

Because GTA IV is a 32-bit application with a relatively fragile
streaming architecture, maximizing theoretical object capacity is not
necessarily the safest way to improve visual distance.

The Safe Balanced Edition therefore uses a different philosophy:

> Keep the visual improvement, but put hard limits around the systems
> that can grow excessively.

------------------------------------------------------------------------

# 2. Pool Capacity Changes

## Original approach

The original configuration can use very large capacities, including
values in the 32K--40K range for some procedural/rendered object pools.

Examples of the original design include values such as:

-   Procedural pool: up to approximately `40960`
-   Rendered-object capacity: up to approximately `32768`
-   Additional internal tracking/source-bank structures with large
    capacities

These values provide substantial headroom, but they also allow the mod
to keep a very large number of objects alive and tracked simultaneously.

## Safe Balanced values

The Safe Balanced Edition uses:

  System                           Safe Balanced
  ------------------------------ ---------------
  Procedural Pool                        `12288`
  Procedural Pool hard ceiling           `16384`
  Rendered Object Pool                    `8192`
  Rendered Object hard ceiling           `16384`
  Source Bank                            `12288`
  Provider Capacity                         `40`

The purpose is not to make the pool small.

`12288` procedural entries are still substantially above a vanilla-style
conservative configuration, while remaining far below the most
aggressive original values.

The `16384` ceiling is a safety boundary rather than a target that the
mod should constantly reach.

This gives the mod enough room to maintain increased procedural
visibility without encouraging unbounded growth.

------------------------------------------------------------------------

# 3. Why the Limits Were Not Left at 32768 / 40960

A large pool is not automatically better.

A larger pool can increase:

-   memory usage;
-   object tracking overhead;
-   streaming workload;
-   hook invocation frequency;
-   lifecycle complexity;
-   the amount of state that must remain valid during camera changes;
-   the amount of work required when GTA IV changes streaming
    priorities.

The problem is especially relevant during cutscenes and other situations
where the game suddenly requests a different group of models,
animations, textures, and world objects.

The Safe Balanced Edition therefore follows this rule:

> A full pool should degrade visual coverage gracefully rather than
> forcing the game to maintain an unnecessarily huge object population.

This is preferable to simply reproducing the original maximum
capacities.

------------------------------------------------------------------------

# 4. Draw Distance

Draw distance is the primary visual goal of this edition.

The previous conservative configuration was intentionally close to
vanilla in order to minimize risk. While that was stable during initial
testing, the visual difference was not always obvious.

The Safe Balanced Edition therefore increases the procedural
draw-distance multiplier to:

``` ini
DistanceMultiplier=1.25
```

with a hard maximum of:

``` ini
MaxDistanceMultiplier=1.50
```

This provides a visible improvement while avoiding extreme distance
multipliers.

## Why 1.25x instead of a much larger value?

Very large distance multipliers can multiply the number of objects that
need to be considered by the streaming system.

For example, increasing the radius does not merely make existing grass
visible. It can cause substantially more procedural objects to become
eligible for creation, tracking, rendering, and streaming.

A moderate multiplier therefore gives a better stability/visual-quality
balance.

------------------------------------------------------------------------

# 5. Global Model Draw Distance Is Not Modified

The Safe Balanced Edition deliberately does **not** enable the legacy
global model draw-distance mutation.

The relevant behavior remains disabled.

This is important because a global modification of model draw distance
can affect many objects that are unrelated to procedural grass.

The goal of this project is specifically:

-   procedural grass;
-   procedural vegetation;
-   procedural props;
-   their streaming/visibility behavior.

It is not intended to rewrite the draw distance of every model in GTA
IV.

Keeping the global model-distance mutation disabled reduces unintended
side effects and makes the behavior easier to reason about.

------------------------------------------------------------------------

# 6. Density Behavior

The public density value remains:

``` ini
Density=1.0
```
------------------------------------------------------------------------

# 7. Graceful Capacity Handling

The objective of the capacity limits is not to make grass disappear as
soon as the pool becomes busy.

The intended priority is:

1.  Preserve nearby procedural grass and objects.
2.  Preserve medium-distance objects where possible.
3.  Reduce the least important/distant procedural coverage first when
    capacity becomes constrained.
4.  Never compensate for a full pool by aggressively allocating enormous
    additional pools.
5.  Never intentionally return the entire scene to vanilla density just
    because a safety limit has been reached.

This is an important difference from simply using a very small pool.

The pool is a **safety ceiling**, not a mechanism for constantly
reducing grass.

------------------------------------------------------------------------

# 8. Runtime Stability

The original implementation contains several systems that are reasonable
for development and reverse-engineering but are not all necessary for a
normal gameplay-oriented build.

The Safe Balanced Edition reduces unnecessary runtime pressure wherever
possible.

The design avoids:

-   unnecessarily large runtime pools;
-   unnecessary global model-distance mutation;
-   unnecessary diagnostic file generation;
-   aggressive runtime expansion;
-   treating optional diagnostics as a reason to terminate the game.

The core procedural-object functionality remains the focus.

------------------------------------------------------------------------

# 9. `.gfr` Flight Recorder Removal

The original project contains a flight-recorder/diagnostic system
capable of writing `.gfr` data to disk.

The Safe Balanced Edition does **not** generate `.gfr` files.

The disk-writing path has been disabled/removed from the gameplay build.

This was done for several reasons:

-   the flight recorder is not required for normal gameplay;
-   it creates additional file I/O;
-   it adds diagnostic complexity that is unnecessary for the release
    build;
-   the goal of this edition is a clean gameplay-oriented implementation
    rather than a debugging build.

The in-memory functionality required by the remaining runtime code is
kept only where necessary.

No `.gfr` file should be created during normal gameplay.

------------------------------------------------------------------------

# 10. Debug Logging and Watchdog Behavior

The gameplay configuration does not enable the original diagnostic
behavior by default.

Debug logging and hang-watchdog functionality are disabled unless
explicitly enabled for development/testing.

This prevents normal gameplay from being affected by diagnostic
operations that are useful while developing the mod but unnecessary
during ordinary play.

The purpose is to separate:

**development diagnostics**

from

**release gameplay behavior**.

------------------------------------------------------------------------

# 11. Process Termination Behavior

The original source contains defensive paths involving process
termination such as `TerminateProcess` / `ExitProcess`.

Such mechanisms can be appropriate when a mod detects an unrecoverable
initialization or compatibility problem.

However, optional diagnostic failures should not be treated as gameplay
failures.

The Safe Balanced design therefore avoids using optional diagnostics as
a reason to terminate the game.

The principle is:

> A diagnostic subsystem failing should not automatically become a
> game-ending event.

This does not mean that every internal failure can safely be ignored.
Critical initialization failures still need to fail safely.

------------------------------------------------------------------------

# 12. Runtime Hook Philosophy

The project still uses the runtime hooks required for the
procedural-object and grass functionality.

However, the Safe Balanced Edition avoids adding hooks merely to
increase visual quality when the same goal can be achieved through the
existing procedural-distance path.

The purpose is to minimize the amount of game code that is intercepted.

Every additional hook introduces another point where:

-   calling conventions matter;
-   registers/state must be preserved;
-   object lifetimes must remain valid;
-   game-version assumptions can break;
-   invalid pointers can result in crashes.

Therefore the edition favors a smaller, clearly defined hook surface.

------------------------------------------------------------------------

# 13. Cutscene Stability

Cutscenes are treated as a particularly important stability scenario.

The original problematic behavior reported during testing was:

-   video/image stops responding;
-   audio continues;
-   eventually the game may recover or crash.

Cutscenes can create unusually heavy streaming transitions because the
game can rapidly request different assets and change camera visibility.

The Safe Balanced configuration therefore avoids extremely large object
populations and aggressive runtime pool expansion.

The goal is to reduce the amount of procedural state competing with the
game's normal streaming workload during these transitions.

Cutscenes should therefore be considered a primary validation scenario
for this edition.

------------------------------------------------------------------------

# 14. Save Safety

The Safe Balanced Edition does not modify GTA IV save data.

The removal of `.gfr` generation also means that the gameplay build does
not need to maintain a flight-recorder file beside the game files.

The mod should therefore not require any changes to the normal GTA IV
save workflow.

As with any ASI/plugin that hooks the game executable, normal good
practice still applies:

-   keep backups of important saves;
-   do not assume that any third-party game modification can provide an
    absolute guarantee against every possible game failure.

------------------------------------------------------------------------

# 15. Code Cleanup

The original project is a reverse-engineered game modification and
therefore contains code that is necessarily more complex than a
conventional application.

However, the Safe Balanced Edition follows several cleanup principles.

### Single responsibility

Subsystems should have a clear purpose:

-   configuration;
-   pool management;
-   procedural object handling;
-   draw-distance handling;
-   hooks;
-   compatibility;
-   diagnostics.

### Reduced global state

Global state should be limited to information genuinely required across
runtime hooks.

### Explicit limits

Important capacities are explicitly bounded rather than relying on very
large implicit values.

### No unnecessary generated files

The release build does not require `.gfr` output.

### Clear build structure

The Visual Studio project is designed for:

``` text
Visual Studio 2026
MSVC v145
Win32 / x86
```

All source files, headers, include files, assembly bridge files,
solution and project files are kept together in a single directory for
easier inspection and building.

------------------------------------------------------------------------

# 16. Visual Studio 2026 Compatibility

The project targets:

``` text
Platform: Win32 / x86
Toolset: MSVC v145
IDE: Visual Studio 2026
```

GTA IV is a 32-bit application, so the plugin must be built for
x86/Win32.

The source does not rely on GCC-only inline assembly syntax inside
normal MSVC C source files.

Assembly bridge functionality is kept in a dedicated x86 assembly source
file rather than mixing incompatible compiler-specific inline assembly
into the C implementation.

This makes the project structure more appropriate for modern Visual
Studio builds.

------------------------------------------------------------------------

# 17. What Was Intentionally NOT Changed

The following were deliberately left alone:

-   GTA IV save format;
-   game density setting above `1.0`;
-   provider capacity (`40`);
-   unrelated model draw distances;
-   unrelated game rendering systems;
-   global model-info draw distance;
-   texture quality;
-   graphics settings;
-   vanilla game assets.

The goal is a focused procedural streaming fix, not a general graphics
overhaul.

------------------------------------------------------------------------

# 18. Configuration Summary

The Safe Balanced baseline is approximately:

``` ini
[ProceduralPool]
; Conservative stability-oriented preset.
; Fixed capacities avoid the more aggressive automatic pool expansion.
Capacity=16384
RenderedObjectCapacity=8192
ProviderCapacity=40

; 1.25x procedural-object distance: visible improvement without the
; aggressive 2x-4x ranges that can amplify streaming pressure.
DistanceMultiplier=1.25

; Keep density neutral. Distance is increased, density is not.
PlantDensityMultiplier=1.0
ProceduralObjectDensityMultiplier=1.0
DensityClassMask=3

; Logging is useful for diagnosing compatibility without enabling a message box.
DebugLog=1
HangWatchdog=0
HangTimeoutSeconds=30
HangMessageBox=0
FullHangDump=0
```

Exact option names may depend on the final configuration parser used by
the build.

------------------------------------------------------------------------

# 19. Expected Result

Compared with vanilla GTA IV, the intended result is:

-   visibly greater procedural grass/object visibility;
-   more distant grass remaining present;
-   better procedural object streaming;
-   no artificial increase in global model draw distance;
-   no `.gfr` files;
-   lower peak object-pool pressure than the original aggressive
    configuration;
-   lower diagnostic/runtime overhead;
-   better behavior under heavy streaming conditions.

Compared with the original aggressive version, the intended result is:

-   lower maximum pool sizes;
-   lower risk of excessive runtime memory/state pressure;
-   less aggressive global modification;
-   fewer unnecessary diagnostic side effects;
-   more conservative failure behavior;
-   a more maintainable Visual Studio build structure.

------------------------------------------------------------------------

# 20. Important Disclaimer

This edition is designed to **reduce crash risk**, not to mathematically
guarantee that GTA IV can never crash.

The project modifies internal game behavior through runtime hooks. GTA
IV's internal addresses, structures, streaming system and object
lifetimes are not officially documented APIs.

Therefore the correct goal is:

> Reduce unnecessary pressure and dangerous failure modes while
> retaining a meaningful visual improvement.

A completely crash-free guarantee would require exhaustive testing
across every mission, cutscene, location, weather state, camera state,
save state, and game configuration.

For practical validation, the most important tests are:

1.  dense city areas;
2.  rapid camera rotation;
3.  entering/leaving interiors;
4.  driving quickly through the city;
5.  mission transitions;
6.  cutscenes;
7.  long continuous sessions;
8.  saving and loading;
9.  areas with large amounts of procedural vegetation.

------------------------------------------------------------------------

# Final Design Philosophy

The original mod demonstrates that GTA IV's procedural grass/object
system can be pushed substantially beyond its conservative vanilla
behavior.

This edition intentionally does **not** chase the maximum possible
number of objects.

Instead, it aims for a balanced point:

**More distance than vanilla\
+ more useful grass/object coverage\
+ controlled pool sizes\
+ no `.gfr` generation\
+ reduced diagnostic overhead\
+ safer behavior under heavy streaming**

The central principle is:

> **Prefer a small visual degradation at the extreme streaming limit
> over risking instability in the entire game.**

That is the reason this edition uses bounded pools and moderate distance
scaling rather than simply reproducing the original 32K/40K-style
configuration.
