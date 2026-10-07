# GTAIV Procedural Fixes — Conservative SAFE / Visual Studio 2026

This source tree is intentionally conservative for everyday GTA IV use.

## Build layout

All source, header, generated, project, and documentation files are kept in this single directory. There is no `src` directory and no required secondary source directory.

Open `GTAIVProceduralFixes.sln` in Visual Studio 2026 and build `Release | Win32`.

The project uses the Visual Studio 2026 `v145` MSVC toolset and C11 language mode. Precompiled headers are disabled so every translation unit is self-contained and does not depend on a hidden `pch.h`/`stdafx.h` include.

## Conservative runtime policy

- Required procedural grass/prop hooks are retained.
- Draw-distance scaling is enabled with a conservative default of 1.25x and a hard runtime ceiling of 1.5x.
- Procedural density remains at 1.0x by default; the mod does not compensate for increased distance by multiplying grass count.
- Operational capacity is bounded at 16,384.
- Rendered-object capacity is bounded at 8,192.
- Provider capacity remains 40, matching the supported vanilla-side contract.
- No 32K/40K runtime pool is exposed or allocated.
- No gameplay-time pool resize/reallocation is introduced.
- Pool exhaustion fails closed: new entries are refused/clamped instead of writing beyond fixed storage.
- The original engine streaming/lifecycle path remains authoritative; the mod only adds bounded hooks around the supported contracts.
- No `.gfr` or flight-recorder file is generated.
- Test/fixture-only process termination paths are not part of the production project.
- The production runtime contains no `ExitProcess`/`TerminateProcess` call. Initialization, compatibility, allocation, and pool-limit failures return failure/disable the mod instead of terminating GTA IV.
- PCH is disabled and each project source/header has explicit dependencies, reducing `undefined struct` errors caused by include order.

## Important limitation

The current source does not invent a distance-based eviction algorithm when the underlying callback does not provide a trustworthy world-space distance for each candidate. In that case it uses bounded rejection rather than guessing an eviction order. This is intentional: a fake "farthest first" policy could corrupt ownership/lifetime state and would be less safe than refusing a new distant candidate.

## Build output

`build\\Release\\GTAIV.EFLC.ProceduralFixes.asi`

## Compatibility

The installer continues to require a verified executable identity/patch contract before modifying the game. An unsupported or mismatched executable is rejected without applying partial runtime patches.
