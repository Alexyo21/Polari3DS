#include <3ds.h>
#include "fmt.h"
#include "draw.h"
#include "ifile.h"
#include "menu.h"
#include "menus.h"
#include "menus/miscellaneous.h"
#include "menus/sysconfig.h"
#include "luma_config.h"
#include "luma_shared_config.h"
#include "menus/config_extra.h"
#include "luminance.h"
#include "menus/n3ds.h"
#include "menus/screen_filters.h"
#include "config_template_ini.h"

#include "configExtra_ini.h"

config_extra configExtra = { .suppressLeds = true, .cutSlotPower = false, .cutSleepWifi = false, .homeToRosalina = false, .toggleBottomLcd = false };
bool configExtraSaved = false;

static const char menuText[6][32] = {
    "Automatically suppress LEDs",
    "Cut power to TWL Flashcards",
    "Cut 3DS WiFi in sleep mode",
    "Home button opens Rosalina",
    "St+Se toggle bottom LCD in menu",
    "Save config. Changes saved"
};

static char menuDisplay[5][64];

Menu configExtraMenu = {
    "Extra config menu",
    {
        { menuText[0], METHOD, .method = &ConfigExtra_SetSuppressLeds},
        { menuText[1], METHOD, .method = &ConfigExtra_SetCutSlotPower},
        { menuText[2], METHOD, .method = &ConfigExtra_SetCutSleepWifi},
        { menuText[3], METHOD, .method = &ConfigExtra_SetHomeToRosalina},
        { menuText[4], METHOD, .method = &ConfigExtra_SetToggleBottomLcd},
        { menuText[5], METHOD, .method = &ConfigExtra_WriteConfigExtra},
        {},
    }
};

void ConfigExtra_SetSuppressLeds(void) 
{
    configExtra.suppressLeds = !configExtra.suppressLeds;
    ConfigExtra_UpdateMenuItem(0, configExtra.suppressLeds);
    configExtraSaved = false;
    ConfigExtra_UpdateMenuItem(5, configExtraSaved);
}

void ConfigExtra_SetCutSlotPower(void) 
{
    configExtra.cutSlotPower = !configExtra.cutSlotPower;
    ConfigExtra_UpdateMenuItem(1, configExtra.cutSlotPower);
    configExtraSaved = false;
    ConfigExtra_UpdateMenuItem(5, configExtraSaved);
}

void ConfigExtra_SetCutSleepWifi(void) 
{
    configExtra.cutSleepWifi = !configExtra.cutSleepWifi;
    ConfigExtra_UpdateMenuItem(2, configExtra.cutSleepWifi);
    configExtraSaved = false;
    ConfigExtra_UpdateMenuItem(5, configExtraSaved);
}

void ConfigExtra_SetHomeToRosalina(void) 
{
    configExtra.homeToRosalina = !configExtra.homeToRosalina;
    ConfigExtra_UpdateMenuItem(3, configExtra.homeToRosalina);
    configExtraSaved = false;
    ConfigExtra_UpdateMenuItem(5, configExtraSaved);
}

void ConfigExtra_SetToggleBottomLcd(void) 
{
    configExtra.toggleBottomLcd = !configExtra.toggleBottomLcd;
    ConfigExtra_UpdateMenuItem(4, configExtra.toggleBottomLcd);
    configExtraSaved = false;
    ConfigExtra_UpdateMenuItem(5, configExtraSaved);
}

void ConfigExtra_UpdateMenuItem(int menuIndex, bool value)
{
    sprintf(menuDisplay[menuIndex], "%s: %s", menuText[menuIndex], value ? "[true]" : "[false]");
    configExtraMenu.items[menuIndex].title = menuDisplay[menuIndex];
}

void ConfigExtra_UpdateAllMenuItems(void)
{
    ConfigExtra_UpdateMenuItem(0, configExtra.suppressLeds);
    ConfigExtra_UpdateMenuItem(1, configExtra.cutSlotPower);
    ConfigExtra_UpdateMenuItem(2, configExtra.cutSleepWifi);
    ConfigExtra_UpdateMenuItem(3, configExtra.homeToRosalina);
    ConfigExtra_UpdateMenuItem(4, configExtra.toggleBottomLcd);
    ConfigExtra_UpdateMenuItem(5, configExtraSaved);
}

void ConfigExtra_ReadConfigExtra(void)
{
    IFile file;
    Result res = 0;

    res = IFile_Open(&file, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""),
            fsMakePath(PATH_ASCII, "/luma/configExtra.ini"), FS_OPEN_READ);
        
    if(R_SUCCEEDED(res))
    {
        u64 total;
        res = IFile_Read(&file, &total, &configExtra, sizeof(configExtra));
        IFile_Close(&file);
        if(R_SUCCEEDED(res)) 
        {
            configExtraSaved = true;
        }
    }
}

void ConfigExtra_WriteConfigExtra(void)
{
    IFile file;
    Result res = 0;

    res = IFile_Open(&file, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""),
            fsMakePath(PATH_ASCII, "/luma/configExtra.ini"), FS_OPEN_CREATE | FS_OPEN_WRITE);
        
    if(R_SUCCEEDED(res))
    {
        u64 total;
        res = IFile_Write(&file, &total, &configExtra, sizeof(configExtra), 0);
        IFile_Close(&file);

        if(R_SUCCEEDED(res)) 
        {
            configExtraSaved = true;
            ConfigExtra_UpdateMenuItem(5, configExtraSaved);
        }
    }
}
