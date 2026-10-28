# Themes & Skins

SMS ships with a finished look out of the box — a full-screen desktop background, a set of device and file icons, and a boot splash — but the desktop background is skinnable. This page explains the **desktop skin** (a `.smi` background image), where SMS looks for it, and what this fork's refreshed artwork adds.

ℹ You don't need a skin to use SMS

 Skinning is optional. A fresh install uses the built-in desktop and icons. A skin only replaces the **appearance** — it never changes which devices or formats SMS supports. If you never touch the skin settings, you simply keep the default look.

## The skin engine

SMS has always had a lightweight skin engine. The one thing it lets you replace is the **desktop background**, by pointing SMS at a `.smi` skin file:

| File | Extension | What it changes |
| --- | --- | --- |
| Desktop skin | `.smi` | The full-screen background behind the device menu, file browser and status line — the "wallpaper". |

A `.smi` is an **IPU-encoded image** — a compact picture format the PlayStation 2's IPU hardware decodes directly, the same colour pipeline SMS uses to draw video. When a desktop skin is set and loads successfully, its picture is stretched to fill the screen and **replaces the default background**; if no skin is set (or a skin file fails to load), SMS falls back to its built-in desktop.

ℹ A note on `.mbf` files

 Alongside the skin setting you may see a separate `.mbf` option. Despite living near the skin settings, `.mbf` is **not** a background image — it is an optional custom **font** used to draw text, most useful for subtitle scripts that need characters the built-in font doesn't cover. It changes how *text* looks, not the wallpaper. Subtitle rendering is covered on the [Subtitles](https://nathanneurotic.github.io/Simple-Media-System/subtitles.html) page.

ℹ Where skins are chosen

 The desktop skin name is an ordinary SMS setting. You pick it from the on-screen menus alongside the other player options, and — like every other setting — it is written to your saved configuration so it persists across reboots. See [Settings Reference](https://nathanneurotic.github.io/Simple-Media-System/settings.html) for how the settings menu is laid out and how configuration is saved.

## This fork's look

This build ships with a **refreshed desktop** and a **full set of device icons** on top of the classic skin engine. The default artwork is separate from skin loading; the October work also adds skin-input validation and a late-config repaint fix. Compared with older SMS builds you get:

- A redrawn full-screen **desktop background** and a matching **boot splash** shown while SMS starts up.
- A complete, consistent set of **device icons** — including a dedicated **MX4SIO** microSD icon (older builds reused another device's icon for it), plus icons for the internal HDD, MMCE, i.LINK and SMB.
- Redrawn **file and status icons** used throughout the browser and menus.

These are baked into the program itself, so they are the same on every device you boot from. Applying a custom `.smi` skin overrides the desktop background on top of them; the icons stay as the built-in set.

## Applying a skin

SMS looks for skin files in a `Skins` folder, and the location depends on how SMS was started:

### Use the launch folder first

On a remountable launch source, including a memory-card boot, SMS first tries `Skins/MyTheme.smi` beside the ELF. For `mc1:/APPS/SMS.elf`, that is `mc1:/APPS/Skins/MyTheme.smi`.

### Check the card fallback

If the launch-folder skin cannot be read, SMS tries `mc?:/SMS/Skins/MyTheme.smi` on the configured card slot. See [asset lookup order](https://nathanneurotic.github.io/Simple-Media-System/configuration.html#assets) rather than assuming the presence of a memory card changes the primary path.

### Select the skin, then save

Put the `.smi` file in the appropriate `Skins` folder, choose it from the on-screen menu, and save your settings so it is remembered. On the next redraw the desktop background changes to your skin.

If a skin doesn't appear

 Make sure the file has the exact `.smi` extension and sits directly in the `Skins` folder SMS is looking in (memory card *or* boot device, per above), and that the name you selected matches the file name. If a skin file can't be read or isn't a valid IPU image, SMS quietly falls back to its built-in desktop rather than showing an error — so a missing background usually means a wrong path, name or file format.

## Making a skin

Because `.smi` is an IPU-encoded image and not a plain PNG or JPEG, you don't just rename a picture — the image has to be **converted into the IPU format** with a converter tool first. The workflow, conceptually, is:

### Start from a full-screen image

Design your wallpaper at a normal screen resolution. SMS stretches the skin to fill the display, so a 4:3 image sized for standard-definition output gives the most predictable result.

### Convert it to `.smi`

Run the image through an IPU converter to produce the `.smi` file. This is the step that turns an ordinary picture into something SMS's IPU decoder can load.

### Drop it in `Skins` and select it

Copy the finished `.smi` into the `Skins` folder as in [Applying a skin](https://nathanneurotic.github.io/Simple-Media-System/themes.html#applying), pick it on-screen, and save.

<details><summary>Why isn&#x27;t a skin just a PNG or JPEG?</summary>

The desktop skin loads through the PS2's IPU hardware, which decodes a specific image format rather than PNG/JPEG. Converting your picture to `.smi` up front means SMS can load and display the background quickly using the same hardware path it uses for video, without doing a general-purpose image decode every time it draws the desktop. The device and file icons in this fork are handled by a separate artwork pipeline and are built into the program, so they aren't part of the `.smi` skin.

</details>

## Device icons

Every storage source in SMS has its own icon in the top device menu, so you can tell at a glance which device is selected. This fork ships a distinct icon for each:

| Device | Icon shows |
| --- | --- |
| USB | USB mass-storage drive |
| MX4SIO (microSD) | A dedicated microSD icon — new in this fork; older builds reused another device's icon |
| Internal HDD | Hard-disk drive |
| i.LINK | i.LINK / IEEE 1394 storage |
| MMCE (SD2PSX / MemCard PRO2) | Memory-card-slot storage device |
| SMB network share | Network share |
| CD / DVD | Optical disc — data CD or DVD |
| Audio CD (CDDA) | Audio disc |

The disc icon reflects the kind of disc in the drive — a data CD, a DVD and an audio CD are shown differently — so the device menu stays informative even when the same optical drive holds different media. See [Devices Overview](https://nathanneurotic.github.io/Simple-Media-System/devices.html) for how the device menu itself works.

## HD output deliberately uses a gradient

720p / 1080i background behavior

In 720p and 1080i modes SMS uses a plain gradient instead of loading full-screen desktop artwork, because the HD framebuffer consumes the VRAM needed by that texture. Use an SD mode when checking a wallpaper. This deliberate fallback is separate from [#27's skin-after-restart report](https://nathanneurotic.github.io/Simple-Media-System/known-issues.html#skin).

On a remountable launch path, skins are tried in `Skins/` beside the ELF first, even on memory-card boots; the configured card's `SMS/Skins/` is a fallback. See [asset lookup order](https://nathanneurotic.github.io/Simple-Media-System/configuration.html#assets). Test skin persistence after saving and relaunching.

## See also

- [Settings Reference Where skin names live & how config saves](https://nathanneurotic.github.io/Simple-Media-System/settings.html)
- [Devices Overview The device menu & icons in context](https://nathanneurotic.github.io/Simple-Media-System/devices.html)
- [Troubleshooting When something looks wrong](https://nathanneurotic.github.io/Simple-Media-System/troubleshooting.html)
- [Credits & Sources Artwork & contributors](https://nathanneurotic.github.io/Simple-Media-System/credits.html)

---
[Searchable manual](https://nathanneurotic.github.io/Simple-Media-System/themes.html) · [Edit the maintained source](https://github.com/NathanNeurotic/Simple-Media-System/blob/master/docs-site/content/themes.html)
