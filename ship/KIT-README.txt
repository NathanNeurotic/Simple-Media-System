Simple-Media-System-kit.zip - the full install kit
==================================================

The same layout as the Launchstation and RiptOPL kits: copy everything to the
root of a USB drive, MX4SIO, MMCE or exFAT HDD and SMS shows up in app lists
that read APPS/ (OPL, OSDMenu and others). The separate APP_SMS3.zip and
APP_SMS3.psu downloads hold the same app folder.

  APPS/APP_SMS3/      SMS3.ELF (the packed build, the same program as
                      Simple-Media-System-packed.elf), title.cfg, and the
                      icon.sys and three icons made by Berion.
  APP_SMS3.psu        The same six files as a memory-card save. Install it with
                      "Extract PSU" (PSU paste) in a memory-card folder from
                      wLaunchELF or Launchstation; it becomes mc?:/APP_SMS3/.
  LANGUAGES/          Translations for this exact build. Copy your language's
                      SMS.lng_XX next to SMS3.ELF and rename it SMS.lng (or put
                      it on the memory card as mc0:/SMS/SMS.lng). Packs from
                      another release are refused, so use these.
  LICENSES/smbman/    Licence notices for the SMB network module built into
                      SMS3.ELF.

Settings: SMS keeps SMS.cfg (and SMS.smb) in the folder SMS3.ELF starts from
(started from an SMB share, it uses an attached USB or MMCE device, or the
memory card). To keep them somewhere else, put a one-line text file
SMSPATH.TXT next to SMS3.ELF that names the folder, for example

    mc0:/SMS3/

"mc:" means the memory card SMS started from. SMS creates the folder on the
first save (one level only) and, on a memory card, adds an icon to it. Fonts,
skins, the language file and IPCONFIG.DAT stay where they are. A hard-drive
(hdd0:) folder is not supported for SMSPATH.TXT.

Documentation: https://nathanneurotic.github.io/Simple-Media-System/
