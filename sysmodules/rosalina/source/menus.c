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

#include <3ds.h>
#include <3ds/os.h>
#include <3ds/services/hid.h>
#include "menus.h"
#include "menu.h"
#include "draw.h"
#include "menus/process_list.h"
#include "menus/n3ds.h"
#include "menus/debugger_menu.h"
#include "debugger.h"
#include "menus/miscellaneous.h"
#include "menus/sysconfig.h"
#include "luma_config.h"
#include "luma_shared_config.h"
#include "menus/config_extra.h"
#include "menus/screen_filters.h"
#include "luminance.h"
#include "plugin.h"
#include "ifile.h"
#include "memory.h"
#include "fmt.h"
#include "process_patches.h"
#include "luminance.h"
#include "pmdbgext.h"
#include "menus/quick_switchers.h"
#include "menus/chainloader.h"
#include "config_template_ini.h"
#include "configExtra_ini.h"

Menu rosalinaMenu = {
    "Rosalina menu",
    {
        { "Screenshot NFTs", METHOD, .method = &RosalinaMenu_TakeScreenshot },
        { "Change screen brightness", METHOD, .method = &RosalinaMenu_ChangeScreenBrightness },
        { "Cheater", METHOD, .method = &RosalinaMenu_Cheats },
        { "", METHOD, .method = PluginLoader__MenuCallback },
        { "Process hacker", METHOD, .method = &RosalinaMenu_ProcessList },
        { "Debugger options...", MENU, .menu = &debuggerMenu },
        { "System configuration...", MENU, .menu = &sysconfigMenu },
        { "Screen filters...", MENU, .menu = &screenFiltersMenu },
        { "New 3DS settings...", MENU, .menu = &N3DSMenu, .visibility = &menuCheckN3ds },
        { "Quick-Switchers...", MENU, .menu = &quickSwitchersMenu },
        { "Miscellaneous options...", MENU, .menu = &miscellaneousMenu },
        { "Save settings", METHOD, .method = &RosalinaMenu_SaveSettings },
        { "Go to Home", METHOD, .method = &RosalinaMenu_HomeMenu },
        { "Power & performance options", METHOD, .method = &RosalinaMenu_PowerPerformanceOptions },
        { "Credits", METHOD, .method = &RosalinaMenu_ShowCredits },
        { "Debug info", METHOD, .method = &RosalinaMenu_ShowDebugInfo, .visibility = &rosalinaMenuShouldShowDebugInfo },
        {},
    }
};
    
bool rosalinaMenuShouldShowDebugInfo(void)
{
    // Don't show on release builds

    s64 out;
    svcGetSystemInfo(&out, 0x10000, 0x200);
    return out == 0;
}

void RosalinaMenu_SaveSettings(void)
{
    Result res = LumaConfig_SaveSettings();
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Save settings");
        if (R_SUCCEEDED(res))
            Draw_DrawString(10, 30, COLOR_WHITE, "Operation succeeded.");
        else
            Draw_DrawFormattedString(10, 30, COLOR_WHITE, "Operation failed (0x%08lx).", res);
        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while (!(waitInput() & KEY_B) && !menuShouldExit);
}

void RosalinaMenu_ShowDebugInfo(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    char memoryMap[512];
    formatMemoryMapOfProcess(memoryMap, 511, CUR_PROCESS_HANDLE);

    s64 kextAddrSize;
    svcGetSystemInfo(&kextAddrSize, 0x10000, 0x300);
    u32 kextPa = (u32)((u64)kextAddrSize >> 32);
    u32 kextSize = (u32)kextAddrSize;

    u32 kernelVer = osGetKernelVersion();
    FS_SdMmcSpeedInfo speedInfo;

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Rosalina -- Debug info");

        u32 posY = Draw_DrawString(10, 30, COLOR_WHITE, memoryMap);
        posY = Draw_DrawFormattedString(10, posY, COLOR_WHITE, "Kernel ext PA: %08lx - %08lx\n\n", kextPa, kextPa + kextSize);
        posY = Draw_DrawFormattedString(
            10, posY, COLOR_WHITE, "Kernel version: %lu.%lu-%lu\n",
            GET_VERSION_MAJOR(kernelVer), GET_VERSION_MINOR(kernelVer), GET_VERSION_REVISION(kernelVer)
        );
        if (mcuFwVersion != 0)
        {
            posY = Draw_DrawFormattedString(
                10, posY, COLOR_WHITE, "MCU FW version: %lu.%lu\n",
                GET_VERSION_MAJOR(mcuFwVersion), GET_VERSION_MINOR(mcuFwVersion)
            );
        }

        if (R_SUCCEEDED(FSUSER_GetSdmcSpeedInfo(&speedInfo)))
        {
            u32 clkDiv = 1 << (1 + (speedInfo.sdClkCtrl & 0xFF));
            posY = Draw_DrawFormattedString(
                10, posY, COLOR_WHITE, "SDMC speed: HS=%d %lukHz\n",
                (int)speedInfo.highSpeedModeEnabled, SYSCLOCK_SDMMC / (1000 * clkDiv)
            );
        }
        if (R_SUCCEEDED(FSUSER_GetNandSpeedInfo(&speedInfo)))
        {
            u32 clkDiv = 1 << (1 + (speedInfo.sdClkCtrl & 0xFF));
            posY = Draw_DrawFormattedString(
                10, posY, COLOR_WHITE, "NAND speed: HS=%d %lukHz\n",
                (int)speedInfo.highSpeedModeEnabled, SYSCLOCK_SDMMC / (1000 * clkDiv)
            );
        }
        {
            posY = Draw_DrawFormattedString(
                10, posY, COLOR_WHITE, "APPMEMTYPE: %lu\n",
                OS_KernelConfig->app_memtype
            );
        }
        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while (!(waitInput() & KEY_B) && !menuShouldExit);
}

void RosalinaMenu_ShowCredits(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Rosalina -- Luma3DS credits");

        u32 posY = Draw_DrawString(10, 30, COLOR_WHITE, "Luma3DS (c) 2016-2024 AuroraWright, TuxSH") + SPACING_Y;

        posY = Draw_DrawString(10, posY + SPACING_Y, COLOR_WHITE, "3DSX loading code by fincs");
        posY = Draw_DrawString(10, posY + SPACING_Y, COLOR_WHITE, "Networking code & basic GDB functionality by Stary");
        posY = Draw_DrawString(10, posY + SPACING_Y, COLOR_WHITE, "InputRedirection by Stary (PoC by ShinyQuagsire)");

        posY += 2 * SPACING_Y;

        Draw_DrawString(10, posY, COLOR_WHITE,
            (
                " Special thanks to:\n"
                "  fincs, WinterMute, mtheall, piepie62,\n"
                "  Luma3DS contributors, libctru contributors,\n"
                "  and other people.\n\n"
                "  Credits for this fork:\n"
                "  DullPointer, Cooolgamer, PabloMK7, D0k3,\n"
                "  ByebyeSky, Sono, Nutez, Core2Extreme, Popax21,\n"
                "  Peach, Nikki, truedread, Aspargas2, Peppe,\n"
                "  Simo, Manuele, Defit and everyone in the\n"
                "  Homebrew Galaxy group and all the people\n"
                "  in godmode9 group for their guidance\n"
                "  and a lot of others people more"
            ));

        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while (!(waitInput() & KEY_B) && !menuShouldExit);
}

void RosalinaMenu_ChangeScreenBrightness(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    // gsp:LCD GetLuminance is stubbed on O3DS so we have to implement it ourselves... damn it.
    // Assume top and bottom screen luminances are the same (should be; if not, we'll set them to the same values).
    u32 luminanceTop = getCurrentLuminance(true);
    u32 luminanceBot = getCurrentLuminance(false);
    u32 minLum = getMinLuminancePreset();
    u32 maxLum = getMaxLuminancePreset();
    u32 trueMax = 172; // https://www.3dbrew.org/wiki/GSPLCD:SetBrightnessRaw
    u32 trueMin = 0;
    // hacky but N3DS coeffs for top screen don't seem to work and O3DS coeffs when using N3DS return 173 max brightness
    luminanceTop = luminanceTop == 173 ? trueMax : luminanceTop;

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Screen brightness");
        u32 posY = 30;
        posY = Draw_DrawFormattedString(
            10,
            posY,
            COLOR_WHITE,
            "Preset: %lu to %lu, Extended: 0 to 172.\n",
            minLum,
            maxLum
        );
         posY = Draw_DrawFormattedString(
            10,
            posY,
            luminanceTop > trueMax ? COLOR_RED : COLOR_WHITE,
            "Top screen luminance: %lu\n",
            luminanceTop
        );
        posY = Draw_DrawFormattedString(
            10,
            posY,
            luminanceBot > trueMax ? COLOR_RED : COLOR_WHITE,
            "Bottom screen luminance: %lu \n\n",
            luminanceBot
        );
        posY = Draw_DrawString(10, posY, COLOR_GREEN, "Controls:\n");
        posY = Draw_DrawString(10, posY, COLOR_WHITE, "Up/Down for +/-1, Right/Left for +/-10.\n");
        posY = Draw_DrawString(10, posY, COLOR_WHITE, "Hold X/A for Top/Bottom screen only. \n");
        posY = Draw_DrawFormattedString(10, posY, COLOR_WHITE, "Hold L/R for extended limits (<%lu may glitch). \n", minLum);
        if (hasTopScreen) { posY = Draw_DrawString(10, posY, COLOR_WHITE, "Press Y to toggle screen backlights.\n\n"); }
        
        posY = Draw_DrawString(10, posY, COLOR_WHITE, "Press START to begin, B to exit.\n\n");

        posY = Draw_DrawString(10, posY, COLOR_RED, "WARNING: \n");
        posY = Draw_DrawString(10, posY, COLOR_WHITE, "  * values rarely glitch >172, do not use these!\n");
        posY = Draw_DrawString(10, posY, COLOR_WHITE, "  * all changes revert on shell reopening.\n");
        posY = Draw_DrawString(10, posY, COLOR_WHITE, "  * bottom framebuffer will be visible until exit.\n");
        posY = Draw_DrawString(10, posY, COLOR_WHITE, "  * bottom screen functions as normal with\nbacklight turned off.\n");
        Draw_FlushFramebuffer();
        Draw_Unlock();

        u32 pressed = waitInputWithTimeout(1000);

        if (pressed & KEY_START)
            break;

        if (pressed & KEY_B)
            return;
    }
    while (!menuShouldExit);

    Draw_Lock();

    Draw_RestoreFramebuffer();
    Draw_FreeFramebufferCache();

    svcKernelSetState(0x10000, 2);  // unblock gsp
    gspLcdInit();                   // assume it doesn't fail. If it does, brightness won't change, anyway.

    // gsp:LCD will normalize the brightness between top/bottom screen, handle PWM, etc.

    s32 lumTop = (s32)luminanceTop;
    s32 lumBot = (s32)luminanceBot;

    do
    {
        u32 kHeld = 0;
        kHeld = HID_PAD;
        u32 pressed = waitInputWithTimeout(1000);
        if (pressed & DIRECTIONAL_KEYS)
        {
            if (kHeld & KEY_X)
            {
                if (pressed & KEY_UP)
                    lumTop += 1;
                else if (pressed & KEY_DOWN)
                    lumTop -= 1;
                else if (pressed & KEY_RIGHT)
                    lumTop += 10;
                else if (pressed & KEY_LEFT)
                    lumTop -= 10;
            }
            else if (kHeld & KEY_A)
            {
                if (pressed & KEY_UP)
                    lumBot += 1;
                else if (pressed & KEY_DOWN)
                    lumBot -= 1;
                else if (pressed & KEY_RIGHT)
                    lumBot += 10;
                else if (pressed & KEY_LEFT)
                    lumBot -= 10;
                    
            }
            else 
            {
                if (pressed & KEY_UP)
                {
                    lumTop += 1;
                    lumBot += 1;
                }
                else if (pressed & KEY_DOWN)
                {
                    lumTop -= 1;
                    lumBot -= 1;
                }
                else if (pressed & KEY_RIGHT)
                {
                    lumTop += 10;
                    lumBot += 10;
                }
                else if (pressed & KEY_LEFT)
                {
                    lumTop -= 10;
                    lumBot -= 10;
                }
            }

            if (kHeld & (KEY_L | KEY_R))
            {
                lumTop = lumTop > (s32)trueMax ? (s32)trueMax : lumTop;
                lumBot = lumBot > (s32)trueMax ? (s32)trueMax : lumBot;
                lumTop = lumTop < (s32)trueMin ? (s32)trueMin : lumTop;
                lumBot = lumBot < (s32)trueMin ? (s32)trueMin : lumBot;
            }
            else
            {
                lumTop = lumTop > (s32)maxLum ? (s32)maxLum : lumTop;
                lumBot = lumBot > (s32)maxLum ? (s32)maxLum : lumBot;
                lumTop = lumTop < (s32)minLum ? (s32)minLum : lumTop;
                lumBot = lumBot < (s32)minLum ? (s32)minLum : lumBot;
            }

            if (lumTop >= (s32)minLum && lumBot >= (s32)minLum) {
                GSPLCD_SetBrightnessRaw(BIT(GSP_SCREEN_TOP), lumTop);
                GSPLCD_SetBrightnessRaw(BIT(GSP_SCREEN_BOTTOM), lumBot);
            }       
            else {
                setBrightnessAlt(lumTop, lumBot);
            }
        }
        
       if ((pressed & KEY_Y) && hasTopScreen)
        {   
            u8 result, botStatus, topStatus;
            mcuHwcInit();
            MCUHWC_ReadRegister(0x0F, &result, 1);  // https://www.3dbrew.org/wiki/I2C_Registers#Device_3
            mcuHwcExit();  
            botStatus = (result >> 5) & 1;  // right shift result to bit 5 ("Bottom screen backlight on") and perform bitwise AND with 1
            topStatus = (result >> 6) & 1;  // bit06: Top screen backlight on

            if (botStatus == 1 && topStatus == 1)
            {
                GSPLCD_PowerOffBacklight(BIT(GSP_SCREEN_BOTTOM));
            }
            else if (botStatus == 0 && topStatus == 1)
            {
                GSPLCD_PowerOnBacklight(BIT(GSP_SCREEN_BOTTOM));
                GSPLCD_PowerOffBacklight(BIT(GSP_SCREEN_TOP));
            }
            else if (topStatus == 0)
            {
                GSPLCD_PowerOnBacklight(BIT(GSP_SCREEN_TOP));
            }
        }

        if (pressed & KEY_B)
            break;
    }
    while (!menuShouldExit);

    gspLcdExit();
    svcKernelSetState(0x10000, 2); // block gsp again

    if (R_FAILED(Draw_AllocateFramebufferCache(FB_BOTTOM_SIZE)))
    {
        // Shouldn't happen
        __builtin_trap();
    }
    else
        Draw_SetupFramebuffer();

    Draw_Unlock();
}

void RosalinaMenu_PowerPerformanceOptions(void) 
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Power & performance options");
        Draw_DrawString(10, 30, COLOR_WHITE, "Press X to power off, press A to reboot.");
        Draw_DrawString(10, 40, COLOR_RED, "Press Y to force reboot");
        Draw_DrawString(10, 50, COLOR_WHITE, "Press Up: boot Homebrew with max app memory*.");
        Draw_DrawString(10, 60, COLOR_RED, "*pressing home or power button will cause crash!");
        if (isN3DS) {
            Draw_DrawString(10, 70, COLOR_WHITE, "Press Left: reboot with core2 redirect.");
            Draw_DrawString(10, 80, COLOR_WHITE, "Press Right: boot Homebrew with max mem* & core2.");
        }
        Draw_DrawString(10, 90, COLOR_WHITE, "Press Down: clear performance settings and reboot.");
        Draw_DrawString(10, 110, COLOR_WHITE, "Press B to go back.");
        Draw_FlushFramebuffer();
        Draw_Unlock();

        u32 pressed = waitInputWithTimeout(1000);

        if (pressed & KEY_X)  // Soft shutdown.
        {
            menuLeave();
            srvPublishToSubscriber(0x203, 0);
            return;
        }
        else if (pressed & KEY_A)
        {
            menuLeave();
            APT_HardwareResetAsync();
            return;
        }
        else if (pressed & KEY_Y)
        {
            svcKernelSetState(7);
            __builtin_unreachable();
            return;
        }
        else if(pressed & KEY_B)
        {
           return;
        }
        else if (pressed & DIRECTIONAL_KEYS)
        {
            if (pressed & KEY_UP)
            {
                menuLeave();
                LumaConfig_SavePerformanceSettings(true, true, false);
                svcKernelSetState(7);
                return;
            }
            else if ((pressed & KEY_LEFT) && isN3DS)
            {    
                menuLeave();            
                LumaConfig_SavePerformanceSettings(false, false, true);
                svcKernelSetState(7);
                return;
            }
            else if ((pressed & KEY_RIGHT) && isN3DS)
            {
                menuLeave();
                LumaConfig_SavePerformanceSettings(true, true, true);
                svcKernelSetState(7);
                return;
            }
            else if (pressed & KEY_DOWN)
            {
                menuLeave();
                LumaConfig_SavePerformanceSettings(false, false, false);
                svcKernelSetState(7);
                return;
            }
        }
    }
    while (!menuShouldExit);
}

void RosalinaMenu_HomeMenu(void)  // Trigger Home Button press
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    do
    {
        srvPublishToSubscriber(0x204, 0);

        Draw_Lock();
        Draw_ClearFramebuffer();
        Draw_DrawString(10, 30, COLOR_WHITE, "Exit Rosalina to get back to the Home Menu.");
        Draw_DrawString(10, 40, COLOR_WHITE, "Press A to confirm");
        Draw_FlushFramebuffer();
        Draw_Unlock();

        u32 pressed = waitInputWithTimeout(1000);

        if (pressed & KEY_A)
        {
          return;
        }
        else if (pressed & KEY_B)
            return;
    }
    while (!menuShouldExit);
}

#define TRY(expr) if(R_FAILED(res = (expr))) goto end;

static s64 timeSpentConvertingScreenshot = 0;
static s64 timeSpentWritingScreenshot = 0;

static Result RosalinaMenu_WriteScreenshot(IFile *file, u32 width, bool top, bool left)
{
    u64 total;
    Result res = 0;
    u32 lineSize = 3 * width;

    // When dealing with 800px mode (800x240 with half-width pixels), duplicate each line
    // to restore aspect ratio and obtain faithful 800x480 screenshots
    u32 scaleFactorY = width > 400 ? 2 : 1;
    u32 numLinesScaled = 240 * scaleFactorY;
    u32 remaining = lineSize * numLinesScaled;

    TRY(Draw_AllocateFramebufferCacheForScreenshot(remaining));

    u8 *framebufferCache = (u8 *)Draw_GetFramebufferCache();
    u8 *framebufferCacheEnd = framebufferCache + Draw_GetFramebufferCacheSize();

    u8 *buf = framebufferCache;
    Draw_CreateBitmapHeader(framebufferCache, width, numLinesScaled);
    buf += 54;

    u32 y = 0;    
    // Our buffer might be smaller than the size of the screenshot...
    while (remaining != 0)
    {
        s64 t0 = svcGetSystemTick();
        u32 available = (u32)(framebufferCacheEnd - buf);
        u32 size = available < remaining ? available : remaining;
        u32 nlines = size / (lineSize * scaleFactorY);
        Draw_ConvertFrameBufferLines(buf, width, y, nlines, scaleFactorY, top, left);

        s64 t1 = svcGetSystemTick();
        timeSpentConvertingScreenshot += t1 - t0;
        TRY(IFile_Write(file, &total, framebufferCache, (y == 0 ? 54 : 0) + lineSize * nlines * scaleFactorY, 0)); // don't forget to write the header
        timeSpentWritingScreenshot += svcGetSystemTick() - t1;

        y += nlines;
        remaining -= lineSize * nlines * scaleFactorY;
        buf = framebufferCache;
    }
    end:

    Draw_FreeFramebufferCache();
    return res;
}

void RosalinaMenu_TakeScreenshot(void)
{
    IFile file;
    Result res = 0;

    char filename[64];
    char dateTimeStr[32];

    FS_Archive archive;
    FS_ArchiveID archiveId;
    s64 out;
    bool isSdMode;

    timeSpentConvertingScreenshot = 0;
    timeSpentWritingScreenshot = 0;

    if (R_FAILED(svcGetSystemInfo(&out, 0x10000, 0x203)))
        svcBreak(USERBREAK_ASSERT);
    isSdMode = (bool)out;

    archiveId = isSdMode ? ARCHIVE_SDMC : ARCHIVE_NAND_RW;
    Draw_Lock();
    Draw_RestoreFramebuffer();
    Draw_FreeFramebufferCache();

    svcFlushEntireDataCache();

    bool is3d;
    u32 topWidth, bottomWidth;  // actually Y-dim

    Draw_GetCurrentScreenInfo(&bottomWidth, &is3d, false);
    Draw_GetCurrentScreenInfo(&topWidth, &is3d, true);

    res = FSUSER_OpenArchive(&archive, archiveId, fsMakePath(PATH_EMPTY, ""));
    if (R_SUCCEEDED(res))
    {
        res = FSUSER_CreateDirectory(archive, fsMakePath(PATH_ASCII, "/luma/screenshots"), 0);
        if ((u32)res == 0xC82044BE)  // directory already exists
            res = 0;
        FSUSER_CloseArchive(archive);
    }

    dateTimeToString(dateTimeStr, osGetTime(), true);

    sprintf(filename, "/luma/screenshots/%s_top.bmp", dateTimeStr);
    TRY(IFile_Open(&file, archiveId, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, filename), FS_OPEN_CREATE | FS_OPEN_WRITE));
    TRY(RosalinaMenu_WriteScreenshot(&file, topWidth, true, true));
    TRY(IFile_Close(&file));

    sprintf(filename, "/luma/screenshots/%s_bot.bmp", dateTimeStr);
    TRY(IFile_Open(&file, archiveId, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, filename), FS_OPEN_CREATE | FS_OPEN_WRITE));
    TRY(RosalinaMenu_WriteScreenshot(&file, bottomWidth, false, true));
    TRY(IFile_Close(&file));

    if (is3d && (Draw_GetCurrentFramebufferAddress(true, true) != Draw_GetCurrentFramebufferAddress(true, false)))
    {
        sprintf(filename, "/luma/screenshots/%s_top_right.bmp", dateTimeStr);
        TRY(IFile_Open(&file, archiveId, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, filename), FS_OPEN_CREATE | FS_OPEN_WRITE));
        TRY(RosalinaMenu_WriteScreenshot(&file, topWidth, true, false));
        TRY(IFile_Close(&file));
    }

end:
    IFile_Close(&file);

    if (R_FAILED(Draw_AllocateFramebufferCache(FB_BOTTOM_SIZE)))
        __builtin_trap();  // We're f***ed if this happens

    svcFlushEntireDataCache();
    Draw_SetupFramebuffer();
    Draw_Unlock();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Screenshot");
        if (R_FAILED(res))
            Draw_DrawFormattedString(10, 30, COLOR_WHITE, "Operation failed (0x%08lx).", (u32)res);
        else
        {
            u32 t1 = (u32)(1000 * timeSpentConvertingScreenshot / SYSCLOCK_ARM11);
            u32 t2 = (u32)(1000 * timeSpentWritingScreenshot / SYSCLOCK_ARM11);
            u32 posY = 30;
            posY = Draw_DrawString(10, posY, COLOR_WHITE, "Operation succeeded.\n\n");
            posY = Draw_DrawFormattedString(10, posY, COLOR_WHITE, "Time spent converting:    %5lums\n", t1);
            posY = Draw_DrawFormattedString(10, posY, COLOR_WHITE, "Time spent writing files: %5lums\n", t2);
        }

        Draw_FlushFramebuffer();
        Draw_Unlock();
    }
    while (!(waitInput() & KEY_B) && !menuShouldExit);

#undef TRY
}
