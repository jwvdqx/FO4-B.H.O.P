# Contextual Ammo (Fallout 4)

One F4SE DLL + one INI line. Kill anything with a gun and you may get ammo **for the caliber the gun is
firing right now** — read from the equipped weapon *instance*, so caliber-swap mods just work
(e.g. a service rifle converted from 5.56 to .308 pays out .308). Any modded gun that uses vanilla
(or any) ammo works; no patches, no ESP, no scripts, no save baggage.

## The switch
`Data\F4SE\Plugins\ContextualAmmo.ini` → `Enabled=1` (on) / `Enabled=0` (off). Re-read when you load a
save or close the Pause menu.

## The math (`src/Formula.h`)
- threat = level + maxHealth / 10
- chance of nothing = clamp(0.95 · e^(−threat/40), 15%, 95%)
- rounds = 1..(1 + round(0.75·√threat)), divided by √(ammo caps value), randomly rounded

| victim | nothing | typical payout |
|---|---|---|
| chicken / rat (10mm) | ~90% | 1–2 |
| raider (5.56) | ~60% | 1–3 |
| deathclaw (.308) | ~19% | 1–6 |
| deathclaw (fusion core / mini nuke) | ~60–70% | 0–1 |

Only the **killing hit** counts: ammo is paid only if the gun in your hands landed it
(grenades, mines, melee give no gun ammo).

**Scrounger** (vanilla perk, ranks 1–4): −10% "nothing" chance and +25% rounds per rank
(raider at rank 4: ~34% nothing, up to 6 rounds).

## Grenades (needs Demolition Expert rank 4)
- Enemies that carry grenades can give you **0–3 of one they carry**, by toughness
  (raider 0–1, gunner 0–2, overlord-tier 0–3).
- Only if your grenades did **≥ 20% of its max health in total**, or a grenade landed the kill.
- **Grenade kills: 30% chance its own grenades go off** — they're removed, you get none, and the
  nuke mine explosion goes off on the body. Stay back.
- Excluded from every pool: explosives of 400+ damage (nuke mines, Nuka grenades, mini nukes,
  and similar mod explosives).

## Requirements
Fallout 4 1.11.240, F4SE 0.7.9, Address Library for F4SE Plugins.

## Build
CI: `.github/workflows/contextual-ammo.yml` (Windows, xmake, CommonLibF4). Push a tag `ammo-v1.0.0` to publish a release zip.
