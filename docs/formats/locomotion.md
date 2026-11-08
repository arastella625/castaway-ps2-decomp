# Locomotion: how fast a routing Sim moves

How a walking or running Sim's position and animation advance along a route. **Proven** means read from the PS2 code (SLUS_216.64) at the address given; **likely** means read on Wii (`ts2c`), where PS2 wasn't checked line by line. Wii gives the names. PS2's `SAnimator2` fields sit 8 bytes later than Wii's; the offsets below are PS2's.

## Units

- **Positions** are floats in sixteenths of a tile (the `FTilePt` fixed point: tile × 16). The animator keeps the Sim's route position at `+0xA8`, `+0xAC`.
- **Speeds** are sixteenths of a tile per second.
- **A clip's root travel is in tiles.** `setMovementVelocityFromAnimation` multiplies it by 16 to get the same units as a position.

So **1.8 tiles a second** means 1.8 of the clip's own length units a second.

## 1. Where the position comes from (proven)

**A speed, not the clip's root motion.** The route step moves the position by speed × dt in a straight line towards the next route point. It never reads the clip's root track.

- **`advanceAlongNode`** (Wii name; PS2 `0x106A10`) calls **`moveTowardsDestination`** (PS2 `0x106EA0`).
- **The speed** is `GetMovementVelocityFromStyle(style)` × `GetTrackScaleVelocityFactor()`. These are vtable slots 53 and 52 (`+0x1B0`, `+0x1A8`) of the adult Sim animator's vtable at `0x4A7B18`. The style is the animator's `+0x1B4`.
- **The step:** the distance to the next route point (an `FTilePt` from the route vector at `+0x98`, index `+0xA0`).
  - Under 1e-4: the Sim snaps to the point.
  - Otherwise it moves by speed × dt along the direction.
  - If that would overshoot, it lands on the point and the unused part of dt goes on to the next segment.
- **`sidestepAlongNode`** (PS2 `0x106860`) is the same, without acceleration.

**The speeds** (`AdultSimAnimator::GetMovementVelocityFromStyle`, PS2 `0x123AA0`, **matched**, `fe1a444`):

| Style | Speed | Tiles a second |
|---|---|---|
| 0 walk, 8, 9, anything above 9 | 28.8 | **1.8** |
| 1, 2 run | 64.0 | **4.0** |
| 3, 4 | the speed measured from that style's clip (`+0x154`, `+0x158`) | — |
| 5, 7 | 0 | 0 |
| 6 sidestep | 24.0 | 1.5 |

- `GetTrackScaleVelocityFactor` (PS2 `0x11D238`, **matched**) always returns 1.
- **Wii differs:** its run speed is 60.8 (its `MiscTweakTool::kSimMoveRunVelocity`), and its style 9 is a separate free-move run at 60.8. PS2 has 64 and folds 9 into walk.
- These are the adult animator's speeds. Other animator classes (the other vtables at `0x4A7FB8`, `0x4A8208`, `0x4A8458` and `0x4A86A8`) have their own slot 53, which wasn't read.

## 2. The clip's playback rate (proven)

**The walk and run clips are sped up or slowed down to fit the route speed, so the feet don't slide.**

**The clip's own speed** is measured once per style by `setMovementVelocityFromAnimation` (PS2 `0x1060B8`), and cached by a bit in `+0xE0`:

```
duration    = (N - 1) / rate                      (rate: the clip's +0x70; N: its frame count, +0x18)
time        = duration * (N - 2) / (N - 1)        (= (N - 2) / rate, the time to frame N - 2)
travel      = root position's forward (y) component at frame N - 2
clipSpeed   = travel * 16 / time                  only if travel > 0.2 (kMinWalkAnimDistance, 0x491E90)
```

It's stored at `+0x140` (walk), `+0x144` (run), `+0x148`/`+0x14C` (the same two when `+0x7B4` is set), `+0x154` (style 3) or `+0x158` (style 4).

**`handleWalkRunAnimation`** (PS2 around `0x108A60`) then sets the track speed (`EAnimController::SetTrackSpeed`, `0x3F3E30`):

```
walk (style 0):  rate = GetMovementVelocityFromStyle(0) / clipSpeed[+0x140] * speedRatio[+0x1BC] * 1 * timeMultiplier
run (1, 2):      rate = GetMovementVelocityFromStyle(style) / clipSpeed[+0x144] * speedRatio * 1 * timeMultiplier
```

- `speedRatio` is the current speed ÷ the style's full speed, which `moveTowardsDestination` writes while the Sim is in route state 3.
- `timeMultiplier` is `+0x1A8 × +0x1B0`. Both are 1.0 from the animator's constructor (`0x11D3D0`). **Likely** they're the game-speed setting (Wii's `UpdateSetTimeMultiplier` reads `cSimulator::GetSpeedMultiplier`).
- **Nothing in the move or rate path reads motives, mood or terrain.** The speed depends only on the style.
- The style comes from the Sim's object data 0x11 (see 4), which trees can set.

## 3. Time base (proven)

- **The route step's dt** is the global float at `0x49FDC4`, multiplied by the time multiplier.
- That global starts at 1/60 and is rewritten each frame from the measured frame time (`0x1BF0E4` converts milliseconds; `0x3E3014` multiplies a count by a constant). **So movement is in real seconds**, whatever the frame rate.
- **A clip's length at game speed** is (N − 1) / (its stored rate at `+0x70`), before the playback rate above.

Whether the Port's 2.233 s for `af_walk_base` is that length depends on its N and stored rate. I couldn't check it from the code alone: the clip data isn't read here. If 2.233 s and 1.5 tiles are right, the game plays the walk clip at about 28.8 / (16 × 1.5 / 2.233) ≈ 2.7×. That's quick enough that the clip's stored rate is worth a second look.

## 4. Walk/run switch, acceleration and stopping

**Likely, from Wii**, with the PS2 constants checked where noted.

**Walk/run switch** (`UpdateWalkRunAnimation`):

- The Sim uses the style in its object data 0x11 only when the destination is more than `GetWalkToRunDistance` (16 = 1 tile) away and the rest of the route more than `GetRunToWalkDistance` (32 = 2 tiles).
- Near the end it drops back to style 0, walk.
- Between walk and run, the two clips are blended by track intensity: (speed − walk) / (run − walk) (`GetWalkRunIntensityRatio`).

**Acceleration** (`getUseSpeed`, inlined in PS2's `0x106EA0`). The time to reach full speed, `a`, is per style, each divided by the time multiplier. **Proven, PS2 constants:**

| Style | `a` |
|---|---|
| 1 | 1.0 s |
| 2 | 0.3 s |
| Others, including walk | 0.1 s |

- Speeding up: speed rises by full speed / `a` per second, up to full speed.
- Slowing down: from above full speed it falls at half that rate.

**Stopping** at the end of the route (the last two points):

- The Sim starts braking within 0.5 × speed × `a` of the destination.
- Walking brakes over 1.44 sixteenths (under a tenth of a tile), so a walk stops almost dead.
- A style-1 run brakes over 2 tiles.
- With object data 9 set, it's instead `max(sqrt(distance / brakingDistance), 0.3)` × speed.

**walk speed = 28.8 sixteenths of a tile per second = 1.8 tiles (clip length units) a second, constant once up to speed after 0.1 s, with the walk clip played at 1.8 ÷ (its own measured tiles a second); run = 4.0 tiles a second (PS2; Wii 3.8).**
