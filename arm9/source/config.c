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

#define _GNU_SOURCE // for strchrnul

#include <assert.h>
#include <strings.h>
#include "config.h"
#include "memory.h"
#include "fs.h"
#include "utils.h"
#include "screen.h"
#include "draw.h"
#include "emunand.h"
#include "buttons.h"
#include "pin.h"
#include "i2c.h"
#include "ini.h"
#include "firm.h"

#include "config_template_ini.h" // note that it has an extra NUL byte inserted

#define MAKE_LUMA_VERSION_MCU(major, minor, build) (u16)(((major) & 0xFF) << 8 | ((minor) & 0x1F) << 5 | ((build) & 7))

#define FLOAT_CONV_MULT 100000000ll
#define FLOAT_CONV_PRECISION 8u

CfgData configData;
ConfigurationStatus needConfig;
static CfgData oldConfig;

static CfgDataMcu configDataMcu;
static_assert(sizeof(CfgDataMcu) > 0, "wrong data size");

// INI parsing
// ===========================================================

static const char *singleOptionIniNamesBoot[] = {
    "autoboot_emunand",
    "enable_external_firm_and_modules",
    "enable_game_patching",
    "app_syscore_threads_on_core_2",
    "show_system_settings_string",
    "show_gba_boot_screen",
    "enable_perf_scheduler",
    "allow_updown_leftright_dsi",
    "cut_wifi_sleep_mode",
    "use_dev_unitinfo",
    "disable_arm11_exception_handlers",
    "enable_safe_firm_rosalina",
    "disable_errdisp_enable_instant_reboot",
};

static const char *singleOptionIniNamesMisc[] = {
    "show_advanced_settings",
    "patch_hardware_crypto",
};

static const char *keyNames[] = {
    "A", "B", "Select", "Start", "Right", "Left", "Up", "Down", "R", "L", "X", "Y",
    "?", "?",
    "ZL", "ZR",
    "?", "?", "?", "?",
    "Touch",
    "?", "?", "?",
    "CStick Right", "CStick Left", "CStick Up", "CStick Down",
    "CPad Right", "CPad Left", "CPad Up", "CPad Down",
};

static int parseBoolOption(bool *out, const char *val)
{
    *out = false;
    if (strlen(val) != 1) {
        return -1;
    }

    if (val[0] == '0') {
        return 0;
    } else if (val[0] == '1') {
        *out = true;
        return 0;
    } else {
        return -1;
    }
}

static int parseDecIntOptionImpl(s64 *out, const char *val, size_t numDigits, s64 minval, s64 maxval)
{
    *out = 0;
    s64 res = 0;
    size_t i = 0;

    s64 sign = 1;
    if (numDigits >= 2) {
        if (val[0] == '+') {
            ++i;
        } else if (val[0] == '-') {
            sign = -1;
            ++i;
        }
    }

    for (; i < numDigits; i++) {
        u64 n = (u64)(val[i] - '0');
        if (n > 9) {
            return -1;
        }

        res = 10*res + n;
    }

    res *= sign;
    if (res <= maxval && res >= minval) {
        *out = res;
        return 0;
    } else {
        return -1;
    }
}

static int parseDecIntOption(s64 *out, const char *val, s64 minval, s64 maxval)
{
    return parseDecIntOptionImpl(out, val, strlen(val), minval, maxval);
}

static int parseDecFloatOption(s64 *out, const char *val, s64 minval, s64 maxval)
{
    s64 sign = 1;// intPart < 0 ? -1 : 1;

    switch (val[0]) {
        case '\0':
            return -1;
        case '+':
            ++val;
            break;
        case '-':
            sign = -1;
            ++val;
            break;
        default:
            break;
    }

    // Reject "-" and "+"
    if (val[0] == '\0') {
        return -1;
    }

    char *point = strchrnul(val, '.');

    // Parse integer part, then fractional part
    s64 intPart = 0;
    s64 fracPart = 0;
    int rc = 0;

    if (point == val) {
        // e.g. -.5
        if (val[1] == '\0')
            return -1;
    }
    else {
        rc = parseDecIntOptionImpl(&intPart, val, point - val, INT64_MIN, INT64_MAX);
    }

    if (rc != 0) {
        return -1;
    }

    s64 intPartAbs = sign == -1 ? -intPart : intPart;
    s64 res = 0;
    bool of = __builtin_mul_overflow(intPartAbs, FLOAT_CONV_MULT, &res);

    if (of) {
        return -1;
    }

    s64 mul = FLOAT_CONV_MULT / 10;

    // Check if there's a fractional part
    if (point[0] != '\0' && point[1] != '\0') {
        for (char *pos = point + 1; *pos != '\0' && mul > 0; pos++) {
            if (*pos < '0' || *pos > '9') {
                return -1;
            }

            res += (*pos - '0') * mul;
            mul /= 10;
        }
    }


    res = sign * (res + fracPart);

    if (res <= maxval && res >= minval && !of) {
        *out = res;
        return 0;
    } else {
        return -1;
    }
}

static int parseHexIntOption(u64 *out, const char *val, u64 minval, u64 maxval)
{
    *out = 0;
    size_t numDigits = strlen(val);
    u64 res = 0;

    for (size_t i = 0; i < numDigits; i++) {
        char c = val[i];
        if ((u64)(c - '0') <= 9) {
            res = 16*res + (u64)(c - '0');
        } else if ((u64)(c - 'a') <= 5) {
            res = 16*res + (u64)(c - 'a' + 10);
        } else if ((u64)(c - 'A') <= 5) {
            res = 16*res + (u64)(c - 'A' + 10);
        } else {
            return -1;
        }
    }

    if (res <= maxval && res >= minval) {
        *out = res;
        return 0;
    } else {
        return -1;
    }
}

static int parseKeyComboOption(u32 *out, const char *val)
{
    const char *startpos = val;
    const char *endpos;

    *out = 0;
    u32 keyCombo = 0;
    do {
        // Copy the button name (note that 16 chars is longer than any of the key names)
        char name[17];
        endpos = strchr(startpos, '+');
        size_t n = endpos == NULL ? 16 : endpos - startpos;
        n = n > 16 ? 16 : n;
        strncpy(name, startpos, n);
        name[n] = '\0';

        if (strcmp(name, "?") == 0) {
            // Lol no, bail out
            return -1;
        }

        bool found = false;
        for (size_t i = 0; i < sizeof(keyNames)/sizeof(keyNames[0]); i++) {
            if (strcasecmp(keyNames[i], name) == 0) {
                found = true;
                keyCombo |= 1u << i;
            }
        }

        if (!found) {
            return -1;
        }

        if (endpos != NULL) {
            startpos = endpos + 1;
        }
    } while(endpos != NULL && *startpos != '\0');

    if (*startpos == '\0') {
        // Trailing '+'
        return -1;
    } else {
        *out = keyCombo;
        return 0;
    }
}

static void menuComboToString(char *out, u32 combo)
{
    char *outOrig = out;
    out[0] = 0;
    for(int i = 31; i >= 0; i--)
    {
        if(combo & (1 << i))
        {
            strcpy(out, keyNames[i]);
            out += strlen(keyNames[i]);
            *out++ = '+';
        }
    }

    if (out != outOrig)
        out[-1] = 0;
}

static int encodedFloatToString(char *out, s64 val)
{
    s64 sign = val >= 0 ? 1 : -1;

    s64 intPart = (sign * val) / FLOAT_CONV_MULT;
    s64 fracPart = (sign * val) % FLOAT_CONV_MULT;

    while (fracPart % 10 != 0) {
        // Remove trailing zeroes
        fracPart /= 10;
    }

    int n = sprintf(out, "%lld", sign * intPart);
    if (fracPart != 0) {
        n += sprintf(out + n, ".%0*lld", (int)FLOAT_CONV_PRECISION, fracPart);

        // Remove trailing zeroes
        int n2 = n - 1;
        while (out[n2] == '0') {
            out[n2--] = '\0';
        }

        n = n2;
    }

    return n;
}

static bool hasIniParseError = false;
static int iniParseErrorLine = 0;

#define CHECK_PARSE_OPTION(res) do { if((res) < 0) { hasIniParseError = true; iniParseErrorLine = lineno; return 0; } } while(false)

static int configIniHandler(void* user, const char* section, const char* name, const char* value, int lineno)
{
    CfgData *cfg = (CfgData *)user;
    if (strcmp(section, "meta") == 0) {
        if (strcmp(name, "config_version_major") == 0) {
            s64 opt;
            CHECK_PARSE_OPTION(parseDecIntOption(&opt, value, 0, 0xFFFF));
            cfg->formatVersionMajor = (u16)opt;
            return 1;
        } else if (strcmp(name, "config_version_minor") == 0) {
            s64 opt;
            CHECK_PARSE_OPTION(parseDecIntOption(&opt, value, 0, 0xFFFF));
            cfg->formatVersionMinor = (u16)opt;
            return 1;
        } else {
            CHECK_PARSE_OPTION(-1);
        }
    } else if (strcmp(section, "boot") == 0) {
        // Simple options displayed on the Luma3DS boot screen
        for (size_t i = 0; i < sizeof(singleOptionIniNamesBoot)/sizeof(singleOptionIniNamesBoot[0]); i++) {
            if (strcmp(name, singleOptionIniNamesBoot[i]) == 0) {
                bool opt;
                CHECK_PARSE_OPTION(parseBoolOption(&opt, value));
                cfg->config |= (u32)opt << i;
                return 1;
            }
        }

        // Multi-choice options displayed on the Luma3DS boot screen

        if (strcmp(name, "default_emunand_number") == 0) {
            s64 opt;
            CHECK_PARSE_OPTION(parseDecIntOption(&opt, value, 1, 4));
            cfg->multiConfig |= (opt - 1) << (2 * (u32)DEFAULTEMU);
            return 1;
        } else if (strcmp(name, "brightness_level") == 0) {
            s64 opt;
            CHECK_PARSE_OPTION(parseDecIntOption(&opt, value, 1, 4));
            cfg->multiConfig |= (4 - opt) << (2 * (u32)BRIGHTNESS);
            return 1;
        } else if (strcmp(name, "splash_position") == 0) {
            if (strcasecmp(value, "off") == 0) {
                cfg->multiConfig |= 0 << (2 * (u32)SPLASH);
                return 1;
            } else if (strcasecmp(value, "before payloads") == 0) {
                cfg->multiConfig |= 1 << (2 * (u32)SPLASH);
                return 1;
            } else if (strcasecmp(value, "after payloads") == 0) {
                cfg->multiConfig |= 2 << (2 * (u32)SPLASH);
                return 1;
            } else {
                CHECK_PARSE_OPTION(-1);
            }
        } else if (strcmp(name, "splash_duration_ms") == 0) {
            // Not displayed in the menu anymore, but more configurable
            s64 opt;
            CHECK_PARSE_OPTION(parseDecIntOption(&opt, value, 0, 0xFFFFFFFFu));
            cfg->splashDurationMsec = (u32)opt;
            return 1;
        }
        else if (strcmp(name, "pin_lock_num_digits") == 0) {
            s64 opt;
            u32 encodedOpt;
            CHECK_PARSE_OPTION(parseDecIntOption(&opt, value, 0, 8));
            // Only allow for 0 (off), 4, 6 or 8 'digits'
            switch (opt) {
                case 0: encodedOpt = 0; break;
                case 4: encodedOpt = 1; break;
                case 6: encodedOpt = 2; break;
                case 8: encodedOpt = 3; break;
                default: {
                    CHECK_PARSE_OPTION(-1);
                }
            }
            cfg->multiConfig |= encodedOpt << (2 * (u32)PIN);
            return 1;
        } else if (strcmp(name, "app_launch_new_3ds_cpu") == 0) {
            if (strcasecmp(value, "off") == 0) {
                cfg->multiConfig |= 0 << (2 * (u32)NEWCPU);
                return 1;
            } else if (strcasecmp(value, "clock") == 0) {
                cfg->multiConfig |= 1 << (2 * (u32)NEWCPU);
                return 1;
            } else if (strcasecmp(value, "l2") == 0) {
                cfg->multiConfig |= 2 << (2 * (u32)NEWCPU);
                return 1;
            } else if (strcasecmp(value, "clock+l2") == 0) {
                cfg->multiConfig |= 3 << (2 * (u32)NEWCPU);
                return 1;
            } else {
                CHECK_PARSE_OPTION(-1);
            }
        } else if (strcmp(name, "autoboot_mode") == 0) {
            if (strcasecmp(value, "off") == 0) {
                cfg->multiConfig |= 0 << (2 * (u32)AUTOBOOTMODE);
                return 1;
            } else if (strcasecmp(value, "3ds") == 0) {
                cfg->multiConfig |= 1 << (2 * (u32)AUTOBOOTMODE);
                return 1;
            } else if (strcasecmp(value, "dsi") == 0) {
                cfg->multiConfig |= 2 << (2 * (u32)AUTOBOOTMODE);
                return 1;
            } else {
                CHECK_PARSE_OPTION(-1);
            }
        } else if (strcmp(name, "force_audio_output") == 0) {
            if (strcasecmp(value, "off") == 0) {
                cfg->multiConfig |= 0 << (2 * (u32)FORCEAUDIOOUTPUT);
                return 1;
            } else if (strcasecmp(value, "headphones") == 0) {
                cfg->multiConfig |= 1 << (2 * (u32)FORCEAUDIOOUTPUT);
                return 1;
            } else if (strcasecmp(value, "speakers") == 0) {
                cfg->multiConfig |= 2 << (2 * (u32)FORCEAUDIOOUTPUT);
                return 1;
            } else {
                CHECK_PARSE_OPTION(-1);
            }
        } else {
            CHECK_PARSE_OPTION(-1);
        }
    } else if (strcmp(section, "rosalina") == 0) {
        // Rosalina options
        if (strcmp(name, "hbldr_3dsx_titleid") == 0) {
            u64 opt;
            CHECK_PARSE_OPTION(parseHexIntOption(&opt, value, 0, 0xFFFFFFFFFFFFFFFFull));
            cfg->hbldr3dsxTitleId = opt;
            return 1;
        } else if (strcmp(name, "rosalina_menu_combo") == 0) {
            u32 opt;
            CHECK_PARSE_OPTION(parseKeyComboOption(&opt, value));
            cfg->rosalinaMenuCombo = opt;
            return 1;
        } else if (strcmp(name, "plugin_loader_enabled") == 0) {
            bool opt;
            CHECK_PARSE_OPTION(parseBoolOption(&opt, value));
            cfg->pluginLoaderFlags = opt ? cfg->pluginLoaderFlags | 1 : cfg->pluginLoaderFlags & ~1;
            return 1;
        } else if (strcmp(name, "ntp_tz_offset_min") == 0) {
            s64 opt;
            CHECK_PARSE_OPTION(parseDecIntOption(&opt, value, -779, 899));
            cfg->ntpTzOffetMinutes = (s16)opt;
            return 1;
        } else {
            CHECK_PARSE_OPTION(-1);
        }
    } else if (strcmp(section, "screen_filters") == 0) {
        if (strcmp(name, "screen_filters_top_cct") == 0) {
            s64 opt;
            CHECK_PARSE_OPTION(parseDecIntOption(&opt, value, 1000, 25100));
            cfg->topScreenFilter.cct = (u32)opt;
            return 1;
        } else if (strcmp(name, "screen_filters_top_gamma") == 0) {
            s64 opt;
            CHECK_PARSE_OPTION(parseDecFloatOption(&opt, value, 0, 8 * FLOAT_CONV_MULT));
            cfg->topScreenFilter.gammaEnc = opt;
            return 1;
        } else if (strcmp(name, "screen_filters_top_contrast") == 0) {
            s64 opt;
            CHECK_PARSE_OPTION(parseDecFloatOption(&opt, value, 0, 255 * FLOAT_CONV_MULT));
            cfg->topScreenFilter.contrastEnc = opt;
            return 1;
        } else if (strcmp(name, "screen_filters_top_brightness") == 0) {
            s64 opt;
            CHECK_PARSE_OPTION(parseDecFloatOption(&opt, value, -1 * FLOAT_CONV_MULT, 1 * FLOAT_CONV_MULT));
            cfg->topScreenFilter.brightnessEnc = opt;
            return 1;
        } else if (strcmp(name, "screen_filters_top_invert") == 0) {
            bool opt;
            CHECK_PARSE_OPTION(parseBoolOption(&opt, value));
            cfg->topScreenFilter.invert = opt;
            return 1;
        } else if (strcmp(name, "screen_filters_bot_cct") == 0) {
            s64 opt;
            CHECK_PARSE_OPTION(parseDecIntOption(&opt, value, 1000, 25100));
            cfg->bottomScreenFilter.cct = (u32)opt;
            return 1;
        } else if (strcmp(name, "screen_filters_bot_gamma") == 0) {
            s64 opt;
            CHECK_PARSE_OPTION(parseDecFloatOption(&opt, value, 0, 8 * FLOAT_CONV_MULT));
            cfg->bottomScreenFilter.gammaEnc = opt;
            return 1;
        } else if (strcmp(name, "screen_filters_bot_contrast") == 0) {
            s64 opt;
            CHECK_PARSE_OPTION(parseDecFloatOption(&opt, value, 0, 255 * FLOAT_CONV_MULT));
            cfg->bottomScreenFilter.contrastEnc = opt;
            return 1;
        } else if (strcmp(name, "screen_filters_bot_brightness") == 0) {
            s64 opt;
            CHECK_PARSE_OPTION(parseDecFloatOption(&opt, value, -1 * FLOAT_CONV_MULT, 1 * FLOAT_CONV_MULT));
            cfg->bottomScreenFilter.brightnessEnc = opt;
            return 1;
        } else if (strcmp(name, "screen_filters_bot_invert") == 0) {
            bool opt;
            CHECK_PARSE_OPTION(parseBoolOption(&opt, value));
            cfg->bottomScreenFilter.invert = opt;
            return 1;
        } else {
            CHECK_PARSE_OPTION(-1);
        }
    } else if (strcmp(section, "autoboot") == 0) {
        if (strcmp(name, "autoboot_dsi_titleid") == 0) {
            u64 opt;
            CHECK_PARSE_OPTION(parseHexIntOption(&opt, value, 0, 0xFFFFFFFFFFFFFFFFull));
            cfg->autobootTwlTitleId = opt;
            return 1;
        } else if (strcmp(name, "autoboot_3ds_app_mem_type") == 0) {
            s64 opt;
            CHECK_PARSE_OPTION(parseDecIntOption(&opt, value, 0, 4));
            cfg->autobootCtrAppmemtype = (u8)opt;
            return 1;
        } else {
            CHECK_PARSE_OPTION(-1);
        }
    } else if (strcmp(section, "misc") == 0) {
        for (size_t i = 0; i < sizeof(singleOptionIniNamesMisc)/sizeof(singleOptionIniNamesMisc[0]); i++) {
            if (strcmp(name, singleOptionIniNamesMisc[i]) == 0) {
                bool opt;
                CHECK_PARSE_OPTION(parseBoolOption(&opt, value));
                cfg->config |= (u32)opt << (i + (u32)SHOWADVANCEDSETTINGS);
                return 1;
            }
        }
        
        if (strcmp(name, "volume_slider_override") == 0) {
            s64 opt;
            CHECK_PARSE_OPTION(parseDecIntOption(&opt, value, -1, 100));
            cfg->volumeSliderOverride = (s8)opt;
            return 1;
        } else {
              CHECK_PARSE_OPTION(-1);
        }
    } else {
        CHECK_PARSE_OPTION(-1);
    }
}

static size_t saveLumaIniConfigToStr(char *out)
{
    const CfgData *cfg = &configData;

    char lumaVerStr[64];
    char lumaRevSuffixStr[16];
    char rosalinaMenuComboStr[128];

    const char *splashPosStr;
    const char *n3dsCpuStr;
    const char *autobootModeStr;
    const char *forceAudioOutputStr;

    switch (MULTICONFIG(SPLASH)) {
        default: case 0: splashPosStr = "off"; break;
        case 1: splashPosStr = "before payloads"; break;
        case 2: splashPosStr = "after payloads"; break;
    }

    switch (MULTICONFIG(NEWCPU)) {
        default: case 0: n3dsCpuStr = "off"; break;
        case 1: n3dsCpuStr = "clock"; break;
        case 2: n3dsCpuStr = "l2"; break;
        case 3: n3dsCpuStr = "clock+l2"; break;
    }

    switch (MULTICONFIG(AUTOBOOTMODE)) {
        default: case 0: autobootModeStr = "off"; break;
        case 1: autobootModeStr = "3ds"; break;
        case 2: autobootModeStr = "dsi"; break;
    }

    switch (MULTICONFIG(FORCEAUDIOOUTPUT)) {
        default: case 0: forceAudioOutputStr = "off"; break;
        case 1: forceAudioOutputStr = "headphones"; break;
        case 2: forceAudioOutputStr = "speakers"; break;
    }

    if (VERSION_BUILD != 0) {
        sprintf(lumaVerStr, "Polari3DS v%d.%d.%d", (int)VERSION_MAJOR, (int)VERSION_MINOR, (int)VERSION_BUILD);
    } else {
        sprintf(lumaVerStr, "Polari3DS v%d.%d", (int)VERSION_MAJOR, (int)VERSION_MINOR);
    }

    if (ISRELEASE) {
        strcpy(lumaRevSuffixStr, "");
    } else {
        sprintf(lumaRevSuffixStr, "-%08lx", (u32)COMMIT_HASH);
    }

    menuComboToString(rosalinaMenuComboStr, cfg->rosalinaMenuCombo);

    static const int pinOptionToDigits[] = { 0, 4, 6, 8 };
    int pinNumDigits = pinOptionToDigits[MULTICONFIG(PIN)];

    char topScreenFilterGammaStr[32];
    char topScreenFilterContrastStr[32];
    char topScreenFilterBrightnessStr[32];
    encodedFloatToString(topScreenFilterGammaStr, cfg->topScreenFilter.gammaEnc);
    encodedFloatToString(topScreenFilterContrastStr, cfg->topScreenFilter.contrastEnc);
    encodedFloatToString(topScreenFilterBrightnessStr, cfg->topScreenFilter.brightnessEnc);

    char bottomScreenFilterGammaStr[32];
    char bottomScreenFilterContrastStr[32];
    char bottomScreenFilterBrightnessStr[32];
    encodedFloatToString(bottomScreenFilterGammaStr, cfg->bottomScreenFilter.gammaEnc);
    encodedFloatToString(bottomScreenFilterContrastStr, cfg->bottomScreenFilter.contrastEnc);
    encodedFloatToString(bottomScreenFilterBrightnessStr, cfg->bottomScreenFilter.brightnessEnc);

    int n = sprintf(
        out, (const char *)config_template_ini,
        lumaVerStr, lumaRevSuffixStr,

        (int)CONFIG_VERSIONMAJOR, (int)CONFIG_VERSIONMINOR,
        (int)CONFIG(AUTOBOOTEMU), (int)CONFIG(LOADEXTFIRMSANDMODULES),
        (int)CONFIG(PATCHGAMES), (int)CONFIG(REDIRECTAPPTHREADS),
        (int)CONFIG(PATCHVERSTRING), (int)CONFIG(SHOWGBABOOT),
        (int)CONFIG(PERFORMANCEMODE), (int)CONFIG(ALLOWUPDOWNLEFTRIGHTDSI),
        (int)CONFIG(CUTWIFISLEEP), (int)CONFIG(PATCHUNITINFO),
        (int)CONFIG(DISABLEARM11EXCHANDLERS), (int)CONFIG(ENABLESAFEFIRMROSALINA),
        (int)CONFIG(NOERRDISPINSTANTREBOOT),
        
        1 + (int)MULTICONFIG(DEFAULTEMU), 4 - (int)MULTICONFIG(BRIGHTNESS),
        splashPosStr, (unsigned int)cfg->splashDurationMsec,
        pinNumDigits, n3dsCpuStr,
        autobootModeStr, forceAudioOutputStr,

        cfg->hbldr3dsxTitleId, rosalinaMenuComboStr, (int)(cfg->pluginLoaderFlags & 1),
        (int)cfg->ntpTzOffetMinutes,

        (int)cfg->topScreenFilter.cct, (int)cfg->bottomScreenFilter.cct,
        topScreenFilterGammaStr, bottomScreenFilterGammaStr,
        topScreenFilterContrastStr, bottomScreenFilterContrastStr,
        topScreenFilterBrightnessStr, bottomScreenFilterBrightnessStr,
        (int)cfg->topScreenFilter.invert, (int)cfg->bottomScreenFilter.invert,

        cfg->autobootTwlTitleId, (int)cfg->autobootCtrAppmemtype,
        
        cfg->volumeSliderOverride,
        
        (int)CONFIG(SHOWADVANCEDSETTINGS),
        (int)CONFIG(HARDWAREPATCHING)
    );

    return n < 0 ? 0 : (size_t)n;
}

static char tmpIniBuffer[0x2000];

static bool readLumaIniConfig(void)
{
    u32 rd = fileRead(tmpIniBuffer, "lumae.ini", sizeof(tmpIniBuffer) - 1);
    if (rd == 0) return false;

    tmpIniBuffer[rd] = '\0';

    return ini_parse_string(tmpIniBuffer, &configIniHandler, &configData) >= 0 && !hasIniParseError;
}

static bool writeLumaIniConfig(void)
{
    size_t n = saveLumaIniConfigToStr(tmpIniBuffer);
    return n != 0 && fileWrite(tmpIniBuffer, "lumae.ini", n);
}

// ===========================================================

static void writeConfigMcu(void)
{
    u8 data[sizeof(CfgDataMcu)];

    // Set Luma version
    configDataMcu.lumaVersion = MAKE_LUMA_VERSION_MCU(VERSION_MAJOR, VERSION_MINOR, VERSION_BUILD);

    // Set bootconfig from CfgData
    configDataMcu.bootCfg = configData.bootConfig;

    memcpy(data, &configDataMcu, sizeof(CfgDataMcu));

    // Fix checksum
    u8 checksum = 0;
    for (u32 i = 0; i < sizeof(CfgDataMcu) - 1; i++)
        checksum += data[i];
    checksum = ~checksum;
    data[sizeof(CfgDataMcu) - 1] = checksum;
    configDataMcu.checksum = checksum;

    I2C_writeReg(I2C_DEV_MCU, 0x60, 200 - sizeof(CfgDataMcu));
    I2C_writeRegBuf(I2C_DEV_MCU, 0x61, data, sizeof(CfgDataMcu));
}

static bool readConfigMcu(void)
{
    u8 data[sizeof(CfgDataMcu)];
    u16 curVer = MAKE_LUMA_VERSION_MCU(VERSION_MAJOR, VERSION_MINOR, VERSION_BUILD);

    // Select free reg id, then access the data regs
    I2C_writeReg(I2C_DEV_MCU, 0x60, 200 - sizeof(CfgDataMcu));
    I2C_readRegBuf(I2C_DEV_MCU, 0x61, data, sizeof(CfgDataMcu));
    memcpy(&configDataMcu, data, sizeof(CfgDataMcu));

    u8 checksum = 0;
    for (u32 i = 0; i < sizeof(CfgDataMcu) - 1; i++)
        checksum += data[i];
    checksum = ~checksum;

    if (checksum != configDataMcu.checksum || configDataMcu.lumaVersion > curVer)
    {
        // Invalid data stored in MCU...
        memset(&configDataMcu, 0, sizeof(CfgDataMcu));
        configData.bootConfig = 0;
        // Perform upgrade process (ignoring failures)
        doLumaUpgradeProcess();
        writeConfigMcu();

        return false;
    }

    if (configDataMcu.lumaVersion < curVer)
    {
        // Perform upgrade process (ignoring failures)
        doLumaUpgradeProcess();
        writeConfigMcu();
    }

    return true;
}

bool readConfig(void)
{
    bool retMcu, ret;

    retMcu = readConfigMcu();
    ret = readLumaIniConfig();
    if(!retMcu || !ret ||
       configData.formatVersionMajor != CONFIG_VERSIONMAJOR ||
       configData.formatVersionMinor != CONFIG_VERSIONMINOR)
    {
        memset(&configData, 0, sizeof(CfgData));
        configData.formatVersionMajor = CONFIG_VERSIONMAJOR;
        configData.formatVersionMinor = CONFIG_VERSIONMINOR;
        configData.config |= 1u << PATCHVERSTRING;
        configData.splashDurationMsec = 2000;
        configData.volumeSliderOverride = -1;
        configData.hbldr3dsxTitleId = HBLDR_DEFAULT_3DSX_TID;
#ifdef BUILD_FOR_GDB      
        configData.rosalinaMenuCombo = 1u << 9 | 1u << 6; // L+Up
#else
        configData.rosalinaMenuCombo = 1u << 9 | 1u << 7 | 1u << 2; // L+Up+Select
#endif
        configData.topScreenFilter.cct = 6500; // default temp, no-op
        configData.topScreenFilter.gammaEnc = 1 * FLOAT_CONV_MULT; // 1.0f
        configData.topScreenFilter.contrastEnc = 1 * FLOAT_CONV_MULT; // 1.0f
        configData.bottomScreenFilter = configData.topScreenFilter;
        configData.autobootTwlTitleId = AUTOBOOT_DEFAULT_TWL_TID;
        ret = false;
    }
    else
        ret = true;

    configData.bootConfig = configDataMcu.bootCfg;
    oldConfig = configData;

    return ret;
}

void writeConfig(bool isConfigOptions)
{
    bool updateMcu, updateIni;

    if (needConfig == CREATE_CONFIGURATION)
    {
        updateMcu = !isConfigOptions; // We've already committed it once (if it wasn't initialized)
        updateIni = isConfigOptions;
        needConfig = MODIFY_CONFIGURATION;
    }
    else
    {
        updateMcu = !isConfigOptions && configData.bootConfig != oldConfig.bootConfig;
        updateIni = isConfigOptions && (configData.config != oldConfig.config || configData.multiConfig != oldConfig.multiConfig);
    }

    if (updateMcu)
        writeConfigMcu();

    if(updateIni && !writeLumaIniConfig())
        error("Error writing the configuration file");
}

void configMenu(bool oldPinStatus, u32 oldPinMode)
{
    static const char *multiOptionsText[]  = { "Default EmuNAND: 1( ) 2( ) 3( ) 4( )",
                                               "Screen brightness: 4( ) 3( ) 2( ) 1( )",
                                               "Splash: Off( ) Before( ) After( ) payloads",
                                               "PIN lock: Off( ) 4( ) 6( ) 8( ) digits",
                                               "New 3DS CPU: Off( ) Clock( ) L2( ) Clock+L2( )",
                                               "Hbmenu autoboot: Off( ) 3DS( ) DSi( )",
                                               "Force audio: Off( ) Headphones( ) Speakers( )"
                                             };

    static const char *singleOptionsText[] = { "( ) Autoboot EmuNAND",
                                               "( ) Enable loading external FIRMs and modules",
                                               "( ) Enable game patching",
                                               "( ) Redirect app. syscore threads to core2",
                                               "( ) Show NAND or user string in System Settings",
                                               "( ) Show GBA boot screen in patched AGB_FIRM",
                                               "( ) Patch scheduler cpu to perf mode",
                                               "( ) Allow Left+Right / Up+Down combos for DSi",
                                               "( ) Cut 3DS Wifi in sleep mode",
                                               "( ) Set developer UNITINFO",
                                               "( ) Disable Arm11 exception handlers",                                               
                                               "( ) Enable Rosalina on SAFE_FIRM",
                                               "( ) Enable instant reboot + disable Errdisp",
                                               "( ) Show Advanced Settings",
                                               "( ) Enable Nand Cid and Otp hardware patching",
                                                                                              
                                               // Should always be the last 2 entries
                                               "\nBoot chainloader",
                                               "\nSave and exit"
                                             };

    static const char *optionsDescription[]  = { "Select the default EmuNAND.\n\n"
                                                 "It will be booted when no directional\n"
                                                 "pad buttons are pressed (Up/Right/Down\n"
                                                 "/Left equal EmuNANDs 1/2/3/4).",

                                                 "Select the screen brightness.",

                                                 "Enable splash screen support.\n\n"
                                                 "\t* 'Before payloads' displays it\n"
                                                 "before booting payloads\n"
                                                 "(intended for splashes that display\n"
                                                 "button hints).\n\n"
                                                 "\t* 'After payloads' displays it\n"
                                                 "afterwards.\n\n"
                                                 "Edit the duration in lumae.ini (3s\n"
                                                 "default).",

                                                 "Activate a PIN lock.\n\n"
                                                 "The PIN will be asked each time\n"
                                                 "Luma3DS boots.\n\n"
                                                 "4, 6 or 8 digits can be selected.\n\n"
                                                 "The ABXY buttons and the directional\n"
                                                 "pad buttons can be used as keys.\n\n"
                                                 "A message can also be displayed\n"
                                                 "(refer to the wiki for instructions).",

                                                 "Select the New 3DS CPU mode.\n\n"
                                                 "This won't apply to\n"
                                                 "New 3DS exclusive/enhanced games.\n\n"
                                                 "'Clock+L2' can cause issues with some\n"
                                                 "games.",

                                                 "Enable autobooting into homebrew menu,\n"
                                                 "either into 3DS or DSi mode.\n\n"
                                                 "Autobooting into a gamecard title is\n"
                                                 "not supported.\n\n"
                                                 "Refer to the \"autoboot\" section in the\n"
                                                 "configuration file to configure\n"
                                                 "this feature.",
                                                 
                                                 "Force audio output to HPs or speakers.\n\n"
                                                 "Currently only for NATIVE_FIRM.\n\n"
                                                 "Due to software limitations, this gets\n"
                                                 "undone if you actually insert then\n"
                                                 "remove HPs (just enter then exit sleep\n"
                                                 "mode if this happens).\n\n"
                                                 "Also gets bypassed for camera shutter\n"
                                                 "sound.",



                                                 "If enabled, an EmuNAND\n"
                                                 "will be launched on boot.\n\n"
                                                 "Otherwise, SysNAND will.\n\n"
                                                 "Hold L on boot to switch NAND.\n\n"
                                                 "To use a different EmuNAND from the\n"
                                                 "default, hold a directional pad button\n"
                                                 "(Up/Right/Down/Left equal EmuNANDs\n"
                                                 "1/2/3/4).",

                                                 "Enable loading external FIRMs and\n"
                                                 "system modules.\n\n"
                                                 "This isn't needed in most cases.\n\n"
                                                 "Refer to the wiki for instructions.",

                                                 "Enable overriding the region and\n"
                                                 "language configuration and the usage\n"
                                                 "of patched code binaries, exHeaders,\n"
                                                 "IPS code patches and LayeredFS\n"
                                                 "for specific games.\n\n"
                                                 "Also makes certain DLCs for out-of-\n"
                                                 "region games work.\n\n"
                                                 "Refer to the wiki for instructions.",

                                                 "Redirect app. threads that would spawn\n"
                                                 "on core1, to core2 (which is an extra\n"
                                                 "CPU core for applications that usually\n"
                                                 "remains unused).\n\n"
                                                 "This improves the performance of very\n"
                                                 "demanding games (like Pok\x82mon US/UM)\n" // CP437
                                                 "by about 10%. Can break some games\n"
                                                 "and other applications.\n",

                                                 "Enable showing the current NAND:\n\n"
                                                 "\t* Sys  = SysNAND\n"
                                                 "\t* Emu  = EmuNAND 1\n"
                                                 "\t* EmuX = EmuNAND X\n\n"
                                                 "or a user-defined custom string in\n"
                                                 "System Settings.\n\n"
                                                 "Refer to the wiki for instructions.",

                                                 "Enable showing the GBA boot screen\n"
                                                 "when booting GBA games.",

                                                 "Enable patching the default scheduler\n"
                                                 "for cpu arm11 used in 3DS software by\n"
                                                 "its contents, like games, apps, ecc.\n\n"
                                                 "It may happen deadlock if some apps,\n"
                                                 "take to many resources, so use this\n"
                                                 "when needed not all times, like:\n"
                                                 "in games where latency is essential,\n"
                                                 "or apps that perform heavy tasks.",

                                                 "Allow Left+Right and Up+Down button\n"
                                                 "combos (using DPAD and CPAD\n"
                                                 "simultaneously) in DS(i) software.\n\n"
                                                 "Commercial software filter these\n"
                                                 "combos on their own too, though.\n\n"
                                                 "Enable replacing the default upscaling\n"
                                                 "filter used for DS(i) software by the\n"
                                                 "contents of:\n\n"
                                                 "/luma/twl_upscaling_filter.bin\n\n"
                                                 "Refer to the wiki for further details.",
                                                 
                                                 "Cut the 3DS wifi in sleep mode.\n\n"
                                                 "Useful to save battery but prevent\n"
                                                 "some features like streetpass or\n"
                                                 "spotpass to work on sleep mode.\n\n"
                                                 "Use this if you don't use them\n"
                                                 "want to save battery in sleep mode.",
                                                 
                                                 "Make the console be always detected\n"
                                                 "as a development unit, and conversely.\n"
                                                 "(which breaks online features, amiibo\n"
                                                 "and retail CIAs, but allows installing\n"
                                                 "and booting some developer software).\n\n"
                                                 "Only select this if you know what you\n"
                                                 "are doing!",
                                                 
                                                 "Disables the fatal error exception\n"
                                                 "handlers for the Arm11 CPU.\n\n"
                                                 "Note: Disabling the exception handlers\n"
                                                 "will disqualify you from submitting\n"
                                                 "issues or bug reports to the Luma3DS\n"
                                                 "GitHub repository!",

                                                 "Enables Rosalina, the kernel ext.\n"
                                                 "and sysmodule reimplementations on\n"
                                                 "SAFE_FIRM (New 3DS only).\n\n"
                                                 "Also suppresses QTM error 0xF96183FE,\n"
                                                 "allowing to use 8.1-11.3 N3DS on\n"
                                                 "New 2DS XL consoles.\n\n"
                                                 "Only select this if you know what you\n"
                                                 "are doing!",
                                                 
                                                 "Disable rebooting after an Errdisp\n"
                                                 "error occurs. It also enable instant\n"
                                                 "reboot combo, this can corrupt your\n"
                                                 "SDcard so be careful with this.\n"
                                                 "The combo is A + B + X + Y + Start.\n\n"
                                                 "Only select this if you know what you\n"
                                                 "are doing!\n\n"
                                                 "Also added hardware error bypass and\n"
                                                 "bypass broken nvram.",
                                                 
                                                 "Disabling this will hide extra\n"
                                                 "settings from the luma configuration\n"
                                                 "menu.",
                                                 
                                                 "Enabling this will cause the complete\n"
                                                 "of the otp and nand cid, so that you\n"
                                                 "can use another console nand backup\n"
                                                 "on another hardware, so use this carefuly\n"
                                                 "are doing and bla bla you already know.\n\n"
                                                 "Remember to put nand_cid.bin and otp.bin\n"
                                                 "to sd luma directory cause well is where\n"
                                                 "it reads them and works only on sd card,\n"
                                                 "cause the nand is still encrypted.",
                                                 
                                                 // Should always be the last 2 entries
                                                 "Boot to the Luma3DS chainloader menu.",
                                                 
                                                 "Save the changes and exit. To discard\n"
                                                 "any changes press the POWER button.\n"
                                                 "Use START as a shortcut to this entry."
                                               };

    FirmwareSource nandType = FIRMWARE_SYSNAND;
    if(isSdMode)
    {
        // Check if there is at least one emuNAND
        u32 emuIndex = 0;
        nandType = FIRMWARE_EMUNAND;
        locateEmuNand(&nandType, &emuIndex, false);
    }

    struct multiOption {
        u32 posXs[4];
        u32 posY;
        u32 enabled;
        bool visible;
    } multiOptions[] = {
        { .visible = nandType == FIRMWARE_EMUNAND },
        { .visible = true },
        { .visible = true },
        { .visible = true },
        { .visible = ISN3DS },
        { .visible = true },
        { .visible = true }, // audio rerouting, hidden not anymore 
    };

    struct singleOption {
        u32 posY;
        bool enabled;
        bool visible;
    } singleOptions[] = {
        { .visible = nandType == FIRMWARE_EMUNAND },
        { .visible = true },
        { .visible = true },
        { .visible = ISN3DS },
        { .visible = true },
        { .visible = true },
        { .visible = true },
        { .visible = true },
        { .visible = true },
        { .visible = CONFIG(SHOWADVANCEDSETTINGS) },
        { .visible = CONFIG(SHOWADVANCEDSETTINGS) },
        { .visible = ((CONFIG(SHOWADVANCEDSETTINGS)) && (ISN3DS))},
        { .visible = CONFIG(SHOWADVANCEDSETTINGS) },
        { .visible = false },
        { .visible = false },
        { .visible = true },
        { .visible = true },
    };

    //Calculate the amount of the various kinds of options and pre-select the first single one
    u32 multiOptionsAmount = sizeof(multiOptions) / sizeof(struct multiOption),
        singleOptionsAmount = sizeof(singleOptions) / sizeof(struct singleOption),
        totalIndexes = multiOptionsAmount + singleOptionsAmount - 1,
        selectedOption = 0,
        singleSelected = 0;
    bool isMultiOption = false;

    //Parse the existing options
    for(u32 i = 0; i < multiOptionsAmount; i++)
    {
        //Detect the positions where the "x" should go
        u32 optionNum = 0;
        for(u32 j = 0; optionNum < 4 && j < strlen(multiOptionsText[i]); j++)
            if(multiOptionsText[i][j] == '(') multiOptions[i].posXs[optionNum++] = j + 1;
        while(optionNum < 4) multiOptions[i].posXs[optionNum++] = 0;

        multiOptions[i].enabled = MULTICONFIG(i);
    }
    for(u32 i = 0; i < singleOptionsAmount; i++)
        singleOptions[i].enabled = CONFIG(i);

    initScreens();

    static const char *bootTypes[] = { "B9S",
                                       "B9S (ntrboot)",
                                       "FIRM0",
                                       "FIRM1" };

    drawString(true, 10, 10, COLOR_TITLE, CONFIG_TITLE);
    drawString(true, 10, 10 + SPACING_Y, COLOR_TITLE, "Use the DPAD and A to change settings");
    drawFormattedString(false, 10, SCREEN_HEIGHT - 2 * SPACING_Y, COLOR_YELLOW, "Booted from %s via %s", isSdMode ? "SD" : "CTRNAND", bootTypes[(u32)bootType]);

    //Character to display a selected option
    char selected = 'x';

    u32 endPos = 10 + 2 * SPACING_Y;

    //Display all the multiple choice options in white
    for(u32 i = 0; i < multiOptionsAmount; i++)
    {
        if(!multiOptions[i].visible) continue;

        multiOptions[i].posY = endPos + SPACING_Y;
        endPos = drawString(true, 10, multiOptions[i].posY, COLOR_WHITE, multiOptionsText[i]);
        drawCharacter(true, 10 + multiOptions[i].posXs[multiOptions[i].enabled] * SPACING_X, multiOptions[i].posY, COLOR_WHITE, selected);
    }

    endPos += SPACING_Y / 2;

    //Display all the normal options in white except for the first one
    for(u32 i = 0, color = COLOR_GREEN; i < singleOptionsAmount; i++)
    {
        if(!singleOptions[i].visible) continue;

        singleOptions[i].posY = endPos + SPACING_Y;
        endPos = drawString(true, 10, singleOptions[i].posY, color, singleOptionsText[i]);
        if(singleOptions[i].enabled && singleOptionsText[i][0] == '(') drawCharacter(true, 10 + SPACING_X, singleOptions[i].posY, color, selected);

        if(color == COLOR_GREEN)
        {
            singleSelected = i;
            selectedOption = i + multiOptionsAmount;
            color = COLOR_WHITE;
        }
    }

    drawString(false, 10, 10, COLOR_WHITE, optionsDescription[selectedOption]);

    bool startPressed = false;
    //Boring configuration menu
    while(true)
    {
        u32 pressed = 0;
        if (!startPressed)
        do
        {
            pressed = waitInput(true) & MENU_BUTTONS;
        }
        while(!pressed);

        // Force the selection of "save and exit" and trigger it.
        if(pressed & BUTTON_START)
        {
            startPressed = true;
            // This moves the cursor to the last entry
            pressed = BUTTON_RIGHT;
        }

        if(pressed & DPAD_BUTTONS)
        {
            //Remember the previously selected option
            u32 oldSelectedOption = selectedOption;

            while(true)
            {
                switch(pressed & DPAD_BUTTONS)
                {
                    case BUTTON_UP:
                        selectedOption = !selectedOption ? totalIndexes : selectedOption - 1;
                        break;
                    case BUTTON_DOWN:
                        selectedOption = selectedOption == totalIndexes ? 0 : selectedOption + 1;
                        break;
                    case BUTTON_LEFT:
                        pressed = BUTTON_DOWN;
                        selectedOption = 0;
                        break;
                    case BUTTON_RIGHT:
                        pressed = BUTTON_UP;
                        selectedOption = totalIndexes;
                        break;
                    default:
                        break;
                }

                if(selectedOption < multiOptionsAmount)
                {
                    if(!multiOptions[selectedOption].visible) continue;

                    isMultiOption = true;
                    break;
                }
                else
                {
                    singleSelected = selectedOption - multiOptionsAmount;

                    if(!singleOptions[singleSelected].visible) continue;

                    isMultiOption = false;
                    break;
                }
            }

            if(selectedOption == oldSelectedOption && !startPressed) continue;

            //The user moved to a different option, print the old option in white and the new one in red. Only print 'x's if necessary
            if(oldSelectedOption < multiOptionsAmount)
            {
                drawString(true, 10, multiOptions[oldSelectedOption].posY, COLOR_WHITE, multiOptionsText[oldSelectedOption]);
                drawCharacter(true, 10 + multiOptions[oldSelectedOption].posXs[multiOptions[oldSelectedOption].enabled] * SPACING_X, multiOptions[oldSelectedOption].posY, COLOR_WHITE, selected);
            }
            else
            {
                u32 singleOldSelected = oldSelectedOption - multiOptionsAmount;
                drawString(true, 10, singleOptions[singleOldSelected].posY, COLOR_WHITE, singleOptionsText[singleOldSelected]);
                if(singleOptions[singleOldSelected].enabled) drawCharacter(true, 10 + SPACING_X, singleOptions[singleOldSelected].posY, COLOR_WHITE, selected);
            }

            if(isMultiOption) drawString(true, 10, multiOptions[selectedOption].posY, COLOR_GREEN, multiOptionsText[selectedOption]);
            else drawString(true, 10, singleOptions[singleSelected].posY, COLOR_GREEN, singleOptionsText[singleSelected]);

            drawString(false, 10, 10, COLOR_BLACK, optionsDescription[oldSelectedOption]);
            drawString(false, 10, 10, COLOR_WHITE, optionsDescription[selectedOption]);
        }
        else if (pressed & BUTTON_A || startPressed)
        {
            //The selected option's status changed, print the 'x's accordingly
            if(isMultiOption)
            {
                u32 oldEnabled = multiOptions[selectedOption].enabled;
                drawCharacter(true, 10 + multiOptions[selectedOption].posXs[oldEnabled] * SPACING_X, multiOptions[selectedOption].posY, COLOR_BLACK, selected);
                multiOptions[selectedOption].enabled = (oldEnabled == 3 || !multiOptions[selectedOption].posXs[oldEnabled + 1]) ? 0 : oldEnabled + 1;

                if(selectedOption == BRIGHTNESS) updateBrightness(multiOptions[BRIGHTNESS].enabled);
            }
            else
            {
                // Save and exit was selected.
                if (singleSelected == singleOptionsAmount - 1)
                {
                    drawString(true, 10, singleOptions[singleSelected].posY, COLOR_YELLOW, singleOptionsText[singleSelected]);
                    startPressed = false;
                    break;
                }
                else if (singleSelected == singleOptionsAmount - 2) {
                    loadHomebrewFirm(0);
                    break;
                }
                else
                {
                    bool oldEnabled = singleOptions[singleSelected].enabled;
                    singleOptions[singleSelected].enabled = !oldEnabled;
                    if(oldEnabled) drawCharacter(true, 10 + SPACING_X, singleOptions[singleSelected].posY, COLOR_BLACK, selected);
                }
            }
        }

        //In any case, if the current option is enabled (or a multiple choice option is selected) we must display a red 'x'
        if(isMultiOption) drawCharacter(true, 10 + multiOptions[selectedOption].posXs[multiOptions[selectedOption].enabled] * SPACING_X, multiOptions[selectedOption].posY, COLOR_GREEN, selected);
        else if(singleOptions[singleSelected].enabled && singleOptionsText[singleSelected][0] == '(') drawCharacter(true, 10 + SPACING_X, singleOptions[singleSelected].posY, COLOR_GREEN, selected);
    }

    //Parse and write the new configuration
    configData.multiConfig = 0;
    for(u32 i = 0; i < multiOptionsAmount; i++)
        configData.multiConfig |= multiOptions[i].enabled << (i * 2);

    configData.config = 0;
    for(u32 i = 0; i < singleOptionsAmount; i++)
        configData.config |= (singleOptions[i].enabled ? 1 : 0) << i;

    writeConfig(true);

    u32 newPinMode = MULTICONFIG(PIN);

    if(newPinMode != 0) newPin(oldPinStatus && newPinMode == oldPinMode, newPinMode);
    else if(oldPinStatus)
    {
        if(!fileDelete(PIN_FILE))
            error("Unable to delete PIN file");
    }

    while(HID_PAD & PIN_BUTTONS);
    wait(2000ULL);
}
