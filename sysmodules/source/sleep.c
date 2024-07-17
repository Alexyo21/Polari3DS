/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2018 Aurora Wright, TuxSH
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
#include "plugin/plgldr.h"
#include "plugin/plgloader.h"
#include "sleep.h"
#include "menus/config_extra.h"

static bool         g_isSleeping = false;
static LightEvent   g_onWakeUpEvent;

void    IR__HandleSleep(bool isSleeping); ///< See in input_redirection.c

void    Sleep__Init(void)
{
    // No need anymore, handled by ServiceManager
    // srvSubscribe(0x214); ///< Sleep entry
    // srvSubscribe(0x213); ///< Sleep exit
    LightEvent_Init(&g_onWakeUpEvent, RESET_STICKY);
}

void    Sleep__HandleNotification(u32 notifId)
{
    if (notifId == 0x214) ///< Sleep entry
    {
        LightEvent_Clear(&g_onWakeUpEvent);
        g_isSleeping = true;
        // IR patch creates sleep issue, so we need to handle it
        // IR__HandleSleep(g_isSleeping);
        // Plugins do not receive 0x214 notifications, so we send it via our custom service
        if (PLG__GetConfigMemoryStatus() == PLG_CFG_RUNNING)
            PLG__NotifyEvent(PLG_SLEEP_ENTRY, false);
    }
    else if (notifId == 0x213) ///< Sleep exit
    {
        g_isSleeping = false;
        LightEvent_Signal(&g_onWakeUpEvent);
        // IR patch creates sleep issue, so we need to handle it
        // IR__HandleSleep(g_isSleeping);
        // Plugins actually receives 0x213 notifications, but since we send sleep entry, let's do sleep exit as well
        if (PLG__GetConfigMemoryStatus() == PLG_CFG_RUNNING)
            PLG__NotifyEvent(PLG_SLEEP_EXIT, false);
    }
}

bool    Sleep__Status(void)
{
    if (g_isSleeping)
    {
        LightEvent_Wait(&g_onWakeUpEvent);
        return true;
    }
    return false;
}
