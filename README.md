[center][b][size=5]GTAIV Grass & Procedural Props Fix [/size][/b][/center]
[center]GTA IV normally limits how many procedural objects can exist simultaneously. Once those limits are reached, grass and other details may stop appearing, leaving large or inconsistent empty patches. This mod expands the relevant limits and offers configurable density settings.[/center]
[center]co-developed by iammrmikeman and mehraanakbarii.[/center]
[center][img]https://staticdelivery.nexusmods.com/mods/253/images/1230/1230-1785482352-111774481.jpg[/img][/center]
[center][b][size=5]Main features[/size][/b][/center]
[code]Increased maximum grass and vegetation capacity.
Increased capacity for litter, rubbish and procedural ground props.
Improved grass and vegetation coverage across Liberty City.
Configurable grass, vegetation and procedural-object density.
Adjustable procedural-object generation and visibility distance.
Individual density controls for grass, vegetation, litter and other object classes.
Rebalanced procedural and rendered-object pool capacities.
Expanded shared matrix-pool support for generated props.
Configurable generator matrix limits where applicable.
Cone-based, camera-aware procedural-prop generation.
Predictive camera-turn handling to help reduce visible prop pop-in.
Adjustable spawning radius, cone angle and prop-release limits.
Optional water-reflection control for generated procedural props.[/code]
[center][/center]
[center][img]https://staticdelivery.nexusmods.com/mods/253/images/1230/1230-1785482362-778906314.jpg[/img][/center]
[center][b][size=5]Installation instructions[/size][/b][/center]
[center]Install FusionFix OR your prefered .asi laoder
Copy these files into your Fusion Fix GTA IV ASI plugin folder:[/center]
[center][size=2][font=Monaco, Menlo, Consolas, monospace]GTAIV.EFLC.ProceduralFixes.asi[/font][/size][/center]
[center][size=2][font=Monaco, Menlo, Consolas, monospace]GTAIV.EFLC.ProceduralFixes.ini[/font][/size][/center]
[center][img]https://staticdelivery.nexusmods.com/mods/253/images/1230/1230-1785482362-715458878.jpg[/img][/center]
[center][b][size=5]Development Update[/size][/b][/center]
[center][size=3]Originally created by [b]iammrmikeman[/b], Grass Fix is now officially co-developed with [b]mehraanakbarii[/b].[/size][/center]
[center][size=3]We're bringing our development work together, combining our improvements and continuing the project collaboratively.[/size][/center]
[center][b]
[size=5]New Features & Improvements[/size][/b][/center]
[center][b]ConeBack:[/b] Camera-aware procedural prop generation with configurable spawning and camera prediction.[/center]
[center][size=3][b]Generator Improvements:[/b] Expanded matrix-pool support and additional generator controls.[/size][/center]
[center][size=3][b]Water Reflections:[/b] New optional reflection controls for procedural props.[/size][/center]
[center][size=3][b]Performance:[/b] Rebalanced object capacities and reduced background overhead.[/size][/center]
[center][size=3][b]Stability:[/b] Improved startup and failure handling, with unnecessary diagnostics removed.[/size][/center]
[center][/center]
[center][size=3][b]Thank you[/b] to everyone supporting the project.
We look forward to continuing its development and bringing further improvements to[b] GTA IV.[/b][/size][/center]
[center][/center]
[center][b][size=5]Recommended settings:[/size][/b][/center]
[code][ProceduralPool]
Capacity=16384
RenderedObjectCapacity=8192
ProviderCapacity=40

DistanceMultiplier=1.25
PlantDensityMultiplier=1.0
ProceduralObjectDensityMultiplier=1.0
DensityClassMask=3

DebugLog=1 
HangWatchdog=0
HangTimeoutSeconds=30 
HangMessageBox=0
FullHangDump=0

GeneratorMatrixLimit=4096
GeneratorMatrixPool=0
PropsInWaterReflections=1

ConeBack=70
ConeFullAngle=75
ConeTurnLeadMs=300[/code]
[img]https://staticdelivery.nexusmods.com/mods/253/images/1230/1230-1786299933-103406746.png[/img]
[center][/center]
[center][b][size=5]Requirements:[/size][/b][/center]
[center] your prefered .asi laoder[/center]
[center][/center]
[center][size=5][b]Highly Recomended Mods in Screenshots:[/b][/size][/center]
[center][url=https://github.com/Tomasak/Console-Visuals/releases/]Console Vegetation[/url][/center]
[center][url=https://www.nexusmods.com/gta4/mods/417]Solitude 3[/url][/center]
[center]
[b][size=5]Mod Compatibility[/size][/b][/center]
[center]Props Restoration and Restored Vegetation are not recommended alongside Grass Fix v3.4[/center]
[center] as they may cause instability or crashes.[/center]
[center][b]mehraanakbarii[/b], has provided modified compatibility versions of both mods, [/center]
[center]with potentially conflicting files removed available in [url=https://discord.com/invite/4pWnszrvMP]Grass Fix Discord[/url][/center]

[center][b][size=5]Supported GTA IV versions[/size][/b][/center]
[center]GTA IV 1.0.4.0 [/center]
[center]GTA IV 1.0.7.0[/center]
[center]GTA IV 1.0.8.0[/center]
[center]GTA IV Complete Edition 1.2.0.59[/center]
[center]GTA IV Vice City Next Gen Edition[/center]
[center]
[img]https://staticdelivery.nexusmods.com/mods/253/images/1230/1230-1787726665-177038007.jpg[/img]
[b]Open Source[/b]
[code]GTAIV Grass & Procedural Props Fix is now open source as of version 1.0, allowing the implementation and future development of the project to be inspected by the community.[/code]
[/center]
[center][/center]
[center][u][b][size=4]My Mods Will Always Be Free[/size][/b][/u][/center]
[center][url=https://www.youtube.com/@Iammrmikeman]Youtube[/url] - Please Support me[url=https://www.youtube.com/@Iammrmikeman][/url][/center]
[center]Reach me: [url=https://www.nexusmods.com/profile/iammrmikeman]iammrmikeman25@gmail.com[/url] [/center]
[center]My Discord Server - [url=https://discord.gg/4pWnszrvMP]iammrmikeman Mods[/url][/center]
[center]Be patient — might be buried in debug log[/center]
[center][url=https://github.com/iammrmikemanmods/gta-iv-grass-fix]Grass Fix GitHub[/url][/center]

[list]
Big thank you to TJDM, whose brilliant video“GTA IV Has a Grass Problem,” inspired us to create this mod.[/list]
[center][img]https://staticdelivery.nexusmods.com/mods/253/images/1230/1230-1787726659-261216645.jpg[/img][/center]
