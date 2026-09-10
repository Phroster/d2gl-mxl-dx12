# Object indicators (local experiment)

Set `ObjectLabels=1` under `[NativeLoot]` in `mxl-native-loot.ini` and restart.
Set it to `0` to return to the normal item-only display.

Chests and racks get a native floor pulse and a compact name. Urns and barrels
get just the pulse; other usable objects get a small glint. Locked, large and special containers get a stronger cue;
special containers have a stronger glow. Usable shrines get a name and pulse.
Wells have a quiet cue. Exploding
containers use a red cue. Opened/broken loot containers, spent shrines,
doors, waypoints, portals, personal stash and non-interactive scenery are excluded.
Hidden stashes remain highlighted as loot containers. Existing dropped-item labels are unchanged.
Object effects follow the centre of the native body sprite when available.

Names share the loot layout. Object names and effects are clickable when
`ClickEffects=1`. Actual dropped items and native character targets
keep priority. The game still handles walking, opening and activation normally.

The experiment keeps current-frame snapshots for up to 1024 visible objects;
the twelve-name limit applies only to labels, not ordinary object cues.
It does not scan rooms or retain unit pointers. It reuses the existing immutable
native pulse cells and font atlas. No new artwork, file loading during play,
automatic gameplay actions, or diagnostic recording is added. Object metadata is read
only after the existing checks for the supported game DLLs have passed.
