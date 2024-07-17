#pragma once

#include <3ds/types.h>
#include <3ds/exheader.h>
#include "ifile.h"
#include "util.h"
#include "luma_shared_config.h"

#define MAKE_BRANCH(src,dst)      (0xEA000000 | ((u32)((((u8 *)(dst) - (u8 *)(src)) >> 2) - 2) & 0xFFFFFF))
#define MAKE_BRANCH_LINK(src,dst) (0xEB000000 | ((u32)((((u8 *)(dst) - (u8 *)(src)) >> 2) - 2) & 0xFFFFFF))

#define CONFIG(a)        (((config >> (a)) & 1) != 0)
#define MULTICONFIG(a)   ((multiConfig >> (2 * (a))) & 3)
#define BOOTCONFIG(a, b) ((bootConfig >> (a)) & (b))

#define BOOTCFG_NAND         BOOTCONFIG(0, 1)
#define BOOTCFG_EMUINDEX     BOOTCONFIG(1, 3)
#define BOOTCFG_NOFORCEFLAG  BOOTCONFIG(3, 1)
#define BOOTCFG_NTRCARDBOOT  BOOTCONFIG(4, 1)

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

typedef struct {
	bool suppressLeds;
	bool cutSlotPower;
	bool cutSleepWifi;
	bool homeToRosalina;
	bool toggleBottomLcd;
	bool turnLedsOffStandby;
	bool perGamePlugin;
} config_extra;

extern u32 config, multiConfig, bootConfig;
extern bool isN3DS, isSdMode, nextGamePatchDisabled;

void patchCode(u64 progId, u16 progVer, u8 *code, u32 size, u32 textSize, u32 roSize, u32 dataSize, u32 roAddress, u32 dataAddress);
bool loadTitleCodeSection(u64 progId, u8 *code, u32 size);
bool loadTitleExheaderInfo(u64 progId, ExHeader_Info *exheaderInfo);
bool useN3dsSettings(u64 progId);

Result openSysmoduleCxi(IFile *outFile, u64 progId);
bool readSysmoduleCxiNcchHeader(Ncch *outNcchHeader, IFile *file);
bool readSysmoduleCxiExHeaderInfo(ExHeader_Info *outExhi, const Ncch *ncchHeader, IFile *file);
bool readSysmoduleCxiCode(u8 *outCode, u32 *outSize, u32 maxSize, IFile *file, const Ncch *ncchHeader);

bool usePerGamePluginSetting(void);
bool enablePluginForTitle(u64 progId);
