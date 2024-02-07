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
    // Credit profi200
    u8 tmp;
    Result res = cdcChkInit();

    if (R_SUCCEEDED(res)) res = CDCCHK_ReadRegisters2(0, 116, &tmp, 1); // CDC_REG_VOL_MICDET_PIN_SAR_ADC
    if (currVolumeSliderOverride >= 0)
        tmp &= ~0x80;
    else
        tmp |= 0x80;
    if (R_SUCCEEDED(res)) res = CDCCHK_WriteRegisters2(0, 116, &tmp, 1);

    if (currVolumeSliderOverride >= 0) {
        s8 calculated = -128 + (((float)currVolumeSliderOverride/100.f) * 108);
        if (calculated > -20)
            res = -1; // Just in case
        s8 volumes[2] = {calculated, calculated}; // Volume in 0.5 dB steps. -128 (muted) to 48. Do not go above -20 (100%).
        if (R_SUCCEEDED(res)) res = CDCCHK_WriteRegisters2(0, 65, volumes, 2); // CDC_REG_DAC_L_VOLUME_CTRL, CDC_REG_DAC_R_VOLUME_CTRL
    }

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
        Draw_DrawString(10, 10, COLOR_TITLE, "System configuration menu");
        u32 posY = Draw_DrawString(10, 30, COLOR_WHITE, "Y: Toggle volume slider override.\nDPAD: Adjust the volume level.\nA: Apply\nB: Go back\n\n");
        Draw_DrawString(10, posY, COLOR_WHITE, "Current status:");
        posY = Draw_DrawString(100, posY, (tempVolumeOverride == -1) ? COLOR_RED : COLOR_GREEN, (tempVolumeOverride == -1) ? " DISABLED" : " ENABLED ");
        if (tempVolumeOverride != -1) {
            posY = Draw_DrawFormattedString(30, posY, COLOR_WHITE, "\nValue: [%d%%]", tempVolumeOverride);
        } else {
            posY = Draw_DrawString(30, posY, COLOR_WHITE, "\n                 ");
        }

        u32 pressed = waitInputWithTimeout(1000);

        if(pressed & KEY_A)
        {
            currVolumeSliderOverride = tempVolumeOverride;
            Result res = ApplyVolumeOverride();
            LumaConfig_SaveSettings();
            if (R_SUCCEEDED(res))
                Draw_DrawString(10, posY, COLOR_GREEN, "\nSuccess!");
            else
                Draw_DrawFormattedString(10, posY, COLOR_RED, "\nFailed: 0x%08lX", res);
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
        else if ((pressed & (KEY_DUP | KEY_DDOWN | KEY_DLEFT | KEY_DRIGHT)) && tempVolumeOverride != -1)
        {
            Draw_DrawString(10, posY, COLOR_WHITE, "\n                 ");
            if (pressed & KEY_DUP)
                tempVolumeOverride++;
            else if (pressed & KEY_DDOWN)
                tempVolumeOverride--;
            else if (pressed & KEY_DRIGHT)
                tempVolumeOverride+=10;
            else if (pressed & KEY_DLEFT)
                tempVolumeOverride-=10;

            if (tempVolumeOverride < 0)
                tempVolumeOverride = 0;
            if (tempVolumeOverride > 100)
                tempVolumeOverride = 100;
        }

    } while(!menuShouldExit);
}
