# B.H.O.P. - Boston Hopping Overhaul Project (v1.0.0)

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
- Walls take your speed away. Short "airborne" moments on stairs count as ground.
- Uses the game's own physics step time, so VATS slow-motion and time changes stay in sync.

## Air and bunnyhopping

- Half-Life air control: momentum kept in the air, air strafing (hold left/right and turn the same way),
  with an optional **strafe assist** for sticks.
- Bunnyhop: jump again right after landing and friction doesn't slow you. HL1's bunnyhop cap and HL2's jump
  boost (with its ABH quirk) are included and can be turned off.

## Camera (first person)

- Half-Life 1 view bob, strafe roll and landing tilt. "Like the game" = on in HL1 mode, off in HL2 mode.

## Sounds

- Half-Life 1/2 footsteps, jump and landing sounds by surface, timed like Half-Life; vanilla player footsteps
  muted (NPCs untouched). Same reverb as vanilla footsteps (works with reverb mods). Sounds follow you.

## Perks

- Bonus mode (default): Moving Target = faster sprint, Strong Back = weight slows you less, Adamantium Skeleton =
  softer landing tilt, Aquaboy/Aquagirl = no water drag, Sneak = quieter steps, Agility = better air control.
- Unlock mode: air control needs Agility 5, bunnyhop needs Moving Target 1, ABH needs Moving Target 3.

## Install

- Install the release zip with your mod manager (or extract into `Fallout 4\`).
  Requires F4SE, Address Library for F4SE Plugins and MCM.
- Half-Life sound files are not included: put your own copy in `Data\Sound\FX\BHOP\HL1\` and `...\HL2\`
  as loose files.
- Updating from an older version: delete `Data\MCM\Settings\BHOP.ini` once so new defaults apply.

## Build

Every push is built by GitHub Actions. Publishing a release attaches the mod zip automatically.

## Logs

`Documents\My Games\Fallout4\F4SE\BHOP.log` (MCM -> Debug -> Debug log).
