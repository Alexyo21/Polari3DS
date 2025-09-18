/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2021 Aurora Wright, TuxSH
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
#include "menus/sysconfig.h"
#include "luma_config.h"
#include "luma_shared_config.h"
#include "menus/miscellaneous.h"
#include "menus/config_extra.h"
#include "memory.h"
#include "draw.h"
#include "fmt.h"
#include "utils.h"
#include "ifile.h"
#include "menu.h"
#include "menus.h"
#include "volume.h"
#include "luminance.h"
#include "menus/screen_filters.h"
#include "config_template_ini.h"
#include "configExtra_ini.h"
#include "luminance.h"

Menu sysconfigMenu = {
    "Menu configurazione di sistema",
    {
        { "Controlla la connessione wireless", METHOD, .method = &SysConfigMenu_ControlWifi },
        { "Attiva/Disattiva LED", METHOD, .method = &SysConfigMenu_ToggleLEDs },
        { "Attiva/Disattiva Wireless", METHOD, .method = &SysConfigMenu_ToggleWireless },
        { "Attiva/Disattiva la cartella rehid: ", METHOD, .method = &SysConfigMenu_ToggleRehidFolder },
        { "Attiva/Disattiva il tasto Power", METHOD, .method=&SysConfigMenu_TogglePowerButton },
        { "Attiva/Disattiva power allo slot delle schede", METHOD, .method=&SysConfigMenu_ToggleCardIfPower},
        { "Ricalibrazione luminosita' schermo permanente", METHOD, .method = &Luminance_RecalibrateBrightnessDefaults },
        { "Controllo volume software", METHOD, .method = &AdjustVolume },
        { "Config. extra...", MENU, .menu = &configExtraMenu },
        { "Consigli", METHOD, .method = &SysConfigMenu_Tip },
        {},
    }
};

bool isConnectionForced = false;
static char rehidPath[16] = "/rehid";
static char rehidOffPath[16] = "/rehid_off";

void SysConfigMenu_ToggleLEDs(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Menu configurazione di sistema");
        Draw_DrawString(10, 30, COLOR_WHITE, "Press A per attivare/disattivare, premi B per tornare indietro.");
        Draw_DrawString(10, 50, COLOR_RED, "ATTENZIONE:");
        Draw_DrawString(10, 60, COLOR_WHITE, "  * Entrare in modalita' riposo resettera' lo stato dei LED!");
        Draw_DrawString(10, 70, COLOR_WHITE, "  * I LED non possono essere attivati quando la batteria e' bassa!");
        Draw_FlushFramebuffer();
        Draw_Unlock();

        u32 pressed = waitInputWithTimeout(1000);

        if(pressed & KEY_A)
        {
            menuToggleLeds();
        }
        else if(pressed & KEY_B)
            return;
    }
    while(!menuShouldExit);
}

void SysConfigMenu_ToggleWireless(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    bool nwmRunning = isServiceUsable("nwm::EXT");

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Menu configurazione di sistema");
        Draw_DrawString(10, 30, COLOR_WHITE, "Premi A per Attivare/Disattivare, premi B per tornare indietro.");

        u8 wireless = (*(vu8 *)((0x10140000 | (1u << 31)) + 0x180));

        if(nwmRunning)
        {
            Draw_DrawString(10, 50, COLOR_WHITE, "Stato attuale:");
            Draw_DrawString(100, 50, (wireless ? COLOR_GREEN : COLOR_RED), (wireless ? " ACCESO " : " SPENTO "));
        }
        else
        {
            Draw_DrawString(10, 50, COLOR_RED, "NWM non e' in esecuzione.");
            Draw_DrawString(10, 60, COLOR_RED, "Se sei attualmente nel Menu di Test,");
            Draw_DrawString(10, 70, COLOR_RED, "esci e premi R+DESTRA per attivare/disattivare il wifi.");
            Draw_DrawString(10, 80, COLOR_RED, "Altrimenti, esci e aspetta qualche secondo.");
        }

        Draw_FlushFramebuffer();
        Draw_Unlock();

        u32 pressed = waitInputWithTimeout(1000);

        if(pressed & KEY_A && nwmRunning)
        {
            nwmExtInit();
            NWMEXT_ControlWirelessEnabled(!wireless);
            nwmExtExit();
        }
        else if(pressed & KEY_B)
            return;
    }
    while(!menuShouldExit);
}

void SysConfigMenu_UpdateStatus(bool control)
{
    MenuItem *item = &sysconfigMenu.items[1];

    if(control)
    {
        item->title = "Controlla la connessione wireless";
        item->method = &SysConfigMenu_ControlWifi;
    }
    else
    {
        item->title = "Disabilita la connessione wireless forzata";
        item->method = &SysConfigMenu_DisableForcedWifiConnection;
    }
}

static bool SysConfigMenu_ForceWifiConnection(u32 slot)
{
    char ssid[0x20 + 1] = {0};
    isConnectionForced = false;

    if(R_FAILED(acInit()))
        return false;

    acuConfig config = {0};
    ACU_CreateDefaultConfig(&config);
    ACU_SetNetworkArea(&config, 2);
    ACU_SetAllowApType(&config, 1 << slot);
    ACU_SetRequestEulaVersion(&config);

    Handle connectEvent = 0;
    svcCreateEvent(&connectEvent, RESET_ONESHOT);

    bool forcedConnection = false;
    if(R_SUCCEEDED(ACU_ConnectAsync(&config, connectEvent)))
    {
        if(R_SUCCEEDED(svcWaitSynchronization(connectEvent, -1)) && R_SUCCEEDED(ACU_GetSSID(ssid)))
            forcedConnection = true;
    }
    svcCloseHandle(connectEvent);

    if(forcedConnection)
    {
        isConnectionForced = true;
        SysConfigMenu_UpdateStatus(false);
    }
    else
        acExit();

    char infoString[80] = {0};
    u32 infoStringColor = forcedConnection ? COLOR_GREEN : COLOR_RED;
    if(forcedConnection)
        sprintf(infoString, "Connessione forzata con successo a: %s", ssid);
    else
       sprintf(infoString, "Fallito il tent. di connes. allo slot: %d", (int)slot + 1);
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Menu configurazione di sistema");
        Draw_DrawString(10, 30, infoStringColor, infoString);
        Draw_DrawString(10, 40, COLOR_WHITE, "Premi B per tornare indietro.");
        Draw_FlushFramebuffer();
        Draw_Unlock();

        u32 pressed = waitInputWithTimeout(1000);

        if(pressed & KEY_B)
            break;
    }
    while(!menuShouldExit);

    return forcedConnection;
}

void SysConfigMenu_TogglePowerButton(void)
{
    u32 mcuIRQMask;

    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    mcuHwcInit();
    MCUHWC_ReadRegister(0x18, (u8*)&mcuIRQMask, 4);
    mcuHwcExit();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Menu configurazione di sistema");
        Draw_DrawString(10, 30, COLOR_WHITE, "Premi A per Attivare/Disattivare, Premi B per tornare indietro.");

        Draw_DrawString(10, 50, COLOR_WHITE, "Stato attuale:");
        Draw_DrawString(100, 50, (((mcuIRQMask & 0x00000001) == 0x00000001) ? COLOR_RED : COLOR_GREEN), (((mcuIRQMask & 0x00000001) == 0x00000001) ? " DISABILITATO" : " ABILITATO "));

        Draw_FlushFramebuffer();
        Draw_Unlock();

        u32 pressed = waitInputWithTimeout(1000);

        if(pressed & KEY_A)
        {
            mcuHwcInit();
            MCUHWC_ReadRegister(0x18, (u8*)&mcuIRQMask, 4);
            mcuIRQMask ^= 0x00000001;
            MCUHWC_WriteRegister(0x18, (u8*)&mcuIRQMask, 4);
            mcuHwcExit();
        }
        else if(pressed & KEY_B)
            return;
    }
    while(!menuShouldExit);
}

void SysConfigMenu_ControlWifi(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    u32 slot = 0;
    char ssids[3][32] = {{0}};

    Result resInit = acInit();
    for (u32 i = 0; i < 3; i++)
    {
        if (R_SUCCEEDED(ACI_LoadNetworkSetting(i)))
            ACI_GetNetworkWirelessEssidSecuritySsid(ssids[i]);
        else
            strcpy(ssids[i], "(non configurato)");
    }
    if (R_SUCCEEDED(resInit))
        acExit();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Menu Configurazione di sistema");
        u32 posY = Draw_DrawString(10, 30, COLOR_WHITE, "Premi A per forzare la connessione per uno slot, Premi B per annullare\n\n");

        for (u32 i = 0; i < 3; i++)
        {
            Draw_DrawString(10, posY + SPACING_Y * i, COLOR_TITLE, slot == i ? ">" : " ");
            Draw_DrawFormattedString(30, posY + SPACING_Y * i, COLOR_WHITE, "[%d] %s", (int)i + 1, ssids[i]);
        }

        Draw_FlushFramebuffer();
        Draw_Unlock();

        u32 pressed = waitInputWithTimeout(1000);

        if(pressed & KEY_A)
        {
            if(SysConfigMenu_ForceWifiConnection(slot))
            {
                // Connection successfully forced, return from this menu to prevent ac handle refcount leakage.
                break;
            }

            Draw_Lock();
            Draw_ClearFramebuffer();
            Draw_FlushFramebuffer();
            Draw_Unlock();
        }
        else if(pressed & KEY_DOWN)
        {
            slot = (slot + 1) % 3;
        }
        else if(pressed & KEY_UP)
        {
            slot = (3 + slot - 1) % 3;
        }
        else if(pressed & KEY_B)
            return;
    }
    while(!menuShouldExit);
}

void SysConfigMenu_DisableForcedWifiConnection(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    acExit();
    SysConfigMenu_UpdateStatus(true);

    do
    {
        Draw_Lock();
       Draw_DrawString(10, 10, COLOR_TITLE, "Menu configurazione di sistema");
        Draw_DrawString(10, 30, COLOR_WHITE, "Connessione forzata disabilitata con successo.\nNota:La connessione automatica puo' rimanere rotta.");
        u32 pressed = waitInputWithTimeout(1000);
        if(pressed & KEY_B)
            return;
    }
    while(!menuShouldExit);
}

void SysConfigMenu_ToggleCardIfPower(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    bool cardIfStatus = false;
    bool updatedCardIfStatus = false;

    do
    {
        Result res = FSUSER_CardSlotGetCardIFPowerStatus(&cardIfStatus);
        if (R_FAILED(res)) cardIfStatus = false;

        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Menu configurazione di sistema");
        u32 posY = Draw_DrawString(10, 30, COLOR_WHITE, "Premi A peer attivare/disattivare, premi B per tornare indietro.\n\n");
        posY = Draw_DrawString(10, posY, COLOR_WHITE, "Inserire o rimuovere una scheda resettera' lo stato,\ne dovrai reinserire la scheda se vorrai\ngiocarci.\n\n");
        Draw_DrawString(10, posY, COLOR_WHITE, "Stato attuale:");
        Draw_DrawString(100, posY, !cardIfStatus ? COLOR_RED : COLOR_GREEN, !cardIfStatus ? " DISABILITATO " : " ABILITATO ");

        Draw_FlushFramebuffer();
        Draw_Unlock();

        u32 pressed = waitInputWithTimeout(1000);

        if(pressed & KEY_A)
        {
            if (!cardIfStatus)
                res = FSUSER_CardSlotPowerOn(&updatedCardIfStatus);
            else
                res = FSUSER_CardSlotPowerOff(&updatedCardIfStatus);

            if (R_SUCCEEDED(res))
                cardIfStatus = !updatedCardIfStatus;
        }
        else if(pressed & KEY_B)
            return;
    }
    while(!menuShouldExit);
}

void SysConfigMenu_ToggleRehidFolder(void)
{
    FS_Archive sdmcArchive = 0;

    if(R_SUCCEEDED(FSUSER_OpenArchive(&sdmcArchive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""))))
    {
        MenuItem *item = &sysconfigMenu.items[3];

        if(R_SUCCEEDED(FSUSER_RenameDirectory(sdmcArchive, fsMakePath(PATH_ASCII, rehidPath), sdmcArchive, fsMakePath(PATH_ASCII, rehidOffPath))))
            {
                item->title = "Attiva/Disattiva cartella rehid: [Disabilitata]";
            }
        else if(R_SUCCEEDED(FSUSER_RenameDirectory(sdmcArchive, fsMakePath(PATH_ASCII, rehidOffPath), sdmcArchive, fsMakePath(PATH_ASCII, rehidPath))))
            {
                item->title = "Attiva/Disattiva cartella rehid: [Abilitata]";
            }

        FSUSER_CloseArchive(sdmcArchive);
    }
}

void SysConfigMenu_UpdateRehidFolderStatus(void)
{
    Handle dir;
    FS_Archive sdmcArchive = 0;

    if(R_SUCCEEDED(FSUSER_OpenArchive(&sdmcArchive, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""))))
    {
        MenuItem *item = &sysconfigMenu.items[3];

        if(R_SUCCEEDED(FSUSER_OpenDirectory(&dir, sdmcArchive, fsMakePath(PATH_ASCII, rehidPath))))
            {
                FSDIR_Close(dir);
                item->title = "Attiva/Disattiva cartella rehid: [Abilitata]";
            }
            else
            {
                item->title = "Attiva/Disattiva cartella rehid: [Disabilitata]";
            }

        FSUSER_CloseArchive(sdmcArchive);
    }
}

void SysConfigMenu_Tip(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Consigli");
        Draw_DrawString(10, 30, COLOR_WHITE, "Sul menu Rosalina:");
        Draw_DrawString(10, 50, COLOR_WHITE, "  * Premi start per attivare/disattivare il Wifi");
        Draw_DrawString(10, 60, COLOR_WHITE, "  * Premi select per attivare/disattivare i LEDs (non possono essere attivati");
        Draw_DrawString(10, 70, COLOR_WHITE, "  se la batteria e' bassa o il sistema e'");
        Draw_DrawString(10, 80, COLOR_WHITE, "  in mod. riposo)");
        Draw_DrawString(10, 90, COLOR_WHITE, "  * Premi Y per forzare il led blu (accetta il bypass della");
        Draw_DrawString(10, 100, COLOR_WHITE, "  restrizione dell'attivazione con batteria bassa)");
        Draw_DrawString(10, 120, COLOR_WHITE, " Mentre il Sistema e' in esecuzione:");
        Draw_DrawString(10, 140, COLOR_WHITE, "  * Premi A + B + X + Y + Start per riavviare istantaneamente");
        Draw_DrawString(10, 150, COLOR_WHITE, "  * Premi Start + Select per attivare/disattivare lo schermo inf.");
        Draw_DrawString(10, 170, COLOR_WHITE, "  *Usa l'ExtraConfigMenu per vedere piu' impostazioni su luma");

        Draw_FlushFramebuffer();
        Draw_Unlock();

        u32 pressed = waitInputWithTimeout(1000);

        if(pressed & KEY_B)
            return;
    }
    while(!menuShouldExit);
}

void SysConfigMenu_ChangeScreenBrightness(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    // gsp:LCD GetLuminance is stubbed on O3DS so we have to implement it ourselves... damn it.
    // Assume top and bottom screen luminances are the same (should be; if not, we'll set them to the same values).
    u32 luminance = getCurrentLuminance(false);
    u32 minLum = getMinLuminancePreset();
    u32 maxLum = getMaxLuminancePreset();

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Screen brightness");
        u32 posY = 30;
        posY = Draw_DrawFormattedString(
            10,
            posY,
            COLOR_WHITE,
            "Current luminance: %lu (min. %lu, max. %lu)\n\n",
            luminance,
            minLum,
            maxLum
        );
        posY = Draw_DrawString(10, posY, COLOR_WHITE, "Controls: Up/Down for +-1, Right/Left for +-10.\n");
        posY = Draw_DrawString(10, posY, COLOR_WHITE, "Press A to start, B to exit.\n\n");

        posY = Draw_DrawString(10, posY, COLOR_RED, "WARNING: \n");
        posY = Draw_DrawString(10, posY, COLOR_WHITE, "  * value will be limited by calibration.\n");
        posY = Draw_DrawString(10, posY, COLOR_WHITE, "  * bottom framebuffer will be restored until\nyou exit.");
        Draw_FlushFramebuffer();
        Draw_Unlock();

        u32 pressed = waitInputWithTimeout(1000);

        if (pressed & KEY_A)
            break;

        if (pressed & KEY_B)
            return;
    }
    while (!menuShouldExit);

    Draw_Lock();

    Draw_RestoreFramebuffer();
    Draw_FreeFramebufferCache();

    svcKernelSetState(0x10000, 2); // unblock gsp
    gspLcdInit(); // assume it doesn't fail. If it does, brightness won't change, anyway.

    // gsp:LCD will normalize the brightness between top/bottom screen, handle PWM, etc.

    s32 lum = (s32)luminance;

    do
    {
        u32 pressed = waitInputWithTimeout(1000);
        if (pressed & DIRECTIONAL_KEYS)
        {
            if (pressed & KEY_UP)
                lum += 1;
            else if (pressed & KEY_DOWN)
                lum -= 1;
            else if (pressed & KEY_RIGHT)
                lum += 10;
            else if (pressed & KEY_LEFT)
                lum -= 10;

            lum = lum < (s32)minLum ? (s32)minLum : lum;
            lum = lum > (s32)maxLum ? (s32)maxLum : lum;

            // We need to call gsp here because updating the active duty LUT is a bit tedious (plus, GSP has internal state).
            // This is actually SetLuminance:
            GSPLCD_SetBrightnessRaw(BIT(GSP_SCREEN_TOP) | BIT(GSP_SCREEN_BOTTOM), lum);
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
