#include "itcm.h"
#include "fs.h"
#include "utils.h"
#include "memory.h"
#include "config.h"

/* Patches the ITCM with the OTP provided,
 * functionally bypassing error 022-2812.
 */
void patchITCM(void)
{
    Otp otp;    
    u32 otpSize = fileRead(&otp, OTP_PATH, sizeof(Otp));

    // Error checking
    if (otpSize != sizeof(Otp))
      {
         error("OTP is not the correct size.");
      }
    
      else
      
      {
    
      if (otp.magic != OTP_MAGIC)
         {
            error("Unable to parse OTP. Is it decrypted properly?");
         }
         
         else
         
         {
           
           if (CONFIG(HARDWAREPATCHING))
            {
                // Setting relevant values in memory to struct parsed from file
                ARM9_ITCM->otp.magic = otp.magic;
                ARM9_ITCM->otp.deviceId = otp.deviceId;
                ARM9_ITCM->otp.ctcertFlags = otp.ctcertFlags;
                ARM9_ITCM->otp.ctcertIssuer = otp.ctcertIssuer;
                ARM9_ITCM->otp.timestampYear = otp.timestampYear;
                ARM9_ITCM->otp.timestampMonth = otp.timestampMonth;
                ARM9_ITCM->otp.timestampDay = otp.timestampDay;
                ARM9_ITCM->otp.timestampHour = otp.timestampHour;
                ARM9_ITCM->otp.timestampMinute = otp.timestampMinute;
                ARM9_ITCM->otp.timestampSecond = otp.timestampSecond;
                ARM9_ITCM->otp.ctcertExponent = otp.ctcertExponent;
                memcpy(ARM9_ITCM->otp.fallbackKeyY, otp.fallbackKeyY, sizeof(otp.fallbackKeyY));   
                memcpy(ARM9_ITCM->otp.ctcertPrivK, otp.ctcertPrivK, sizeof(otp.ctcertPrivK));
                memcpy(ARM9_ITCM->otp.ctcertSignature, otp.ctcertSignature, sizeof(otp.ctcertSignature));
                memcpy(ARM9_ITCM->otp.zero, otp.zero, sizeof(otp.zero));
                memcpy(ARM9_ITCM->otp.random, otp.random, sizeof(otp.random));
                memcpy(ARM9_ITCM->otp.hash, otp.hash, sizeof(otp.hash));
            }
            
            else
            
            {
                // Setting relevant values in memory to struct parsed from file
                ARM9_ITCM->otp.deviceId = otp.deviceId;
                ARM9_ITCM->otp.timestampYear = otp.timestampYear;
                ARM9_ITCM->otp.timestampMonth = otp.timestampMonth;
                ARM9_ITCM->otp.timestampDay = otp.timestampDay;
                ARM9_ITCM->otp.timestampHour = otp.timestampHour;
                ARM9_ITCM->otp.timestampMinute = otp.timestampMinute;
                ARM9_ITCM->otp.timestampSecond = otp.timestampSecond;
                ARM9_ITCM->otp.ctcertExponent = otp.ctcertExponent;
                memcpy(ARM9_ITCM->otp.ctcertPrivK, otp.ctcertPrivK, sizeof(otp.ctcertPrivK));
                memcpy(ARM9_ITCM->otp.ctcertSignature, otp.ctcertSignature, sizeof(otp.ctcertSignature));
            }
         }
      }
}

void PatchITCMCid(void)
{
    NandInfo nandinfo;   
    u32 cidSize = fileRead((&nandinfo.nandCid), CID_PATH, sizeof(nandinfo.nandCid));
    
    // Error checking
    if (cidSize != sizeof(nandinfo.nandCid))
    {
        error("NandCid is not the correct size.");
    }
    
    else
    
    {
        memcpy(ARM9_ITCM->nandinfo.nandCid, nandinfo.nandCid, sizeof(nandinfo.nandCid));
    }
}
