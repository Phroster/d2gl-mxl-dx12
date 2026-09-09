# Native loot effects implementation

Release 1.15 uses animated Diablo sprites for real ground items, with readable
names and optional click-to-pickup interaction. The included 2,468-base catalog
has 66 item profiles. See the [loot guide](loot-effects-guide.md) for categories,
progression rules and controls. These categories describe visual importance,
not trade prices or affix rolls, and do not read the user's native filter rules.

## World effects

On the verified 1.13c/Sigma build, a guarded D2Client world-unit draw hook scopes
each real ground item. The existing D2Gfx image hook captures its identity and
local sprite alignment. On the next frame, the first world-unit draw emits the
captured effects before drawing the ordinary unit. Later world draws cover them.

Both local unit tables are checked. A candidate must match the same ground item,
base, seed and act. Camera coordinates are recomputed with native projection;
old unit pointers are not retained. View changes invalidate old alignment and
picked-up items fail the ground-mode check.

Diablo's D2CMP and D2Glide/D2DDraw routines decode and draw the DC6 animations.
No gameplay units are spawned. Bloom is baked from each frame's bright pixels
and drawn with native additive blending before the main sprite. The effect and
bloom share the world pass. Names use the existing HD text rendering pass.

Artwork is authored at four times native pixel density, on a canvas at least
1024x1024, and downsampled into the native palette. Wide or tall frames are split
into cells no larger than 256x256. Bloom is blurred before splitting. Item rank
sets the effect and label sizes; quiet supplies retain a readable label floor.

Each selected item gets a 792 ms landing pulse followed by a smaller 600 ms
repeating ground pulse. A bounded identity history prevents repeated landing
bursts when revisiting remembered loot and rearms after an observed pickup or
new flight-to-ground transition. First-seen existing loot also pulses.

## Loading and drawing cost

CMake runs `native_loot_bake` to build and compress the artwork into both renderer
DLLs. The bank contains 135 distinct resources; identical profile artwork is
shared. At startup, resource sizes, checksums and DC6 boundaries are validated
before unpacking and native normalization. Missing or invalid assets disable the
effects rather than generating them during a gameplay frame.

The runtime keeps a fixed sprite bank and bounded item/name caches. Separate
frame budgets allow 12 minor, 24 valuable and 24 major effects; common supplies
cannot consume the major-item allowance. Sprite tile counts bound drawing work.
These limits are not a measurement of frame times in a crowded live game.

The shared Glide sprite cache keys texture contents by both width and height.
Reusing an address or identical bytes at another aspect ratio cannot reuse an
incompatible atlas rectangle. Current-frame sprite slots remain pinned.

Loot cells use their own immutable texture identities. On an atlas miss, their
pixels come from the intended DC6 frame; cache hits require no extra decode.
Native geometry, clipping, blending and world order remain in use. Invalid
cells are skipped, and diagnostics record native texture-source mismatches.

## Labels and pickup

Names come from Sigma's inventory-tooltip formatter, with a bounded cache.
Equipment and jewel text uses Sigma's rarity colour independently of effect
colour. This preserves the same rare name and quality shown in inventory. Labels
follow both dropping and grounded items; a missing name is retried on the next
draw. The four rank scales are 0.95, 1.10, 1.25 and 1.40 relative to the normal
dropped-item font, in addition to the user's HD text scale.

Nonmagical equipment with the native runeword flag is classified before ordinary
tier cutoffs. Finished runewords retain names, effects and click targets without
changing item quality or the native filter matcher. The optional native filter
profile separately aligns the selected sacred-base keep and map rules.

Nearby names are measured before drawing and arranged into stacks ordered by
effect importance, item quality and equipment tier. Full multiline bounds and
clickable borders stay separate. Stable item identities retain settled positions;
inventory panels and the HUD bound the available area. Dense piles use another
column when a stack fills the visible height. Label sizes and effect artwork
are unchanged. The label layout regression covers ordering, spacing, clicking,
camera movement, panel boundaries and the full 60-item draw budget.

A guarded native selection-update hook extends item selection to visible effect
and label bounds. It uses native item eligibility and respects menus, the HUD,
inventory panels, held cursor items and native combat targets. Selection results
are cached until the frame, mouse or input state changes. Old Alt/show-items
controls must be cleared by the player if they interfere with the new controls.

Real mouse input still reaches the game. The game handles walking, pickup range,
inventory checks and its ordinary pickup action. The wrapper does not construct
pickup network messages. Item properties, game resolution, simulation timing and
audio samples are unchanged. The additional D2Client detours are the world draw
at RVA 0x6CC00 and selection update at RVA 0x51E80; native game files on disk are
unchanged. Existing renderer hooks remain in place.

## Compatibility, diagnostics and validation

Startup verifies the supported game DLL and item-archive hashes and checks native
entry points before installing hooks. A mismatch disables the affected feature
and records the reason in `mxl-native-loot-<process ID>.log`. Diagnostics include
startup timings, draw counts, rejected identities and interaction counts. Frame
samples are bounded; the normal loop does not continuously write diagnostic logs.

`native_loot_test` checks classification, progression, budgets, both unit tables,
identity expiry, view invalidation, pulses, tiled frame reconstruction and asset
validation. With a game folder supplied, an offline helper exercises the real
D2CMP normalizer, decoder, drawer and cleanup. A renderer DLL argument also
compares its embedded artwork byte-for-byte with generated frames, including
profile aliases. `native_loot_pickup_test` covers selection, input caching,
label sizing and panel boundaries. See the [build guide](BUILD.md).

Live checks cover town and outdoor drops, hovering and clicking, inventory-open
pickup, camera movement and effects behind foreground objects. Offline checks
alone do not establish compatibility with every live scene or future game build.

Set `Enabled=0` in `mxl-native-loot.ini` and restart to disable the feature, or set
`ClickEffects=0` to keep visuals with original pickup controls. See
[installation and backups](INSTALL.md) to restore a previous renderer.
