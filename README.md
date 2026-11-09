# B.H.O.P. - Boston Hopping Overhaul Project (test build v0.4.0)

Half-Life 1 and 2 player movement and sounds for Fallout 4 (runtime 1.11.240, F4SE 0.7.9).

## Movement (stage 1)

- Half-Life ground acceleration and friction (friction 4, accelerate 10, stop speed 100) at
  Half-Life's tick rate (HL1: 100/s, HL2: 66.67/s).
- Weight: fuller inventory = slower to start, turn and stop. Power Armor heavier still.
- Walls take your speed away, like in Half-Life.
- Fallout's walk/run/sprint speeds for now; ground only; player only.

## Camera (new in 0.4.0)

- Half-Life 1 view bob, strafe roll and landing tilt (first person). "Like the game" = on in
  HL1 mode, off in HL2 mode (HL2's camera doesn't bob), or force them on/off.

## Sounds

- The player's footsteps, jump and landing sounds come from Half-Life 1 or 2,
  picked by the surface under you (concrete, metal, grate, wood, dirt, grass, tile, water...).
- Timed like Half-Life: a step every 0.3 s running, 0.4 s walking, quieter when walking/sneaking,
  left/right foot pattern, floor sound at full volume when you jump and land.
- The player's vanilla footsteps are muted while it's on. NPC footsteps (and mods like
  Project Reality Footsteps) are left alone.
- Our sounds reuse the vanilla footstep "output model", so they get the same reverb and
  distance settings as normal footsteps, including changes from reverb mods.
- Toggle: Mod Config -> B.H.O.P. -> Sounds (or `bHalfLifeSounds`).

## Settings (MCM)

Pause menu -> Mod Config -> **B.H.O.P.**: movement mode, Power Armor, ground values, weight,
sounds, volume and timing. Changes apply when you close the menu.
Defaults: `Data\MCM\Config\BHOP\settings.ini`. Your changes: `Data\MCM\Settings\BHOP.ini`.
Surface overrides (`Material.<name>=<sound>`) can be added to either file.

## Build

Every push to this repo is built by GitHub Actions (Actions tab).
Download the **BHOP** artifact.

## Install

- Install the release zip with your mod manager (or extract into `Fallout 4\`). It contains
  `Data\F4SE\Plugins\BHOP.dll` and `Data\MCM\Config\BHOP\`. Requires F4SE, Address Library and MCM.
- The sounds zip -> extract into `Fallout 4\` so you get
  `Data\Sound\FX\BHOP\HL1\` and `...\HL2\` (keep them as loose files, not in a BA2).

The sound files are Valve's, from your own copy of the games. Keep them for personal use and
don't include them if you ever upload the mod.

## Test

Walk, run, sneak and jump on different floors (concrete, metal, wood, dirt, water), then send
`Documents\My Games\Fallout4\F4SE\BHOP.log`.
