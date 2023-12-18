#pragma once

// This file contains the structure of ITCM on the ARM9, as initialized by boot9.

#include "types.h"
#include "memmap.h"
#include "fs.h"
#include "memory.h"
#include "utils.h"
#include "config.h"

#define STATIC_ASSERT(...) \
    _Static_assert((__VA_ARGS__), #__VA_ARGS__)

// Structure of the decrypted version of the OTP.  Part of this is in ITCM.
// https://www.3dbrew.org/wiki/OTP_Registers#Plaintext_OTP
typedef struct _Otp {
    // 00: Magic value OTP_MAGIC (little-endian).
    u32 magic;
    // 04: DeviceId, mostly used for TWL stuff.
    u32 deviceId;
    // 08: Fallback keyY for movable.sed.
    u8 fallbackKeyY[16];
    // 18: CtCert flags.  When >= 5, ctcertExponent is big-endian?  Always 05 in systems I've seen.
    u8 ctcertFlags;
    // 19: 00 = retail, nonzero (always 02?) = dev
    u8 ctcertIssuer;
    // 1A-1F: CtCert timestamp - can be used as timestamp of SoC.  Year is minus 1900.
    u8 timestampYear;
    u8 timestampMonth;
    u8 timestampDay;
    u8 timestampHour;
    u8 timestampMinute;
    u8 timestampSecond;
    // 20: CtCert ECDSA exponent.  Big-endian if ctcertFlags >= 5?
    u32 ctcertExponent;
    // 24: CtCert ECDSA private key, in big-endian.  First two bytes always zero?
    u8 ctcertPrivK[32];
    // 44: CtCert ECDSA signature, in big-endian.  Proves CtCert was made by Nintendo.
    u8 ctcertSignature[60];
    // 80: Zero.
    u8 zero[16];
    // 90: Random(?) data used for generation of console-specific keys.
    u8 random[0x50];
    // E0: SHA-256 hash of the rest of OTP.
    u8 hash[256 / 8];
} __attribute__((__packed__)) Otp;

// Value of Otp::magic
#define OTP_MAGIC 0xDEADB00F
// Value to add to Otp::timestampYear to get actual year.
#define OTP_YEAR_BIAS 1900

// Sanity checking.
STATIC_ASSERT(offsetof(Otp, magic) == 0x00);
STATIC_ASSERT(offsetof(Otp, deviceId) == 0x04);
STATIC_ASSERT(offsetof(Otp, fallbackKeyY) == 0x08);
STATIC_ASSERT(offsetof(Otp, ctcertFlags) == 0x18);
STATIC_ASSERT(offsetof(Otp, ctcertIssuer) == 0x19);
STATIC_ASSERT(offsetof(Otp, timestampYear) == 0x1A);
STATIC_ASSERT(offsetof(Otp, timestampMonth) == 0x1B);
STATIC_ASSERT(offsetof(Otp, timestampDay) == 0x1C);
STATIC_ASSERT(offsetof(Otp, timestampHour) == 0x1D);
STATIC_ASSERT(offsetof(Otp, timestampMinute) == 0x1E);
STATIC_ASSERT(offsetof(Otp, timestampSecond) == 0x1F);
STATIC_ASSERT(offsetof(Otp, ctcertExponent) == 0x20);
STATIC_ASSERT(offsetof(Otp, ctcertPrivK) == 0x24);
STATIC_ASSERT(offsetof(Otp, ctcertSignature) == 0x44);
STATIC_ASSERT(offsetof(Otp, zero) == 0x80);
STATIC_ASSERT(offsetof(Otp, random) == 0x90);
STATIC_ASSERT(offsetof(Otp, hash) == 0xE0);
STATIC_ASSERT(sizeof(Otp) == 256);


// Structure of an RSA private key that boot9 puts into ITCM.
// These keys were never used.
typedef struct _Arm9ItcmRsaPrivateKey {
    // 000: RSA modulus.
    u8 modulus[256];
    // 100: RSA private exponent.
    u8 privateExponent[256];
} __attribute__((__packed__)) Arm9ItcmRsaPrivateKey;

STATIC_ASSERT(sizeof(Arm9ItcmRsaPrivateKey) == 512);

typedef struct _NandInfo {
  // nand initialialization
    bool initialized;
  // General SD card flag (including SDHC/SDXC). Set when OP_COND APP CMDs succeed.  
    bool isMmc;
  // General SD card flag (including SDHC/SDXC). Set when OP_COND APP CMDs succeed. 
    bool isSd;
  // CCS bit from OCR. Set for SDHC and SDXC. 
    bool isSdhc;
  // nand cid info, In TMIO response format.  
    u32 nandCid[4];
  // In TMIO response format.  
    u32 csd[4];
  // ???
    u32 ocr;
  // ???
    u64 scr;
  // ???
    u16 rca;
  // ???
    u8 rsvd[2];
  // Last driver result/error code.
    u32 result;
  // Last R1 card status.
    u32 cardStatus;
  // something regarding sd/emmc clock??? 
    u16 sd_clk_ctrl;
  // some sd option clock maybe or readonly??? 
    u16 sd_option;
  // (SD) Set when CSD v2.0 (NOT v3.0) or (MMC) when the capacity from CSD is higher than 2 GiB.  
    bool highCapacity;
  // ??? 
    u8 rsvd2[3];
  // Capacity in sectors.
    u32 sectors;
  // Capacity in sectors of SD protected area from 512 bit SD status. 0 for (e)MMC.
    u32 sdProtSectors;
  
  // Failed init attempts?
    u32 initFails;
  // Init fail result? Same format as result?
    u32 initFailResult;
  // Failed read/write attempts?
    u32 rwFails;
  // Read/write fail result? Same format as result?  
    u32 rwFailResult;
    
  // TMIO controller number (1-based).
    u32 controller;
  // TMIO port number. 
    u32 portNum;
} __attribute__((__packed__)) NandInfo;

STATIC_ASSERT(offsetof(NandInfo, initialized) == 0x00);
STATIC_ASSERT(offsetof(NandInfo, isMmc) == 0x01);
STATIC_ASSERT(offsetof(NandInfo, isSd) == 0x02);
STATIC_ASSERT(offsetof(NandInfo, isSdhc) == 0x03);
STATIC_ASSERT(offsetof(NandInfo, nandCid) == 0x04);
STATIC_ASSERT(offsetof(NandInfo, csd) == 0x14);
STATIC_ASSERT(offsetof(NandInfo, ocr) == 0x24);
STATIC_ASSERT(offsetof(NandInfo, scr) == 0x28);
STATIC_ASSERT(offsetof(NandInfo, rca) == 0x30);
STATIC_ASSERT(offsetof(NandInfo, rsvd) == 0x32);
STATIC_ASSERT(offsetof(NandInfo, result) == 0x34);
STATIC_ASSERT(offsetof(NandInfo, cardStatus) == 0x38);
STATIC_ASSERT(offsetof(NandInfo, sd_clk_ctrl) == 0x3C);
STATIC_ASSERT(offsetof(NandInfo, sd_option) == 0x3E);
STATIC_ASSERT(offsetof(NandInfo, highCapacity) == 0x40);
STATIC_ASSERT(offsetof(NandInfo, rsvd2) == 0x41);
STATIC_ASSERT(offsetof(NandInfo, sectors) == 0x44);
STATIC_ASSERT(offsetof(NandInfo, sdProtSectors) == 0x48);
STATIC_ASSERT(offsetof(NandInfo, initFails) == 0x4C);
STATIC_ASSERT(offsetof(NandInfo, initFailResult) == 0x50);
STATIC_ASSERT(offsetof(NandInfo, rwFails) == 0x54);
STATIC_ASSERT(offsetof(NandInfo, rwFailResult) == 0x58);
STATIC_ASSERT(offsetof(NandInfo, controller) == 0x5C);
STATIC_ASSERT(offsetof(NandInfo, portNum) == 0x60);
STATIC_ASSERT(sizeof(NandInfo) == 100);

// Structure of the data in ARM9 ITCM, filled in by boot9.
// https://www.3dbrew.org/wiki/OTP_Registers#Plaintext_OTP
typedef struct _Arm9Itcm {
    // 0000: Uninitialized / available.
    u8 uninitializedBefore[0x3700];
    // 3700: Copied code from boot9 to disable boot9.
    u8 boot9DisableCode[0x100];
    // 3800: Decrypted OTP.  boot9 zeros last 0x70 bytes (Otp::random and Otp::hash).
    Otp otp;
    // 3900: Copy of NAND MBR, which includes the NCSD header.
    u8 ncsd[0x200];
    // 3B00: Decrypted FIRM header for the FIRM that was loaded by boot9.
    u8 decryptedFirmHeader[0x200];
    // 3D00: RSA modulus for keyslots 0-3 as left by boot9 (big-endian).
    u8 rsaKeyslotModulus[4][0x100];
    // 4100: RSA private keys for who knows what; nothing ever used them.
    Arm9ItcmRsaPrivateKey rsaPrivateKeys[4];
    // 4900: RSA public keys (moduli) for things Nintendo signs.
    u8 rsaModulusCartNCSD[0x100];
    u8 rsaModulusAccessDesc[0x100];
    u8 rsaModulusUnused2[0x100];
    u8 rsaModulusUnused3[0x100];
    // 4D00: Unknown keys; unused.
    u8 unknownKeys[8][16];
    // 4D80: NAND CID
    NandInfo nandinfo;
    // 4DE4: Padding after NAND CID; uninitialized.
    u8 uninitializedCid[0x1C];
    // 4E00: Unused data before TWL keys.
    u8 uninitializedMiddle[0x200];
    // 5000: RSA modulus for TWL System Menu.
    u8 rsaModulusTwlSystemMenu[0x80];
    // 5080: RSA modulus for TWL Wi-Fi firmware and DSi Sound.
    u8 rsaModulusTwlSound[0x80];
    // 5100: RSA modulus for TWL system applications.
    u8 rsaModulusTwlSystemApps[0x80];
    // 5180: RSA modulus for TWL normal applications.
    u8 rsaModulusTwlNormalApps[0x80];
    // 5200: TWL ???
    u8 twlUnknown1[0x10];
    // 5210: TWL keyY for per-console ES blocks.
    u8 twlKeyYUniqueES[0x10];
    // 5220: TWL keyY for fixed-key ES blocks.
    u8 twlKeyYFixedES[0x10];
    // 5230: TWL ???
    u8 twlUnknown2[0xD0];
    // 5300: TWL common key.
    u8 twlCommonKey[0x10];
    // 5310: TWL ???
    u8 twlUnknown3[0x40];
    // 5350: TWL normal key for keyslot 0x02.
    u8 twlKeyslot02NormalKey[0x10];
    // 5360: TWL ???
    u8 twlUnknown4[0x20];
    // 5380: TWL keyX for keyslot 0x00.
    u8 twlKeyslot00KeyX[0x10];
    // 5390: TWL ???
    u8 twlUnknown5[8];
    // 5398: TWL "Tad" crypto keyX.
    u8 twlTadKeyX[0x10];
    // 53A8: TWL middle two words of keyslot 0x03 keyX.
    u8 twlKeyslot03KeyXMiddle[8];
    // 53B0: TWL ???
    u8 twlUnknown6[12];
    // 53BC: TWL keyY for keyslot 0x01.  Truncated (last word becomes E1A00005).
    u8 twlKeyslot01KeyY[12];
    // 53C8: TWL keyY for NAND crypto on retail TWL.
    u8 twlNANDKeyY[0x10];
    // 53D8: TWL ???
    u8 twlUnknown7[8];
    // 53E0: TWL Blowfish cart key.
    u8 twlBlowfishCartKey[0x1048];
    // 6428: NTR Blowfish cart key.
    u8 ntrBlowfishCartKey[0x1048];
    // 7470: End of the line.
    u8 uninitializedEnd1[0x790];
    // 7C00: ...But wait, this buffer is used for the FIRM header during firmlaunch on >= 9.5.0.
    u8 firmlaunchHeader[0x100];
    // 7D00: Back to our regularly-scheduled emptiness.
    u8 uninitializedEnd2[0x300];
} __attribute__((__packed__)) Arm9Itcm;

// Sanity checking.
STATIC_ASSERT(offsetof(Arm9Itcm, otp) == 0x3800);
STATIC_ASSERT(offsetof(Arm9Itcm, nandinfo) == 0x4D80);
STATIC_ASSERT(offsetof(Arm9Itcm, twlNANDKeyY) == 0x53C8);
STATIC_ASSERT(offsetof(Arm9Itcm, twlBlowfishCartKey) == 0x53E0);
STATIC_ASSERT(offsetof(Arm9Itcm, ntrBlowfishCartKey) == 0x6428);
STATIC_ASSERT(sizeof(Arm9Itcm) == 0x8000);


// Macro for accessing ITCM.
#define ARM9_ITCM ((Arm9Itcm*) __ITCM_ADDR)

// Default path for the OTP is in the luma directory of SDCARD.
#define OTP_PATH "sdmc:/luma/otp.bin"

void patchITCM(void);

#define CID_PATH "sdmc:/luma/nand_cid.bin"

void PatchITCMCid(void);
