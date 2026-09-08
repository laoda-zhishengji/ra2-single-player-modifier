# RA2 Single-Player Modifier

面向 Steam《红色警戒 2》原版 V1.006 的单机修改器研究与产品化工程。

> 仅用于本地单人游戏。请勿用于联机、局域网、竞技或任何需要公平性的场景。

## Project scope

- Target: Steam Red Alert II original `game.exe`, V1.006
- Runtime-only process changes; the game executable, saves, and `rules.ini` remain untouched
- Windows desktop UI with runtime status monitoring and feature toggles
- No support for Yuri's Revenge, mods, unofficial patches, other versions, or online play

## Features

- Money no-decrease while preserving normal income
- No power load
- Instant production
- Instant superweapons
- Toggleable fog/map support
- Build-distance bypass while retaining terrain, occupancy, water/land, resource, technology, and AI checks
- Automatic repair and garrison civilian-building repair

## Repository layout

| Path | Purpose |
| --- | --- |
| `product/` | Product-facing controller, services, status bridge, and UI |
| `patcher/` | Runtime patch and service implementations |
| `scanner/`, `probe/`, `tracer/` | Reproducible research and address-discovery utilities |
| `assets/` | UI artwork and feature icons |
| `release/` | Packaging script and release notes |
| `dist/RA2SinglePlayerModifier/` | Latest runnable product package |

## Build and run

See [BUILD.md](BUILD.md) for the Windows build procedure. Generate the runnable package with [release/package.ps1](release/package.ps1), then launch `dist/RA2SinglePlayerModifier/ra2_product_ui.exe` after entering a single-player mission.

For convenient distribution, the release page also provides a single-file launcher, `RA2SinglePlayerModifier.exe`. It embeds the product components, expands them into a temporary runtime directory, and starts the same UI without requiring users to manage the component files manually.

The project intentionally excludes chat exports, diagnostic logs, screenshots, local memory snapshots, compiler caches, game saves, and user-specific data.

## Status

The current package is a tested preview build for the stated Steam V1.006 target. Feature behavior can vary with game state; use the acceptance checklist in [RELEASE_ACCEPTANCE.md](RELEASE_ACCEPTANCE.md) when validating changes.

## License

No open-source license has been selected yet. Until one is added, all rights are reserved by the repository owner.
