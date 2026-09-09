# Native loot effects implementation

Release 1.11 adds animated Diablo sprites to real ground items, using the
existing native world rendering path.

The `spectacle-v5` revision uses the complete 2,468-base catalog, with
62 item profiles / 59 distinct visual combinations. See [the effect guide](loot-effects-guide.md)
for the current categories and leveling rules. Shards, crystals, shrines, signets, dyes, common and advanced
crafting items, scythes and untyped endgame rewards now have explicit coverage.

- Common runes and selected sacred bases get a small glimmer.
- Perfect gems get floating crystals; tiered uniques get tall bright beams.
- Sacred uniques and sets get wider beams with spiral light trails, coloured gold and green respectively.
- Angelic/Mastercrafted items and selected special drops get the tallest beam.

These categories express visual importance, not a market-price estimate. The
native filter and its existing notifications and map markers are unchanged.
The effects add visuals; they do not add or alter sounds.

High-value beam canvases are 192 pixels
tall for tiered uniques, 288 for sacred uniques/high runes, and 352 for top loot.
Sparkles have independently spaced paths, twinkling star tips and diamond flecks.
Each selected item gets a 792ms expanding landing pulse followed by a smaller,
smooth 600ms ground pulse. A 512-entry identity history avoids replaying landing
bursts after camera/panel changes, clears on leaving the game, and rearms on a
new flight-to-ground transition or an observed pickup. First-seen existing loot
also pulses; very old identities can be evicted from this bounded history.
The fixed category budgets bound the additional drawing work. Native decoder
tests verify the sprites; they do not measure busy-scene frame times.

## How it works

On the verified 1.13c/Sigma build, a guarded D2Client world-unit draw hook scopes
each real ground item. The existing D2Gfx image hook captures that item's identity
and local sprite alignment. On the next frame, the first actual world-unit draw
through that same verified hook emits the captured effects before drawing the
ordinary unit. Subsequent world draws cover them. This uses the native world-unit
route reached by Sigma, without depending on a separate stock foreground phase.

Only identities actually drawn the preceding frame are eligible. Both local unit
tables are checked, matching the renderer's established lookup pattern. The
second table contains ordinary replicated drops. Either candidate
must still match the same ground item, base, seed and act. Camera
coordinates are recomputed with the native projection; old screen coordinates
and unit pointers are not retained. Level, viewport, panel and perspective
changes invalidate the old alignment. Picked-up items fail the ground-mode
check. New items gain their effects one rendered frame after first being drawn.

Diablo's own D2CMP and D2Glide/D2DDraw routines select, decode and draw the DC6
animations. Labels and ImGui are not involved, and no gameplay units are spawned.
The bloom is a precomputed Gaussian blur of each frame's bright pixels, encoded
in the native palette and drawn with native additive DrawMode 3 before the main
sprite. It affects these loot sprites only. Both layers share the earlier world
pass; this is not a fullscreen postprocessing effect or a new depth buffer.

Valuable-drop artwork is generated on a canvas at least 1024x1024, at four times native
pixel density. Curves and spark positions retain fractional coordinates. A 4x4
area filter reduces each native pixel into the verified unit palette, and native
additive blending preserves soft edges without dark fringes. This improves edge
coverage; it does not introduce 1024-pixel runtime sprites or change the game's
resolution. Tall beams use extra vertical authoring space. Final beam images
are at most 176x352, with padded bloom images at most 200x376. These are split
into two native cells, each at most 256 pixels tall. Signed cell offsets join
their rows without overlap. Bloom is blurred before splitting, avoiding seams.
Landing pulses fit a single cell, at most 240x96. The larger floor aura preserves
the previous world anchor through a five-pixel offset compensation.

No item properties, resolution values, simulation timing, audio samples or
network messages are changed. The additional memory patch is a draw-function
detour at D2Client RVA 0x6CC00. Existing 1.1 renderer hooks remain in place. Native
DLL files on disk are unchanged.

The bank contains 4,128 distinct cells, with identical art shared among related
profiles and 24 shared rank/colour landing animations. They are generated once.
The decoder test exercises every profile including aliases: 4,320 cells total.
There is no per-item allocation,
name matching or per-frame file access. Separate frame budgets allow 12 small,
24 medium and 24 large effects. Common items cannot consume the large-effects
budget. Small effects use one sprite draw; medium effects use two with bloom;
tall beams use four. A landing pulse temporarily adds one draw. The fixed sprite
set and native hardware caches live until
process exit. These caps bound the extra work but do not establish live frame times.

Startup verifies the exact game DLL and item-archive hashes. If the native draw
entry has already been modified, this experiment disables itself and writes the
reason to `mxl-native-loot-<process ID>.log`. Counts are saved at three startup
samples (frames 60, 300 and 900), when leaving the game and on normal shutdown.
After those three samples there are no ongoing frame-loop disk writes. Counts
include world entries, ground captures, queued identities, rejection reasons,
effect and bloom draws, and the last projected position.
The revision also records character level, draw counts by profile, total native
sprite draws and landing-pulse draws.
Character level is read through the existing native stat getter once per frame;
no additional draw hook or gameplay patch was added for this expansion.

## Validation and playing test

`native_loot_test` checks ground modes, item indices, priorities, budgets, one-frame
identity expiry, queue resets, view invalidation and DC6 boundaries. A regression
test covers a drop found only in the second local table, a reused first-table ID,
pickup, stale identity, landing timing, repeat suppression, history eviction and
tick wrap. Every joined frame is reconstructed without missing/overlapping rows.
Transparent landing endpoints are checked as well. With the
installed game directory as its argument, it also calls
the real D2CMP sprite normalizer, frame selector, software drawer and hardware
cache cleanup in an independent offline helper. It compares every decoded pixel
and checks surrounding memory guards. This does not establish live compatibility.

After restarting the game, drop a perfect gem and a unique with Alt released.
Check town and outdoors, move around them, open/close an inventory panel, pick
them up, and check effects near walls and a busy pile of loot. The effects should
follow each item and disappear on pickup. Live positioning, occlusion and frame
times still require that playing test.

## Disable or restore

Set `Enabled=0` in `mxl-native-loot.ini` and restart to disable loot effects.
To restore your previous renderer, close the game and launcher and restore the
files you backed up before installing. See [installation and backups](INSTALL.md).
