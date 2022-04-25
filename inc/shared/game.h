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

#ifndef GAME_H
#define GAME_H

//
// game.h -- game dll information visible to server
//

#define GAME_API_VERSION    666

// edict->svflags

#define SVF_NOCLIENT            0x00000001  // don't send entity to clients, even if it has effects
#define SVF_DEADMONSTER         0x00000002  // treat as CONTENTS_DEADMONSTER for collision
#define SVF_MONSTER             0x00000004  // treat as CONTENTS_MONSTER for collision

// edict->solid values

typedef enum {
    SOLID_NOT,          // no interaction with other objects
    SOLID_TRIGGER,      // only touch when inside, after moving
    SOLID_BBOX,         // touch on edge
    SOLID_BSP           // bsp clip, touch on edge
} solid_t;

// extended features

#define GMF_CLIENTNUM               0x00000001
#define GMF_PROPERINUSE             0x00000002
#define GMF_WANT_ALL_DISCONNECTS    0x00000008

#define GMF_ENHANCED_SAVEGAMES      0x00000400
#define GMF_VARIABLE_FPS            0x00000800
#define GMF_EXTRA_USERINFO          0x00001000
#define GMF_IPV6_ADDRESS_AWARE      0x00002000

//===============================================================

#define MAX_ENT_CLUSTERS    16


typedef struct edict_s edict_t;
typedef struct gclient_s gclient_t;


#ifndef GAME_INCLUDE

struct gclient_s {
    player_state_t  ps;     // communicated by server to clients
    int             ping;

    // the game dll can add anything it wants after
    // this point in the structure
    int             clientNum;
};


struct edict_s {
    entity_state_t  s;
    struct gclient_s    *client;
    bool    inuse;
    bool    linked;
    int     linkcount;

    // FIXME: move these fields to a server private sv_entity_t
    int         areanum, areanum2;

    //================================

    int         svflags;            // SVF_NOCLIENT, SVF_DEADMONSTER, SVF_MONSTER, etc
    vec3_t      mins, maxs;
    vec3_t      absmin, absmax, size;
    solid_t     solid;
    int         clipmask;
    edict_t     *owner;

    // the game dll can add anything it wants after
    // this point in the structure
};

#endif      // GAME_INCLUDE

//===============================================================

//
// functions provided by the main engine
//
typedef struct {
    // special messages
    void (*SV_BroadcastPrint)(client_print_type_t printlevel, const char *message);
    void (*Com_LPrint)(print_type_t type, const char *message);
    void (*SV_ClientPrint)(edict_t *ent, client_print_type_t printlevel, const char *message);
    void (*SV_CenterPrint)(edict_t *ent, const char *message);
    void (*SV_StartSound)(vec3_t origin, edict_t *ent, int channel, int soundindex, float volume, float attenuation, int pitch_shift);

    // config strings hold all the index strings, the lightstyles,
    // and misc data like the sky definition and cdtrack.
    // All of the current configstrings are sent to clients when
    // they connect, and changes are sent to all connected clients.
    void (*SV_SetConfigString)(uint32_t num, const char *string);
    size_t (*SV_GetConfigString)(uint32_t num, char *buffer, size_t len);

    void (*Com_Error)(error_type_t type, const char *message);

    // the *index functions create configstrings and some internal server state
    int (*SV_ModelIndex)(const char *name);
    int (*SV_SoundIndex)(const char *name);
    int (*SV_ImageIndex)(const char *name);

    void (*SV_SetBrushModel)(edict_t *ent, const char *name);

    // collision detection
    void (*SV_Trace)(trace_t *tr, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, edict_t *passent, int contentmask);
    int (*SV_PointContents)(vec3_t point);
    bool (*SV_InVis)(vec3_t p1, vec3_t p2, vis_set_t vis, bool ignore_areas);
    void (*SV_SetAreaPortalState)(int portalnum, bool open);
    bool (*SV_GetAreaPortalState)(int portalnum);
    bool (*SV_AreasConnected)(int area1, int area2);

    // an entity will never be sent to a client or used for collision
    // if it is not passed to linkentity.  If the size, position, or
    // solidity changes, it must be relinked.
    void (*SV_LinkEntity)(edict_t *ent);
    void (*SV_UnlinkEntity)(edict_t *ent);     // call before removing an interactive edict
    bool (*SV_EntityLinked)(edict_t *ent);
    size_t (*SV_AreaEdicts)(vec3_t mins, vec3_t maxs, edict_t **list, size_t maxcount, int areatype);
    bool (*SV_EntityCollide)(vec3_t mins, vec3_t maxs, edict_t *ent);
    void (*Pmove)(pmove_t *pmove);          // player movement code common with client prediction

    // network messaging
    void (*SV_DropClient)(edict_t *ent, const char *reason);
    void (*SV_Multicast)(vec3_t origin, multicast_t to, bool reliable);
    void (*SV_Unicast)(edict_t *ent, bool reliable);
    void (*WriteChar)(int c);
    void (*WriteByte)(int c);
    void (*WriteShort)(int c);
    void (*WriteLong)(int c);
    void (*WriteString)(const char *s);
    void (*WritePos)(const vec3_t pos);
    void (*WriteDir)(const vec3_t pos);
    void (*WriteAngle)(float f);
    void (*WriteAngle16)(float f);
    void (*WriteData)(const void *data, size_t len);

    // managed memory allocation
    void (*Z_Free)(void *ptr);
    void *(*Z_Realloc)(void *ptr, size_t size);
    void *(*Z_TagMalloc)(size_t size, unsigned tag) q_malloc;
    void (*Z_FreeTags)(unsigned tag);

    // console variable interaction
    void (*Cvar_Get)(cvarRef_t *cvar, const char *var_name, const char *value, int flags);
    bool (*Cvar_Update)(cvarRef_t *cvar);
    void (*Cvar_SetString)(cvarRef_t *cvar, const char *value, bool force);
    void (*Cvar_SetInteger)(cvarRef_t *cvar, int value, bool force);
    void (*Cvar_SetFloat)(cvarRef_t *cvar, float value, bool force);

    // ClientCommand and ServerCommand parameter access
    int (*Cmd_Argc)(void);
    size_t (*Cmd_Argv)(int n, char *buffer, size_t len);
    size_t (*Cmd_Args)(char *buffer, size_t len);
    size_t (*Cmd_RawArgs)(char *buffer, size_t len);

    // add commands to the server console as if they were typed in
    // for map changing, etc
    void (*Cbuf_AddText)(const char *text);

    // filesystem
    int (*FS_LoadFileEx)(const char *path, void **buffer, unsigned flags, unsigned tag);
} game_import_t;

// entity type ID
typedef enum {
    ENT_PACKET,
    ENT_AMBIENT,
    ENT_PRIVATE,

    ENT_TOTAL
} entity_type_t;

//
// functions exported by the game subsystem
//
typedef struct {
    int         apiversion;

    // the init function will only be called when a game starts,
    // not each time a level is loaded.  Persistant data for clients
    // and the server can be allocated in init
    void (*Init)(void);
    void (*Shutdown)(void);

    // each new level entered will cause a call to SpawnEntities
    void (*SpawnEntities)(const char *mapname, const char *entstring, const char *spawnpoint);

    // Read/Write Game is for storing persistant cross level information
    // about the world state and the clients.
    // WriteGame is called every time a level is exited.
    // ReadGame is called on a loadgame.
    void (*WriteGame)(const char *filename, bool autosave);
    void (*ReadGame)(const char *filename);

    // ReadLevel is called after the default map information has been
    // loaded with SpawnEntities
    void (*WriteLevel)(const char *filename);
    void (*ReadLevel)(const char *filename);

    bool (*ClientConnect)(edict_t *ent, char *userinfo);
    void (*ClientBegin)(edict_t *ent);
    void (*ClientUserinfoChanged)(edict_t *ent, char *userinfo);
    void (*ClientDisconnect)(edict_t *ent);
    void (*ClientCommand)(edict_t *ent);
    void (*ClientThink)(edict_t *ent, usercmd_t *cmd);

    void (*RunFrame)(void);

    // ServerCommand will be called when an "sv <command>" command is issued on the
    // server console.
    // The game can issue gi.argc() / gi.argv() commands to get the rest
    // of the parameters
    void (*ServerCommand)(void);

    //
    // global variables shared between game and server
    //

    // The edict array is allocated in the game dll so it
    // can vary in size from one game to another.
    //
    // The size will be fixed when ge->Init() is called
    int           edict_size;
    edict_t       *entities;
    int           num_entities[ENT_TOTAL];
} game_export_t;

#endif // GAME_H
