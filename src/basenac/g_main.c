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

#include "g_local.h"

game_locals_t   game;
level_locals_t  level;
static game_import_t   gi;
game_export_t   globals;
spawn_temp_t    st;

int sm_meat_index;
int snd_fry;
int meansOfDeath;

cvarRef_t   deathmatch;
cvarRef_t   coop;
cvarRef_t   dmflags;
cvarRef_t   skill;
cvarRef_t   fraglimit;
cvarRef_t   timelimit;
cvarRef_t   password;
cvarRef_t   spectator_password;
cvarRef_t   needpass;
cvarRef_t   maxspectators;
cvarRef_t   g_select_empty;
cvarRef_t   dedicated;
cvarRef_t   nomonsters;

cvarRef_t   filterban;

cvarRef_t   sv_maxvelocity;
cvarRef_t   sv_gravity;

cvarRef_t   sv_rollspeed;
cvarRef_t   sv_rollangle;

cvarRef_t   run_pitch;
cvarRef_t   run_roll;
cvarRef_t   bob_up;
cvarRef_t   bob_pitch;
cvarRef_t   bob_roll;

cvarRef_t   sv_cheats;

cvarRef_t   flood_msgs;
cvarRef_t   flood_persecond;
cvarRef_t   flood_waitdelay;

cvarRef_t   sv_maplist;

cvarRef_t   sv_features;

void SpawnEntities(const char *mapname, const char *entities, const char *spawnpoint);
void ClientThink(edict_t *ent, usercmd_t *cmd);
bool ClientConnect(edict_t *ent, char *userinfo);
void ClientUserinfoChanged(edict_t *ent, char *userinfo);
void ClientDisconnect(edict_t *ent);
void ClientBegin(edict_t *ent);
void ClientCommand(edict_t *ent);
void WriteGame(const char *filename, bool autosave);
void ReadGame(const char *filename);
void WriteLevel(const char *filename);
void ReadLevel(const char *filename);
void InitGame(void);
void G_RunFrame(void);

//===================================================================


void ShutdownGame(void)
{
    Com_Print("==== ShutdownGame ====\n");

    Z_FreeTags(TAG_LEVEL);
    Z_FreeTags(TAG_GAME);
}

void G_InitEntityList(edict_t *list)
{
    memset(list, 0, MAX_EDICTS * globals.edict_size);

    for (int i = 0; i < MAX_EDICTS; i++, list++) {
        list->s.number = i;
    }
}

edict_t *G_CreateEntityList(void)
{
    edict_t *list = Z_TagMalloc(MAX_EDICTS * globals.edict_size, TAG_GAME);
    G_InitEntityList(list);
    return list;
}

/*
============
InitGame

This will be called when the dll is first loaded, which
only happens when a new game is started or a save game
is loaded.
============
*/
void InitGame(void)
{
    Com_Print("==== InitGame ====\n");

    Q_srand(time(NULL));

    //FIXME: sv_ prefix is wrong for these
    Cvar_Get(&sv_rollspeed, "sv_rollspeed", "200", 0);
    Cvar_Get(&sv_rollangle, "sv_rollangle", "2", 0);
    Cvar_Get(&sv_maxvelocity, "sv_maxvelocity", "2000", 0);
    Cvar_Get(&sv_gravity, "sv_gravity", "800", 0);

    // noset vars
    Cvar_Get(&dedicated, "dedicated", "0", CVAR_NOSET);

	Cvar_Get(&nomonsters, "nomonsters", "0", 0);

    // latched vars
    Cvar_Get(NULL, "gamename", GAMEVERSION , CVAR_SERVERINFO | CVAR_LATCH);
    Cvar_Get(NULL, "gamedate", __DATE__ , CVAR_SERVERINFO | CVAR_LATCH);
    
    Cvar_Get(&deathmatch, "deathmatch", "0", CVAR_LATCH);
    Cvar_Get(&coop, "coop", "0", CVAR_LATCH);
    Cvar_Get(&skill, "skill", "1", CVAR_LATCH);

    // change anytime vars
    Cvar_Get(&maxspectators, "maxspectators", "4", CVAR_SERVERINFO);
    Cvar_Get(&sv_cheats, "cheats", "0", CVAR_SERVERINFO);
    Cvar_Get(&dmflags, "dmflags", "0", CVAR_SERVERINFO);
    Cvar_Get(&fraglimit, "fraglimit", "0", CVAR_SERVERINFO);
    Cvar_Get(&timelimit, "timelimit", "0", CVAR_SERVERINFO);
    Cvar_Get(&password, "password", "", CVAR_USERINFO);
    Cvar_Get(&spectator_password, "spectator_password", "", CVAR_USERINFO);
    Cvar_Get(&needpass, "needpass", "0", CVAR_SERVERINFO);
    Cvar_Get(&filterban, "filterban", "1", 0);

    Cvar_Get(&g_select_empty, "g_select_empty", "0", CVAR_ARCHIVE);

    Cvar_Get(&run_pitch, "run_pitch", "0.002", 0);
    Cvar_Get(&run_roll, "run_roll", "0.005", 0);
    Cvar_Get(&bob_up, "bob_up", "0.005", 0);
    Cvar_Get(&bob_pitch, "bob_pitch", "0.002", 0);
    Cvar_Get(&bob_roll, "bob_roll", "0.002", 0);

    // flood control
    Cvar_Get(&flood_msgs, "flood_msgs", "4", 0);
    Cvar_Get(&flood_persecond, "flood_persecond", "4", 0);
    Cvar_Get(&flood_waitdelay, "flood_waitdelay", "10", 0);

    // dm map list
    Cvar_Get(&sv_maplist, "sv_maplist", "", 0);

    // obtain server features
    Cvar_Get(&sv_features, "sv_features", NULL, 0);

    // export our own features
    cvarRef_t g_features;
    Cvar_Get(&g_features, "g_features", va("%d", G_FEATURES), 0);
    Cvar_Set(&g_features, G_FEATURES, true);

    // items
    InitItems();

    game.helpmessage1[0] = 0;
    game.helpmessage2[0] = 0;

    cvarRef_t maxclients;

    Cvar_Get(&maxclients, "maxclients", "4", CVAR_SERVERINFO | CVAR_LATCH);
    game.maxclients = maxclients.integer;

    // initialize all entities for this game
    globals.entities = G_CreateEntityList();
    game.world = globals.entities;
    globals.num_entities[ENT_PACKET] = game.maxclients + 1;
    globals.num_entities[ENT_AMBIENT] = globals.num_entities[ENT_PRIVATE] = 0;

    // initialize all clients for this game
    game.clients = Z_TagMallocz(game.maxclients * sizeof(game.clients[0]), TAG_GAME);
}


/*
=================
GetGameAPI

Returns a pointer to the structure with all entry points
and global variables
=================
*/
q_exported game_export_t *GetGameAPI(game_import_t *import)
{
    gi = *import;

    globals.apiversion = GAME_API_VERSION;
    globals.Init = InitGame;
    globals.Shutdown = ShutdownGame;
    globals.SpawnEntities = SpawnEntities;

    globals.WriteGame = WriteGame;
    globals.ReadGame = ReadGame;
    globals.WriteLevel = WriteLevel;
    globals.ReadLevel = ReadLevel;

    globals.ClientThink = ClientThink;
    globals.ClientConnect = ClientConnect;
    globals.ClientUserinfoChanged = ClientUserinfoChanged;
    globals.ClientDisconnect = ClientDisconnect;
    globals.ClientBegin = ClientBegin;
    globals.ClientCommand = ClientCommand;

    globals.RunFrame = G_RunFrame;

    globals.ServerCommand = ServerCommand;

    globals.edict_size = sizeof(edict_t);

    return &globals;
}

//======================================================================

/*
=================
ClientEndServerFrames
=================
*/
void ClientEndServerFrames(void)
{
    int     i;
    edict_t *ent;

    // calc the player views now that all pushing
    // and damage has been added
    for (i = 0 ; i < game.maxclients; i++) {
        ent = globals.entities + 1 + i;
        if (!ent->inuse || !ent->client)
            continue;
        ClientEndServerFrame(ent);
    }

}

/*
=================
CreateTargetChangeLevel

Returns the created target changelevel
=================
*/
edict_t *CreateTargetChangeLevel(char *map)
{
    edict_t *ent;

    ent = G_Spawn();
    ent->classname = "target_changelevel";
    Q_snprintf(level.nextmap, sizeof(level.nextmap), "%s", map);
    ent->map = level.nextmap;
    return ent;
}

/*
=================
EndDMLevel

The timelimit or fraglimit has been exceeded
=================
*/
void EndDMLevel(void)
{
    edict_t     *ent;
    char *s, *t, *f;
    static const char *seps = " ,\n\r";

    // stay on same level flag
    if (dmflags.integer & DF_SAME_LEVEL) {
        BeginIntermission(CreateTargetChangeLevel(level.mapname));
        return;
    }

    // see if it's in the map list
    if (sv_maplist.string[0]) {
        s = Z_TagCopyString(sv_maplist.string, TAG_GAME);
        f = NULL;
        t = strtok(s, seps);
        while (t != NULL) {
            if (Q_stricmp(t, level.mapname) == 0) {
                // it's in the list, go to the next one
                t = strtok(NULL, seps);
                if (t == NULL) { // end of list, go to first one
                    if (f == NULL) // there isn't a first one, same level
                        BeginIntermission(CreateTargetChangeLevel(level.mapname));
                    else
                        BeginIntermission(CreateTargetChangeLevel(f));
                } else
                    BeginIntermission(CreateTargetChangeLevel(t));
                free(s);
                return;
            }
            if (!f)
                f = t;
            t = strtok(NULL, seps);
        }
        Z_Free(s);
    }

    if (level.nextmap[0]) // go to a specific map
        BeginIntermission(CreateTargetChangeLevel(level.nextmap));
    else {  // search for a changelevel
        ent = G_Find(NULL, FOFS(classname), "target_changelevel");
        if (!ent) {
            // the map designer didn't include a changelevel,
            // so create a fake ent that goes back to the same level
            BeginIntermission(CreateTargetChangeLevel(level.mapname));
            return;
        }
        BeginIntermission(ent);
    }
}


/*
=================
CheckNeedPass
=================
*/
void CheckNeedPass(void)
{
    bool updated = false;

    updated = Cvar_Update(&password) || updated;
    updated = Cvar_Update(&spectator_password) || updated;

    // if password or spectator_password has changed, update needpass
    // as needed
    if (updated) {
        int need = 0;

        if (password.string[0] && Q_stricmp(password.string, "none"))
            need |= 1;
        if (spectator_password.string[0] && Q_stricmp(spectator_password.string, "none"))
            need |= 2;

        Cvar_Set(&needpass, need, false);
    }
}

/*
=================
CheckDMRules
=================
*/
void CheckDMRules(void)
{
    int         i;
    gclient_t   *cl;

    if (level.intermission_time)
        return;

    if (!deathmatch.integer)
        return;

    if (timelimit.value) {
        if (level.time >= G_MinToMs(timelimit.value)) {
            SV_BroadcastPrint(PRINT_HIGH, "Timelimit hit.\n");
            EndDMLevel();
            return;
        }
    }

    if (fraglimit.integer) {
        for (i = 0 ; i < game.maxclients ; i++) {
            cl = game.clients + i;
            if (!globals.entities[i + 1].inuse)
                continue;

            if (cl->resp.score >= fraglimit.integer) {
                SV_BroadcastPrint(PRINT_HIGH, "Fraglimit hit.\n");
                EndDMLevel();
                return;
            }
        }
    }
}


/*
=============
ExitLevel
=============
*/
void ExitLevel(void)
{
    int     i;
    edict_t *ent;

    Cbuf_AddText(va("gamemap \"%s\"\n", level.changemap));

    level.changemap = NULL;
    level.exitintermission = 0;
    level.intermission_time = 0;
    ClientEndServerFrames();

    // clear some things before going to next level
    for (i = 0 ; i < game.maxclients; i++) {
        ent = globals.entities + 1 + i;
        if (!ent->inuse)
            continue;
        if (ent->health > ent->client->pers.max_health)
            ent->health = ent->client->pers.max_health;
    }

}

// Update cvars that may be updated by the server
static void G_UpdateCvars(void)
{
    Cvar_Update(&maxspectators);
    Cvar_Update(&sv_cheats);
    Cvar_Update(&dmflags);
    Cvar_Update(&fraglimit);
    Cvar_Update(&timelimit);
    Cvar_Update(&password);
    Cvar_Update(&filterban);
    Cvar_Update(&g_select_empty);
    Cvar_Update(&run_pitch);
    Cvar_Update(&run_roll);
    Cvar_Update(&bob_up);
    Cvar_Update(&bob_pitch);
    Cvar_Update(&bob_roll);
    Cvar_Update(&flood_msgs);
    Cvar_Update(&flood_persecond);
    Cvar_Update(&flood_waitdelay);

    // Paril: gravity change support.
    // this is just so you can change it via console still
    // for shenanigans. sv_gravity should be 800 at all
    // times normally though.
    if (Cvar_Update(&sv_gravity)) {
        level.gravity = sv_gravity.value;
    }
}

/*
================
G_RunFrame

Advances the world by 0.1 seconds
================
*/
void G_RunFrame(void)
{
    G_UpdateCvars();

    level.time += BASE_FRAMETIME;

    // choose a client for monsters to target this frame
    AI_SetSightClient();

    // exit intermissions

    if (level.exitintermission) {
        ExitLevel();
        return;
    }

    //
    // treat each object in turn
    // even the world gets a chance to think
    //
    for (edict_t *ent = globals.entities; ent; ent = G_NextEnt(ent))
    {
        level.current_entity = ent;

        VectorCopy(ent->s.origin, ent->s.old_origin);

        // if the ground entity moved, make sure we are still on it
        if ((ent->groundentity) && (ent->groundentity->linkcount != ent->groundentity_linkcount)) {
            ent->groundentity = NULL;
            if (!(ent->flags & (FL_SWIM | FL_FLY)) && (ent->svflags & SVF_MONSTER)) {
                M_CheckGround(ent);
            }
        }

        if (ent->client) {
            ClientBeginServerFrame(ent);
        } else {
            G_RunEntity(ent);
        }
    }

    // see if it is time to end a deathmatch
    CheckDMRules();

    // see if needpass needs updated
    CheckNeedPass();

    // build the playerstate_t structures for all players
    ClientEndServerFrames();
}

// API wrappers

void SV_CenterPrint(edict_t *ent, const char *message)
{
    gi.SV_CenterPrint(ent, message);
}

void SV_CenterPrintf(edict_t *ent, const char *fmt, ...)
{
    Com_VarArgs(MAXPRINTMSG);
    gi.SV_CenterPrint(ent, msg);
}

void SV_BroadcastPrint(client_print_type_t level, const char *message)
{
    gi.SV_BroadcastPrint(level, message);
}

void SV_BroadcastPrintf(client_print_type_t level, const char *fmt, ...)
{
    Com_VarArgs(MAXPRINTMSG);
    gi.SV_BroadcastPrint(level, msg);
}

void SV_ClientPrint(edict_t *ent, client_print_type_t level, const char *message)
{
    gi.SV_ClientPrint(ent, level, message);
}

void SV_ClientPrintf(edict_t *ent, client_print_type_t level, const char *fmt, ...)
{
    Com_VarArgs(MAXPRINTMSG);
    gi.SV_ClientPrint(ent, level, msg);
}

void Com_LPrint(print_type_t type, const char *message)
{
    gi.Com_LPrint(type, message);
}

_Noreturn void Com_Error(error_type_t type, const char *message)
{
    gi.Com_Error(type, message);
}

void Z_Free(void *ptr)
{
    gi.Z_Free(ptr);
}

void *Z_Realloc(void *ptr, size_t size)
{
    return gi.Z_Realloc(ptr, size);
}

void *Z_TagMalloc(size_t size, memtag_t tag)
{
    return gi.Z_TagMalloc(size, tag);
}

void *Z_TagMallocz(size_t size, memtag_t tag) q_malloc
{
    return memset(gi.Z_TagMalloc(size, tag), 0, size);
}

char *Z_TagCopyString(const char *in, memtag_t tag) q_malloc
{
    if (!in) {
        return NULL;
    }

    size_t len = strlen(in) + 1;
    return memcpy(Z_TagMalloc(len, tag), in, len);
}

void Z_FreeTags(memtag_t tag)
{
    gi.Z_FreeTags(tag);
}

int Cmd_Argc(void)
{
    return gi.Cmd_Argc();
}

char *Cmd_Argv(int arg)
{
    static char arg_buffer[8][MAX_INFO_STRING] = { 0 };
    size_t l = gi.Cmd_Argv(arg, arg_buffer[arg % 8], sizeof(arg_buffer[0]));

    if (l == sizeof(arg_buffer[0]) - 1) {
        Com_WPrint("Cmd_Argv truncated");
    }

    return arg_buffer[arg];
}

 static char args_buffer[MAX_INFO_STRING] = { 0 };

char *Cmd_Args(void)
{
    size_t l = gi.Cmd_Args(args_buffer, sizeof(args_buffer));

    if (l == sizeof(args_buffer) - 1) {
        Com_WPrint("Cmd_Args truncated");
    }

    return args_buffer;
}

char *Cmd_RawArgs(void)
{
    size_t l = gi.Cmd_RawArgs(args_buffer, sizeof(args_buffer));

    if (l == sizeof(args_buffer) - 1) {
        Com_WPrint("Cmd_RawArgs truncated");
    }

    return args_buffer;
}

void Cbuf_AddText(const char *text)
{
    gi.Cbuf_AddText(text);
}

void SV_SetConfigString(uint32_t num, const char *string)
{
    gi.SV_SetConfigString(num, string);
}

size_t SV_GetConfigString(uint32_t num, char *buffer, size_t len)
{
    return gi.SV_GetConfigString(num, buffer, len);
}

int SV_ModelIndex(const char *name)
{
    return gi.SV_ModelIndex(name);
}

int SV_SoundIndex(const char *name)
{
    return gi.SV_SoundIndex(name);
}

int SV_ImageIndex(const char *name)
{
    return gi.SV_ImageIndex(name);
}

void SV_SetBrushModel(edict_t *ent, const char *model)
{
    gi.SV_SetBrushModel(ent, model);
}

void SV_LinkEntity(edict_t *ent)
{
    gi.SV_LinkEntity(ent);
}

void SV_UnlinkEntity(edict_t *ent)
{
    gi.SV_UnlinkEntity(ent);
}

bool SV_EntityLinked(edict_t *ent)
{
    return gi.SV_EntityLinked(ent);
}

trace_t SV_Trace(const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end,
                 edict_t *passedict, int contentmask)
{
    trace_t tr;
    gi.SV_Trace(&tr, start, mins, maxs, end, passedict, contentmask);
    return tr;
}

int SV_PointContents(const vec3_t p)
{
    return gi.SV_PointContents(p);
}

size_t SV_AreaEdicts(const vec3_t mins, const vec3_t maxs, edict_t **list, size_t maxcount, int areatype)
{
    return gi.SV_AreaEdicts(mins, maxs, list, maxcount, areatype);
}

bool SV_EntityCollide(const vec3_t mins, const vec3_t maxs, edict_t *ent)
{
    return gi.SV_EntityCollide(mins, maxs, ent);
}

void SV_PositionedSound(const vec3_t origin, edict_t *entity, int channel,
                        int soundindex, float volume,
                        float attenuation, int pitch_shift)
{
    if (entity && (!entity->inuse || entity->s.number >= OFFSET_PRIVATE_ENTITIES)) {
        Com_Error(ERR_DROP, "Attempted to write private entity to network message");
    }

    gi.SV_StartSound(origin, entity, channel, soundindex, volume, attenuation, pitch_shift);
}

void SV_StartSound(edict_t *entity, int channel,
                   int soundindex, float volume,
                   float attenuation, int pitch_shift)
{
    if (!entity || !entity->inuse || entity->s.number >= OFFSET_PRIVATE_ENTITIES) {
        Com_Error(ERR_DROP, "Attempted to write private entity to network message");
    }

    gi.SV_StartSound(NULL, entity, channel, soundindex, volume, attenuation, pitch_shift);
}

bool SV_InVis(const vec3_t p1, const vec3_t p2, vis_set_t vis, bool ignore_areas)
{
    return gi.SV_InVis(p1, p2, vis, ignore_areas);
}

void SV_SetAreaPortalState(int portalnum, bool open)
{
    gi.SV_SetAreaPortalState(portalnum, open);
}

bool SV_GetAreaPortalState(int portalnum)
{
    return gi.SV_GetAreaPortalState(portalnum);
}

bool SV_AreasConnected(int area1, int area2)
{
    return gi.SV_AreasConnected(area1, area2);
}

void Pm_Move(pmove_t *pm)
{
    gi.Pmove(pm);
}

void SV_DropClient(edict_t *ent, const char *reason)
{
    gi.SV_DropClient(ent, reason);
}

void SV_Multicast(const vec3_t origin, multicast_t to, bool reliable)
{
    gi.SV_Multicast(origin, to, reliable);
}

void SV_Unicast(edict_t *ent, bool reliable)
{
    if (!ent || !ent->inuse || ent->s.number >= OFFSET_PRIVATE_ENTITIES) {
        Com_Error(ERR_DROP, "Attempted to write private entity to network message");
    }

    gi.SV_Unicast(ent, reliable);
}

void SV_WriteChar(int c)
{
    gi.WriteChar(c);
}

void SV_WriteByte(int c)
{
    gi.WriteByte(c);
}

void SV_WriteShort(int c)
{
    gi.WriteShort(c);
}

void SV_WriteEntity(edict_t *ent)
{
    if (!ent || !ent->inuse || ent->s.number >= OFFSET_PRIVATE_ENTITIES) {
        Com_Error(ERR_DROP, "Attempted to write private entity to network message");
    }

    gi.WriteShort(ent - globals.entities);
}

void SV_WriteLong(int c)
{
    gi.WriteLong(c);
}

void SV_WriteString(const char *s)
{
    gi.WriteString(s);
}

void SV_WritePos(const vec3_t pos)
{
    gi.WritePos(pos);
}

void SV_WriteDir(const vec3_t pos)
{
    gi.WriteDir(pos);
}

void SV_WriteAngle(float f)
{
    gi.WriteAngle(f);
}

void SV_WriteAngle16(float f)
{
    gi.WriteAngle16(f);
}

void SV_WriteData(const void *data, size_t len)
{
    gi.WriteData(data, len);
}

void Cvar_Get(cvarRef_t *cvar, const char *var_name, const char *value, int flags)
{
    gi.Cvar_Get(cvar, var_name, value, flags);
}

bool Cvar_Update(cvarRef_t *cvar)
{
    return gi.Cvar_Update(cvar);
}

void Cvar_SetString(cvarRef_t *cvar, const char *value, bool force)
{
    gi.Cvar_SetString(cvar, value, force);
}

void Cvar_SetInteger(cvarRef_t *cvar, int value, bool force)
{
    gi.Cvar_SetInteger(cvar, value, force);
}

void Cvar_SetFloat(cvarRef_t *cvar, float value, bool force)
{
    gi.Cvar_SetFloat(cvar, value, force);
}

int FS_LoadFileEx(const char *path, void **buffer, unsigned flags, unsigned tag)
{
    return gi.FS_LoadFileEx(path, buffer, flags, tag);
}