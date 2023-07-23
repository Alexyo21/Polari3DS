# Luma3DS
*Noob-proof (N)3DS "Custom Firmware"*

### What's different from official luma3DS?
- It's based from [DullPointer's custom luma3DS](https://github.com/DullPointer/Luma3DS) so it contains all of their added features such as night mode,
- Changed some text and colors cuz why not,
- Instant reboot: Press A + B + X + Y + Start to instantly reboot the console,
- Toggle bottom screen anywhere: inspired by thirdtube, press Start + Select to toggle bottom screen,
- Power off and reboot are now in a Power options menu (don't know why that wasn't already the case),
- Workaround for toggle led in low battery, press Y in rosalina menu to make it blue so it can be turned off,
- Errdisp doesn't reboot the console, so you can experiment playing games and remove cart/sd card for fun,
- Oh yeah brightness is broken, don't go way down while changing brightness in rosalina menu or it'll set it very high (won't break the screen don't worry)
- I made it for fun, it's not meant to be used for everyone, it's just that I use it for me, and made it open source so other people can inspire of this and make a better custom luma, if you plan to use it as your main cfw, well do, but keep in mind it's based on a older version of luma3DS.

### What it is
**Luma3DS** is a program to patch the system software of (New) Nintendo (2)3DS handheld consoles "on the fly", adding features such as per-game language settings, debugging capabilities for developers, and removing restrictions enforced by Nintendo such as the region lock.

It also allows you to run unauthorized ("homebrew") content by removing signature checks.
To use it, you will need a console capable of running homebrew software on the Arm9 processor.

Since v8.0, Luma3DS has its own in-game menu, triggerable by <kbd>L+Down+Select</kbd> (see the [release notes](https://github.com/LumaTeam/Luma3DS/releases/tag/v8.0)).

#
### Compiling
* Prerequisites
    1. git
    2. [makerom](https://github.com/jakcron/Project_CTR) in PATH
    3. [firmtool](https://github.com/TuxSH/firmtool)
    4. Up-to-date devkitARM+libctru
1. Clone the repository with `git clone https://github.com/LumaTeam/Luma3DS.git`
2. Run `make`.

    The produced `boot.firm` is meant to be copied to the root of your SD card for usage with Boot9Strap.

#
### Setup / Usage / Features
See https://github.com/LumaTeam/Luma3DS/wiki

#
### Credits
See https://github.com/LumaTeam/Luma3DS/wiki/Credits

#
### Licensing
This software is licensed under the terms of the GPLv3. You can find a copy of the license in the LICENSE.txt file.

Files in the GDB stub are instead triple-licensed as MIT or "GPLv2 or any later version", in which case it's specified in the file header.
