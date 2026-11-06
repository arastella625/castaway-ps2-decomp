# Playtest plan

One session of about an hour, to compare decoded assets with the game running. `tools/livecheck.py`
saves what it needs while you play, so nothing has to be written down.

## Before you start

Run these from `D:\castaway_assets`, in two terminals, with the paths for your machine. The watcher
waits for the game and stops by itself 15 minutes after the game closes.

    python -I tools/livecheck.py watch D:/tools/castaway_work/ram_snaps
    D:\tools\castaway_work\pcsx2_test\pcsx2-qt.exe -fastboot -- "<path to iso>"

Keys: Cross `X`, Circle `C`, Square `Z`, Triangle `S`, Start `Enter`, Select `Backspace`, D-pad
arrows, left stick `I` `J` `K` `L`, L1 `Q`, R1 `E`, L2 `1`, R2 `3`. `F8` takes a screenshot.
Press `F8` whenever something looks interesting or different from our exports.

The watcher saves up to 30 snapshots. It saves when a Sim's finished maps change, so each new look
needs confirming with Wear or Done, or by entering the game. Early on, check it's working: the
watcher's terminal should print a line after the first Sim is confirmed.

## Create-a-Sim: 25 minutes

Confirm each look before moving on.

| # | Sim | Set up | Answers |
|--:|---|---|---|
| 1 | Female | Skin 1/5, default clothes | Whether Create-a-Sim maps are 256 pixels |
| 2 | Female | Skin 5/5, body type 2 | Skin tone order, body type |
| 3 | Female | Captain's hat, then a straw hat | Why our captain's hat looks ragged, and `hb` hair |
| 4 | Female | Glasses, necklace, bracelet, belt | Small tiles in the `ll` map |
| 5 | Female | Each Accessory Set, 1 to 3 | `ha1` to `ha3` |
| 6 | Female | Body and head sliders at both extremes, then one face feature at a time | Morphed faces, heavy bodies, head shapes |
| 7 | Male | Skin 2/5, beard, default clothes | Male Sims, facial hair |
| 8 | Male | Hat and accessories | Whether the male rules match |
| 9 | Either | Three outfits with colour options | Colour letters, torso, sleeve and leg pairings |

## In the game: 30 minutes

| # | Do | Answers |
|--:|---|---|
| 10 | Walk, run, sit, swim if possible, for a few minutes | Animation |
| 11 | Change clothes at a dresser | Whether maps rebuild in play |
| 12 | Play until clothes wear out, if that happens | Degraded textures, `d1` and `d2` |
| 13 | Meet another Sim, a chimp or a boar | Rigs and textures outside Create-a-Sim |
| 14 | Visit a few lots, and press `F8` on each | Object sizes on a real lot |
| 15 | On the second lot, look for purple, or for soft dark or light washes on the ground | How `missingshader` terrain is drawn: see `formats/world.md` |
| 16 | `F8` on a screen full of text | Whether our font spacing is right, as it looks wide |
| 17 | Listen to the music in stereo, and to a looping sound | Left and right order, and how streams loop |
| 18 | Stand next to placed objects on a lot, and `F8` | Object positions and heights, against `tools/placed.py list` |

## Afterwards

Run this for each snapshot:

    python -I tools/livecheck.py compare "<path to iso>" D:/tools/castaway_work/ram_snaps/snap_01.bin

Snapshots and screenshots are game data, so keep them out of the repo.
