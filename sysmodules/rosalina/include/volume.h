#pragma once

#include <3ds/types.h>
#include "utils.h"
#include "luma_config.h"
#include "luma_shared_config.h"

extern s8 currVolumeSliderOverride;

float Volume_ExtractVolume(int nul, int one, int slider);
void Volume_AdjustVolume(u8* out, int slider, float value);
void Volume_ControlVolume(void);
void LoadConfig(void);
void AdjustVolume(void);

