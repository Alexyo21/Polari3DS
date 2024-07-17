/*
*   This file is part of Luma3DS
*   Copyright (C) 2023 Aurora Wright, TuxSH
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

#ifndef __LUMACONFIG_H__
#define __LUMACONFIG_H__

#include <3ds/types.h>
#include "luma_shared_config.h"
#include "utils.h"
#include "volume.h"

#define CONFIG(a)        (((cfg->config >> (a)) & 1) != 0)
#define MULTICONFIG(a)   ((cfg->multiConfig >> (2 * (a))) & 3)
#define BOOTCONFIG(a, b) ((cfg->bootConfig >> (a)) & (b))

enum singleOptions
{
    AUTOBOOTEMU = 0,
    LOADEXTFIRMSANDMODULES,
    PATCHGAMES,
    REDIRECTAPPTHREADS,
    PATCHVERSTRING,
    SHOWGBABOOT,
    PERFORMANCEMODE,
    ALLOWUPDOWNLEFTRIGHTDSI,
    CUTWIFISLEEP,
    PATCHUNITINFO,
    DISABLEARM11EXCHANDLERS,
    ENABLESAFEFIRMROSALINA,
    NOERRDISPINSTANTREBOOT,
    SHOWADVANCEDSETTINGS,
    HARDWAREPATCHING,
};

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

void LumaConfig_ConvertComboToString(char *out, u32 combo);
Result LumaConfig_SaveSettings(void);
void LumaConfig_RequestSaveSettings(void);
Result LumaConfig_SavePerformanceSettings(bool homeBrew, bool maxAppMem, bool core2);

#endif
