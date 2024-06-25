/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2020 Aurora Wright, TuxSH
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*   Additional Terms 7.b and 7.c of GPLv3 apply to this file:
*       * Requiring preservation of specified reasonable legal notices or
*         author attributions in that material or in the Appropriate Legal
*         Notices displayed by works containing it.
*       * Prohibiting misrepresentation of the origin of that material,
*         or requiring that modified versions of such material be marked in
*         reasonable ways as different from the original version.
*/

#pragma once

#include "types.h"
#include "firm.h"
#include "config_template_ini.h"
#include "configExtra_ini.h"

#define AUTOBOOT_DEFAULT_TWL_TID    0x0003000453524C41ull

#define CONFIG(a)        (((configData.config >> (a)) & 1) != 0)
#define MULTICONFIG(a)   ((configData.multiConfig >> (2 * (a))) & 3)
#define BOOTCONFIG(a, b) ((configData.bootConfig >> (a)) & (b))

#define CONFIG_FILE         "lumae.ini"
#define CONFIG_VERSIONMAJOR 5
#define CONFIG_VERSIONMINOR 2

#define BOOTCFG_NAND         BOOTCONFIG(0, 1)
#define BOOTCFG_EMUINDEX     BOOTCONFIG(1, 3)
#define BOOTCFG_NOFORCEFLAG  BOOTCONFIG(3, 1)
#define BOOTCFG_NTRCARDBOOT  BOOTCONFIG(4, 1)

#define FVERSION_MAJOR 13 // fake version spoofing
#define FVERSION_MINOR 1
#define FVERSION_BUILD 2

enum multiOptions
{
    DEFAULTEMU = 0,
    BRIGHTNESS,
    SPLASH,
    PIN,
    NEWCPU,
    AUTOBOOTMODE,
    FORCEAUDIOOUTPUT,
};

enum singleOptions
{
    AUTOBOOTEMU = 0,
    LOADEXTFIRMSANDMODULES,
    PATCHGAMES,
    REDIRECTAPPTHREADS,
    PATCHVERSTRING,
    SHOWGBABOOT,
    ENABLEDSIEXTFILTER,
    ALLOWUPDOWNLEFTRIGHTDSI,
    CUTWIFISLEEP,
    PATCHUNITINFO,
    DISABLEARM11EXCHANDLERS,
    ENABLESAFEFIRMROSALINA,
    NOERRDISPINSTANTREBOOT,
    SHOWADVANCEDSETTINGS,
    HARDWAREPATCHING,
};

typedef enum ConfigurationStatus
{
    DONT_CONFIGURE = 0,
    MODIFY_CONFIGURATION,
    CREATE_CONFIGURATION
} ConfigurationStatus;

extern CfgData configData;

bool readConfig(void);
void writeConfig(bool isConfigOptions);
void configMenu(bool oldPinStatus, u32 oldPinMode);
