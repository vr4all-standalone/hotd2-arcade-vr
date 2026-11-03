## HOTD2 calibration update — 2026-10-30

Current new/missing/invalid/Reset defaults are Custom22% minimum /75% maximum /6.7m shrink, matching HOTD Model2 by explicit user request. Valid saved choices are preserved without forced migration. Ranges and artwork math are unchanged. Earlier common starting values below are historical.

# Reticle controls regression tests

From this source tree, run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/reticle-controls-test.ps1
```

Requirements: Python3, the existing MinGW-w64 GCC/GLEW installation at `D:/msys64/mingw64`, and a Windows desktop OpenGL3.3 driver. The hidden WGL test context does not start a game, OpenXR session or headset. The scripts compile the real production Option/Settings/INI/menu and glow shader/draw/host endpoint code under `build-win/vrtest/reticle-controls`.

Every adapter here offers Custom / None and starts with minimum size25% and shrink distance6m. Missing, malformed and out-of-range saved values use those starting defaults. Valid existing explicit reticle choices persist. Legacy global or per-game panel/world values do not choose the starting shrink distance. The actual effective physical panel distance still feeds the Gunblade size-reference formula and never moves the real 3D target.

The INI suite covers strict mode validation, both numeric ranges/steps, real menu edits and save/load, global preference ownership,6m defaults even when legacy effective panels differ, and preservation of existing explicit tuned settings. The actual GPU suite verifies opaque logical-player red/blue arms on black and bright scenery, unchanged scene pixels through the hollow center, common world vertices for both eyes, true hit targets and distance-size floor, Laser/None independence, and unchanged menu/native-service pointers. MOK also exercises the original staff pulse/motes.

No In-game choice is offered because persistent native reticle artwork has not been verified in these adapters. Retained angle, assignment, package, launcher, game-specific and effect suites remain in their existing locations. ROM-based acceptance, headset launch/play, performance and publication are separate checks.