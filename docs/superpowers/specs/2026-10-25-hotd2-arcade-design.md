# HOTD2 arcade standalone VR

The user requests another standalone mod matching the four earlier NAOMI ports, explicitly using the arcade game rather than the Dreamcast release.

Use the current Death Crimson OX standalone fork as the implementation base because it includes the shared persisted menu, P1-left/P2-right hand assignment, solo gun transfer, reload modes, haptic levels, controller labels and first-run model preparation. Work in E:/___flattovrdev/hotd2-arcade-vr, isolated from the other active port projects.

Support the measured MAME `hotd2` USA cartridge (`epr-21585.ic22`, CRC b23d1a0c, plus twenty 8 MiB MPR chips). Require an authentic supported HOTD2-specific BIOS (`hod2bios.zip`, or supported BIOS embedded in the game ZIP). The user's existing naomi.zip includes suitable HOTD2 BIOS bytes and can supply a private research copy. Ordinary NAOMI BIOS bytes are insufficient. No CHD or Dreamcast media is accepted by the standalone build. Other arcade revisions remain unsupported until measured independently.

Create its own Android application identity com.vrlightgunhub.hotd2arcade, launch/start screen, title, configuration/saves and release file names hotd2-arcade-vr-quest.apk and hotd2-arcade-vr-pcvr.zip. Preserve upstream and earlier-port credits/licenses. Never package proprietary ROMs, BIOS, RAM/VRAM dumps, generated native weapon caches or signing secrets.

Measure the arcade camera/projection matrices, stock hit-test lens, player/gun state and calibration from private emulator research. Guard any code patches against exact stock words. A successful emulator boot alone does not establish VR support. Verify stereo reconstruction and tracked aiming against the actual arcade renderer, two players, reloads, menu persistence and hand swapping through the inherited simulator. Native gun model preparation must use this arcade cartridge; retain the bundled licensed gun choices as fallback. Do not use Dreamcast assets or offsets as assumptions.

Produce local Quest and Windows builds, reproducible build/package scripts and a validation record stating what was exercised and what remains untested. Physical headset validation cannot be claimed without it. Connect the arcade port to VR4ALL only when an actual accessible release exists; preserve the existing upstream Dreamcast choice and Maze of the Kings integration. Publishing is a separate external step and must not be falsely claimed as part of local build verification.

# Implementation plan

1. Establish a private arcade research run using an existing unrestricted build; capture boot identity and multiple RAM/VRAM/render samples from attract mode and actual play.
2. Add test-first standalone cartridge/BIOS/media gates and Android file checks; exercise missing, corrupted, alternate-version and embedded-BIOS inputs.
3. Add an arcade-only measured rendering/input profile, state/calibration mapping and native weapon capture. Verify profile math and guarded patches with targeted tests, and compare renders/aim against the real cartridge.
4. Apply distinct app branding, setup instructions and packaging. Build Windows/OpenXR and Quest arm64; run setup, native policy, menu/hand/input and actual-game simulator checks.
5. Independently review changes, fix findings, produce clean code-only artifacts and SHA-256 evidence, and report material limits accurately.
