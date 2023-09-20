#ifndef __CONFIGEXTRA_H__
#define __CONFIGEXTRA_H__

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
#include "utils.h"

#define CONFIG_FILE         "configExtra.ini"

typedef struct {
	bool suppressLeds;
	bool cutSlotPower;
	bool cutSleepWifi;
	bool homeToRosalina;
	bool toggleBottomLcd;
	bool turnLedsOffStandby;
} config_extra;

extern config_extra configExtra;
extern Menu configExtraMenu;

void ConfigExtra_SetSuppressLeds(void);
void ConfigExtra_SetCutSlotPower(void);
void ConfigExtra_SetCutSleepWifi(void);
void ConfigExtra_SetHomeToRosalina(void);
void ConfigExtra_SetToggleBottomLcd(void);
void ConfigExtra_SetTurnLedsOffStandby(void);
void ConfigExtra_UpdateMenuItem(int menuIndex, bool value);
void ConfigExtra_UpdateAllMenuItems(void);
void ConfigExtra_ReadConfigExtra(void);
void ConfigExtra_WriteConfigExtra(void);

#endif
