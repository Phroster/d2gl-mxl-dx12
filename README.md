<p align="center">
  <img src="docs/banner-dx12-v1.1.svg" alt="MXL Smooth Motion DX12 — smoother gameplay and sharper visuals for Median XL" width="880">
</p>

<p align="center">
  <strong>Enjoy Median XL with smoother movement, steadier battles and sharper visuals.</strong><br>
  D2GL graphics and D2FPS movement smoothing, brought together with DirectX 12.
</p>

<p align="center">
  <a href="https://github.com/Phroster/mxl-smooth-motion-dx12/releases/download/v1.1/mxl-smooth-motion-dx12-1.1.zip"><strong>⬇️ Download 1.1</strong></a>
  &nbsp; · &nbsp;
  <a href="#install--copy-paste-play">📦 Installation</a>
  &nbsp; · &nbsp;
  <a href="docs/SETTINGS.md">⚙️ Settings</a>
</p>

---

## ✨ What you get

| | In your game |
|---|---|
| 🎮 **Smoother movement** | Play above the original 25 FPS in single player and online, with smoother movement between game updates. |
| ⚔️ **Steadier crowded fights** | Improved performance in busy areas with lots of enemies and effects. |
| 🖼️ **Sharper visuals** | HD text and cursor, upscaling, sharpening, bloom and other picture options. |
| 🗺️ **Automatic map reveal** | Your act map reveals on entry, so pressing T afterward does not repeat the full reveal. |
| 🎨 **ReShade support** | Use your favourite DirectX ReShade presets alongside the built-in graphics options. |

The game keeps its normal speed. Your FPS target follows your monitor by default, and you can change it in the settings.

<a id="install--copy-paste-play"></a>

## 📦 Install in four steps

1. **Set up the launcher.** Let Median XL finish updating. Choose **Glide or DirectDraw**, turn **Windowed** off, and tick both **Glide3x.dll** and **Ddraw.dll** under **Unofficial Graphic Drivers**.

   ![Enable Glide3x.dll and Ddraw.dll in the Median XL launcher](docs/launcher-settings.png)

2. **Close the game and launcher.** [Download the ZIP](https://github.com/Phroster/mxl-smooth-motion-dx12/releases/download/v1.1/mxl-smooth-motion-dx12-1.1.zip) and extract it.

3. **Copy these six files** into your Median XL game folder, beside `Game.exe`. Choose **Replace** when asked.

   ```text
   glide3x.dll
   ddraw.dll
   d2gl.mpq
   d2gl.ini
   d2fps.ini
   mxl-diagnostics.ini
   ```

4. **Launch Median XL and play.** Movement smoothing and automatic map reveal turn on by themselves.

**Keep the game's existing `d2fps.dll`.** It is required and is not replaced by this download.

The included settings are ready to use. Back up your current files first if you want to keep a custom setup.

### ⬆️ Already using MXL Smooth Motion DX12?

Close the game and launcher, then replace **`glide3x.dll`, `ddraw.dll` and `mxl-diagnostics.ini`**. Keep your existing `d2gl.ini`, `d2fps.ini` and matching `d2gl.mpq` to preserve your settings.

## ⚙️ Make it yours

| You want to… | Here's how |
|---|---|
| Change the picture | Press **Ctrl+O** for shaders, sharpening, bloom, HD text and cursor options. |
| Set an FPS limit | Open **Ctrl+O → FPS**. Save your changes and restart the game. |
| Switch fullscreen or windowed | Press **Alt+Enter**. Keep Windowed off in the launcher. |
| Check movement smoothing | Open **Ctrl+O** and look for **Multiplayer Smoothing Fix: On**. |
| Edit settings in Notepad | Close the game, edit `d2gl.ini` or `d2fps.ini`, save and launch again. |

**Map reveal:** preparing a new act's map can briefly pause the game on entry. Returning to an already revealed act does not repeat the work; a new game starts fresh.

[📖 Full settings guide](docs/SETTINGS.md) · [Keep your settings or restore a backup](docs/INSTALL.md)

## 🎨 Using ReShade

Select `Game.exe` in the [ReShade installer](https://reshade.me) and choose **DirectX 10/11/12**. Keep your preset and shader folder.

Already using OpenGL ReShade? Switch it to **DirectX 10/11/12**. ReShade is optional and downloaded separately.

## 💻 Requirements

- **Median XL 2.14.0** with **Diablo II: Lord of Destruction 1.13c**.
- **Windows 10 or newer**.
- A graphics card that supports **DirectX 12**.

## 💛 Credits & thanks

**D2GL and D2FPS are the foundations of this package.** A big thank-you to their creators and to the people who brought these improvements to the Median XL community.

| Creator | Credit |
|---|---|
| **[Bayaraa — D2GL](https://github.com/bayaraa/d2gl)** | The original D2GL graphics wrapper, HD text and cursor, shaders and in-game graphics menu. |
| **[Jarcho — D2FPS](https://github.com/Jarcho/d2-rs/tree/main/d2fps)** | The FPS engine and movement smoothing that bring higher frame rates to classic Diablo II. |
| **Pooquer** | The early Median XL adaptations of D2GL. |
| **[GavinK88 — D2GL for Median XL](https://github.com/GavinK88/d2gl-mxl-1.0)** | The Median XL D2GL fork on which this edition is based. |
| **Median XL team** | Median XL itself, its game features and the community around it. |
| **[Phroster](https://github.com/Phroster/mxl-smooth-motion-dx12)** | MXL Smooth Motion DX12 and maintenance of this edition. |

Thanks also to **Bolrog**, **Mir Drualga**, the **libretro shader community**, **Omar Cornut**, and the many developers whose contributions are part of D2GL. Their [original acknowledgements](https://github.com/GavinK88/d2gl-mxl-1.0#credits) and [library credits](https://github.com/bayaraa/d2gl/blob/master/THIRD_PARTY_LICENSES.md) are preserved.

And to **Blizzard North** and the **Diablo II modding community**: thank you for the game and the creativity that keep it alive.

---

<p align="center">
  <a href="https://github.com/Phroster/mxl-smooth-motion-dx12/releases/download/v1.1/mxl-smooth-motion-dx12-1.1.zip"><strong>⬇️ Download MXL Smooth Motion DX12 1.1</strong></a>
</p>

<details>
<summary>Source code & licensing</summary>

[Build guide](docs/BUILD.md) · [Architecture](docs/ARCHITECTURE.md) · [Original projects](docs/UPSTREAM.json) · [License](LICENSE)

</details>
