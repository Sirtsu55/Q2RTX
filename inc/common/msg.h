/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef MSG_H
#define MSG_H

#include "common/protocol.h"
#include "common/sizebuf.h"

typedef enum {
    MSG_PS_IGNORE_GUNINDEX      = (1 << 0),
    MSG_PS_IGNORE_GUNFRAMES     = (1 << 1),
    MSG_PS_IGNORE_BLEND         = (1 << 2),
    MSG_PS_IGNORE_VIEWANGLES    = (1 << 3),
    MSG_PS_IGNORE_DELTAANGLES   = (1 << 4),
    MSG_PS_IGNORE_PREDICTION    = (1 << 5)      // mutually exclusive with IGNORE_VIEWANGLES
} msgPsFlags_t;

typedef enum {
    MSG_ES_FORCE        = (1 << 0),
    MSG_ES_NEWENTITY    = (1 << 1),
    MSG_ES_FIRSTPERSON  = (1 << 2),
    MSG_ES_AMBIENT      = (1 << 3) // always short
} msgEsFlags_t;

extern sizebuf_t    msg_write;
extern byte         msg_write_buffer[MAX_MSGLEN];

extern sizebuf_t    msg_read;
extern byte         msg_read_buffer[MAX_MSGLEN];

extern const entity_state_t    nullEntityState;
extern const player_state_t    nullPlayerState;
extern const usercmd_t         nullUserCmd;

void    MSG_Init(void);

void    MSG_BeginWriting(void);
void    MSG_WriteChar(int c);
void    MSG_WriteByte(int c);
void    MSG_WriteShort(int c);
void    MSG_WriteLong(int c);
void    MSG_WriteVarInt(uint64_t i);
void    MSG_WriteString(const char *s);
void    MSG_WriteCoord(float f);
void    MSG_WritePos(const vec3_t pos);
void    MSG_WriteAngle(float f);
void    MSG_WriteAngle16(float f);
#if USE_CLIENT
void    MSG_WriteBits(int value, int bits);
int     MSG_WriteDeltaUsercmd(const usercmd_t *from, const usercmd_t *cmd);
#endif
void    MSG_WriteDir(const vec3_t vector);
uint32_t MSG_EntityWillWrite(const entity_state_t *from,
                             const entity_state_t *to,
                             msgEsFlags_t         flags);
void MSG_WriteDeltaEntity(const entity_state_t *from,
                          const entity_state_t *to,
                          msgEsFlags_t         flags);
void MSG_WriteDeltaPacketEntity(const entity_state_t *from,
                                const entity_state_t *to,
                                msgEsFlags_t         flags);
void MSG_WriteDeltaAmbientEntity(const entity_state_t *from,
                                 const entity_state_t *to,
                                 msgEsFlags_t         flags);
int     MSG_WriteDeltaPlayerstate(const player_state_t *from, player_state_t *to, msgPsFlags_t flags);
void MSG_WriteGamestate(char *configstrings, entity_state_t *baselines, size_t num_baselines, entity_state_t *ambients, uint16_t num_ambients, uint8_t ambient_state_id, msgEsFlags_t esFlags);

static inline void *MSG_WriteData(const void *data, size_t len)
{
    return memcpy(SZ_GetSpace(&msg_write, len), data, len);
}

static inline void MSG_FlushTo(sizebuf_t *buf)
{
    SZ_Write(buf, msg_write.data, msg_write.cursize);
    SZ_Clear(&msg_write);
}

void    MSG_BeginReading(void);
byte    *MSG_ReadData(size_t len);
int     MSG_ReadChar(void);
int     MSG_ReadByte(void);
int     MSG_ReadShort(void);
int     MSG_ReadWord(void);
int     MSG_ReadLong(void);
uint64_t MSG_ReadVarInt(void);
size_t  MSG_ReadString(char *dest, size_t size);
size_t  MSG_ReadStringLine(char *dest, size_t size);
#if USE_CLIENT
void    MSG_ReadPos(vec3_t pos);
void    MSG_ReadDir(vec3_t vector);
#endif
int     MSG_ReadBits(int bits);
void    MSG_ReadDeltaUsercmd(const usercmd_t *from, usercmd_t *to);
int     MSG_ParseEntityBits(int *bits, msgEsFlags_t flags);
void    MSG_ParseDeltaEntity(const entity_state_t *from, entity_state_t *to, int number, int bits, msgEsFlags_t flags);
void    MSG_ParseDeltaPacketEntity(const entity_state_t *from, entity_state_t *to, int number, int bits, msgEsFlags_t flags);
void    MSG_ParseDeltaAmbientEntity(const entity_state_t *from, entity_state_t *to, int number, int bits, msgEsFlags_t flags);
#if USE_CLIENT
void    MSG_ParseDeltaPlayerstate(const player_state_t *from, player_state_t *to, int flags, int extraflags);
#endif

#ifdef _DEBUG
#if USE_CLIENT
void    MSG_ShowDeltaPlayerstateBits(int flags, int extraflags);
void    MSG_ShowDeltaUsercmdBits(int bits);
void    MSG_ShowDeltaEntityBits(int bits);
const char *MSG_ServerCommandString(int cmd);
#define MSG_ShowSVC(cmd) \
    Com_LPrintf(PRINT_DEVELOPER, "%3zu:%s\n", msg_read.readcount - 1, \
        MSG_ServerCommandString(cmd))
#endif // USE_CLIENT
#endif // _DEBUG


//============================================================================

static inline int MSG_PackBBox(const vec3_t mins, const vec3_t maxs, bool *safe)
{
    int x, zd, zu;

    // assume that x/y are equal and symetric
    x = maxs[0];
    clamp(x, 1, 255);

    // z is not symetric
    zd = -mins[2];
    clamp(zd, 0, 255);

    // and z maxs can be negative...
    zu = maxs[2] + 32768;
    clamp(zu, 0, 65535);

    if (x != maxs[0] || x != -mins[0] || x != maxs[1] || x != -mins[1] ||
        zd != -mins[2] || (maxs[2] + 32768) != zu) {
        *safe = false;
    } else {
        *safe = true;
    }

    return ((unsigned)zu << 16) | (zd << 8) | x;
}

static inline void MSG_UnpackBBox(int solid, vec3_t mins, vec3_t maxs)
{
    int x, zd, zu;

    x = solid & 255;
    zd = (solid >> 8) & 255;
    zu = ((solid >> 16) & 65535) - 32768;

    mins[0] = mins[1] = -x;
    maxs[0] = maxs[1] = x;
    mins[2] = -zd;
    maxs[2] = zu;
}

#endif // MSG_H
