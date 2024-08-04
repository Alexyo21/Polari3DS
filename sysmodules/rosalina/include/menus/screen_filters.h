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

#include <3ds/types.h>
#include "menu.h"
#include "utils.h"

extern Menu screenFiltersMenu;

typedef struct ScreenFilter {
    u16 cct;
    bool invert;
    u8 colorCurveCorrection;
    float gamma;
    float contrast;
    float brightness;
} ScreenFilter;

extern ScreenFilter topScreenFilter;
extern ScreenFilter bottomScreenFilter;

void ScreenFiltersMenu_RestoreSettings(void);
void ScreenFiltersMenu_LoadConfig(void);

void ScreenFiltersMenu_SetDefault(void);            // 6500K (default)

void ScreenFiltersMenu_SetAquarium(void);           // 10000K
void ScreenFiltersMenu_SetOvercastSky(void);        // 7500K
void ScreenFiltersMenu_SetDaylight(void);           // 5500K
void ScreenFiltersMenu_SetFluorescent(void);        // 4200K
void ScreenFiltersMenu_SetHalogen(void);            // 3400K
void ScreenFiltersMenu_SetIncandescent(void);       // 2700K
void ScreenFiltersMenu_SetWarmIncandescent(void);   // 2300K
void ScreenFiltersMenu_SetCandle(void);             // 1900K
void ScreenFiltersMenu_SetEmber(void);              // 1200K

void ScreenFiltersMenu_SetSrgbColorCurves(void);
void ScreenFiltersMenu_RestoreColorCurves(void);

void ScreenFiltersMenu_AdvancedConfiguration(void);

void ScreenFilter_SuppressLeds(void);
