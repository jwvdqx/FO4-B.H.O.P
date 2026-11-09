# B.H.O.P. - Boston Hopping Overhaul Project (v1.4.0)

Half-Life 1 and 2 player movement, camera and sounds for Fallout 4 (runtime 1.11.240, F4SE 0.7.9).
Everything is set in-game: Pause menu -> Mod Config -> **B.H.O.P.** Changes apply when you close the menu.

## Movement

- **Direct stick input**: the stick is read directly, so Fallout's own slow speed-up/slow-down is skipped
  (first person). Fallout still decides when you may move (dialogue, scenes).
- Half-Life ground friction and acceleration. By default every frame is split into Half-Life-sized steps
  (10 ms in HL1, 15 ms in HL2) done right away, each using the direction you faced at that moment: you get
  Half-Life's results at any frame rate (air strafing gains what it does at 100 fps) with no added delay.
  **Responsiveness** slider for more or less grip.
- **Speeds**: read from Fallout's own movement data (walk/run/sprint/sneak, Power Armor), checked against what
  Fallout actually does, so they're right from the first step.
- **Speeds**: Fallout's own, or Half-Life's (HL2: walk 150, run 190, sprint 320 using Fallout's AP drain;
  HL1: run 320, sprint optional).
- **Surfaces**: ice slides, mud/snow/sand drag, water slows you (Source-style surface friction).
- **Weight**: carry weight and Power Armor slow your starts and turns.
- Walls, NPCs and doors: the part of your speed going into them is removed and you slide along the rest
  (Source's ClipVelocity). The mod measures how far you really moved and finds the wall from the direction
  you slid, so sliding along a wall keeps your forward speed. Optional wall probe (on by default, an
  experiment): rays check the real wall surface to get its exact direction.
- Slopes use the floor direction the game reports, so going up or across a slope never counts as a wall.
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

- Crouch-jump: press sneak in the air (while rising or just after the top) to lift your feet like Half-Life's
  duck-jump: HL1 +18 units, HL2 +36 units, or scaled to Fallout's own jump. You land sneaking, like HL keeps
  you ducked. Fallout can't shrink your body mid-air, so this is an upward boost giving the same extra height;
  normal physics still stops you at ceilings.
- Edge friction (optional, off by default): Half-Life 1 doubles friction when there's no floor just ahead,
  so you stop at ledges. Checked with three rays across your width, 16 units ahead and 34 down, like HL1.
- **Jumping and gravity** (optional, off by default): Half-Life gravity with Fallout's jump height (same
  height, quicker jump), or exact Half-Life (HL1 jumps 45 units, HL2 21). The mod then handles your vertical
  speed in the air; ceilings and launches (explosions) are followed. Fall damage may change: test it.

## Camera (first person)

- Half-Life 1 view bob, strafe roll and landing tilt. "Like the game" = on in HL1 mode, off in HL2 mode.
  The bob is Half-Life's real size (before v1.4 it was a third of it; lower **Bob strength** if you liked that).

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

- Wall and push detection compare how far you really moved with what the mod asked for; the game gives no
  wall direction, so it's worked out from your motion (and the optional wall probe).
- Perks are found by name (English by default; change the names in the settings file for other languages).
- No speedometer yet.

## Code layout

- `src/core/` - the movement rules as plain functions with no Fallout code: `HLMath.h` (Valve's formulas:
  friction, acceleration, air acceleration, wall clipping, HL1 bunnyhop cap, HL2 jump boost, step sounds,
  landing punch) and `MoveCore.h` (Half-Life-sized steps, walls/slopes/pushes, jumping and gravity).
- `src/fo4/` - the Fallout side, one part per file: settings, RTTI, game state, camera, sounds, movement
  state, recording, and the velocity hook. `src/main.cpp` includes them in order.

## Tests and replays

`tests/test_hlmath.cpp` tests the core on every build. With **Record movement** on (MCM -> Debug), closing
the pause menu records the next seconds of movement to `Documents\My Games\Fallout4\F4SE\BHOP_record.csv`.
`bhop_replay BHOP_record.csv` runs the movement core on exactly those moments, to check changes without
playing.

## Threads

- Movement runs on a physics thread; settings are only replaced on the main thread while holding the same
  lock the movement code uses, so movement always sees a complete set of settings.
- Camera effects, sounds, footstep muting, the jump/sneak key hooks and settings reloads are expected to run on the
  main thread. The debug log prints a "thread check" line for each so this can be confirmed in-game.
- Perk records are only touched on the main thread; movement reads published rank counts.

## Build

Every push is built by GitHub Actions (with the tests and the replay tool). Publishing a release on GitHub builds once more
and attaches `BHOP-<version>.zip`.

## Logs

`Documents\My Games\Fallout4\F4SE\BHOP.log`. Detailed logging is off by default: MCM -> Debug -> Debug log.

## License

GPL-3.0, see `LICENSE`.
