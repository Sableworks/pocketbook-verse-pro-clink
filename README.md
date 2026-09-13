# Clink — PocketBook Verse Pro Color

A match-3 game for the **PocketBook Verse Pro Color (B300 / Kaleido 3)**. Built with InkView / SDK 6. Other PocketBook models are untested.

The on-device UI is in **English**.

## Install on device

1. Build `clink.app` (see below) or copy a prebuilt binary.
2. Connect the PocketBook via USB (PC Link / mass storage).
3. Copy `clink.app` to `applications/` on device storage (`/mnt/ext1/applications/`).
4. **Disconnect USB** — with PC Link active, apps often cannot see files.
5. Launch **Clink** from the applications menu.

Progress is stored in `/mnt/ext1/.clink_save`. High scores are in `/mnt/ext1/.clink_scores`.

## Modes

| Mode | Rules |
| --- | --- |
| **Classic** | Fill the level meter. The run ends when no moves remain (the board shuffles once for free, then Mix charges). |
| **Endless** | No game over. The board reshuffles automatically. |
| **Timed** | 75 seconds. A cascade of 2+ adds 2 seconds; each level adds 8 seconds (cap 150). The clock keeps running during cascades. |

## Specials

| How you make it | Effect |
| --- | --- |
| **4 in a row** | **Burst** — detonates a 3×3 when matched or smashed |
| **L or T** | **Star** — clears its row and column |
| **5 in a row** | **Prism** — swap with any stone to wipe that color; two prisms wipe the board |

Specials chain: a blast that hits another special detonates it too.

## Tools

You start with a small stock and earn more as you level up. A cascade of 5+ grants one Smash; creating a Prism in a cascade grants one Bolt (once per cascade, not per wave).

| Button | Action |
| --- | --- |
| **Hint** | Highlight a legal swap and its direction |
| **Smash** | Destroy one stone (specials explode) |
| **Bolt** | Destroy the tapped row |
| **Mix** | Shuffle the board |

## Controls

| Action | Gesture / key |
| --- | --- |
| Swap | **Swipe** a stone toward a neighbor, or tap two adjacent stones |
| Arm Smash / Bolt | Tap the tool, then a stone (tap the tool again to cancel) |
| Hint / Mix | Tap the button |
| Pause | **Menu** |
| High Scores / Help pages | Swipe left / right, or tap |
| Exit | **Quit** on the title screen (Home also works) |

Side keys still move a cursor if you want them; the whole game is playable with a finger.

Stones use **color plus a unique shape** (circle, diamond, triangle, square, hex, plus, ring) so the board stays readable on Kaleido 3, where color is muted.

Cascades play in three beats: highlight, empty cells, then refill. The clock uses a small partial refresh so the panel is not fully redrawn every second.

## Build from source

The Docker image includes the PocketBook ARM toolchain for **Verse Pro Color (B300)**.

```bash
# One-time — build the image (Apple Silicon: force amd64)
docker build --platform linux/amd64 -t pb-clink-builder .

# Compile
docker run --rm --platform linux/amd64 -v "$(pwd):/project" pb-clink-builder \
  -c 'mkdir -p build && cd build && cmake -DCMAKE_TOOLCHAIN_FILE=/SDK/share/cmake/arm_conf.cmake .. && cmake --build .'
```

Or `./build.sh` if you already have `pb-rsvp-builder` or `pb-clink-builder`.

Output: `build/clink.app`.

## Host tests

Match-3 logic (no InkView SDK):

```bash
cc -O2 -Wall -I. -o tests/host_smoke board.c game.c tests/host_smoke.c && ./tests/host_smoke
```

## Credits

Made by Mateusz Blumensztajn ([Sableworks](https://github.com/Sableworks)).
