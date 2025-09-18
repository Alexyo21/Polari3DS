#include <3ds.h>
#include "draw.h"
#include "menus.h"
#include "volume.h"

s8 currVolumeSliderOverride = -1;

float Volume_ExtractVolume(int nul, int one, int slider)
{
    if(slider <= nul || one < nul)
        return 0;
    
    if(one == nul) //hardware returns 0 on divbyzero
        return (slider > one) ? 1.0F : 0;
    
    float ret = (float)(slider - nul) / (float)(one - nul);
    if((ret + (1 / 256.0F)) < 1.0F)
        return ret;
    else
        return 1.0F;
}

void Volume_AdjustVolume(u8* out, int slider, float value)
{
    int smin = 0xFF;
    int smax = 0x00;
    
    if(slider)
    {
        float diff = 1.0F;
        
        int min;
        int max;
        for(min = 0; min != slider; min++)
        for(max = slider; max != 0x100; max++)
        {
            float rdiff = value - Volume_ExtractVolume(min, max, slider);
            
            if(rdiff < 0 || rdiff > diff)
                continue;
            
            diff = rdiff;
            smin = min;
            smax = max;
            
            if(rdiff < (1 / 256.0F))
                break;
        }
    }
    
    out[0] = smin & 0xFF;
    out[1] = smax & 0xFF;
}

static Result ApplyVolumeOverride(void)
{
    // This feature repurposes the functionality used for the camera shutter sound.
    // As such, it interferes with it:
    //     - shutter volume is set to the override instead of its default 100% value
    //     - due to implementation details, having the shutter sound effect play will
    //       make this feature stop working until the volume override is reapplied by
    //       going back to this menu

    // Original credit: profi200

    u8 i2s1Mux;
    u8 i2s2Mux;
    Result res = cdcChkInit();

    if (R_SUCCEEDED(res)) res = CDCCHK_ReadRegisters2(0,  116, &i2s1Mux, 1); // used for shutter sound in TWL mode, and all GBA/DSi/3DS application
    if (R_SUCCEEDED(res)) res = CDCCHK_ReadRegisters2(100, 49, &i2s2Mux, 1); // used for shutter sound in CTR mode and CTR mode library applets

    if (currVolumeSliderOverride >= 0)
    {
        i2s1Mux &= ~0x80;
        i2s2Mux |=  0x20;
    }
    else
    {
        i2s1Mux |=  0x80;
        i2s2Mux &= ~0x20;
    }

    s8 i2s1Volume;
    s8 i2s2Volume;
    if (currVolumeSliderOverride >= 0)
    {
        i2s1Volume = -128 + (((float)currVolumeSliderOverride/100.f) * 108);
        i2s2Volume = i2s1Volume;
    }
    else
    {
        // Restore shutter sound volumes. This sould be sourced from cfg,
        // however the values are the same everwhere
        i2s1Volume =  -3; // -1.5 dB (115.7%, only used by TWL applications when taking photos)
        i2s2Volume = -20; // -10 dB  (100%)
    }

    // Write volume overrides values before writing to the pinmux registers
    if (R_SUCCEEDED(res)) res = CDCCHK_WriteRegisters2(0, 65, &i2s1Volume, 1); // CDC_REG_DAC_L_VOLUME_CTRL
    if (R_SUCCEEDED(res)) res = CDCCHK_WriteRegisters2(0, 66, &i2s1Volume, 1); // CDC_REG_DAC_R_VOLUME_CTRL
    if (R_SUCCEEDED(res)) res = CDCCHK_WriteRegisters2(100, 123, &i2s2Volume, 1);

    if (R_SUCCEEDED(res)) res = CDCCHK_WriteRegisters2(0, 116, &i2s1Mux, 1);
    if (R_SUCCEEDED(res)) res = CDCCHK_WriteRegisters2(100, 49, &i2s2Mux, 1);

    cdcChkExit();
    return res;
}

void LoadConfig(void)
{
    s64 out = -1;
    svcGetSystemInfo(&out, 0x10000, 7);
    currVolumeSliderOverride = (s8)out;
    if (currVolumeSliderOverride >= 0)
        ApplyVolumeOverride();
}

void AdjustVolume(void)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();

    s8 tempVolumeOverride = currVolumeSliderOverride;
    static s8 backupVolumeOverride = -1;
    if (backupVolumeOverride == -1)
        backupVolumeOverride = tempVolumeOverride >= 0 ? tempVolumeOverride : 85;

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Menu configurazione di sistema");
        u32 posY = Draw_DrawString(10, 30, COLOR_WHITE, "Y: Attiva/Disattiva l'override del cursore del volume.\nDPAD: Aggiusta il livello del volume.\nA: Applica\nB: Torna indietro\n\n");
        Draw_DrawString(10, posY, COLOR_WHITE, "Stato attuale:");
        posY = Draw_DrawString(100, posY, (tempVolumeOverride == -1) ? COLOR_RED : COLOR_GREEN, (tempVolumeOverride == -1) ? " DISABILITATO " : " ABILITATO ");

        if (tempVolumeOverride != -1) {
            posY = Draw_DrawFormattedString(30, posY, COLOR_WHITE, "\nValore: [%d%%]", tempVolumeOverride);
        } else {
            posY = Draw_DrawString(30, posY, COLOR_WHITE, "\n                 ");
        }

        Draw_FlushFramebuffer();
        Draw_Unlock();

        u32 pressed = waitInputWithTimeout(1000);

        Draw_Lock();

        if(pressed & KEY_A)
        {
            currVolumeSliderOverride = tempVolumeOverride;
            Result res = ApplyVolumeOverride();
            LumaConfig_SaveSettings();
            if (R_SUCCEEDED(res))
                Draw_DrawString(10, posY, COLOR_GREEN, "\nSuccesso!");
            else
                Draw_DrawFormattedString(10, posY, COLOR_RED, "\nFallito: 0x%08lX", res);
        }
        else if(pressed & KEY_B)
            return;
        else if(pressed & KEY_Y)
        {
            Draw_DrawString(10, posY, COLOR_WHITE, "\n                 ");
            if (tempVolumeOverride == -1) {
                tempVolumeOverride = backupVolumeOverride;
            } else {
                backupVolumeOverride = tempVolumeOverride;
                tempVolumeOverride = -1;
            }
        }
        else if ((pressed & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT)) && tempVolumeOverride != -1)
        {
            Draw_DrawString(10, posY, COLOR_WHITE, "\n                 ");
            if (pressed & KEY_UP)
                tempVolumeOverride++;
            else if (pressed & KEY_DOWN)
                tempVolumeOverride--;
            else if (pressed & KEY_RIGHT)
                tempVolumeOverride+=10;
            else if (pressed & KEY_LEFT)
                tempVolumeOverride-=10;

            if (tempVolumeOverride < 0)
                tempVolumeOverride = 0;
            if (tempVolumeOverride > 100)
                tempVolumeOverride = 100;
        }

        Draw_FlushFramebuffer();
        Draw_Unlock();
    } while(!menuShouldExit);
}
