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

#include <3ds.h>
#include <stdarg.h>
#include "fmt.h"
#include "draw.h"
#include "font.h"
#include "memory.h"
#include "menu.h"
#include "utils.h"
#include "csvc.h"

#define KERNPA2VA(a)            ((a) + (GET_VERSION_MINOR(osGetKernelVersion()) < 44 ? 0xD0000000 : 0xC0000000))

static u32 gpuSavedFramebufferAddr1, gpuSavedFramebufferAddr2, gpuSavedFramebufferFormat, gpuSavedFramebufferStride, gpuSavedFillColor;
static u32 framebufferCacheSize;
static void *framebufferCache;
static RecursiveLock lock;

void Draw_Init(void)
{
    RecursiveLock_Init(&lock);
}

void Draw_Lock(void)
{
    RecursiveLock_Lock(&lock);
}

void Draw_Unlock(void)
{
    RecursiveLock_Unlock(&lock);
}

void Draw_DrawCharacter(u32 posX, u32 posY, u32 color, char character)
{
    u16 *const fb = (u16 *)FB_BOTTOM_VRAM_ADDR;

    s32 y;
    for(y = 0; y < 10; y++)
    {
        char charPos = font[character * 10 + y];

        s32 x;
        for(x = 6; x >= 1; x--)
        {
            u32 screenPos = (posX * SCREEN_BOT_HEIGHT * 2 + (SCREEN_BOT_HEIGHT - y - posY - 1) * 2) + (5 - x) * 2 * SCREEN_BOT_HEIGHT;
            u32 pixelColor = ((charPos >> x) & 1) ? color : COLOR_BLACK;
            fb[screenPos / 2] = pixelColor;
        }
    }
}


u32 Draw_DrawString(u32 posX, u32 posY, u32 color, const char *string)
{
    for(u32 i = 0, line_i = 0; i < strlen(string); i++)
        switch(string[i])
        {
            case '\n':
                posY += SPACING_Y;
                line_i = 0;
                break;

            case '\t':
                line_i += 2;
                break;

            default:
                //Make sure we never get out of the screen
                if(line_i >= ((SCREEN_BOT_WIDTH) - posX) / SPACING_X)
                {
                    posY += SPACING_Y;
                    line_i = 1; //Little offset so we know the same string continues
                    if(string[i] == ' ') break; //Spaces at the start look weird
                }

                Draw_DrawCharacter(posX + line_i * SPACING_X, posY, color, string[i]);

                line_i++;
                break;
        }

    return posY;
}

u32 Draw_DrawFormattedString(u32 posX, u32 posY, u32 color, const char *fmt, ...)
{
    char buf[DRAW_MAX_FORMATTED_STRING_SIZE + 1];
    va_list args;
    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);

    return Draw_DrawString(posX, posY, color, buf);
}

void Draw_FillFramebuffer(u32 value)
{
    memset(FB_BOTTOM_VRAM_ADDR, value, FB_BOTTOM_SIZE);
}

void Draw_ClearFramebuffer(void)
{
    Draw_FillFramebuffer(0);
}

Result Draw_AllocateFramebufferCache(u32 size)
{
    // Can't use fbs in FCRAM when Home Menu is active (AXI config related maybe?)
    u32 addr = 0x0D000000;
    u32 tmp;

    size = (size + 0xFFF) >> 12 << 12; // round-up

    if (framebufferCache != NULL)
        __builtin_trap();

    Result res = svcControlMemoryEx(&tmp, addr, 0, size, MEMOP_ALLOC, MEMREGION_SYSTEM | MEMPERM_READWRITE, true);
    if (R_FAILED(res))
    {
        framebufferCache = NULL;
        framebufferCacheSize = 0;
    }
    else
    {
        framebufferCache = (u32 *)addr;
        framebufferCacheSize = size;
    }

    return res;
}

Result Draw_AllocateFramebufferCacheForScreenshot(u32 size)
{
    u32 remaining = (u32)osGetMemRegionFree(MEMREGION_SYSTEM);
    u32 sz = remaining < size ? remaining : size;
    return Draw_AllocateFramebufferCache(sz);
}

void Draw_FreeFramebufferCache(void)
{
    u32 tmp;
    if (framebufferCache != NULL)
        svcControlMemory(&tmp, (u32)framebufferCache, 0, framebufferCacheSize, MEMOP_FREE, 0);
    framebufferCacheSize = 0;
    framebufferCache = NULL;
}

void *Draw_GetFramebufferCache(void)
{
    return framebufferCache;
}

u32 Draw_GetFramebufferCacheSize(void)
{
    return framebufferCacheSize;
}

u32 Draw_SetupFramebuffer(void)
{
    while((GPU_PSC0_CNT | GPU_PSC1_CNT | GPU_TRANSFER_CNT | GPU_CMDLIST_CNT) & 1);

    Draw_FlushFramebuffer();
    memcpy(framebufferCache, FB_BOTTOM_VRAM_ADDR, FB_BOTTOM_SIZE);
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();

    u32 format = GPU_FB_BOTTOM_FMT;

    gpuSavedFramebufferAddr1 = GPU_FB_BOTTOM_ADDR_1;
    gpuSavedFramebufferAddr2 = GPU_FB_BOTTOM_ADDR_2;
    gpuSavedFramebufferFormat = format;
    gpuSavedFramebufferStride = GPU_FB_BOTTOM_STRIDE;

    format = (format & ~7) | GSP_RGB565_OES;
    format |= 3 << 8; // set VRAM bits

    GPU_FB_BOTTOM_ADDR_1 = GPU_FB_BOTTOM_ADDR_2 = FB_BOTTOM_VRAM_PA;
    GPU_FB_BOTTOM_FMT = format;
    GPU_FB_BOTTOM_STRIDE = 240 * 2;

    gpuSavedFillColor = LCD_BOT_FILLCOLOR;
    LCD_BOT_FILLCOLOR = 0;

    return framebufferCacheSize;
}

void Draw_RestoreFramebuffer(void)
{
    memcpy(FB_BOTTOM_VRAM_ADDR, framebufferCache, FB_BOTTOM_SIZE);
    Draw_FlushFramebuffer();

    LCD_BOT_FILLCOLOR = gpuSavedFillColor;
    GPU_FB_BOTTOM_ADDR_1 = gpuSavedFramebufferAddr1;
    GPU_FB_BOTTOM_ADDR_2 = gpuSavedFramebufferAddr2;
    GPU_FB_BOTTOM_FMT = gpuSavedFramebufferFormat;
    GPU_FB_BOTTOM_STRIDE = gpuSavedFramebufferStride;
}

void Draw_FlushFramebuffer(void)
{
    svcFlushProcessDataCache(CUR_PROCESS_HANDLE, (u32)FB_BOTTOM_VRAM_ADDR, FB_BOTTOM_SIZE);
}

u32 Draw_GetCurrentFramebufferAddress(bool top, bool left)
{
    if(GPU_FB_BOTTOM_SEL & 1)
    {
        if(left)
            return top ? GPU_FB_TOP_LEFT_ADDR_2 : GPU_FB_BOTTOM_ADDR_2;
        else
            return top ? GPU_FB_TOP_RIGHT_ADDR_2 : GPU_FB_BOTTOM_ADDR_2;
    }
    else
    {
        if(left)
            return top ? GPU_FB_TOP_LEFT_ADDR_1 : GPU_FB_BOTTOM_ADDR_1;
        else
            return top ? GPU_FB_TOP_RIGHT_ADDR_1 : GPU_FB_BOTTOM_ADDR_1;
    }
}

void Draw_GetCurrentScreenInfo(u32 *width, bool *is3d, bool top)
{
    if (top)
    {
        bool isNormal2d = (GPU_FB_TOP_FMT & BIT(6)) != 0;
        *is3d = (GPU_FB_TOP_FMT & BIT(5)) != 0;
        *width = !(*is3d) && !isNormal2d ? 800 : 400;
    }
    else
    {
        *is3d = false;
        *width = 320;
    }
}

static inline void Draw_WriteUnaligned(u8 *dst, u32 tmp, u32 size)
{
    memcpy(dst, &tmp, size);
}

void Draw_CreateBitmapHeader(u8 *dst, u32 width, u32 heigth)
{
    static const u8 bmpHeaderTemplate[54] = {
        0x42, 0x4D, 0xCC, 0xCC, 0xCC, 0xCC, 0x00, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x28, 0x00,
        0x00, 0x00, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0x01, 0x00, 0x18, 0x00, 0x00, 0x00,
        0x00, 0x00, 0xCC, 0xCC, 0xCC, 0xCC, 0x12, 0x0B, 0x00, 0x00, 0x12, 0x0B, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    memcpy(dst, bmpHeaderTemplate, 54);
    Draw_WriteUnaligned(dst + 2, 54 + 3 * width * heigth, 4);
    Draw_WriteUnaligned(dst + 0x12, width, 4);
    Draw_WriteUnaligned(dst + 0x16, heigth, 4);
    Draw_WriteUnaligned(dst + 0x22, 3 * width * heigth, 4);
}

static inline void Draw_ConvertPixelToBGR8(u8 *dst, const u8 *src, GSPGPU_FramebufferFormat srcFormat)
{
    u8 red, green, blue;
    switch(srcFormat)
    {
        case GSP_RGBA8_OES:
        {
            u32 px = *(u32 *)src;
            dst[0] = (px >>  8) & 0xFF;
            dst[1] = (px >> 16) & 0xFF;
            dst[2] = (px >> 24) & 0xFF;
            break;
        }
        case GSP_BGR8_OES:
        {
            dst[2] = src[2];
            dst[1] = src[1];
            dst[0] = src[0];
            break;
        }
        case GSP_RGB565_OES:
        {
            // thanks neobrain
            u16 px = *(u16 *)src;
            blue = px & 0x1F;
            green = (px >> 5) & 0x3F;
            red = (px >> 11) & 0x1F;

            dst[0] = (blue  << 3) | (blue  >> 2);
            dst[1] = (green << 2) | (green >> 4);
            dst[2] = (red   << 3) | (red   >> 2);

            break;
        }
        case GSP_RGB5_A1_OES:
        {
            u16 px = *(u16 *)src;
            blue = (px >> 1) & 0x1F;
            green = (px >> 6) & 0x1F;
            red = (px >> 11) & 0x1F;

            dst[0] = (blue  << 3) | (blue  >> 2);
            dst[1] = (green << 3) | (green >> 2);
            dst[2] = (red   << 3) | (red   >> 2);

            break;
        }
        case GSP_RGBA4_OES:
        {
            u16 px = *(u32 *)src;
            blue = (px >> 4) & 0xF;
            green = (px >> 8) & 0xF;
            red = (px >> 12) & 0xF;

            dst[0] = (blue  << 4) | (blue  >> 0);
            dst[1] = (green << 4) | (green >> 0);
            dst[2] = (red   << 4) | (red   >> 0);

            break;
        }
        default: break;
    }
}

typedef struct FrameBufferConvertArgs {
    u8 *buf;
    u32 width;
    u8 startingLine;
    u8 numLines;
    u8 scaleFactorY;
    bool top;
    bool left;
} FrameBufferConvertArgs;

static void Draw_ConvertFrameBufferLinesKernel(const FrameBufferConvertArgs *args)
{
    static const u8 formatSizes[] = { 4, 3, 2, 2, 2 };

    GSPGPU_FramebufferFormat fmt = args->top ? (GSPGPU_FramebufferFormat)(GPU_FB_TOP_FMT & 7) : (GSPGPU_FramebufferFormat)(GPU_FB_BOTTOM_FMT & 7);
    u32 width = args->width;
    u32 stride = args->top ? GPU_FB_TOP_STRIDE : GPU_FB_BOTTOM_STRIDE;

    u32 pa = Draw_GetCurrentFramebufferAddress(args->top, args->left);
    u8 *addr = (u8 *)KERNPA2VA(pa);

    for (u32 y = args->startingLine; y < args->startingLine + args->numLines; y++)
    {
        for (u8 i = 0; i < args->scaleFactorY; i++)
        {
            for(u32 x = 0; x < width; x++)
            {
                __builtin_prefetch(addr + x * stride + y * formatSizes[fmt], 0, 3);
                Draw_ConvertPixelToBGR8(args->buf + (x + width * (args->scaleFactorY * y + i)) * 3 , addr + x * stride + y * formatSizes[fmt], fmt);
            }
        }
    }
}

void Draw_ConvertFrameBufferLines(u8 *buf, u32 width, u32 startingLine, u32 numLines, u32 scaleFactorY, bool top, bool left)
{
    FrameBufferConvertArgs args = { buf, width, (u8)startingLine, (u8)numLines, (u8)scaleFactorY, top, left };
    svcCustomBackdoor(Draw_ConvertFrameBufferLinesKernel, &args);
}
