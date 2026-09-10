# Chests and shrines

Highlights are enabled by default. Set `ObjectLabels=0` under `[NativeLoot]`
in `mxl-native-loot.ini` and restart to hide them.

Chests and racks get a native floor pulse and a compact name. Urns and barrels
get just the pulse; other usable objects get a small glint. Locked, large and special containers get a stronger cue;
special containers have a stronger glow. Usable shrines get a name and pulse.
Wells have a quiet cue. Exploding
containers use a red cue. Opened/broken loot containers, spent shrines,
doors, waypoints, portals, personal stash and non-interactive scenery are excluded.
Hidden stashes remain highlighted as loot containers. Existing dropped-item labels are unchanged.
Object effects follow the centre of the native body sprite when available.
Nearby objects share one glow and a count label, including mixed containers.
Groups stay within 64 by 32 game pixels. Their count updates as objects open;
each object remains individually clickable. Clicking a shared name selects its
most important remaining object. Explosive containers keep a separate red cue.

Names share the loot layout. Object names and effects are clickable when
`ClickEffects=1`. Actual dropped items and native character targets
keep priority. The game still handles walking, opening and activation normally.

Up to twelve object or group names are shown at once. Other highlighted
containers keep their glow and remain individually clickable.
