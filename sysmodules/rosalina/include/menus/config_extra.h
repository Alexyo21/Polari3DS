#pragma once

#include <3ds.h>
#include <3ds/os.h>
#include "fmt.h"
#include "draw.h"
#include "ifile.h"
#include "menu.h"
#include "menus.h"
#include "luminance.h"
#include "luma_config.h"
#include "luma_shared_config.h"

#define CONFIG_FILE         "configExtra.ini"

typedef struct {
	bool suppressLeds;
	bool cutSlotPower;
	bool cutSleepWifi;
	bool homeToRosalina;
	bool toggleBottomLcd;
} config_extra;

extern config_extra configExtra;
extern Menu configExtraMenu;

void ConfigExtra_SetSuppressLeds();
void ConfigExtra_SetCutSlotPower();
void ConfigExtra_SetCutSleepWifi();
void ConfigExtra_SetHomeToRosalina();
void ConfigExtra_SetToggleBottomLcd();
void ConfigExtra_UpdateMenuItem(int menuIndex, bool value);
void ConfigExtra_UpdateAllMenuItems();
void ConfigExtra_ReadConfigExtra();
void ConfigExtra_WriteConfigExtra();
