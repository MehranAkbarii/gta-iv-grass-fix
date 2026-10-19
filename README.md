GTAIV Grass & Procedural Props Fix 
GTA IV normally limits how many procedural objects can exist simultaneously. Once those limits are reached, grass and other details may stop appearing, leaving large or inconsistent empty patches. This mod expands the relevant limits and offers configurable density settings.
co-developed by iammrmikeman and mehraanakbarii.
Main features
Increased maximum grass and vegetation capacity.
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
Optional water-reflection control for generated procedural props.
Installation instructions
Install FusionFix OR your prefered .asi laoder
Copy these files into your Fusion Fix GTA IV ASI plugin folder:
GTAIV.EFLC.ProceduralFixes.asi
GTAIV.EFLC.ProceduralFixes.ini
Development Update
Originally created by iammrmikeman, Grass Fix is now officially co-developed with mehraanakbarii.
We're bringing our development work together, combining our improvements and continuing the project collaboratively.

New Features & Improvements
ConeBack: Camera-aware procedural prop generation with configurable spawning and camera prediction.
Generator Improvements: Expanded matrix-pool support and additional generator controls.
Water Reflections: New optional reflection controls for procedural props.
Performance: Rebalanced object capacities and reduced background overhead.
Stability: Improved startup and failure handling, with unnecessary diagnostics removed.
Thank you to everyone supporting the project.
We look forward to continuing its development and bringing further improvements to GTA IV.
Recommended settings:
[ProceduralPool]
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
ConeTurnLeadMs=300
Requirements:
 your prefered .asi laoder
Highly Recomended Mods in Screenshots:
Console Vegetation
Solitude 3

Mod Compatibility
Props Restoration and Restored Vegetation are not recommended alongside Grass Fix v3.4
 as they may cause instability or crashes.
mehraanakbarii, has provided modified compatibility versions of both mods, 
with potentially conflicting files removed available in Grass Fix Discord
Supported GTA IV versions
GTA IV 1.0.4.0 
GTA IV 1.0.7.0
GTA IV 1.0.8.0
GTA IV Complete Edition 1.2.0.59
GTA IV Vice City Next Gen Edition


Open Source
GTAIV Grass & Procedural Props Fix is now open source as of version 1.0, allowing the implementation and future development of the project to be inspected by the community.
My Mods Will Always Be Free
Youtube - Please Support me
Reach me: iammrmikeman25@gmail.com 
My Discord Server - iammrmikeman Mods
Be patient — might be buried in debug log
Grass Fix GitHub
Big thank you to TJDM, whose brilliant video“GTA IV Has a Grass Problem,” inspired us to create this mod.
