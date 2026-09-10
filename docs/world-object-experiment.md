# Object indicators (local experiment)

Set `ObjectLabels=1` under `[NativeLoot]` in `mxl-native-loot.ini` and restart.
Set it to `0` to return to the normal item-only display.

Ordinary chests, urns, barrels, racks and other loot containers get only a soft
native pulse. Locked, large and special containers also get a readable name;
special containers have a stronger glow. Usable shrines get a name and pulse.
Wells have a quiet pulse; waypoints and your stash have just a name. Exploding
containers use a red pulse. Opened/broken containers, spent shrines, doors and
ordinary scenery are excluded. Existing dropped-item labels are unchanged.

Names share the loot layout; dropped items keep priority. Object names are
informational: open objects using the game's normal click target. The native
hover name takes over when pointing at an object.

The experiment samples at most twelve objects from the current world draw,
without room scans or retained unit pointers. It reuses the existing immutable
native pulse cells and font atlas. No new artwork, file loading during play,
gameplay actions, or diagnostic recording is added. Object metadata is read
only after the existing checks for the supported game DLLs have passed.
