# CustomLuma3DS
*Noob-proof (N)3DS "Custom Firmware"*
discord group

https://discord.gg/3dq8d2nb

## next update 
next build probably will better fix volume software control.

the release build are not synced with the latest commit so if you want to see the latest changes you know how to do it

## What is this fork
for debug build please remember to change also the sysmodule rsf to true forcedebug and false disable debug.

Many of you probably know the NTR CFW that enabled streaming the 3DS' screen to the computer over the debugger.  
This fork implements this functionality into rosalina.  
It's still higly WIP and not intended for public use since framerates are quite slow.  
But it's more stable than the NTR firmware. You can switch games and the games I tested worked flawlessly.  
Some of the code is inspired by NTR but most of it is a complete reimplementation, because I wanted to figure out where the annoying bugs with the NTR firmware are. (e.g. 3DS hanging when switching processes)

### What's in here also  different from official luma3DS?

    Restored UNITINFO and enable rosalina on safe_firm and disable arm11exceptions  and alos cut down wifi options on the luma config menu
    Added shortcuts:
        Press start + select to toggle bottom screen (nice when you watch videos) inspired by This, limit on o2ds
        , cause hardware register for up and down lcd are the same looking for new test though... 
        Press A + B + X + Y + Start to instantly reboot the console. Useful in case of freeze, but don't complain if your sdcard get corrupted because of this. also this needs to be activated 
        Press Start on Rosalina menu to toggle wifi -> From here
        Press Select on Rosalina menu to toggle LEDs -> From this (and press Y to force blue led as a workaround when the battery is low)
    Added n3ds clock + L2 status in rosalina menu -> From here
    Added Software Volume Control -> From here
    Added extended brightness presets -> From this
    separate brightness for both screen
    you can open the rosalina menu with home button remember to disable it if you don't use it(extra config), for who has some button broken
    rosalina emulate also home button you can come the home menu without pressing it...
    Added permanent brightness calibration by Nutez -> From here
    Changed colors on config menu because why not
    Continue running after a errdisp error happens (you can press the instant reboot combo to reboot if nothing works needs to be activated in adavnced menu config.
    and new3ds title configurator so you can chose which game
    extra config menu also changeable from godmode9 if you need to deactivate a feature.
- also abilty to set lareyedfs path for game patching using a txt file in luma/titles/titleid directory of the sd card with specied path.

- also added single screen backloght regulation

- changed rosalina combo defsult is L+Up (Dpad), you can change it after in the config menu.(less buttons to be pressed, useful for broken buttons)

- also fixed up previuos bug, now causing another one...
standby led turning off needs some fix, i need to find some documentation abput sleep mode...

- added logo.bin abilities to replace default 3ds app launch logo try it if you want.

- rehid folder disable option 

- added debug capabilities like Seledreams fork, thanks to you.

- Also added streaming in it juts cause why not, thanks to Byebyesky.

- autoboot in dsi mode will boot directly twilightmenu++

## Usage of the streaming capability

1. Go to the rosalina menu->streaming->start strea
2. Type the IP of your 3DS into the python script
3. Install the dependencies with `pip3 install pygame pillow numpy`
4. Start the script with `python3 streaming.py 4`
- should even work with ntrview for wiiu:

https://github.com/yawut/ntrview-wiiu

or snickerstream on pc:

https://github.com/RattletraPM/Snickerstream/releases

(Click the image for a video demo)
[![Click for video demo](preview.png)](https://youtu.be/SAhSV_xUGCc)

---

### What it is
**Luma3DS** is a program to patch the system software of (New) Nintendo (2)3DS handheld consoles "on the fly", adding features such as per-game language settings, debugging capabilities for developers, and removing restrictions enforced by Nintendo such as the region lock.

It also allows you to run unauthorized ("homebrew") content by removing signature checks.
To use it, you will need a console capable of running homebrew software on the Arm9 processor.

Since v8.0, Luma3DS has its own in-game menu, triggerable by <kbd>L+Up</kbd> (see the [release notes](https://github.com/LumaTeam/Luma3DS/releases/tag/v8.0)).

#
### Compiling
* Prerequisites
    1. git
    2. [makerom](https://github.com/jakcron/Project_CTR) in PATH
    3. [firmtool](https://github.com/TuxSH/firmtool)
    4. Up-to-date devkitARM+libctru
1. Clone the repository with `git clone https://github.com/LumaTeam/Luma3DS.git`
2. Run `make`.

    The produced `boot.firm` is meant to be copied to the root of your SD card for usage with Boot9Strap/fastboot3ds/godmode9.

#
### Setup / Usage / Features
See https://github.com/LumaTeam/Luma3DS/wiki

#
### Credits
See https://github.com/LumaTeam/Luma3DS/wiki/Credits
also added inside the cfw itself (thinking i have to add someone else, though don't remember the name 😅)

#
### Licensing
This software is licensed under the terms of the GPLv3. You can find a copy of the license in the LICENSE.txt file.

Files in the GDB stub are instead triple-licensed as MIT or "GPLv2 or any later version", in which case it's specified in the file header.
