# Loot effects

Native animated loot effects, with bloom and smoother edges. Based on all 2,468
item bases in the installed Median XL catalog. Effects follow real ground items
and work without holding Alt. Pickup and inventory items do not keep an effect.

Effects combine starbursts, spark trails and crystal flecks. Uniques have tall beams; sacred uniques,
high runes and top rewards get the biggest pillars and showers of sparks.

Selected loot gets a large expanding pulse when it first lands in view, fading
over about 0.8 seconds. A smaller, quicker pulse repeats every 0.6 seconds while
it stays on the ground. Walking away and back does not replay the large burst
for remembered items. This applies only to items selected for effects.

| Loot | Visual |
| --- | --- |
| Common runes | Small blue rune symbol; higher common runes get a gold beam and more sparks |
| Enchanted / elemental runes | Tall violet or elemental-coloured rune effects |
| Great runes / Xis | Large red or white beams with rotating rune symbols |
| Perfect gems | Floating faceted crystals in the gem's colour; clusters get a larger crystal effect |
| Arcane shards / crystals / clusters | Orbiting violet or blue fragments; clusters get a larger effect |
| Corrupted shards / crystals | Red fragments and glow |
| Shrines and cycles | Rotating geometric symbols; large and golden cycles get stronger effects |
| Signets | Floating gold seals, with more height and light for greater signets |
| Uniques / sacred uniques / sets | Tall gold or green beams; sacred equipment gets an even taller pillar and star crown |
| Rares | Small amber marks while leveling; sacred rares get brighter marks |
| Useful sacred bases | Blue ground brackets or glow; ethereal bases get extra emphasis |
| Sacred scythes | A floating crescent; sacred unique/set scythes add a gold/green beam |
| Relics, effigies and unique mystic orbs | Large magical symbols and orbiting light; relics and unique orbs get top-level beams |
| Angelic / Mastercrafted gear | Large white / red beams |
| Trophies, quest rewards and special materials | Compass-like symbols with glow; fragments are smaller than completed trophies |
| Crafting scrolls, dyes, oils and catalysts | Distinct crafting symbols or floating orbs, kept smaller for common supplies |
| Caches and treasure | A chest-shaped marker with a burst of light |
| Named endgame rewards | Explicit coverage for mirrors, brands, winds, tenets, marks, crafting scrolls and other untyped rewards |

There are 62 item profiles sharing 59 distinct combinations of shape, size and
colour across 13 visual families. Related materials can share artwork. These
priorities describe item category and progression, not trade prices or affix rolls.

The effects follow the existing cleanup preferences: imperfect gems stop
glimmering at level 50; rare Tier 1/2/3/4 equipment stops at levels 31/51/77/90.
Uniques, sets, crafted and honorific equipment remain eligible. Ordinary blue
equipment stays quiet, while crafting jewels retain a small glimmer.

The existing native filter, notifications and map markers are unchanged. Effects
are capped at 12 small, 24 medium and 24 large items per frame; large loot has a
separate allowance so common items cannot consume it. A crowded pile may exceed
those visual limits. Animation artwork is built and cached once at game startup.

The native diagnostics retain draw counts by item profile, including
`ArcaneShard`, `ArcaneCrystal`, `ScytheBase`, `RuneGreat` and the other categories.
See [installation and backups](INSTALL.md) to keep your settings or restore an earlier build.
