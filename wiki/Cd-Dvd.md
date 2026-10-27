# CD / DVD / CDDA

The optical drive is where SMS began. Long before USB sticks and SD adapters, SMS's whole reason for existing was to play a good-resolution movie **from a burned disc on a completely unmodified PlayStation 2** — no hard drive, no network adapter, no modchip. This page covers the disc types SMS understands, the clever **CDDAFS** trick that started it all, DVD-Video and data-disc playback, plain Audio CDs, and how to burn discs that your PS2 will actually read.

ℹ How discs appear in SMS

 Insert a disc and give the drive a moment to spin up and identify it. SMS auto-detects optical media and shows a **disc device** in the DEVICE menu (the top row of device icons); press `Down` to drop into the file list and browse it. A data disc (CDDAFS, ISO, or DVD-Video) shows its folders and media files; an Audio CD shows its tracks. You do not mount anything by hand — SMS reads the disc's table of contents and presents whatever it finds. If you would rather SMS *not* touch the drive at all (for example on a console where probing the drive is slow or unwanted), there is a **Disable CD/DVD** option in Device settings — see [Notes](https://nathanneurotic.github.io/Simple-Media-System/cd-dvd.html#notes).

## The disc types SMS understands

SMS reads four kinds of optical media, each with its own history and its own use. All of them work on a stock console; none require a chip.

| Disc type | What it is | What you get |
| --- | --- | --- |
| **CDDAFS** | Data hidden on an audio-CD layout so a stock PS2 accepts a burned CD-R | Browse a small filesystem of media files burned onto the disc — the original SMS trick |
| **DVD-Video** | A disc authored in the DVD-Video (UDF) layout, holding your media files | Play video/audio files straight from a burned DVD on an unmodded PS2 |
| **Data CD / DVD (ISO)** | An ordinary ISO 9660 / UDF data disc | Browse the disc like any other filesystem and open media files on it |
| **Audio CD (CDDA)** | A standard Red Book music CD | Play the CD's audio tracks directly |

Whichever disc you insert, the flow is the same: SMS identifies it, shows a disc device, and lets you browse or play. The differences below are about *how the disc was made*, not how you use it once it's in the tray.

## CDDAFS — the burned-CD trick that started SMS

A stock PlayStation 2 refuses to boot or read a burned data CD-R — the console's copy protection treats it as an "illegal" disc. Eugene Plotnikov's answer, borrowing an idea from Drakonite's UMCDR, was **CDDAFS**: a way of storing ordinary data files on a disc that is physically laid out as an **audio CD**. Because the PS2 happily authenticates an audio-CD-shaped disc, a CDDAFS disc reads on (in Eugene's words) hopefully any PS2 — no modchip needed. This is the very first component of SMS, and the reason the project is a "system" and not just a player.

ℹ What CDDAFS actually is

 In Eugene's own description, CDDAFS "just stores data on a CD audio disk, which can be read on any PS2." The disc's audio tracks act as **directories**, and a control track at the start holds global disc information — a name, a description, even little icon bitmaps that a browser can decorate itself with. It is a tiny, purpose-built filesystem riding inside an audio-CD table of contents. SMS reads that structure directly and shows you the files inside.

### Building a CDDAFS disc (conceptually)

You do not author a CDDAFS disc on the PS2 — you build it on a PC and burn it. Eugene wrote a companion Windows program, **CDXplorer**, whose job is to take your files and produce a **cue/bin pair** describing the special audio-CD-with-data layout. You then burn that cue/bin to a CD-R with a burning program, using a **Disc-At-Once** mode (Eugene used Nero's "Disc-At-Once / 96" option). CDXplorer can also read an existing CDDAFS disc back, so you can copy your files off again.

CDDAFS is a create-on-PC, play-on-PS2 workflow

 The CDDAFS filesystem is intentionally simple, and the tooling is old and Windows-only. Treat the exact cue/bin build as a PC task with CDXplorer (or a compatible CDDAFS builder), not something you do from the SMS menus. This page describes CDDAFS *conceptually* so you understand what the disc is and why it works — for the precise button-by-button burn, follow the tool you use to make the disc.

![CDXplorer on Windows showing a new, empty CDDAFS tree ready to have files added.](https://nathanneurotic.github.io/Simple-Media-System/images/usage-cdxplorer01.png)

CDXplorer, Eugene's PC companion tool, building a new CDDAFS layout before it writes the cue/bin pair you burn to CD-R.

![A burning program's Open dialog filtered to image files, with a CDDAFS .cue file selected.](https://nathanneurotic.github.io/Simple-Media-System/images/installation-cdburn01.png)

Burning a CDDAFS disc: your burning software opens the `.cue` that CDXplorer produced and writes it to a blank CD-R.

### Add your files on the PC

In CDXplorer (or a compatible CDDAFS tool), start a new disc and add the media files you want on it — for example a DivX/XviD movie and its soundtrack. The classic use is a single feature-length movie per disc.

### Produce the cue/bin

Have the tool generate the **cue/bin pair** that describes the CDDAFS (audio-CD) layout. This is the image you will burn — not a normal data ISO.

### Burn to CD-R with Disc-At-Once

Burn the cue/bin to a blank CD-R using a **Disc-At-Once** write mode. Because the disc masquerades as an audio CD, the PS2 accepts it. Burn slowly and use good media (see [Notes](https://nathanneurotic.github.io/Simple-Media-System/cd-dvd.html#notes)).

### Play it in SMS

Put the disc in the PS2, launch SMS, wait for the disc device to appear, drop into the file list, and open your movie. It plays straight off the burned CD-R on a stock console.

<details><summary>How does a CDDAFS disc lay its data out? (the historical detail)</summary>

From Eugene's README: track 0 is a **control track** holding a GUID, a disc name and description, a small disc picture, and a table of 32×32 icon images, followed by a directory table. Each remaining audio track is a **directory** containing file entries — a name, an icon index, and the file's start offset and size — followed by the file data itself. There is one level of directories (historically capped at 99), and the number of files is limited only by the disc's capacity. Eugene notes fitting an ~800 MB movie onto an ordinary 700 MB disc without trouble, since the audio-CD layout foregoes the extra error-correction overhead of a data disc. This is deep background — you never touch these structures by hand; the PC tool writes them and SMS reads them.

</details>

## DVD-Video — playing files from a burned DVD

SMS can also read media files from a **DVD-Video** disc on an unmodified console. Support for reading `.avi` (and other) files from DVD-Video discs was added with a `cdvd.irx` driver that understands the **UDF** filesystem DVD-Video uses. In practice this means you can author a burned DVD in the DVD-Video layout, drop your media files into it, and browse them in SMS exactly like any other device — with the big advantage that a DVD holds far more than a CD-R, so you are not limited to a single small movie per disc.

ℹ Why "DVD-Video" and not a plain data DVD

 The DVD-Video layout is the burned-disc form a stock PS2 is happiest reading, which is why SMS's DVD path targets it. When you make the disc on your PC, choose your burning program's **DVD-Video** compilation type (rather than a plain DVD-ROM data project) and place your media files into it before burning.

![A burning program's New Compilation dialog with the DVD-Video project type selected.](https://nathanneurotic.github.io/Simple-Media-System/images/usage-dvdr01.png)

Authoring the disc on the PC: pick the **DVD-Video** compilation type in your burning software, then add your media files and burn to a blank DVD-R.

Once burned, the DVD behaves like a data source in SMS: wait for the disc device to appear, enter the file list, and open a file. Video decoding uses the PS2's IPU for colour conversion, the same as from any other device — so DivX / XviD / MPEG-4 AVIs, MPEG program streams and the other [supported formats](https://nathanneurotic.github.io/Simple-Media-System/video.html) all play from disc. Soft [subtitles](https://nathanneurotic.github.io/Simple-Media-System/subtitles.html) sitting alongside a video on the disc are picked up too.

## Data CD / DVD (ISO)

Beyond the two burned-disc tricks above, SMS also reads ordinary **ISO 9660 / UDF data discs** — the standard data-disc format. On consoles able to read them, an ISO data CD or DVD simply appears as a disc device you can browse, with your media files inside. This is handy when you already have media on a normal data disc, or when your setup can read data discs directly. The browsing and playback experience is identical to CDDAFS and DVD-Video once the disc is recognised — the only difference is how the disc was mastered.

## Audio CD (CDDA)

SMS also plays plain **Audio CDs** — ordinary Red Book music CDs. Insert a music CD and SMS reads its table of contents, mounts it as an audio disc, and lists its tracks for playback. This is separate from CDDAFS: a CDDAFS disc only *looks* like an audio CD to sneak data past the copy protection, whereas a genuine Audio CD holds real audio tracks that SMS plays as music.

ℹ Audio CD vs data disc — SMS decides for you

 When a disc goes in, SMS inspects it and routes it correctly on its own: a real music CD becomes an **Audio CD** you play track by track, while a data disc (CDDAFS, ISO, or DVD-Video) becomes a filesystem you browse. You do not pick a mode — you just insert the disc and browse or play whatever SMS shows.

## Notes — burning quality, compatibility & what boots

### Burning quality & media

- **Burn slowly, on good media.** A stock PS2 optical drive — especially an older or well-used one — is far happier reading a carefully burned disc. Use quality blank CD-R / DVD-R media and a conservative write speed.
- **CDDAFS needs Disc-At-Once.** Because a CDDAFS disc is written as an audio CD, burn its cue/bin with a **Disc-At-Once** write mode, not Track-At-Once. Eugene developed and tested with Nero's Disc-At-Once/96.
- **One movie per CD-R is the classic case.** A CDDAFS CD-R is small; if you want more media (or larger files) per disc, a DVD-Video disc holds far more. For libraries of media, a rewritable storage device such as [USB](https://nathanneurotic.github.io/Simple-Media-System/usb.html) or [MX4SIO](https://nathanneurotic.github.io/Simple-Media-System/mx4sio.html) is far more convenient than juggling discs.

### Disc & console compatibility

Optical playback depends on your drive and how you boot

 Every PS2 optical drive is a little different, and older drives read burned media less reliably as they age. Beyond the drive itself, **how your console is set up to boot homebrew affects what discs it will start.** A stock console typically needs a boot method (such as a memory-card exploit) to launch SMS in the first place; solutions like a modchip or a disc-boot exploit (e.g. FreeDVDBoot) change what the console will boot directly from a disc. Those boot mechanics are outside SMS itself — once SMS is running, this page describes what it does with whatever disc you insert. If a burned disc will not read, re-burn slower on fresh media before assuming SMS is at fault.

### Disc-related settings

SMS's Device settings include a couple of options that affect the optical drive:

| Option | What it does |
| --- | --- |
| **Disable CD/DVD** | Turns off SMS's automatic CD/DVD detection, so SMS leaves the optical drive alone. Handy if probing the drive is slow or you never use discs. |
| **CD/DVD speed** | Controls the optical read speed (low / medium / high). A lower speed can help a marginal drive read a burned disc; a higher speed is faster when your media and drive are healthy. |

SMS also stops the disc spinning while a movie is paused or while you scroll the file list, and stops the drive at startup — small touches that keep the drive quieter and cooler. See the [Settings Reference](https://nathanneurotic.github.io/Simple-Media-System/settings.html) for where these live in the menus.

### Optical audio out (a bonus)

If your PS2 is connected to a receiver over its **S/PDIF optical** output, SMS can pass compressed **Dolby Digital (AC3)** and **DTS** audio through to that receiver rather than decoding it on the console — useful for films with surround soundtracks played from disc. This is unrelated to CD burning; it is just what SMS can do with a film's audio once it is playing.

## Which disc type should I use?

- [CDDAFS Burned CD-R, stock PS2, one movie](https://nathanneurotic.github.io/Simple-Media-System/cd-dvd.html#cddafs)
- [DVD-Video Burned DVD, more media per disc](https://nathanneurotic.github.io/Simple-Media-System/cd-dvd.html#dvd-video)
- [Audio CD Play a normal music CD](https://nathanneurotic.github.io/Simple-Media-System/cd-dvd.html#audio-cd)
- [Prefer no discs? USB, MX4SIO & more are easier](https://nathanneurotic.github.io/Simple-Media-System/usb.html)

Optical media is SMS's roots and still works on a bone-stock console — but if you have a USB stick, an MX4SIO microSD adapter, an internal HDD or a network share, those hold far more and never wear out a drive. See the [Devices Overview](https://nathanneurotic.github.io/Simple-Media-System/devices.html) for the full picture.

<details><summary>A short history: why SMS is a &quot;system&quot;, not just a player</summary>

SMS was created by Eugene Plotnikov (2001–2005) to answer one question: could an unmodified PS2 play a good-resolution DivX movie with sound, straight off a burned disc, with no extra hardware? To make that possible he needed three pieces — hence "system": **CDDAFS** (the audio-CD data trick so a burned disc would read), **CDXplorer** (the PC tool that builds the disc), and the **player** itself (a heavily PS2-optimised port of parts of FFmpeg, using the console's IPU for colour conversion). The community later added USB, HDD, network, a GUI browser, a skin engine and many more formats — and this fork adds MX4SIO, exFAT HDD, i.LINK, MMCE and on-screen network setup — but the optical-disc heart described on this page is where it all began. See [Credits & Sources](https://nathanneurotic.github.io/Simple-Media-System/credits.html).

</details>

---
[Searchable manual](https://nathanneurotic.github.io/Simple-Media-System/cd-dvd.html) · [Edit the maintained source](https://github.com/NathanNeurotic/Simple-Media-System/blob/master/docs-site/content/cd-dvd.html)
