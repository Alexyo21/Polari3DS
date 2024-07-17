/*
*   This file is part of Luma3DS.
*   Copyright (C) 2016-2020 Aurora Wright, TuxSH
*
*   SPDX-License-Identifier: (MIT OR GPL-2.0-or-later)
*/

#include "gdb/net.h"
#include <stdarg.h>
#include <stdlib.h>
#include "fmt.h"
#include "minisoc.h"

u8 GDB_ComputeChecksum(const char *packetData, u32 len)
{
    u8 cksum = 0;
    for(u32 i = 0; i < len; i++)
        cksum += (u8)packetData[i];

    return cksum;
}

void GDB_EncodeHex(char *dst, const void *src, u32 len)
{
    static const char *alphabet = "0123456789abcdef";
    const u8 *src8 = (u8 *)src;

    for(u32 i = 0; i < len; i++)
    {
        dst[2 * i] = alphabet[(src8[i] & 0xf0) >> 4];
        dst[2 * i + 1] = alphabet[src8[i] & 0x0f];
    }
}

static inline u32 GDB_DecodeHexDigit(char src, bool *ok)
{
    *ok = true;
    if(src >= '0' && src <= '9') return src - '0';
    else if(src >= 'a' && src <= 'f') return 0xA + (src - 'a');
    else if(src >= 'A' && src <= 'F') return 0xA + (src - 'A');
    else
    {
        *ok = false;
        return 0;
    }
}

u32 GDB_DecodeHex(void *dst, const char *src, u32 len)
{
    u32 i = 0;
    bool ok = true;
    u8 *dst8 = (u8 *)dst;
    for(i = 0; i < len && ok && src[2 * i] != 0 && src[2 * i + 1] != 0; i++)
        dst8[i] = (GDB_DecodeHexDigit(src[2 * i], &ok) << 4) | GDB_DecodeHexDigit(src[2 * i + 1], &ok);

    return (!ok) ? i - 1 : i;
}

u32 GDB_EscapeBinaryData(u32 *encodedCount, void *dst, const void *src, u32 len, u32 maxLen)
{
    u8 *dst8 = (u8 *)dst;
    const u8 *src8 = (const u8 *)src;

    maxLen = maxLen >= len ? len : maxLen;

    while((uintptr_t)dst8 < (uintptr_t)dst + maxLen)
    {
        if(*src8 == '$' || *src8 == '#' || *src8 == '}' || *src8 == '*')
        {
            if ((uintptr_t)dst8 + 1 >= (uintptr_t)dst + maxLen)
                break;
            *dst8++ = '}';
            *dst8++ = *src8++ ^ 0x20;
        }
        else
            *dst8++ = *src8++;
    }

    *encodedCount = dst8 - (u8 *)dst;
    return src8 - (u8 *)src;
}

u32 GDB_UnescapeBinaryData(void *dst, const void *src, u32 len)
{
    u8 *dst8 = (u8 *)dst;
    const u8 *src8 = (const u8 *)src;

    while((uintptr_t)src8 < (uintptr_t)src + len)
    {
        if(*src8 == '}')
        {
            src8++;
            *dst8++ = *src8++ ^ 0x20;
        }
        else
            *dst8++ = *src8++;
    }

    return dst8 - (u8 *)dst;
}

const char *GDB_ParseIntegerList(u32 *dst, const char *src, u32 nb, char sep, char lastSep, u32 base, bool allowPrefix)
{
    const char *pos = src;
    const char *endpos;
    bool ok;

    for(u32 i = 0; i < nb; i++)
    {
        u32 n = xstrtoul(pos, (char **)&endpos, (int) base, allowPrefix, &ok);
        if(!ok || endpos == pos)
            return NULL;

        if(i != nb - 1)
        {
            if(*endpos != sep)
                return NULL;
            pos = endpos + 1;
        }
        else
        {
            if(*endpos != lastSep && *endpos != 0)
                return NULL;
            pos = endpos;
        }

        dst[i] = n;
    }

    return pos;
}

const char *GDB_ParseIntegerList64(u64 *dst, const char *src, u32 nb, char sep, char lastSep, u32 base, bool allowPrefix)
{
    const char *pos = src;
    const char *endpos;
    bool ok;

    for(u32 i = 0; i < nb; i++)
    {
        u64 n = xstrtoull(pos, (char **)&endpos, (int) base, allowPrefix, &ok);
        if(!ok || endpos == pos)
            return NULL;

        if(i != nb - 1)
        {
            if(*endpos != sep)
                return NULL;
            pos = endpos + 1;
        }
        else
        {
            if(*endpos != lastSep && *endpos != 0)
                return NULL;
            pos = endpos;
        }

        dst[i] = n;
    }

    return pos;
}

const char *GDB_ParseHexIntegerList(u32 *dst, const char *src, u32 nb, char lastSep)
{
    return GDB_ParseIntegerList(dst, src, nb, ',', lastSep, 16, false);
}

const char *GDB_ParseHexIntegerList64(u64 *dst, const char *src, u32 nb, char lastSep)
{
    return GDB_ParseIntegerList64(dst, src, nb, ',', lastSep, 16, false);
}

int GDB_ReceivePacket(GDBContext *ctx)
{
    char backupbuf[GDB_BUF_LEN + 4];
    memcpy(backupbuf, ctx->buffer, ctx->latestSentPacketSize);
    memset(ctx->buffer, 0, sizeof(ctx->buffer));

    int r = socRecv(ctx->super.sockfd, ctx->buffer, sizeof(ctx->buffer), MSG_PEEK);
    if(r < 1)
        return -1;
    if(ctx->buffer[0] == '+') // GDB sometimes acknowleges TCP acknowledgment packets (yes...). IDA does it properly
    {
        if(ctx->flags & GDB_FLAG_NOACK)
            return -1;

        // Consume it
        r = socRecv(ctx->super.sockfd, ctx->buffer, 1, 0);
        if(r != 1)
            return -1;

        ctx->buffer[0] = 0;

        r = socRecv(ctx->super.sockfd, ctx->buffer, sizeof(ctx->buffer), MSG_PEEK);

        if(r == -1)
            goto packet_error;
    }
    else if(ctx->buffer[0] == '-')
    {
        socSend(ctx->super.sockfd, backupbuf, ctx->latestSentPacketSize, 0);
        return 0;
    }
    int maxlen = r > (int)sizeof(ctx->buffer) ? (int)sizeof(ctx->buffer) : r;

    if(ctx->buffer[0] == '$') // normal packet
    {
        char *pos;
        for(pos = ctx->buffer; pos < ctx->buffer + maxlen && *pos != '#'; pos++);

        if(pos == ctx->buffer + maxlen) // malformed packet
            return -1;

        else
        {
            u8 checksum;
            r = socRecv(ctx->super.sockfd, ctx->buffer, 3 + pos - ctx->buffer, 0);
            if(r != 3 + pos - ctx->buffer || GDB_DecodeHex(&checksum, pos + 1, 1) != 1)
                goto packet_error;
            else if(GDB_ComputeChecksum(ctx->buffer + 1, pos - ctx->buffer - 1) != checksum)
                goto packet_error;

            ctx->commandEnd = pos;
            *pos = 0; // replace trailing '#' by a NUL character
        }
    }
    else if(ctx->buffer[0] == '\x03')
    {
        r = socRecv(ctx->super.sockfd, ctx->buffer, 1, 0);
        if(r != 1)
            goto packet_error;

        ctx->commandEnd = ctx->buffer;
    }

    if(!(ctx->flags & GDB_FLAG_NOACK))
    {
        int r2 = socSend(ctx->super.sockfd, "+", 1, 0);
        if(r2 != 1)
            return -1;
    }

    if(ctx->noAckSent)
    {
        ctx->flags |= GDB_FLAG_NOACK;
        ctx->noAckSent = false;
    }

    return r;

packet_error:
    if(!(ctx->flags & GDB_FLAG_NOACK))
    {
        r = socSend(ctx->super.sockfd, "-", 1, 0);
        if(r != 1)
            return -1;
        else
            return 0;
    }
    else
        return -1;
}

static int GDB_DoSendPacket(GDBContext *ctx, u32 len)
{
    int r = socSend(ctx->super.sockfd, ctx->buffer, len, 0);

    if(r > 0)
        ctx->latestSentPacketSize = r;
    return r;
}

int GDB_SendPacket(GDBContext *ctx, const char *packetData, u32 len)
{
    ctx->buffer[0] = '$';

    memcpy(ctx->buffer + 1, packetData, len);

    char *checksumLoc = ctx->buffer + len + 1;
    *checksumLoc++ = '#';

    hexItoa(GDB_ComputeChecksum(packetData, len), checksumLoc, 2, false);
    return GDB_DoSendPacket(ctx, 4 + len);
}

int GDB_SendFormattedPacket(GDBContext *ctx, const char *packetDataFmt, ...)
{
    // It goes without saying you shouldn't use that with user-controlled data...
    char buf[GDB_BUF_LEN + 1];
    va_list args;
    va_start(args, packetDataFmt);
    int n = vsprintf(buf, packetDataFmt, args);
    va_end(args);

    if(n < 0) return -1;
    else return GDB_SendPacket(ctx, buf, (u32)n);
}

int GDB_SendHexPacket(GDBContext *ctx, const void *packetData, u32 len)
{
    if(4 + 2 * len > GDB_BUF_LEN)
        return -1;

    ctx->buffer[0] = '$';
    GDB_EncodeHex(ctx->buffer + 1, packetData, len);

    char *checksumLoc = ctx->buffer + 2 * len + 1;
    *checksumLoc++ = '#';

    hexItoa(GDB_ComputeChecksum(ctx->buffer + 1, 2 * len), checksumLoc, 2, false);
    return GDB_DoSendPacket(ctx, 4 + 2 * len);
}

int GDB_SendStreamData(GDBContext *ctx, const char *streamData, u32 offset, u32 length, u32 totalSize, bool forceEmptyLast)
{
    char buf[GDB_BUF_LEN];
    if(length > GDB_BUF_LEN - 1)
        length = GDB_BUF_LEN - 1;

    if((forceEmptyLast && offset >= totalSize) || (!forceEmptyLast && offset + length >= totalSize))
    {
        length = offset >= totalSize ? 0 : totalSize - offset;
        buf[0] = 'l';
        memcpy(buf + 1, streamData + offset, length);
        return GDB_SendPacket(ctx, buf, 1 + length);
    }
    else
    {
        buf[0] = 'm';
        memcpy(buf + 1, streamData + offset, length);
        return GDB_SendPacket(ctx, buf, 1 + length);
    }
}

int GDB_SendDebugString(GDBContext *ctx, const char *fmt, ...) // unsecure
{
    /*if(ctx->state == GDB_STATE_DETACHING || !(ctx->flags & GDB_FLAG_PROCESS_CONTINUING))
        return 0;*/

    char formatted[(GDB_BUF_LEN - 1) / 2 + 1];
    ctx->buffer[0] = '$';
    ctx->buffer[1] = 'O';

    va_list args;
    va_start(args, fmt);
    int n = vsprintf(formatted, fmt, args);
    va_end(args);

    if(n <= 0) return n;
    GDB_EncodeHex(ctx->buffer + 2, formatted, 2 * n);

    char *checksumLoc = ctx->buffer + 2 * n + 2;
    *checksumLoc++ = '#';

    hexItoa(GDB_ComputeChecksum(ctx->buffer + 1, 2 * n + 1), checksumLoc, 2, false);

    return GDB_DoSendPacket(ctx, 5 + 2 * n);
}

int GDB_ReplyEmpty(GDBContext *ctx)
{
    return GDB_SendPacket(ctx, "", 0);
}

int GDB_ReplyOk(GDBContext *ctx)
{
    return GDB_SendPacket(ctx, "OK", 2);
}

int GDB_ReplyErrno(GDBContext *ctx, int no)
{
    char buf[] = "E01";
    hexItoa((u8)no, buf + 1, 2, false);
    return GDB_SendPacket(ctx, buf, 3);
}
