# Unlimited Fast Travel - Immersive Mode

A small F4SE plugin that adds **Immersive Mode** to Unlimited Fast Travel: no fast travel from the
Pip-Boy map at night.

- Between **Night starts at** and **Night ends at** (24-hour clock, default 21 -> 6, i.e. 9 PM to 6 AM)
  a fast travel picked on the Pip-Boy map is refused and the HUD says
  **"It's too dark! Fast Travel unavailable."**
- When the night ends (while you play, sleep or wait) the HUD says
  **"The sun shines! Fast travel available."** (can be turned off).
- Everything is on Unlimited Fast Travel's own MCM page, in a new **Immersive Mode** section. Changes
  apply when you close the pause menu (no restart, unlike UFT's own switches).
- Message texts: `sTooDarkMessage` / `sSunMessage` in `Data\MCM\Config\UnlimitedFastTravel\settings.ini`
  (MCM can't edit text).
- Quest and script trips are never blocked: only a trip queued while the Pip-Boy is open, or within
  3 s of it closing.

## Install

Requires F4SE 0.7.9 (Fallout 4 1.11.240), Address Library for F4SE Plugins, MCM and Unlimited Fast
Travel. Install after Unlimited Fast Travel and let it overwrite UFT's two MCM files
(`MCM\Config\UnlimitedFastTravel\config.json` and `settings.ini`): they are UFT's own, with the
Immersive Mode section added. UFT's `UnlimitedFastTravel.dll` itself is not changed or included.

## How it works

`PlayerCharacter::QueueFastTravel` (Address Library ID 2232918) is what the game calls once a fast
travel has been confirmed, after all of the game's checks and UFT's changes to them. It is hooked
(MinHook); at night, for a Pip-Boy map trip, the call is dropped and the message shown. Because it sits
after the checks, it works whatever UFT switches you use, including "Script Block".

Log: `Documents\My Games\Fallout4\F4SE\UnlimitedFastTravelImmersive.log` (says whether the hook went in
and every refused trip).

## Build

`.github/workflows/uft-immersive.yml` builds it (and runs `tests/test_nightrule.cpp`) on every push that
touches `mods/UnlimitedFastTravel/`. Push a `uft-v*` tag to publish a release zip.
