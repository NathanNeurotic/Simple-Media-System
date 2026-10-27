# Languages & Fonts

Install a language pack from the same build as your player. The current table has 311 strings; a pack from an older release can leave the Language row ineffective.

## Install an external language

1. Download the [matching language ZIP](https://nathanneurotic.github.io/Simple-Media-System/releases.html#whats-in-a-release). It contains 19 named variants (including an English variant), the base English template and translator guide.
2. Choose a variant such as `SMS.lng_ES` and rename it `SMS.lng`. Copy it beside the ELF, or to `mc?:/SMS/SMS.lng`. A boot-folder copy has priority.
3. Alternatively highlight a language file in the browser and use its import/update action. Enable the external language with **Browser settings → Language**; choosing English uses the built-in strings.
4. Select the matching **Character set**, save settings and check the UI after relaunch.

Spanish was updated with Nacheras's submitted strings in the October work. Other variants can retain English placeholders for newer entries. Integrated strings and host checks do not establish that every glyph or long label fits on a console.

## Encoding matters

Menu language files use the player's single-byte code pages, not general UTF-8 text rendering. Use the relevant WinLatin1, WinLatin2, Cyrillic or Greek setting. An external `.mbf` font is a separate mechanism, particularly for multibyte subtitles.

Subtitle encoding is different: the subtitle reader recognizes UTF-8 with a BOM and has its own MBCS/font selection. Do not apply menu-language encoding rules to every SRT. See [Subtitles](https://nathanneurotic.github.io/Simple-Media-System/subtitles.html).

## Translate safely

- Start with the [current English template](https://github.com/NathanNeurotic/Simple-Media-System/blob/master/docs/translation/SMS.lng). Keep all 311 entries in order and do not leave blank entries.
- Preserve every printf placeholder, including its type, order, width and precision. The loader does not validate all formatting mistakes for you.
- Keep zero-based entries 166 and 167 as boot paths. Device/protocol names such as MX4SIO, MMCE and UDPFS should remain recognizable.
- Keep labels short enough for the fixed menu boxes. Check real glyph rendering and clipping after import.

The full [translator reference](https://github.com/NathanNeurotic/Simple-Media-System/blob/master/docs/translation/TRANSLATING.txt) explains the fixed positions. If the language does not activate, check file location, pack/build pairing, blank lines and character set before assuming the toggle itself is broken.

## Report a language issue

Include the build manifest, variant, active file path, character set, affected menu and a photograph of clipping or incorrect glyphs. If sharing an edited pack, retain its original byte encoding.

---
[Searchable manual](https://nathanneurotic.github.io/Simple-Media-System/languages.html) · [Edit the maintained source](https://github.com/NathanNeurotic/Simple-Media-System/blob/master/docs-site/content/languages.html)
