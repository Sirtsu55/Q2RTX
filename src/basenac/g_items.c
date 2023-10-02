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

extern const weapon_animation_t weap_axe_activate;
extern const weapon_animation_t weap_shotgun_activate;
extern const weapon_animation_t weap_sshotgun_activate;
extern const weapon_animation_t weap_nailgun_activate;
extern const weapon_animation_t weap_perf_activate;
extern const weapon_animation_t weap_rocket_activate;
extern const weapon_animation_t weap_launch_activate;
extern const weapon_animation_t weap_thunder_activate;
extern const weapon_animation_t weap_blaster_activate;

static gitem_armor_t greenarmor_info  = {100, 100, .30, .00};
static gitem_armor_t yellowarmor_info = {150, 150, .60, .30};
static gitem_armor_t redarmor_info    = {200, 200, .80, .60};

enum {
    IT_HEALTH_IGNORE_MAX    = 1 << 15,
    IT_HEALTH_TIMED         = 1 << 16
};

//======================================================================

void DoRespawn(edict_t *ent)
{
    if (ent->team) {
        edict_t *master;
        int count;
        int choice;

        master = ent->teammaster;

        for (count = 0, ent = master; ent; ent = ent->chain, count++)
            ;

        choice = Q_rand_uniform(count);

        for (count = 0, ent = master; count < choice; ent = ent->chain, count++)
            ;
    }

    ent->svflags &= ~SVF_NOCLIENT;
    ent->solid = SOLID_TRIGGER;
    SV_LinkEntity(ent);

    // send an effect
    ent->s.event = EV_ITEM_RESPAWN;
}

void SetRespawn(edict_t *ent, float delay)
{
    ent->flags |= FL_RESPAWN;
    ent->svflags |= SVF_NOCLIENT;
    ent->solid = SOLID_NOT;
    ent->nextthink = level.time + G_SecToMs(delay);
    ent->think = DoRespawn;
    SV_LinkEntity(ent);
}

//======================================================================

static bool Pickup_Powerup(edict_t *ent, edict_t *other)
{
    ent->item->use(other, ent->item);

    if (deathmatch.integer && !(ent->spawnflags & DROPPED_ITEM)) {
        SetRespawn(ent, ent->item->quantity);
    }

    return true;
}

static void Drop_General(edict_t *ent, gitem_t *item)
{
    Drop_Item(ent, item);
    ent->client->pers.inventory[item->id]--;
    ValidateSelectedItem(ent);
}

//======================================================================

static void Use_Quad(edict_t *ent, gitem_t *item)
{
    ent->client->quad_time = max(ent->client->quad_time, level.time) + 30000;
    SV_StartSound(ent, CHAN_ITEM, SV_SoundIndex(ASSET_SOUND_QUAD_ACTIVATE), 1, ATTN_NORM, 0);
}

//======================================================================

static void Use_Radsuit(edict_t *ent, gitem_t *item)
{
    ent->client->enviro_time = max(ent->client->enviro_time, level.time) + 30000;
    SV_StartSound(ent, CHAN_ITEM, SV_SoundIndex(ASSET_SOUND_BIOSUIT_ACTIVATE), 1, ATTN_NORM, 0);
}

//======================================================================

static void Use_Pentagram(edict_t *ent, gitem_t *item)
{
    ent->client->invincible_time = max(ent->client->invincible_time, level.time) + 30000;
    SV_StartSound(ent, CHAN_ITEM, SV_SoundIndex(ASSET_SOUND_PENT_ACTIVATE), 1, ATTN_NORM, 0);
}

//======================================================================

static bool Pickup_Key(edict_t *ent, edict_t *other)
{
    if (coop.integer) {
        if (other->client->pers.inventory[ent->item->id])
            return false;

        other->client->pers.inventory[ent->item->id] = 1;
        return true;
    }

    other->client->pers.inventory[ent->item->id]++;
    return true;
}

//======================================================================

bool Add_Ammo(edict_t *ent, gitem_t *item, int count)
{
    int         max;

    if (!ent->client)
        return false;

    gitem_id_t index = item->id;

    if (index == ITEM_SHELLS)
        max = ent->client->pers.max_shells;
    else if (index == ITEM_NAILS)
        max = ent->client->pers.max_nails;
    else if (index == ITEM_CELLS)
        max = ent->client->pers.max_cells;
    else if (index == ITEM_ROCKETS)
        max = ent->client->pers.max_rockets;
    else if (index == ITEM_GRENADES)
        max = ent->client->pers.max_grenades;
    else
        return false;

    if (ent->client->pers.inventory[index] == max)
        return false;

    ent->client->pers.inventory[index] += count;

    if (ent->client->pers.inventory[index] > max)
        ent->client->pers.inventory[index] = max;

    return true;
}

static bool Pickup_Ammo(edict_t *ent, edict_t *other)
{
    int count;

    if (ent->count)
        count = ent->count;
    else
        count = ent->item->quantity;

    int oldcount = other->client->pers.inventory[ent->item->id];

    if (!Add_Ammo(other, ent->item, count))
        return false;

    if (!(ent->spawnflags & (DROPPED_ITEM | DROPPED_PLAYER_ITEM)) && deathmatch.integer)
        SetRespawn(ent, 30);

    return true;
}

static void Drop_Ammo(edict_t *ent, gitem_t *item)
{
    edict_t *dropped;
    gitem_id_t index = item->id;

    dropped = Drop_Item(ent, item);

    if (ent->client->pers.inventory[index] >= item->quantity)
        dropped->count = item->quantity;
    else
        dropped->count = ent->client->pers.inventory[index];

    ent->client->pers.inventory[index] -= dropped->count;
    ValidateSelectedItem(ent);
}


//======================================================================

void MegaHealth_think(edict_t *self)
{
    if (self->owner->health > self->owner->max_health) {
        self->nextthink = level.time + 1000;
        self->owner->health -= 1;
        return;
    }

    if (!(self->spawnflags & DROPPED_ITEM) && deathmatch.integer)
        SetRespawn(self, 20);
    else
        G_FreeEdict(self);
}

static bool Pickup_Health(edict_t *ent, edict_t *other)
{
    if (!(ent->item->flags & IT_HEALTH_IGNORE_MAX))
        if (other->health >= other->max_health)
            return false;

    other->health += ent->count ? ent->count : ent->item->quantity;

    if (!(ent->item->flags & IT_HEALTH_IGNORE_MAX)) {
        if (other->health > other->max_health)
            other->health = other->max_health;
    }

    if (ent->item->flags & IT_HEALTH_TIMED) {
        ent->think = MegaHealth_think;
        ent->nextthink = level.time + 5000;
        ent->owner = other;
        ent->flags |= FL_RESPAWN;
        ent->svflags |= SVF_NOCLIENT;
        ent->solid = SOLID_NOT;
    } else {
        if (!(ent->spawnflags & DROPPED_ITEM) && deathmatch.integer)
            SetRespawn(ent, 30);
    }

    return true;
}

//======================================================================

gitem_id_t ArmorIndex(edict_t *ent)
{
    if (!ent->client)
        return ITEM_NULL;

    if (ent->client->pers.inventory[ITEM_ARMOR_GREEN] > 0)
        return ITEM_ARMOR_GREEN;
    else if (ent->client->pers.inventory[ITEM_ARMOR_YELLOW] > 0)
        return ITEM_ARMOR_YELLOW;
    else if (ent->client->pers.inventory[ITEM_ARMOR_RED] > 0)
        return ITEM_ARMOR_RED;

    return ITEM_NULL;
}

static bool Pickup_Armor(edict_t *ent, edict_t *other)
{
    int             old_armor_index;
    gitem_armor_t   *oldinfo;
    gitem_armor_t   *newinfo;
    int             newcount;
    float           salvage;
    int             salvagecount;

    // get info on new armor
    newinfo = ent->item->armor;

    old_armor_index = ArmorIndex(other);

    // handle armor shards specially
    if (ent->item->id == ITEM_ARMOR_SHARD) {
        if (!old_armor_index)
            other->client->pers.inventory[ITEM_ARMOR_GREEN] = ent->item->quantity;
        else
            other->client->pers.inventory[old_armor_index] += ent->item->quantity;
    }

    // if player has no armor, just use it
    else if (!old_armor_index) {
        other->client->pers.inventory[ent->item->id] = newinfo->base_count;
    }

    // use the better armor
    else {
        // get info on old armor
        if (old_armor_index == ITEM_ARMOR_GREEN)
            oldinfo = &greenarmor_info;
        else if (old_armor_index == ITEM_ARMOR_YELLOW)
            oldinfo = &yellowarmor_info;
        else
            oldinfo = &redarmor_info;

        if (newinfo->normal_protection > oldinfo->normal_protection) {
            // calc new armor values
            salvage = oldinfo->normal_protection / newinfo->normal_protection;
            salvagecount = salvage * other->client->pers.inventory[old_armor_index];
            newcount = newinfo->base_count + salvagecount;
            if (newcount > newinfo->max_count)
                newcount = newinfo->max_count;

            // zero count of old armor so it goes away
            other->client->pers.inventory[old_armor_index] = 0;

            // change armor to new item with computed value
            other->client->pers.inventory[ent->item->id] = newcount;
        } else {
            // calc new armor values
            salvage = newinfo->normal_protection / oldinfo->normal_protection;
            salvagecount = salvage * newinfo->base_count;
            newcount = other->client->pers.inventory[old_armor_index] + salvagecount;
            if (newcount > oldinfo->max_count)
                newcount = oldinfo->max_count;

            // if we're already maxed out then we don't need the new armor
            if (other->client->pers.inventory[old_armor_index] >= newcount)
                return false;

            // update current armor value
            other->client->pers.inventory[old_armor_index] = newcount;
        }
    }

    if (!(ent->spawnflags & DROPPED_ITEM) && deathmatch.integer)
        SetRespawn(ent, 20);

    return true;
}

//======================================================================

/*
===============
Touch_Item
===============
*/
void Touch_Item(edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf)
{
    bool    taken;

    if (!other->client)
        return;
    if (other->health < 1)
        return;     // dead people can't pickup
    if (!ent->item->pickup)
        return;     // not a grabbable item?

    taken = ent->item->pickup(ent, other);

    if (taken) {
        // flash the screen
        other->client->bonus_alpha = 0.25f;

        // show icon and name on status bar
        other->client->ps.stats[STAT_PICKUP_ICON] = SV_ImageIndex(ent->item->icon);
        other->client->ps.stats[STAT_PICKUP_STRING] = CS_ITEMS + ent->item->id;
        other->client->pickup_msg_time = level.time + 3000;

        // change selected item
        if (ent->item->use && other->client->pers.inventory[ent->item->id])
            other->client->pers.selected_item = other->client->ps.stats[STAT_SELECTED_ITEM] = ent->item->id;

        if (ent->item->pickup_sound) {
            SV_StartSound(other, CHAN_ITEM, SV_SoundIndex(ent->item->pickup_sound), 1, ATTN_NORM, 0);
        }
    }

    if (!(ent->spawnflags & ITEM_TARGETS_USED)) {
        G_UseTargets(ent, other);
        ent->spawnflags |= ITEM_TARGETS_USED;
    }

    if (!taken)
        return;

    if (!(coop.integer && (ent->item->flags & IT_STAY_COOP)) || (ent->spawnflags & (DROPPED_ITEM | DROPPED_PLAYER_ITEM))) {
        if (ent->flags & FL_RESPAWN)
            ent->flags &= ~FL_RESPAWN;
        else
            G_FreeEdict(ent);
    }
}

//======================================================================

static bool Pickup_Weapon(edict_t *ent, edict_t *other)
{
    gitem_t     *ammo;

    gitem_id_t index = ent->item->id;

    if (((dmflags.integer & DF_WEAPONS_STAY) || coop.integer)
        && other->client->pers.inventory[index]) {
        if (!(ent->spawnflags & (DROPPED_ITEM | DROPPED_PLAYER_ITEM)))
            return false;   // leave the weapon for others to pickup
    }

    if (!other->client->pers.inventory[index]) {
        other->client->inspect = true;
    }

    other->client->pers.inventory[index]++;

    if (!(ent->spawnflags & DROPPED_ITEM)) {

        if (ent->item->ammo) {
            // give them some ammo with it
            ammo = GetItemByIndex(ent->item->ammo);

            if (dmflags.integer & DF_INFINITE_AMMO)
                Add_Ammo(other, ammo, 1000);
            else
                Add_Ammo(other, ammo, ammo->quantity);
        }

        if (!(ent->spawnflags & DROPPED_PLAYER_ITEM)) {
            if (deathmatch.integer) {
                if (dmflags.integer & DF_WEAPONS_STAY)
                    ent->flags |= FL_RESPAWN;
                else
                    SetRespawn(ent, 30);
            }
            if (coop.integer)
                ent->flags |= FL_RESPAWN;
        }
    }

    if (other->client->pers.weapon != ent->item &&
        (other->client->pers.inventory[index] == 1) &&
        (!deathmatch.integer || (other->client->pers.weapon && other->client->pers.weapon->id == ITEM_AXE)))
        other->client->newweapon = ent->item;

    return true;
}

/*
================
Use_Weapon

Make the weapon ready if there is ammo
================
*/
static void Use_Weapon(edict_t *ent, gitem_t *item)
{
    // see if we're already using it
    if (item == ent->client->pers.weapon)
        return;

    if (item->ammo && !g_select_empty.integer && !(item->flags & IT_AMMO)) {
        gitem_id_t ammo_index = item->ammo;
        gitem_t *ammo_item = GetItemByIndex(ammo_index);

        if (!ent->client->pers.inventory[ammo_index]) {
            SV_ClientPrintf(ent, PRINT_HIGH, "No %s for %s.\n", ammo_item->pickup_name, item->pickup_name);
            return;
        }

        if (ent->client->pers.inventory[ammo_index] < item->quantity) {
            SV_ClientPrintf(ent, PRINT_HIGH, "Not enough %s for %s.\n", ammo_item->pickup_name, item->pickup_name);
            return;
        }
    }

    // change to this weapon when down
    ent->client->newweapon = item;
}

/*
================
Drop_Weapon
================
*/
static void Drop_Weapon(edict_t *ent, gitem_t *item)
{
    if (dmflags.integer & DF_WEAPONS_STAY)
        return;

    gitem_id_t index = item->id;

    // see if we're already using it
    if (((item == ent->client->pers.weapon) || (item == ent->client->newweapon)) && (ent->client->pers.inventory[index] == 1)) {
        SV_ClientPrint(ent, PRINT_HIGH, "Can't drop current weapon\n");
        return;
    }

    Drop_Item(ent, item);
    ent->client->pers.inventory[index]--;
}

//======================================================================

void drop_temp_touch(edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf)
{
    if (other == ent->owner)
        return;

    Touch_Item(ent, other, plane, surf);
}

void drop_make_touchable(edict_t *ent)
{
    ent->touch = Touch_Item;
    if (deathmatch.integer) {
        ent->nextthink = level.time + G_SecToMs(29);
        ent->think = G_FreeEdict;
    }
}

edict_t *Drop_Item(edict_t *ent, gitem_t *item)
{
    edict_t *dropped;
    vec3_t  forward, right;
    vec3_t  offset;

    dropped = G_Spawn();

    dropped->classname = item->classname;
    dropped->item = item;
    dropped->spawnflags = DROPPED_ITEM;
    dropped->s.effects = item->world_model_flags;
    dropped->s.renderfx = RF_GLOW;
    VectorSet(dropped->mins, -15, -15, -15);
    VectorSet(dropped->maxs, 15, 15, 15);
    dropped->s.modelindex = SV_ModelIndex(dropped->item->world_model);
    dropped->solid = SOLID_TRIGGER;
    dropped->movetype = MOVETYPE_TOSS;
    dropped->touch = drop_temp_touch;
    dropped->owner = ent;

    if (ent->client) {
        trace_t trace;

        AngleVectors(ent->client->v_angle, forward, right, NULL);
        VectorSet(offset, 24, 0, -16);
        G_ProjectSource(ent->s.origin, offset, forward, right, dropped->s.origin);
        trace = SV_Trace(ent->s.origin, dropped->mins, dropped->maxs,
                         dropped->s.origin, ent, CONTENTS_SOLID);
        VectorCopy(trace.endpos, dropped->s.origin);
    } else {
        AngleVectors(ent->s.angles, forward, right, NULL);
        VectorCopy(ent->s.origin, dropped->s.origin);
    }

    VectorScale(forward, 100, dropped->velocity);
    dropped->velocity[2] = 300;

    dropped->think = drop_make_touchable;
    dropped->nextthink = level.time + 1000;

    SV_LinkEntity(dropped);

    return dropped;
}

void Use_Item(edict_t *ent, edict_t *other, edict_t *activator)
{
    ent->svflags &= ~SVF_NOCLIENT;
    ent->use = NULL;

    if (ent->spawnflags & ITEM_NO_TOUCH) {
        ent->solid = SOLID_BBOX;
        ent->touch = NULL;
    } else {
        ent->solid = SOLID_TRIGGER;
        ent->touch = Touch_Item;
    }

    SV_LinkEntity(ent);
}

//======================================================================

/*
================
droptofloor
================
*/
void droptofloor(edict_t *ent)
{
    trace_t     tr;
    vec3_t      dest;
    float       *v;

    v = tv(-15, -15, -15);
    VectorCopy(v, ent->mins);
    v = tv(15, 15, 15);
    VectorCopy(v, ent->maxs);

    if (ent->model)
        ent->s.modelindex = SV_ModelIndex(ent->model);
    else
        ent->s.modelindex = SV_ModelIndex(ent->item->world_model);
    ent->solid = SOLID_TRIGGER;
    ent->movetype = MOVETYPE_TOSS;
    ent->touch = Touch_Item;

    v = tv(0, 0, -128);
    VectorAdd(ent->s.origin, v, dest);

    tr = SV_Trace(ent->s.origin, ent->mins, ent->maxs, dest, ent, MASK_SOLID);
    if (tr.startsolid) {
        Com_WPrintf("droptofloor: %s startsolid at %s\n", ent->classname, vtos(ent->s.origin));
        G_FreeEdict(ent);
        return;
    }

    VectorCopy(tr.endpos, ent->s.origin);

    if (ent->team) {
        ent->flags &= ~FL_TEAMSLAVE;
        ent->chain = ent->teamchain;
        ent->teamchain = NULL;

        ent->svflags |= SVF_NOCLIENT;
        ent->solid = SOLID_NOT;
        if (ent == ent->teammaster) {
            ent->nextthink = level.time + 1;
            ent->think = DoRespawn;
        }
    }

    if (ent->spawnflags & ITEM_NO_TOUCH) {
        ent->solid = SOLID_BBOX;
        ent->touch = NULL;
        ent->s.effects &= ~EF_ROTATE;
        ent->s.renderfx &= ~RF_GLOW;
    }

    if (ent->spawnflags & ITEM_TRIGGER_SPAWN) {
        ent->svflags |= SVF_NOCLIENT;
        ent->solid = SOLID_NOT;
        ent->use = Use_Item;
    }

    SV_LinkEntity(ent);
}


/*
===============
PrecacheItem

Precaches all data needed for a given item.
This will be called for each item spawned in a level,
and for each item in each client's inventory.
===============
*/
void PrecacheItem(gitem_t *it)
{
    char    *s, *start;
    char    data[MAX_QPATH];
    int     len;
    gitem_t *ammo;

    if (!it)
        return;

    if (it->pickup_sound)
        SV_SoundIndex(it->pickup_sound);
    if (it->world_model)
        SV_ModelIndex(it->world_model);
    if (it->view_model)
        SV_ModelIndex(it->view_model);
    if (it->icon)
        SV_ImageIndex(it->icon);

    // parse everything for its ammo
    if (it->ammo) {
        ammo = GetItemByIndex(it->ammo);
        if (ammo != it) {
            PrecacheItem(ammo);
        }
    }

    // parse the space seperated precache string for other items
    s = it->precaches;
    if (!s || !s[0])
        return;

    while (*s) {
        start = s;
        while (*s && *s != ' ')
            s++;

        len = s - start;
        if (len >= MAX_QPATH || len < 5)
            Com_Errorf(ERR_DROP, "PrecacheItem: %s has bad precache string", it->classname);
        memcpy(data, start, len);
        data[len] = 0;
        if (*s)
            s++;

        // determine type based on extension
        if (!strcmp(data + len - 3, "md2") ||
            !strcmp(data + len - 3, "md3") ||
            !strcmp(data + len - 3, "iqm") ||
            !strcmp(data + len - 3, "sp2"))
            SV_ModelIndex(data);
        else if (!strcmp(data + len - 3, "wav"))
            SV_SoundIndex(data);
        else if (!strcmp(data + len - 3, "pcx") ||
                 !strcmp(data + len - 3, "tga") ||
                 !strcmp(data + len - 3, "png"))
            SV_ImageIndex(data);
    }
}

/*
============
SpawnItem

Sets the clipping size and plants the object on the floor.

Items can't be immediately dropped to floor, because they might
be on an entity that hasn't spawned yet.
============
*/
void SpawnItem(edict_t *ent, gitem_t *item)
{
    PrecacheItem(item);

    if (ent->spawnflags) {
        if (strcmp(ent->classname, "key_power_cube") != 0) {
            ent->spawnflags = 0;
            Com_WPrintf("%s at %s has invalid spawnflags set\n", ent->classname, vtos(ent->s.origin));
        }
    }

    // some items will be prevented in deathmatch
    if (deathmatch.integer) {
        if (dmflags.integer & DF_NO_ARMOR) {
            if (item->flags & IT_ARMOR) {
                G_FreeEdict(ent);
                return;
            }
        }
        if (dmflags.integer & DF_NO_ITEMS) {
            if (item->flags & IT_POWERUP) {
                G_FreeEdict(ent);
                return;
            }
        }
        if (dmflags.integer & DF_NO_HEALTH) {
            if (item->flags & IT_HEALTH) {
                G_FreeEdict(ent);
                return;
            }
        }
        if (dmflags.integer & DF_INFINITE_AMMO) {
            if (item->flags & IT_AMMO) {
                G_FreeEdict(ent);
                return;
            }
        }
    }

    // don't let them drop items that stay in a coop game
    // FIXME: move this so we don't modify itemlist
    if (coop.integer && (item->flags & IT_STAY_COOP)) {
        item->drop = NULL;
    }

    ent->item = item;
    ent->nextthink = level.time + G_FramesToMs(2);    // items start after other solids
    ent->think = droptofloor;
    ent->s.effects = item->world_model_flags;
    ent->s.renderfx = RF_GLOW;
    ent->s.skinnum = item->skinnum;
    if (ent->model)
        SV_ModelIndex(ent->model);
}

//======================================================================

static gitem_t itemlist[] = {
    [ITEM_NULL] = { 0 },  // leave index 0 alone

    //
    // ARMOR
    //

    /*QUAKED item_armor_red (.3 .3 1) (-16 -16 -16) (16 16 16)
    */
    [ITEM_ARMOR_RED] = {
        .classname = "item_armor_red",
        .pickup = Pickup_Armor,
        .pickup_sound = ASSET_SOUND_ARMOR_PICKUP,
        .world_model = ASSET_MODEL_ARMOR, .world_model_flags = EF_ROTATE,
        .skinnum = 2,
        .icon = "i_bodyarmor",
        .pickup_name = "Red Armor",
        .flags = IT_ARMOR,
        .armor = &redarmor_info
    },

    /*QUAKED item_armor_yellow (.3 .3 1) (-16 -16 -16) (16 16 16)
    */
    [ITEM_ARMOR_YELLOW] = {
        .classname = "item_armor_yellow",
        .pickup = Pickup_Armor,
        .pickup_sound = ASSET_SOUND_ARMOR_PICKUP,
        .world_model = ASSET_MODEL_ARMOR, .world_model_flags = EF_ROTATE,
        .icon = "i_combatarmor",
        .pickup_name = "Yellow Armor",
        .flags = IT_ARMOR,
        .armor = &yellowarmor_info
    },

    /*QUAKED item_armor_green (.3 .3 1) (-16 -16 -16) (16 16 16)
    */
    [ITEM_ARMOR_GREEN] = {
        .classname = "item_armor_green",
        .pickup = Pickup_Armor,
        .pickup_sound = ASSET_SOUND_ARMOR_PICKUP,
        .world_model = ASSET_MODEL_ARMOR, .world_model_flags = EF_ROTATE,
        .skinnum = 1,
        .icon = "i_jacketarmor",
        .pickup_name = "Green Armor",
        .flags = IT_ARMOR,
        .armor = &greenarmor_info
    },

    /*QUAKED item_armor_shard (.3 .3 1) (-16 -16 -16) (16 16 16)
    */
    [ITEM_ARMOR_SHARD] = {
        .classname = "item_armor_shard",
        .pickup = Pickup_Armor,
        .pickup_sound = ASSET_SOUND_ARMOR_SHARD_PICKUP,
        .world_model = ASSET_MODEL_ARMOR_SHARD, .world_model_flags = EF_ROTATE,
        .icon = "i_jacketarmor",
        .pickup_name = "Armor Shard",
        .flags = IT_ARMOR,
        .quantity = 2
    },

    //
    // WEAPONS
    //
    /* weapon_axe (.3 .3 1) (-16 -16 -16) (16 16 16)
    */
    [ITEM_AXE] = {
        .classname = "weapon_axe",
        .pickup = Pickup_Weapon,
        .use = Use_Weapon,
        .drop = Drop_Weapon,
        .animation = &weap_axe_activate,
        .pickup_sound = ASSET_SOUND_WEAPON_PICKUP,
        .world_model = ASSET_MODEL_AXE_WORLD, .world_model_flags = EF_ROTATE,
        .view_model = ASSET_MODEL_AXE_VIEW,
        .icon = "w_axe",
        .pickup_name = "Axe",
        .quantity = 1,
        .flags = IT_WEAPON | IT_STAY_COOP,
        .weapmodel = WEAP_BLASTER,
        .weapid = WEAPID_AXE
    },

    /*QUAKED weapon_shotgun (.3 .3 1) (-16 -16 -16) (16 16 16)
    */
    [ITEM_SHOTGUN] = {
        .classname = "weapon_shotgun",
        .pickup = Pickup_Weapon,
        .use = Use_Weapon,
        .drop = Drop_Weapon,
        .animation = &weap_shotgun_activate,
        .pickup_sound = ASSET_SOUND_WEAPON_PICKUP,
        .world_model = ASSET_MODEL_SHOTGUN_WORLD, .world_model_flags = EF_ROTATE,
        .view_model = ASSET_MODEL_SHOTGUN_VIEW,
        .icon = "w_shotgun",
        .pickup_name = "Shotgun",
        .quantity = 1,
        .ammo = ITEM_SHELLS,
        .flags = IT_WEAPON | IT_STAY_COOP,
        .weapmodel = WEAP_SHOTGUN
    },

    /*QUAKED weapon_supershotgun (.3 .3 1) (-16 -16 -16) (16 16 16)
    */
    [ITEM_SSHOTGUN] = {
        .classname = "weapon_supershotgun",
        .pickup = Pickup_Weapon,
        .use = Use_Weapon,
        .drop = Drop_Weapon,
        .animation = &weap_sshotgun_activate,
        .pickup_sound = ASSET_SOUND_WEAPON_PICKUP,
        .world_model = ASSET_MODEL_SSHOTGUN_WORLD, .world_model_flags = EF_ROTATE,
        .view_model = ASSET_MODEL_SSHOTGUN_VIEW,
        .icon = "w_sshotgun",
        .pickup_name = "Double-Barreled Shotgun",
        .quantity = 2,
        .ammo = ITEM_SHELLS,
        .flags = IT_WEAPON | IT_STAY_COOP,
        .weapmodel = WEAP_SUPERSHOTGUN
    },

    /*QUAKED weapon_blaster (.3 .3 1) (-16 -16 -16) (16 16 16)
    */
    [ITEM_BLASTER] = {
        .classname = "weapon_blaster",
        .pickup = Pickup_Weapon,
        .use = Use_Weapon,
        .drop = Drop_Weapon,
        .animation = &weap_blaster_activate,
        .pickup_sound = ASSET_SOUND_WEAPON_PICKUP,
        .world_model = ASSET_MODEL_BLASTER_WORLD, .world_model_flags = EF_ROTATE,
        .view_model = ASSET_MODEL_BLASTER_VIEW,
        .icon = "w_hyperb",
        .pickup_name = "Blaster",
        .quantity = 1,
        .ammo = ITEM_CELLS,
        .flags = IT_WEAPON | IT_STAY_COOP,
        .weapmodel = WEAP_HYPERBLASTER
    },

    /*QUAKED weapon_nailgun (.3 .3 1) (-16 -16 -16) (16 16 16)
    */
    [ITEM_NAILGUN] = {
        .classname = "weapon_nailgun",
        .pickup = Pickup_Weapon,
        .use = Use_Weapon,
        .drop = Drop_Weapon,
        .animation = &weap_nailgun_activate,
        .pickup_sound = ASSET_SOUND_WEAPON_PICKUP,
        .world_model = ASSET_MODEL_NAILGUN_WORLD, .world_model_flags = EF_ROTATE,
        .view_model = ASSET_MODEL_NAILGUN_VIEW,
        .icon = "w_nailg",
        .pickup_name = "Nailgun",
        .quantity = 1,
        .ammo = ITEM_NAILS,
        .flags = IT_WEAPON | IT_STAY_COOP,
        .weapmodel = WEAP_MACHINEGUN,
        .precaches = ASSET_MODEL_NAIL,
    },

    /*QUAKED weapon_perforator (.3 .3 1) (-16 -16 -16) (16 16 16)
    */
    [ITEM_PERFORATOR] = {
        .classname = "weapon_perforator",
        .pickup = Pickup_Weapon,
        .use = Use_Weapon,
        .drop = Drop_Weapon,
        .animation = &weap_perf_activate,
        .pickup_sound = ASSET_SOUND_WEAPON_PICKUP,
        .world_model = ASSET_MODEL_PERFORATOR_WORLD, .world_model_flags = EF_ROTATE,
        .view_model = ASSET_MODEL_PERFORATOR_VIEW,
        .icon = "w_perf",
        .pickup_name = "Perforator",
        .quantity = 1,
        .ammo = ITEM_NAILS,
        .flags = IT_WEAPON | IT_STAY_COOP,
        .weapmodel = WEAP_CHAINGUN,
        .precaches = ASSET_SOUND_PERFORATOR_SPIN " " ASSET_MODEL_NAIL,
    },

    /*QUAKED weapon_grenadelauncher (.3 .3 1) (-16 -16 -16) (16 16 16)
    */
    [ITEM_GRENADE_LAUNCHER] = {
        .classname = "weapon_grenadelauncher",
        .pickup = Pickup_Weapon,
        .use = Use_Weapon,
        .drop = Drop_Weapon,
        .animation = &weap_launch_activate,
        .pickup_sound = ASSET_SOUND_WEAPON_PICKUP,
        .world_model = ASSET_MODEL_LAUNCH_WORLD, .world_model_flags = EF_ROTATE,
        .view_model = ASSET_MODEL_LAUNCH_VIEW,
        .icon = "w_launch",
        .pickup_name = "Grenade Launcher",
        .quantity = 1,
        .ammo = ITEM_GRENADES,
        .flags = IT_WEAPON | IT_STAY_COOP,
        .weapmodel = WEAP_GRENADELAUNCHER,
        .precaches = ASSET_MODEL_GRENADE " " ASSET_SOUND_GRENADE_BOUNCE " " ASSET_SOUND_GRENADE_FIRE " " ASSET_SOUND_GRENADE_EXPLODE,
    },

    /*QUAKED weapon_rocketlauncher (.3 .3 1) (-16 -16 -16) (16 16 16)
    */
    [ITEM_ROCKET_LAUNCHER] = {
        .classname = "weapon_rocketlauncher",
        .pickup = Pickup_Weapon,
        .use = Use_Weapon,
        .drop = Drop_Weapon,
        .animation = &weap_rocket_activate,
        .pickup_sound = ASSET_SOUND_WEAPON_PICKUP,
        .world_model = ASSET_MODEL_ROCKET_WORLD, .world_model_flags = EF_ROTATE,
        .view_model = ASSET_MODEL_ROCKET_VIEW,
        .icon = "w_rocket",
        .pickup_name = "Rocket Launcher",
        .quantity = 1,
        .ammo = ITEM_ROCKETS,
        .flags = IT_WEAPON | IT_STAY_COOP,
        .weapmodel = WEAP_ROCKETLAUNCHER,
        .precaches = ASSET_MODEL_ROCKET " " ASSET_SOUND_ROCKET_FLY " " ASSET_SOUND_ROCKET_FIRE " " ASSET_SOUND_ROCKET_EXPLODE,
    },

    /*QUAKED weapon_thunderbolt (.3 .3 1) (-16 -16 -16) (16 16 16)
    */
    [ITEM_THUNDERBOLT] = {
        .classname = "weapon_thunderbolt",
        .pickup = Pickup_Weapon,
        .use = Use_Weapon,
        .drop = Drop_Weapon,
        .animation = &weap_thunder_activate,
        .pickup_sound = ASSET_SOUND_WEAPON_PICKUP,
        .world_model = ASSET_MODEL_THUNDER_WORLD, .world_model_flags = EF_ROTATE,
        .view_model = ASSET_MODEL_THUNDER_VIEW,
        .icon = "w_thunder",
        .pickup_name = "Lightning Gun",
        .quantity = 1,
        .ammo = ITEM_CELLS,
        .flags = IT_WEAPON | IT_STAY_COOP,
        .weapmodel = WEAP_HYPERBLASTER
        },

    //
    // AMMO ITEMS
    //

    /*QUAKED ammo_shells (.3 .3 1) (-16 -16 -16) (16 16 16)
    */
    [ITEM_SHELLS] = {
        .classname = "ammo_shells",
        .pickup = Pickup_Ammo,
        .drop = Drop_Ammo,
        .pickup_sound = ASSET_SOUND_AMMO_PICKUP,
        .world_model = ASSET_MODEL_SHELLS,
        .icon = "a_shells",
        .pickup_name = "Shells",
        .quantity = 10,
        .flags = IT_AMMO
    },

    /*QUAKED ammo_nails (.3 .3 1) (-16 -16 -16) (16 16 16)
    */
    [ITEM_NAILS] = {
        .classname = "ammo_nails",
        .pickup = Pickup_Ammo,
        .drop = Drop_Ammo,
        .pickup_sound = ASSET_SOUND_AMMO_PICKUP,
        .world_model = ASSET_MODEL_NAILS,
        .icon = "a_nails",
        .pickup_name = "Nails",
        .quantity = 50,
        .flags = IT_AMMO
    },

    /*QUAKED ammo_cells (.3 .3 1) (-16 -16 -16) (16 16 16)
    */
    [ITEM_CELLS] = {
        .classname = "ammo_cells",
        .pickup = Pickup_Ammo,
        .drop = Drop_Ammo,
        .pickup_sound = ASSET_SOUND_AMMO_PICKUP,
        .world_model = ASSET_MODEL_CELLS,
        .icon = "a_cells",
        .pickup_name = "Cells",
        .quantity = 25,
        .flags = IT_AMMO
    },

    /*QUAKED ammo_rockets (.3 .3 1) (-16 -16 -16) (16 16 16)
    */
    [ITEM_ROCKETS] = {
        .classname = "ammo_rockets",
        .pickup = Pickup_Ammo,
        .drop = Drop_Ammo,
        .pickup_sound = ASSET_SOUND_AMMO_PICKUP,
        .world_model = ASSET_MODEL_ROCKETS,
        .icon = "a_rockets",
        .pickup_name = "Rockets",
        .quantity = 5,
        .flags = IT_AMMO
    },

    /*QUAKED ammo_grenades (.3 .3 1) (-16 -16 -16) (16 16 16)
    */
    [ITEM_GRENADES] = {
        .classname = "ammo_grenades",
        .pickup = Pickup_Ammo,
        .drop = Drop_Ammo,
        .pickup_sound = ASSET_SOUND_AMMO_PICKUP,
        .world_model = ASSET_MODEL_GRENADES,
        .icon = "a_grenades",
        .pickup_name = "Grenades",
        .quantity = 5,
        .flags = IT_AMMO
    },

    /*QUAKED key_silver_key (0 .5 .8) (-16 -16 -16) (16 16 16)
    normal door key - silver
    */
    [ITEM_KEY_SILVER] = {
        .classname = "key_silver_key",
        .pickup = Pickup_Key,
        .drop = Drop_General,
        .pickup_sound = ASSET_SOUND_GENERIC_PICKUP,
        .world_model = ASSET_MODEL_SILVER_KEY,
        .world_model_flags = EF_ROTATE,
        .icon = "k_silverkey",
        .pickup_name = "Silver Key",
        .flags = IT_STAY_COOP | IT_KEY,
    },

    /*QUAKED key_gold_key (0 .5 .8) (-16 -16 -16) (16 16 16)
   normal door key - gold
   */
   [ITEM_KEY_GOLD] = {
       .classname = "key_gold_key",
       .pickup = Pickup_Key,
       .drop = Drop_General,
       .pickup_sound = ASSET_SOUND_GENERIC_PICKUP,
       .world_model = ASSET_MODEL_GOLD_KEY,
       .world_model_flags = EF_ROTATE,
       .skinnum = 1,
       .icon = "k_goldkey",
       .pickup_name = "Gold Key",
       .flags = IT_STAY_COOP | IT_KEY,
    },

    /*QUAKED item_quad (0 .5 .8) (-16 -16 -16) (16 16 16)
    */
    [ITEM_QUAD_DAMAGE] = {
        .classname = "item_quad",
        .pickup = Pickup_Powerup,
        .use = Use_Quad,
        .world_model = ASSET_MODEL_QUAD_DAMAGE,
        .world_model_flags = EF_ROTATE,
        .icon = "p_quad",
        .pickup_name = "Quad Damage",
        .flags = IT_POWERUP,
        .precaches = ASSET_SOUND_QUAD_ACTIVATE " " ASSET_SOUND_QUAD_FADE
    },

    /*QUAKED item_invulnerability (0 .5 .8) (-16 -16 -16) (16 16 16)
    */
    [ITEM_PENTAGRAM] = {
        .classname = "item_invulnerability",
        .pickup = Pickup_Powerup,
        .use = Use_Pentagram,
        .world_model = ASSET_MODEL_PENT,
        .world_model_flags = EF_ROTATE,
        .icon = "p_pent",
        .pickup_name = "Pentagram of Protection",
        .flags = IT_POWERUP,
        .precaches = ASSET_SOUND_PENT_ACTIVATE " " ASSET_SOUND_PENT_HIT " " ASSET_SOUND_PENT_FADE
    },

    /*QUAKED item_biosuit (0 .5 .8) (-16 -16 -16) (16 16 16)
    */
    [ITEM_BIOSUIT] = {
        .classname = "item_biosuit",
        .pickup = Pickup_Powerup,
        .use = Use_Radsuit,
        .world_model = ASSET_MODEL_BIOSUIT,
        .world_model_flags = EF_ROTATE,
        .icon = "p_suit",
        .pickup_name = "Biosuit",
        .flags = IT_POWERUP,
        .precaches = ASSET_SOUND_BIOSUIT_ACTIVATE " " ASSET_SOUND_BIOSUIT_FADE
    },

    /*QUAKED item_ring (0 .5 .8) (-16 -16 -16) (16 16 16)
    */
    [ITEM_RING_OF_SHADOWS] = {
        .classname = "item_ring",
        .pickup = Pickup_Powerup,
        .use = Use_Radsuit,
        .world_model = ASSET_MODEL_RING,
        .world_model_flags = EF_ROTATE,
        .icon = "p_ring",
        .pickup_name = "Ring of Shadows",
        .flags = IT_POWERUP
    },

    /*QUAKED item_health (0 .5 .8) (-16 -16 -16) (16 16 16)
   */
    [ITEM_HEALTH_SMALL] = {
        .classname = "item_health_small",
        .pickup = Pickup_Health,
        .pickup_sound = ASSET_SOUND_HEALTH_SMALL_PICKUP,
        .world_model = ASSET_MODEL_HEALTH_SMALL,
        .icon = "i_health",
        .pickup_name = "Health",
        .flags = IT_HEALTH | IT_HEALTH_IGNORE_MAX,
        .quantity = 2
    },

    /*QUAKED item_health_rotten (0 .5 .8) (-16 -16 -16) (16 16 16)
    */
    [ITEM_HEALTH_ROTTEN] = {
        .classname = "item_health_rotten",
        .pickup = Pickup_Health,
        .pickup_sound = ASSET_SOUND_HEALTH_ROTTEN_PICKUP,
        .world_model = ASSET_MODEL_HEALTH_ROTTEN,
        .icon = "i_health",
        .pickup_name = "Health",
        .flags = IT_HEALTH,
        .quantity = 15
    },

    /*QUAKED item_health (0 .5 .8) (-16 -16 -16) (16 16 16)
    */
    [ITEM_HEALTH] = {
        .classname = "item_health",
        .pickup = Pickup_Health,
        .pickup_sound = ASSET_SOUND_HEALTH_PICKUP,
        .world_model = ASSET_MODEL_HEALTH,
        .icon = "i_health",
        .pickup_name = "Health",
        .flags = IT_HEALTH,
        .quantity = 25
    },

    /*QUAKED item_health_mega (0 .5 .8) (-16 -16 -16) (16 16 16)
    */
    [ITEM_HEALTH_MEGA] = {
        .classname = "item_health_mega",
        .pickup = Pickup_Health,
        .pickup_sound = ASSET_SOUND_HEALTH_MEGA_PICKUP,
        .world_model = ASSET_MODEL_HEALTH_MEGA,
        .icon = "i_health",
        .pickup_name = "Mega Health",
        .flags = IT_HEALTH | IT_HEALTH_IGNORE_MAX | IT_HEALTH_TIMED,
        .quantity = 100
    },

    // end of list marker
    { 0 }
};

void InitItems(void)
{
    for (size_t i = 0; i < q_countof(itemlist); i++) {
        itemlist[i].id = i;
    }
}

/*
===============
SetItemNames

Called by worldspawn
===============
*/
void SetItemNames(void)
{
    for (gitem_id_t i = 1 ; i < ITEM_TOTAL ; i++) {
        SV_SetConfigString(CS_ITEMS + i, itemlist[i].pickup_name);
    }
}


//======================================================================

/*
===============
GetItemByIndex
===============
*/
gitem_t *GetItemByIndex(gitem_id_t index)
{
    if (index <= ITEM_NULL || index >= ITEM_TOTAL)
        return NULL;

    return &itemlist[index];
}


/*
===============
FindItemByClassname

===============
*/
gitem_t *FindItemByClassname(char *classname)
{
    gitem_t *it = itemlist;

    for (gitem_id_t i = 1; i < ITEM_TOTAL; i++, it++) {
        if (!it->classname)
            continue;
        if (!Q_stricmp(it->classname, classname))
            return it;
    }

    return NULL;
}

/*
===============
FindItem

===============
*/
gitem_t *FindItem(char *pickup_name)
{
    gitem_t *it = itemlist;

    for (gitem_id_t i = 1 ; i < ITEM_TOTAL; i++, it++) {
        if (!it->pickup_name)
            continue;
        if (!Q_stricmp(it->pickup_name, pickup_name))
            return it;
    }

    return NULL;
}