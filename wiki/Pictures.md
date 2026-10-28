# Pictures

SMS has a built-in **JPEG photo viewer**. Point the file browser at a `.jpg` or `.jpeg` file, press **Cross**, and SMS decodes it on the PS2's JPEG path and fills the screen with your photo. From there you can step forward and back through the pictures in the same folder, rotate an image, and flip between fit-to-screen and actual size.

## Overview

Alongside video and audio, SMS reads **JPEG** still images. JPEG decoding, rescaling and GS upload consume time and memory; large images can take longer to open. The viewer works the same way no matter where the file lives — a burned disc, USB stick, MX4SIO microSD, the internal HDD, i.LINK, MMCE, an SMB share or the UDPFS network drive. If you can browse to the file, you can view it.

ℹ What SMS shows

 The viewer handles standard baseline JPEG photos — the everyday `.jpg`/`.jpeg` files that cameras and phones produce. It is a picture *viewer*, not an editor: it opens and displays images, and lets you page through, rotate, and rescale them on screen.

## Viewing a picture

Getting a photo on screen is the same three-area workflow you use for any file in SMS.

### Pick the device

In the **device menu** (the top row of icons), use **Left/Right** to highlight the device that holds your photos, then press **Down** to drop into the file list.

### Browse to the folder

In the **file list** use **Up/Down** to move (L1/L2 page-scroll long lists), **Cross** to open a folder, and **Triangle** to go up one level. Photos can sit anywhere on the device — SMS browses the raw filesystem, so there is no special "pictures" folder you have to use.

### Open the JPEG

Highlight a `.jpg` or `.jpeg` file and press **Cross**. SMS decodes the image and displays it full-screen, with the file's path shown briefly along the bottom.

If a picture won't open

 If SMS can't decode a file, it shows the filename with an error indication instead of the photo. Progressive or unusual JPEG variants, or files that aren't really JPEGs, are the usual cause — re-save the image as a standard (baseline) JPEG and try again. You can still step to the next picture with **Right**.

## Controls in the viewer

Once a photo is on screen, the pad controls the viewer directly (these also respond to the equivalent buttons on a PS2 DVD remote):

| Button | What it does |
| --- | --- |
| **Right** (or **Cross**) | Next picture in the set. At the last image it wraps back to the first. *Only when a folder was opened with Play all → Images; a single photo opened on its own has nothing to step to.* |
| **Left** | Previous picture. At the first image it wraps to the last. *Same condition as above.* |
| **Square** | Toggle how the image is sized on screen — switches between fit-to-screen and actual (unscaled) view. |
| **Circle** | Rotate the picture. |
| **R1** | Rotate the picture the other way. |
| **Triangle** | Leave the viewer and return to the file browser. |

ℹ Stepping through a folder

 Opening a single `.jpg` with **Cross** shows just that one picture — Left and Right have nothing to step to. To page through a folder, highlight the *folder* itself and choose **Play all → Images**; Left and Right then walk the whole set in order, wrapping at each end.

## Tips

- Use **standard baseline JPEG** files (the normal output of cameras, phones, and most photo software). That is what the viewer is built for.
- The PS2 outputs a standard-definition-style picture, so there is nothing to gain from enormous multi-megapixel files — SMS fits the image to the screen anyway. Reasonable, screen-sized JPEGs open fastest and use the least memory.
- Keep the photos you want to flip through **together in one folder**. Because Left/Right step through the current folder, a tidy folder becomes an easy, browsable set of pictures.
- Faster storage means faster loading. Reading large photos from a quick device (USB, MX4SIO microSD, the internal HDD) feels snappier than over a slow disc or a busy network share.

<details><summary>Can SMS play a slideshow, or zoom into a photo?</summary>

The viewer's confirmed on-screen actions are stepping between images (Left/Right), rotating (Circle / R1), switching between fit-to-screen and actual size (Square), and returning to the browser (Triangle). This documentation only describes controls that are verified in the build, so if you're looking for an automatic timed slideshow or a free zoom-and-pan, treat those as not part of the picture viewer here.

</details>

ℹ Related pages

 Looking for moving pictures instead? See [Video Playback](https://nathanneurotic.github.io/Simple-Media-System/video.html). For where your files can live and how to turn devices on, see the [Devices Overview](https://nathanneurotic.github.io/Simple-Media-System/devices.html) and the [Settings Reference](https://nathanneurotic.github.io/Simple-Media-System/settings.html).

---
[Searchable manual](https://nathanneurotic.github.io/Simple-Media-System/pictures.html) · [Edit the maintained source](https://github.com/NathanNeurotic/Simple-Media-System/blob/master/docs-site/content/pictures.html)
