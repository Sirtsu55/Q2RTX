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



/*
======================================================================

INTERMISSION

======================================================================
*/

void MoveClientToIntermission(edict_t *ent)
{
    if (deathmatch.integer || coop.integer)
        ent->client->showscores = true;
    VectorSnapCoord(level.intermission_origin, ent->s.origin);
    VectorCopy(ent->s.origin, ent->client->ps.pmove.origin);
    VectorCopy(level.intermission_angle, ent->client->ps.viewangles);
    ent->client->ps.pmove.pm_type = PM_FREEZE;
    ent->client->ps.gun[0].index = ent->client->ps.gun[1].index = 0;
    ent->client->ps.blend[3] = 0;
    ent->client->ps.rdflags &= ~RDF_UNDERWATER;

    // clean up powerup info
    ent->client->quad_time = 0;
    ent->client->invincible_time = 0;
    ent->client->enviro_time = 0;
    ent->client->ring_time = 0;

    ent->viewheight = 0;
    ent->s.modelindex = 0;
    ent->s.modelindex2 = 0;
    ent->s.modelindex3 = 0;
    ent->s.modelindex = 0;
    ent->s.effects = 0;
    ent->s.sound = 0;
    ent->solid = SOLID_NOT;

    // add the layout

    if (deathmatch.integer || coop.integer) {
        DeathmatchScoreboardMessage(ent, NULL);
        SV_Unicast(ent, true);
    }

}

void BeginIntermission(edict_t *targ)
{
    int     i, n;
    edict_t *ent, *client;

    if (level.intermission_time)
        return;     // already activated

    game.autosaved = false;

    // respawn any dead clients
    for (i = 0 ; i < game.maxclients ; i++) {
        client = globals.entities + 1 + i;
        if (!client->inuse)
            continue;
        if (client->health <= 0)
            respawn(client);
    }

    level.intermission_time = level.time;
    level.changemap = targ->map;

    if (strstr(level.changemap, "*")) {
        if (coop.integer) {
            for (i = 0 ; i < game.maxclients ; i++) {
                client = globals.entities + 1 + i;
                if (!client->inuse)
                    continue;
                // strip players of all keys between units
                for (n = 1; n < ITEM_TOTAL; n++) {
                    if (GetItemByIndex(n)->flags & IT_KEY)
                        client->client->pers.inventory[n] = 0;
                }
            }
        }
    } else {
        if (!deathmatch.integer) {
            level.exitintermission = 1;     // go immediately to the next level
            return;
        }
    }

    level.exitintermission = 0;

    // find an intermission spot
    ent = G_Find(NULL, FOFS(classname), "info_player_intermission");
    if (!ent) {
        // the map creator forgot to put in an intermission point...
        ent = G_Find(NULL, FOFS(classname), "info_player_start");
        if (!ent)
            ent = G_Find(NULL, FOFS(classname), "info_player_deathmatch");
    } else {
        // chose one of four spots
        i = Q_rand() & 3;
        while (i--) {
            ent = G_Find(ent, FOFS(classname), "info_player_intermission");
            if (!ent)   // wrap around the list
                ent = G_Find(ent, FOFS(classname), "info_player_intermission");
        }
    }

    VectorCopy(ent->s.origin, level.intermission_origin);
    VectorCopy(ent->s.angles, level.intermission_angle);

    // move all clients to the intermission point
    for (i = 0 ; i < game.maxclients ; i++) {
        client = globals.entities + 1 + i;
        if (!client->inuse)
            continue;
        MoveClientToIntermission(client);
    }
}


/*
==================
DeathmatchScoreboardMessage

==================
*/
void DeathmatchScoreboardMessage(edict_t *ent, edict_t *killer)
{
    char    entry[1024];
    char    string[1400];
    int     stringlength;
    int     i, j, k;
    int     sorted[MAX_CLIENTS];
    int     sortedscores[MAX_CLIENTS];
    int     score, total;
    int     x, y;
    gclient_t   *cl;
    edict_t     *cl_ent;
    char    *tag;

    // sort the clients by score
    total = 0;
    for (i = 0 ; i < game.maxclients ; i++) {
        cl_ent = globals.entities + 1 + i;
        if (!cl_ent->inuse || game.clients[i].resp.spectator)
            continue;
        score = game.clients[i].resp.score;
        for (j = 0 ; j < total ; j++) {
            if (score > sortedscores[j])
                break;
        }
        for (k = total ; k > j ; k--) {
            sorted[k] = sorted[k - 1];
            sortedscores[k] = sortedscores[k - 1];
        }
        sorted[j] = i;
        sortedscores[j] = score;
        total++;
    }

    // print level name and exit rules
    string[0] = 0;

    stringlength = strlen(string);

    // add the clients in sorted order
    if (total > 12)
        total = 12;

    for (i = 0 ; i < total ; i++) {
        cl = &game.clients[sorted[i]];
        cl_ent = globals.entities + 1 + sorted[i];

        x = (i >= 6) ? 160 : 0;
        y = 32 + 32 * (i % 6);

        // add a dogtag
        if (cl_ent == ent)
            tag = "tag1";
        else if (cl_ent == killer)
            tag = "tag2";
        else
            tag = NULL;
        if (tag) {
            Q_snprintf(entry, sizeof(entry),
                       "xv %i yv %i picn %s ", x + 32, y, tag);
            j = strlen(entry);
            if (stringlength + j > 1024)
                break;
            strcpy(string + stringlength, entry);
            stringlength += j;
        }

        // send the layout
        Q_snprintf(entry, sizeof(entry),
                   "client %i %i %i %i %i %i ",
                   x, y, sorted[i], cl->resp.score, cl->ping, (int) G_MsToMin(level.time - cl->resp.entertime));
        j = strlen(entry);
        if (stringlength + j > 1024)
            break;
        strcpy(string + stringlength, entry);
        stringlength += j;
    }

    SV_WriteByte(svc_layout);
    SV_WriteString(string);
}


/*
==================
DeathmatchScoreboard

Draw instead of help message.
Note that it isn't that hard to overflow the 1400 byte message limit!
==================
*/
void DeathmatchScoreboard(edict_t *ent)
{
    DeathmatchScoreboardMessage(ent, ent->enemy);
    SV_Unicast(ent, true);
}


/*
==================
Cmd_Score_f

Display the scoreboard
==================
*/
void Cmd_Score_f(edict_t *ent)
{
    ent->client->showinventory = false;
    ent->client->showhelp = false;

    if (!deathmatch.integer && !coop.integer)
        return;

    if (ent->client->showscores) {
        ent->client->showscores = false;
        return;
    }

    ent->client->showscores = true;
    DeathmatchScoreboard(ent);
}


/*
==================
HelpComputer

Draw help computer.
==================
*/
void HelpComputer(edict_t *ent)
{
    char    string[1024];
    char    *sk;

    if (skill.integer == 0)
        sk = "easy";
    else if (skill.integer == 1)
        sk = "medium";
    else if (skill.integer == 2)
        sk = "hard";
    else
        sk = "hard+";

    // send the layout
    Q_snprintf(string, sizeof(string),
               "xv 32 yv 8 picn help "         // background
               "xv 202 yv 12 string2 \"%s\" "      // skill
               "xv 0 yv 24 cstring2 \"%s\" "       // level name
               "xv 0 yv 54 cstring2 \"%s\" "       // help 1
               "xv 0 yv 110 cstring2 \"%s\" "      // help 2
               "xv 50 yv 164 string2 \" kills     goals    secrets\" "
               "xv 50 yv 172 string2 \"%3i/%3i     %i/%i       %i/%i\" ",
               sk,
               level.level_name,
               game.helpmessage1,
               game.helpmessage2,
               level.killed_monsters, level.total_monsters,
               level.found_goals, level.total_goals,
               level.found_secrets, level.total_secrets);

    SV_WriteByte(svc_layout);
    SV_WriteString(string);
    SV_Unicast(ent, true);
}


/*
==================
Cmd_Help_f

Display the current help message
==================
*/
void Cmd_Help_f(edict_t *ent)
{
    // this is for backwards compatability
    if (deathmatch.integer) {
        Cmd_Score_f(ent);
        return;
    }

    ent->client->showinventory = false;
    ent->client->showscores = false;

    if (ent->client->showhelp && (ent->client->pers.game_helpchanged == game.helpchanged)) {
        ent->client->showhelp = false;
        return;
    }

    ent->client->showhelp = true;
    ent->client->pers.helpchanged = 0;
    HelpComputer(ent);
}


//=======================================================================

/*
===============
G_SetStats
===============
*/
void G_SetStats(edict_t *ent)
{
    gitem_t     *item;
    int         index;

    //
    // health
    //
    ent->client->ps.stats[STAT_HEALTH_ICON] = level.pic_health;
    ent->client->ps.stats[STAT_HEALTH] = ent->health;

    //
    // ammo
    //
    if (!ent->client->ammo_index /* || !ent->client->pers.inventory[ent->client->ammo_index] */) {
        ent->client->ps.stats[STAT_AMMO_ICON] = 0;
        ent->client->ps.stats[STAT_AMMO] = 0;
    } else {
        item = GetItemByIndex(ent->client->ammo_index);
        ent->client->ps.stats[STAT_AMMO_ICON] = SV_ImageIndex(item->icon);
        ent->client->ps.stats[STAT_AMMO] = ent->client->pers.inventory[ent->client->ammo_index];
    }

    //
    // armor
    //
    index = ArmorIndex(ent);
    if (index) {
        item = GetItemByIndex(index);
        ent->client->ps.stats[STAT_ARMOR_ICON] = SV_ImageIndex(item->icon);
        ent->client->ps.stats[STAT_ARMOR] = ent->client->pers.inventory[index];
    } else {
        ent->client->ps.stats[STAT_ARMOR_ICON] = 0;
        ent->client->ps.stats[STAT_ARMOR] = 0;
    }

    //
    // pickup message
    //
    if (level.time > ent->client->pickup_msg_time) {
        ent->client->ps.stats[STAT_PICKUP_ICON] = 0;
        ent->client->ps.stats[STAT_PICKUP_STRING] = 0;
    }

    //
    // timers
    //
    if (ent->client->quad_time > level.time) {
        ent->client->ps.stats[STAT_TIMER_ICON] = SV_ImageIndex("p_quad");
        ent->client->ps.stats[STAT_TIMER] = G_MsToSec(ent->client->quad_time - level.time);
    } else if (ent->client->invincible_time > level.time) {
        ent->client->ps.stats[STAT_TIMER_ICON] = SV_ImageIndex("p_invulnerability");
        ent->client->ps.stats[STAT_TIMER] = G_MsToSec(ent->client->invincible_time - level.time);
    } else if (ent->client->enviro_time > level.time) {
        ent->client->ps.stats[STAT_TIMER_ICON] = SV_ImageIndex("p_envirosuit");
        ent->client->ps.stats[STAT_TIMER] = G_MsToSec(ent->client->enviro_time - level.time);
    } else if (ent->client->ring_time > level.time) {
        ent->client->ps.stats[STAT_TIMER_ICON] = SV_ImageIndex("p_ring");
        ent->client->ps.stats[STAT_TIMER] = G_MsToSec(ent->client->ring_time - level.time);
    } else {
        ent->client->ps.stats[STAT_TIMER_ICON] = 0;
        ent->client->ps.stats[STAT_TIMER] = 0;
    }

    //
    // selected item
    //
    if (ent->client->pers.selected_item <= 0)
        ent->client->ps.stats[STAT_SELECTED_ICON] = 0;
    else
        ent->client->ps.stats[STAT_SELECTED_ICON] = SV_ImageIndex(GetItemByIndex(ent->client->pers.selected_item)->icon);

    ent->client->ps.stats[STAT_SELECTED_ITEM] = ent->client->pers.selected_item;

    //
    // layouts
    //
    ent->client->ps.stats[STAT_LAYOUTS] = 0;

    if (deathmatch.integer) {
        if (ent->client->pers.health <= 0 || level.intermission_time
            || ent->client->showscores)
            ent->client->ps.stats[STAT_LAYOUTS] |= 1;
        if (ent->client->showinventory && ent->client->pers.health > 0)
            ent->client->ps.stats[STAT_LAYOUTS] |= 2;
    } else {
        if (ent->client->showscores || ent->client->showhelp)
            ent->client->ps.stats[STAT_LAYOUTS] |= 1;
        if (ent->client->showinventory && ent->client->pers.health > 0)
            ent->client->ps.stats[STAT_LAYOUTS] |= 2;
    }

    //
    // frags
    //
    ent->client->ps.stats[STAT_FRAGS] = ent->client->resp.score;

    //
    // help icon / current weapon if not shown
    //
    if (ent->client->pers.helpchanged && (level.time % 1000) > 500)
        ent->client->ps.stats[STAT_HELPICON] = SV_ImageIndex("i_help");
    else if ((ent->client->pers.hand == CENTER_HANDED || ent->client->ps.fov > 91)
             && ent->client->pers.weapon)
        ent->client->ps.stats[STAT_HELPICON] = SV_ImageIndex(ent->client->pers.weapon->icon);
    else
        ent->client->ps.stats[STAT_HELPICON] = 0;

    ent->client->ps.stats[STAT_SPECTATOR] = 0;
}

/*
===============
G_CheckChaseStats
===============
*/
void G_CheckChaseStats(edict_t *ent)
{
    int i;
    gclient_t *cl;

    for (i = 1; i <= game.maxclients; i++) {
        cl = globals.entities[i].client;
        if (!globals.entities[i].inuse || cl->chase_target != ent)
            continue;
        memcpy(cl->ps.stats, ent->client->ps.stats, sizeof(cl->ps.stats));
        G_SetSpectatorStats(globals.entities + i);
    }
}

/*
===============
G_SetSpectatorStats
===============
*/
void G_SetSpectatorStats(edict_t *ent)
{
    gclient_t *cl = ent->client;

    if (!cl->chase_target)
        G_SetStats(ent);

    cl->ps.stats[STAT_SPECTATOR] = 1;

    // layouts are independant in spectator
    cl->ps.stats[STAT_LAYOUTS] = 0;
    if (cl->pers.health <= 0 || level.intermission_time || cl->showscores)
        cl->ps.stats[STAT_LAYOUTS] |= 1;
    if (cl->showinventory && cl->pers.health > 0)
        cl->ps.stats[STAT_LAYOUTS] |= 2;

    if (cl->chase_target && cl->chase_target->inuse)
        cl->ps.stats[STAT_CHASE] = CS_PLAYERSKINS +
                                   (cl->chase_target - globals.entities) - 1;
    else
        cl->ps.stats[STAT_CHASE] = 0;
}

