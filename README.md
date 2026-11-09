# B.H.O.P. - Boston Hopping Overhaul Project (v1.1.0)

Half-Life 1 and 2 player movement, camera and sounds for Fallout 4 (runtime 1.11.240, F4SE 0.7.9).
Everything is set in-game: Pause menu -> Mod Config -> **B.H.O.P.** Changes apply when you close the menu.

## Movement

- **Direct stick input**: the stick is read directly, so Fallout's own slow speed-up/slow-down is skipped
  (first person). Fallout still decides when you may move (dialogue, scenes).
- Half-Life ground friction and acceleration, run every frame by default for the smoothest, delay-free feel
  (exact HL tick rate available). **Responsiveness** slider for more or less grip.
- **Speeds**: Fallout's own, or Half-Life's (HL2: walk 150, run 190, sprint 320 using Fallout's AP drain;
  HL1: run 320, sprint optional).
- **Surfaces**: ice slides, mud/snow/sand drag, water slows you (Source-style surface friction).
- **Weight**: carry weight and Power Armor slow your starts and turns.
- Walls, NPCs and doors: the part of your speed going into them is removed and you slide along the rest
  (Source's ClipVelocity, using the direction the game pushed you back). Slopes and steps are left alone.
- Pushes (hits, explosions, scripts) add to your momentum, Half-Life style. Scripted walks and anything
  moving you far faster than you can run are left to Fallout.
- Short "airborne" moments on stairs count as ground.
- Uses the game's own physics step time, so VATS slow-motion and time changes stay in sync.

## Air and bunnyhopping

- Half-Life air control: momentum kept in the air, air strafing (hold left/right and turn the same way),
  with an optional **strafe assist** for sticks.
- Bunnyhop: jump again within 0.05 s of landing (original games: one frame) and friction doesn't slow you.
  Action Boy/Girl widens the window; at max rank, hold jump to bunnyhop automatically (auto-jump can also be
  set to always or off). HL1's bunnyhop cap and HL2's jump boost (with its ABH quirk) are included.

## Camera (first person)

- Half-Life 1 view bob, strafe roll and landing tilt. "Like the game" = on in HL1 mode, off in HL2 mode.

## Sounds

- Half-Life 1/2 footsteps, jump and landing sounds by surface, timed like Half-Life; vanilla player footsteps
  muted (NPCs untouched). Same reverb as vanilla footsteps (works with reverb mods). Sounds follow you.

## Perks

- Bonus mode (default): Moving Target = faster sprint, Strong Back = weight slows you less, Adamantium Skeleton =
  softer landing tilt, Aquaboy/Aquagirl = no water drag, Sneak = quieter steps, Agility = better air control,
  Action Boy/Girl = wider bunnyhop window and auto-jump at max rank.
- Unlock mode: air control needs Agility 5, bunnyhop needs Moving Target 1, ABH needs Moving Target 3.

## Install

- Install the release zip with your mod manager (or extract into `Fallout 4\`).
  Requires F4SE, Address Library for F4SE Plugins and MCM.
- Half-Life sound files are not included: put your own copy in `Data\Sound\FX\BHOP\HL1\` and `...\HL2\`
  as loose files.
- Updating from an older version: delete `Data\MCM\Settings\BHOP.ini` once so new defaults apply.

## Sound files

Loose `.wav` (or `.xwm`) files, 16-bit mono recommended, named `<group><number>.wav`:

| Folder | Groups (file names) |
|---|---|
| `Data\Sound\FX\BHOP\HL1\` | `pl_step1-4` (concrete), `pl_metal1-4`, `pl_dirt1-4`, `pl_duct1-4`, `pl_grate1-4`, `pl_tile1-5`, `pl_slosh1-4`, `pl_wade1-4`, `pl_ladder1-4`, `pl_swim1-4`, `pl_fallpain1`/`pl_fallpain3` |
| `Data\Sound\FX\BHOP\HL2\` | `concrete1-4`, `metal1-4`, `metalgrate1-4`, `chainlink1-4`, `duct1-4`, `wood1-4`, `woodpanel1-4`, `grass1-4`, `gravel1-4`, `mud1-4`, `sand1-4`, `dirt1-4`, `tile1-4`, `slosh1-4`, `wade1-4`, `ladder1-4`, `hardboot_generic1-9` (Power Armor), `softshoe_generic1-8` (carpet) |

Groups with exactly 4 files use Half-Life's left/right pattern (1/3 left, 2/4 right); others play a random
file. A missing group falls back to concrete (`concrete` / `pl_step`). The log lists which surface uses
which group.

## How the native hooks work (version-specific)

Built for runtime 1.11.240. All hooks are checked when installed (type and target), installed once, and
chain with other plugins that changed the same functions. Problems are written to the log.

- **Movement**: the player's character controller `SetLinearVelocityImpl` (virtual 0x3A). Only the player.
- **Camera**: the first person camera state's rotation/position (virtuals 0x0C/0x0D).
- **Muting vanilla player footsteps**: `BGSImpactManager`'s footstep listener, found through the game's
  type information. Player footstep events are recognised by finding the player inside the event (the first
  match is accepted). If the listener can't be found, the log says so and vanilla footsteps keep playing
  (double footsteps).

## Known limits

- Collision and push detection compare what the game did with what the mod asked for; there's no direct wall
  normal from the engine, so very noisy geometry may need tuning (`fPushThreshold`).
- HL1's edge friction (extra friction at ledges) isn't included.
- Perks are found by name (English by default; change the names in the settings file for other languages).
- No speedometer or crouch-jump yet.

## Math tests

`src/HLMath.h` holds the Half-Life math (friction, acceleration, air acceleration, wall clipping, HL1
bunnyhop cap, HL2 jump boost, step sounds, landing punch), checked against Valve's published code.
`tests/test_hlmath.cpp` tests it on every build.

## Build

Every push is built by GitHub Actions. Publishing a release attaches the mod zip automatically.

## Logs

`Documents\My Games\Fallout4\F4SE\BHOP.log`. Detailed logging is off by default: MCM -> Debug -> Debug log.

## License

GPL-3.0, see `LICENSE`.
