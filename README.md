<p align="center">
  <img src="docs/banner-dx12-v1.14.svg" alt="MXL Smooth Motion DX12 1.14" width="880">
</p>

<p align="center">
  <a href="https://github.com/Phroster/d2gl-mxl-dx12/releases/download/v1.14/mxl-smooth-motion-dx12-1.14.zip"><strong>Download 1.14</strong></a>
</p>

- Improved performance and fewer FPS drops.
- Smoother movement.
- Loot effects, a leveling filter and easier pickup.
- Automatic map reveal.

<a id="install--copy-paste-play"></a>

## Install

1. Update through the Median XL launcher. Select **Glide or DirectDraw**, turn **Windowed** off, and enable **Glide3x.dll** and **Ddraw.dll** under **Unofficial Graphic Drivers**.
2. Close the game and launcher. Extract the ZIP into your game folder, next to `Game.exe`, and replace the files.
3. Launch and play. **Keep the game's existing `d2fps.dll`.**

<details>
<summary>Launcher settings</summary>

![Launcher settings](docs/launcher-settings.png)

</details>

**Updating?** Replace `glide3x.dll`, `ddraw.dll` and `mxl-diagnostics.ini`; add `mxl-native-loot.ini`. Keep your existing `d2gl.ini`, `d2fps.ini` and `d2gl.mpq` to keep your settings.

**Controls:** Ctrl+O for settings · Alt+Enter for fullscreen · Click loot to pick it up.

**Loot filter:** The ZIP includes an optional leveling filter. [Import instructions](docs/INSTALL.md#loot-filter).

[Installation guide](docs/INSTALL.md) · [Settings](docs/SETTINGS.md) · [Loot guide](docs/loot-effects-guide.md)

**Requires:** Median XL 2.14.0, Diablo II LoD 1.13c, Windows 10+ and a DirectX 12 graphics card.

<details>
<summary>Credits & licenses</summary>

## 💛 Credits & thanks

**D2GL and D2FPS are the foundations of this package.** A big thank-you to their creators and to the people who brought these improvements to the Median XL community.

| Creator | Credit |
|---|---|
| **[Bayaraa — D2GL](https://github.com/bayaraa/d2gl)** | The original D2GL graphics wrapper, HD text and cursor, shaders and in-game graphics menu. |
| **[Jarcho — D2FPS](https://github.com/Jarcho/d2-rs/tree/main/d2fps)** | The FPS engine and movement smoothing that bring higher frame rates to classic Diablo II. |
| **Pooquer** | The early Median XL adaptations of D2GL. |
| **[GavinK88 — D2GL for Median XL](https://github.com/GavinK88/d2gl-mxl-1.0)** | The Median XL D2GL fork on which this edition is based. |
| **Median XL team** | Median XL itself, its game features and the community around it. |
| **[Phroster](https://github.com/Phroster/d2gl-mxl-dx12)** | MXL Smooth Motion DX12 and maintenance of this edition. |

Thanks also to **Bolrog**, **Mir Drualga**, the **libretro shader community**, **Omar Cornut**, and the many developers whose contributions are part of D2GL. Their [original acknowledgements](https://github.com/GavinK88/d2gl-mxl-1.0#credits) and [library credits](https://github.com/bayaraa/d2gl/blob/master/THIRD_PARTY_LICENSES.md) are preserved.

And to **Blizzard North** and the **Diablo II modding community**: thank you for the game and the creativity that keep it alive.

[Build guide](docs/BUILD.md) · [Architecture](docs/ARCHITECTURE.md) · [Original projects](docs/UPSTREAM.json) · [License](LICENSE) · [Licensing & credits](docs/LICENSING.md)

The D2GL-based code is licensed under **GPL-3.0-or-later**. Third-party components retain their own terms. The licensing page lists their notices and source references.

</details>
