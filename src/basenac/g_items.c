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


bool        Pickup_Weapon(edict_t *ent, edict_t *other);
void        Use_Weapon(edict_t *ent, gitem_t *inv);
void        Drop_Weapon(edict_t *ent, gitem_t *inv);

extern const weapon_animation_t weap_axe_activate;
extern const weapon_animation_t weap_perf_activate;
extern const weapon_animation_t weap_shotgun_activate;

gitem_armor_t jacketarmor_info  = { 25,  50, .30, .00, ARMOR_JACKET};
gitem_armor_t combatarmor_info  = { 50, 100, .60, .30, ARMOR_COMBAT};
gitem_armor_t bodyarmor_info    = {100, 200, .80, .60, ARMOR_BODY};

static int  jacket_armor_index;
static int  combat_armor_index;
static int  body_armor_index;
static int  power_screen_index;
static int  power_shield_index;

#define HEALTH_IGNORE_MAX   1
#define HEALTH_TIMED        2

void Use_Quad(edict_t *ent, gitem_t *item);
static gtime_t quad_drop_timeout_hack_ms;

//======================================================================

/*
===============
GetItemByIndex
===============
*/
gitem_t *GetItemByIndex(int index)
{
    if (index == 0 || index >= game.num_items)
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
    int     i;
    gitem_t *it;

    it = itemlist;
    for (i = 0 ; i < game.num_items ; i++, it++) {
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
    int     i;
    gitem_t *it;

    it = itemlist;
    for (i = 0 ; i < game.num_items ; i++, it++) {
        if (!it->pickup_name)
            continue;
        if (!Q_stricmp(it->pickup_name, pickup_name))
            return it;
    }

    return NULL;
}

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

bool Pickup_Powerup(edict_t *ent, edict_t *other)
{
    int     quantity;

    quantity = other->client->pers.inventory[ITEM_INDEX(ent->item)];
    if ((skill.integer == 1 && quantity >= 2) || (skill.integer >= 2 && quantity >= 1))
        return false;

    if (coop.integer && (ent->item->flags & IT_STAY_COOP) && (quantity > 0))
        return false;

    other->client->pers.inventory[ITEM_INDEX(ent->item)]++;

    if (deathmatch.integer) {
        if (!(ent->spawnflags & DROPPED_ITEM))
            SetRespawn(ent, ent->item->quantity);
        if ((dmflags.integer & DF_INSTANT_ITEMS) || ((ent->item->use == Use_Quad) && (ent->spawnflags & DROPPED_PLAYER_ITEM))) {
            if ((ent->item->use == Use_Quad) && (ent->spawnflags & DROPPED_PLAYER_ITEM))
                quad_drop_timeout_hack_ms = ent->nextthink - level.time;
            ent->item->use(other, ent->item);
        }
    }

    return true;
}

void Drop_General(edict_t *ent, gitem_t *item)
{
    Drop_Item(ent, item);
    ent->client->pers.inventory[ITEM_INDEX(item)]--;
    ValidateSelectedItem(ent);
}


//======================================================================

bool Pickup_Adrenaline(edict_t *ent, edict_t *other)
{
    if (!deathmatch.integer)
        other->max_health += 1;

    if (other->health < other->max_health)
        other->health = other->max_health;

    if (!(ent->spawnflags & DROPPED_ITEM) && deathmatch.integer)
        SetRespawn(ent, ent->item->quantity);

    return true;
}

bool Pickup_AncientHead(edict_t *ent, edict_t *other)
{
    other->max_health += 2;

    if (!(ent->spawnflags & DROPPED_ITEM) && deathmatch.integer)
        SetRespawn(ent, ent->item->quantity);

    return true;
}

bool Pickup_Bandolier(edict_t *ent, edict_t *other)
{
    gitem_t *item;
    int     index;

    if (other->client->pers.max_nails < 250)
        other->client->pers.max_nails = 250;
    if (other->client->pers.max_shells < 150)
        other->client->pers.max_shells = 150;
    if (other->client->pers.max_cells < 250)
        other->client->pers.max_cells = 250;

    item = FindItem("nails");
    if (item) {
        index = ITEM_INDEX(item);
        other->client->pers.inventory[index] += item->quantity;
        if (other->client->pers.inventory[index] > other->client->pers.max_nails)
            other->client->pers.inventory[index] = other->client->pers.max_nails;
    }

    item = FindItem("Shells");
    if (item) {
        index = ITEM_INDEX(item);
        other->client->pers.inventory[index] += item->quantity;
        if (other->client->pers.inventory[index] > other->client->pers.max_shells)
            other->client->pers.inventory[index] = other->client->pers.max_shells;
    }

    if (!(ent->spawnflags & DROPPED_ITEM) && deathmatch.integer)
        SetRespawn(ent, ent->item->quantity);

    return true;
}

bool Pickup_Pack(edict_t *ent, edict_t *other)
{
    gitem_t *item;
    int     index;

    if (other->client->pers.max_nails < 300)
        other->client->pers.max_nails = 300;
    if (other->client->pers.max_shells < 200)
        other->client->pers.max_shells = 200;
    if (other->client->pers.max_rockets < 100)
        other->client->pers.max_rockets = 100;
    if (other->client->pers.max_cells < 300)
        other->client->pers.max_cells = 300;

    item = FindItem("nails");
    if (item) {
        index = ITEM_INDEX(item);
        other->client->pers.inventory[index] += item->quantity;
        if (other->client->pers.inventory[index] > other->client->pers.max_nails)
            other->client->pers.inventory[index] = other->client->pers.max_nails;
    }

    item = FindItem("Shells");
    if (item) {
        index = ITEM_INDEX(item);
        other->client->pers.inventory[index] += item->quantity;
        if (other->client->pers.inventory[index] > other->client->pers.max_shells)
            other->client->pers.inventory[index] = other->client->pers.max_shells;
    }

    item = FindItem("Cells");
    if (item) {
        index = ITEM_INDEX(item);
        other->client->pers.inventory[index] += item->quantity;
        if (other->client->pers.inventory[index] > other->client->pers.max_cells)
            other->client->pers.inventory[index] = other->client->pers.max_cells;
    }

    item = FindItem("Rockets");
    if (item) {
        index = ITEM_INDEX(item);
        other->client->pers.inventory[index] += item->quantity;
        if (other->client->pers.inventory[index] > other->client->pers.max_rockets)
            other->client->pers.inventory[index] = other->client->pers.max_rockets;
    }

    if (!(ent->spawnflags & DROPPED_ITEM) && deathmatch.integer)
        SetRespawn(ent, ent->item->quantity);

    return true;
}

//======================================================================

void Use_Quad(edict_t *ent, gitem_t *item)
{
    gtime_t timeout;

    ent->client->pers.inventory[ITEM_INDEX(item)]--;
    ValidateSelectedItem(ent);

    if (quad_drop_timeout_hack_ms) {
        timeout = quad_drop_timeout_hack_ms;
        quad_drop_timeout_hack_ms = 0;
    } else {
        timeout = 30000;
    }

    if (ent->client->quad_time > level.time)
        ent->client->quad_time += timeout;
    else
        ent->client->quad_time = level.time + timeout;

    SV_StartSound(ent, CHAN_ITEM, SV_SoundIndex("items/damage.wav"), 1, ATTN_NORM, 0);
}

//======================================================================

void Use_Breather(edict_t *ent, gitem_t *item)
{
    ent->client->pers.inventory[ITEM_INDEX(item)]--;
    ValidateSelectedItem(ent);

    if (ent->client->breather_time > level.time)
        ent->client->breather_time += 30000;
    else
        ent->client->breather_time = level.time + 30000;

//  SV_StartSound(ent, CHAN_ITEM, SV_SoundIndex("items/damage.wav"), 1, ATTN_NORM);
}

//======================================================================

void Use_Envirosuit(edict_t *ent, gitem_t *item)
{
    ent->client->pers.inventory[ITEM_INDEX(item)]--;
    ValidateSelectedItem(ent);

    if (ent->client->enviro_time > level.time)
        ent->client->enviro_time += 30000;
    else
        ent->client->enviro_time = level.time + 30000;

//  SV_StartSound(ent, CHAN_ITEM, SV_SoundIndex("items/damage.wav"), 1, ATTN_NORM);
}

//======================================================================

void    Use_Invulnerability(edict_t *ent, gitem_t *item)
{
    ent->client->pers.inventory[ITEM_INDEX(item)]--;
    ValidateSelectedItem(ent);

    if (ent->client->invincible_time > level.time)
        ent->client->invincible_time += 30000;
    else
        ent->client->invincible_time = level.time + 30000;

    SV_StartSound(ent, CHAN_ITEM, SV_SoundIndex("items/protect.wav"), 1, ATTN_NORM, 0);
}

//======================================================================

void    Use_Silencer(edict_t *ent, gitem_t *item)
{
    ent->client->pers.inventory[ITEM_INDEX(item)]--;
    ValidateSelectedItem(ent);
    ent->client->silencer_shots += 30;

//  SV_StartSound(ent, CHAN_ITEM, SV_SoundIndex("items/damage.wav"), 1, ATTN_NORM);
}

//======================================================================

bool Pickup_Key(edict_t *ent, edict_t *other)
{
    if (coop.integer) {
        if (strcmp(ent->classname, "key_power_cube") == 0) {
            if (other->client->pers.power_cubes & ((ent->spawnflags & 0x0000ff00) >> 8))
                return false;
            other->client->pers.inventory[ITEM_INDEX(ent->item)]++;
            other->client->pers.power_cubes |= ((ent->spawnflags & 0x0000ff00) >> 8);
        } else {
            if (other->client->pers.inventory[ITEM_INDEX(ent->item)])
                return false;
            other->client->pers.inventory[ITEM_INDEX(ent->item)] = 1;
        }
        return true;
    }
    other->client->pers.inventory[ITEM_INDEX(ent->item)]++;
    return true;
}

//======================================================================

bool Add_Ammo(edict_t *ent, gitem_t *item, int count)
{
    int         index;
    int         max;

    if (!ent->client)
        return false;

    if (item->tag == AMMO_NAILS)
        max = ent->client->pers.max_nails;
    else if (item->tag == AMMO_SHELLS)
        max = ent->client->pers.max_shells;
    else if (item->tag == AMMO_ROCKETS)
        max = ent->client->pers.max_rockets;
    else if (item->tag == AMMO_CELLS)
        max = ent->client->pers.max_cells;
    else
        return false;

    index = ITEM_INDEX(item);

    if (ent->client->pers.inventory[index] == max)
        return false;

    ent->client->pers.inventory[index] += count;

    if (ent->client->pers.inventory[index] > max)
        ent->client->pers.inventory[index] = max;

    return true;
}

bool Pickup_Ammo(edict_t *ent, edict_t *other)
{
    int         oldcount;
    int         count;
    bool        weapon;

    weapon = (ent->item->flags & IT_WEAPON);
    if (weapon && (dmflags.integer & DF_INFINITE_AMMO))
        count = 1000;
    else if (ent->count)
        count = ent->count;
    else
        count = ent->item->quantity;

    oldcount = other->client->pers.inventory[ITEM_INDEX(ent->item)];

    if (!Add_Ammo(other, ent->item, count))
        return false;

    if (weapon && !oldcount) {
        if (other->client->pers.weapon != ent->item && (!deathmatch.integer || other->client->pers.weapon == FindItem("axe")))
            other->client->newweapon = ent->item;
    }

    if (!(ent->spawnflags & (DROPPED_ITEM | DROPPED_PLAYER_ITEM)) && deathmatch.integer)
        SetRespawn(ent, 30);
    return true;
}

void Drop_Ammo(edict_t *ent, gitem_t *item)
{
    edict_t *dropped;
    int     index;

    index = ITEM_INDEX(item);
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

bool Pickup_Health(edict_t *ent, edict_t *other)
{
    if (!(ent->style & HEALTH_IGNORE_MAX))
        if (other->health >= other->max_health)
            return false;

    other->health += ent->count;

    if (!(ent->style & HEALTH_IGNORE_MAX)) {
        if (other->health > other->max_health)
            other->health = other->max_health;
    }

    if (ent->style & HEALTH_TIMED) {
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

int ArmorIndex(edict_t *ent)
{
    if (!ent->client)
        return 0;

    if (ent->client->pers.inventory[jacket_armor_index] > 0)
        return jacket_armor_index;

    if (ent->client->pers.inventory[combat_armor_index] > 0)
        return combat_armor_index;

    if (ent->client->pers.inventory[body_armor_index] > 0)
        return body_armor_index;

    return 0;
}

bool Pickup_Armor(edict_t *ent, edict_t *other)
{
    int             old_armor_index;
    gitem_armor_t   *oldinfo;
    gitem_armor_t   *newinfo;
    int             newcount;
    float           salvage;
    int             salvagecount;

    // get info on new armor
    newinfo = (gitem_armor_t *)ent->item->info;

    old_armor_index = ArmorIndex(other);

    // handle armor shards specially
    if (ent->item->tag == ARMOR_SHARD) {
        if (!old_armor_index)
            other->client->pers.inventory[jacket_armor_index] = 2;
        else
            other->client->pers.inventory[old_armor_index] += 2;
    }

    // if player has no armor, just use it
    else if (!old_armor_index) {
        other->client->pers.inventory[ITEM_INDEX(ent->item)] = newinfo->base_count;
    }

    // use the better armor
    else {
        // get info on old armor
        if (old_armor_index == jacket_armor_index)
            oldinfo = &jacketarmor_info;
        else if (old_armor_index == combat_armor_index)
            oldinfo = &combatarmor_info;
        else // (old_armor_index == body_armor_index)
            oldinfo = &bodyarmor_info;

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
            other->client->pers.inventory[ITEM_INDEX(ent->item)] = newcount;
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

int PowerArmorType(edict_t *ent)
{
    if (!ent->client)
        return POWER_ARMOR_NONE;

    if (!(ent->flags & FL_POWER_ARMOR))
        return POWER_ARMOR_NONE;

    if (ent->client->pers.inventory[power_shield_index] > 0)
        return POWER_ARMOR_SHIELD;

    if (ent->client->pers.inventory[power_screen_index] > 0)
        return POWER_ARMOR_SCREEN;

    return POWER_ARMOR_NONE;
}

void Use_PowerArmor(edict_t *ent, gitem_t *item)
{
    int     index;

    if (ent->flags & FL_POWER_ARMOR) {
        ent->flags &= ~FL_POWER_ARMOR;
        SV_StartSound(ent, CHAN_AUTO, SV_SoundIndex("misc/power2.wav"), 1, ATTN_NORM, 0);
    } else {
        index = ITEM_INDEX(FindItem("cells"));
        if (!ent->client->pers.inventory[index]) {
            SV_ClientPrint(ent, PRINT_HIGH, "No cells for power armor.\n");
            return;
        }
        ent->flags |= FL_POWER_ARMOR;
        SV_StartSound(ent, CHAN_AUTO, SV_SoundIndex("misc/power1.wav"), 1, ATTN_NORM, 0);
    }
}

bool Pickup_PowerArmor(edict_t *ent, edict_t *other)
{
    int     quantity;

    quantity = other->client->pers.inventory[ITEM_INDEX(ent->item)];

    other->client->pers.inventory[ITEM_INDEX(ent->item)]++;

    if (deathmatch.integer) {
        if (!(ent->spawnflags & DROPPED_ITEM))
            SetRespawn(ent, ent->item->quantity);
        // auto-use for DM only if we didn't already have one
        if (!quantity)
            ent->item->use(other, ent->item);
    }

    return true;
}

void Drop_PowerArmor(edict_t *ent, gitem_t *item)
{
    if ((ent->flags & FL_POWER_ARMOR) && (ent->client->pers.inventory[ITEM_INDEX(item)] == 1))
        Use_PowerArmor(ent, item);
    Drop_General(ent, item);
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
        other->client->ps.stats[STAT_PICKUP_STRING] = CS_ITEMS + ITEM_INDEX(ent->item);
        other->client->pickup_msg_time = level.time + 3000;

        // change selected item
        if (ent->item->use)
            other->client->pers.selected_item = other->client->ps.stats[STAT_SELECTED_ITEM] = ITEM_INDEX(ent->item);

        if (ent->item->pickup == Pickup_Health) {
            if (ent->count == 2)
                SV_StartSound(other, CHAN_ITEM, SV_SoundIndex("items/s_health.wav"), 1, ATTN_NORM, 0);
            else if (ent->count == 10)
                SV_StartSound(other, CHAN_ITEM, SV_SoundIndex("items/n_health.wav"), 1, ATTN_NORM, 0);
            else if (ent->count == 25)
                SV_StartSound(other, CHAN_ITEM, SV_SoundIndex("items/l_health.wav"), 1, ATTN_NORM, 0);
            else // (ent->count == 100)
                SV_StartSound(other, CHAN_ITEM, SV_SoundIndex("items/m_health.wav"), 1, ATTN_NORM, 0);
        } else if (ent->item->pickup_sound) {
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
    if (it->ammo && it->ammo[0]) {
        ammo = FindItem(it->ammo);
        if (ammo != it)
            PrecacheItem(ammo);
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
        else if (!strcmp(data + len - 3, "pcx"))
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
            if (item->pickup == Pickup_Armor || item->pickup == Pickup_PowerArmor) {
                G_FreeEdict(ent);
                return;
            }
        }
        if (dmflags.integer & DF_NO_ITEMS) {
            if (item->pickup == Pickup_Powerup) {
                G_FreeEdict(ent);
                return;
            }
        }
        if (dmflags.integer & DF_NO_HEALTH) {
            if (item->pickup == Pickup_Health || item->pickup == Pickup_Adrenaline || item->pickup == Pickup_AncientHead) {
                G_FreeEdict(ent);
                return;
            }
        }
        if (dmflags.integer & DF_INFINITE_AMMO) {
            if ((item->flags == IT_AMMO) || (strcmp(ent->classname, "weapon_bfg") == 0)) {
                G_FreeEdict(ent);
                return;
            }
        }
    }

    if (coop.integer && (strcmp(ent->classname, "key_power_cube") == 0)) {
        ent->spawnflags |= (1 << (8 + level.power_cubes));
        level.power_cubes++;
    }

    // don't let them drop items that stay in a coop game
    if (coop.integer && (item->flags & IT_STAY_COOP)) {
        item->drop = NULL;
    }

    ent->item = item;
    ent->nextthink = level.time + G_FramesToMs(2);    // items start after other solids
    ent->think = droptofloor;
    ent->s.effects = item->world_model_flags;
    ent->s.renderfx = RF_GLOW;
    if (ent->model)
        SV_ModelIndex(ent->model);
}

//======================================================================

gitem_t itemlist[] = {
    { 0 },  // leave index 0 alone

    //
    // ARMOR
    //

    /*QUAKED item_armor_body (.3 .3 1) (-16 -16 -16) (16 16 16)
    */
    {
        .classname = "item_armor_body",
        .pickup = Pickup_Armor,
        .pickup_sound = "misc/ar1_pkup.wav",
        .world_model = "models/items/armor/body/tris.md2", .world_model_flags = EF_ROTATE,
        .icon = "i_bodyarmor",
        .pickup_name = "Body Armor",
        .flags = IT_ARMOR,
        .info = &bodyarmor_info,
        .tag = ARMOR_BODY
    },

    /*QUAKED item_armor_combat (.3 .3 1) (-16 -16 -16) (16 16 16)
    */
    {
        .classname = "item_armor_combat",
        .pickup = Pickup_Armor,
        .pickup_sound = "misc/ar1_pkup.wav",
        .world_model = "models/items/armor/combat/tris.md2", .world_model_flags = EF_ROTATE,
        .icon = "i_combatarmor",
        .pickup_name = "Combat Armor",
        .flags = IT_ARMOR,
        .info = &combatarmor_info,
        .tag = ARMOR_COMBAT
    },

    /*QUAKED item_armor_jacket (.3 .3 1) (-16 -16 -16) (16 16 16)
    */
    {
        .classname = "item_armor_jacket",
        .pickup = Pickup_Armor,
        .pickup_sound = "misc/ar1_pkup.wav",
        .world_model = "models/items/armor/jacket/tris.md2", .world_model_flags = EF_ROTATE,
        .icon = "i_jacketarmor",
        .pickup_name = "Jacket Armor",
        .flags = IT_ARMOR,
        .info = &jacketarmor_info,
        .tag = ARMOR_JACKET
    },

    /*QUAKED item_armor_shard (.3 .3 1) (-16 -16 -16) (16 16 16)
    */
    {
        .classname = "item_armor_shard",
        .pickup = Pickup_Armor,
        .pickup_sound = "misc/ar2_pkup.wav",
        .world_model = "models/items/armor/shard/tris.md2", .world_model_flags = EF_ROTATE,
        .icon = "i_jacketarmor",
        .pickup_name = "Armor Shard",
        .flags = IT_ARMOR,
        .tag = ARMOR_SHARD
    },

#if 0
    /*QUAKED item_power_screen (.3 .3 1) (-16 -16 -16) (16 16 16)
    */
    {
        "item_power_screen",
        Pickup_PowerArmor,
        Use_PowerArmor,
        Drop_PowerArmor,
        NULL,
        "misc/ar3_pkup.wav",
        "models/items/armor/screen/tris.md2", EF_ROTATE,
        NULL,
        /* icon */      "i_powerscreen",
        /* pickup */    "Power Screen",
        /* width */     0,
        60,
        NULL,
        IT_ARMOR,
        0,
        NULL,
        0,
        /* precache */ ""
    },

    /*QUAKED item_power_shield (.3 .3 1) (-16 -16 -16) (16 16 16)
    */
    {
        "item_power_shield",
        Pickup_PowerArmor,
        Use_PowerArmor,
        Drop_PowerArmor,
        NULL,
        "misc/ar3_pkup.wav",
        "models/items/armor/shield/tris.md2", EF_ROTATE,
        NULL,
        /* icon */      "i_powershield",
        /* pickup */    "Power Shield",
        /* width */     0,
        60,
        NULL,
        IT_ARMOR,
        0,
        NULL,
        0,
        /* precache */ "misc/power2.wav misc/power1.wav"
    },
#endif

    //
    // WEAPONS
    //

    /* weapon_axe (.3 .3 1) (-16 -16 -16) (16 16 16)
    */
    {
        .classname = "weapon_axe",
        .use = Use_Weapon,
        .animation = &weap_axe_activate,
        .pickup_sound = "misc/w_pkup.wav",
        .view_model = "models/weapons/v_axe/axe.iqm",
        .icon = "w_blaster",
        .pickup_name = "Axe",
        .flags = IT_WEAPON | IT_STAY_COOP,
        .tag = WEAP_BLASTER,
        .weapid = WEAPID_AXE
    },

    /*QUAKED weapon_shotgun (.3 .3 1) (-16 -16 -16) (16 16 16)
    */
    {
        .classname = "weapon_shotgun",
        .pickup = Pickup_Weapon,
        .use = Use_Weapon,
        .drop = Drop_Weapon,
        .animation = &weap_shotgun_activate,
        .pickup_sound = "misc/w_pkup.wav",
        .world_model = "models/weapons/g_shotg/g_shotg.md3", .world_model_flags = EF_ROTATE,
        .view_model = "models/weapons/v_shotg/v_shotg.iqm",
        .icon = "w_shotgun",
        .pickup_name = "Shotgun",
        .quantity = 1,
        .ammo = "Shells",
        .flags = IT_WEAPON | IT_STAY_COOP,
        .tag = WEAP_SHOTGUN
    },

    /*QUAKED weapon_perforator (.3 .3 1) (-16 -16 -16) (16 16 16)
    */
    {
        .classname = "weapon_perforator",
        .pickup = Pickup_Weapon,
        .use = Use_Weapon,
        .drop = Drop_Weapon,
        .animation = &weap_perf_activate,
        .pickup_sound = "misc/w_pkup.wav",
        .world_model = "models/weapons/g_perf/g_perf.md3", .world_model_flags = EF_ROTATE,
        .view_model = "models/weapons/v_perf/v_perf.iqm",
        .icon = "w_chaingun",
        .pickup_name = "Perforator",
        .quantity = 1,
        .ammo = "nails",
        .flags = IT_WEAPON | IT_STAY_COOP,
        .tag = WEAP_CHAINGUN,
        .precaches = "misc/lasfly.wav"
    },

    //
    // AMMO ITEMS
    //

    /*QUAKED ammo_shells (.3 .3 1) (-16 -16 -16) (16 16 16)
    */
    {
        .classname = "ammo_shells",
        .pickup = Pickup_Ammo,
        .drop = Drop_Ammo,
        .pickup_sound = "misc/am_pkup.wav",
        .world_model = "models/items/ammo/shells/shellbox_small.md3",
        .icon = "a_shells",
        .pickup_name = "Shells",
        .quantity = 10,
        .flags = IT_AMMO,
        .tag = AMMO_SHELLS
    },

    /*QUAKED ammo_nails (.3 .3 1) (-16 -16 -16) (16 16 16)
    */
    {
        .classname = "ammo_nails",
        .pickup = Pickup_Ammo,
        .drop = Drop_Ammo,
        .pickup_sound = "misc/am_pkup.wav",
        .world_model = "models/items/ammo/nails/nailbox.md3",
        .icon = "a_nails",
        .pickup_name = "Nails",
        .quantity = 50,
        .flags = IT_AMMO,
        .tag = AMMO_NAILS
    },

#if 0
    /*QUAKED ammo_cells (.3 .3 1) (-16 -16 -16) (16 16 16)
    */
    {
        "ammo_cells",
        Pickup_Ammo,
        NULL,
        Drop_Ammo,
        NULL,
        "misc/am_pkup.wav",
        "models/items/ammo/cells/medium/tris.md2", 0,
        NULL,
        /* icon */      "a_cells",
        /* pickup */    "Cells",
        /* width */     3,
        50,
        NULL,
        IT_AMMO,
        0,
        NULL,
        AMMO_CELLS,
        /* precache */ ""
    },

    /*QUAKED ammo_rockets (.3 .3 1) (-16 -16 -16) (16 16 16)
    */
    {
        "ammo_rockets",
        Pickup_Ammo,
        NULL,
        Drop_Ammo,
        NULL,
        "misc/am_pkup.wav",
        "models/items/ammo/rockets/medium/tris.md2", 0,
        NULL,
        /* icon */      "a_rockets",
        /* pickup */    "Rockets",
        /* width */     3,
        5,
        NULL,
        IT_AMMO,
        0,
        NULL,
        AMMO_ROCKETS,
        /* precache */ ""
    },

    //
    // POWERUP ITEMS
    //
    /*QUAKED item_quad (.3 .3 1) (-16 -16 -16) (16 16 16)
    */
    {
        "item_quad",
        Pickup_Powerup,
        Use_Quad,
        Drop_General,
        NULL,
        "items/pkup.wav",
        "models/items/quaddama/tris.md2", EF_ROTATE,
        NULL,
        /* icon */      "p_quad",
        /* pickup */    "Quad Damage",
        /* width */     2,
        60,
        NULL,
        IT_POWERUP,
        0,
        NULL,
        0,
        /* precache */ "items/damage.wav items/damage2.wav items/damage3.wav"
    },

    /*QUAKED item_invulnerability (.3 .3 1) (-16 -16 -16) (16 16 16)
    */
    {
        "item_invulnerability",
        Pickup_Powerup,
        Use_Invulnerability,
        Drop_General,
        NULL,
        "items/pkup.wav",
        "models/items/invulner/tris.md2", EF_ROTATE,
        NULL,
        /* icon */      "p_invulnerability",
        /* pickup */    "Invulnerability",
        /* width */     2,
        300,
        NULL,
        IT_POWERUP,
        0,
        NULL,
        0,
        /* precache */ "items/protect.wav items/protect2.wav items/protect4.wav"
    },

    /*QUAKED item_silencer (.3 .3 1) (-16 -16 -16) (16 16 16)
    */
    {
        "item_silencer",
        Pickup_Powerup,
        Use_Silencer,
        Drop_General,
        NULL,
        "items/pkup.wav",
        "models/items/silencer/tris.md2", EF_ROTATE,
        NULL,
        /* icon */      "p_silencer",
        /* pickup */    "Silencer",
        /* width */     2,
        60,
        NULL,
        IT_POWERUP,
        0,
        NULL,
        0,
        /* precache */ ""
    },

    /*QUAKED item_breather (.3 .3 1) (-16 -16 -16) (16 16 16)
    */
    {
        "item_breather",
        Pickup_Powerup,
        Use_Breather,
        Drop_General,
        NULL,
        "items/pkup.wav",
        "models/items/breather/tris.md2", EF_ROTATE,
        NULL,
        /* icon */      "p_rebreather",
        /* pickup */    "Rebreather",
        /* width */     2,
        60,
        NULL,
        IT_STAY_COOP | IT_POWERUP,
        0,
        NULL,
        0,
        /* precache */ "items/airout.wav"
    },

    /*QUAKED item_enviro (.3 .3 1) (-16 -16 -16) (16 16 16)
    */
    {
        "item_enviro",
        Pickup_Powerup,
        Use_Envirosuit,
        Drop_General,
        NULL,
        "items/pkup.wav",
        "models/items/enviro/tris.md2", EF_ROTATE,
        NULL,
        /* icon */      "p_envirosuit",
        /* pickup */    "Environment Suit",
        /* width */     2,
        60,
        NULL,
        IT_STAY_COOP | IT_POWERUP,
        0,
        NULL,
        0,
        /* precache */ "items/airout.wav"
    },

    /*QUAKED item_ancient_head (.3 .3 1) (-16 -16 -16) (16 16 16)
    Special item that gives +2 to maximum health
    */
    {
        "item_ancient_head",
        Pickup_AncientHead,
        NULL,
        NULL,
        NULL,
        "items/pkup.wav",
        "models/items/c_head/tris.md2", EF_ROTATE,
        NULL,
        /* icon */      "i_fixme",
        /* pickup */    "Ancient Head",
        /* width */     2,
        60,
        NULL,
        0,
        0,
        NULL,
        0,
        /* precache */ ""
    },

    /*QUAKED item_adrenaline (.3 .3 1) (-16 -16 -16) (16 16 16)
    gives +1 to maximum health
    */
    {
        "item_adrenaline",
        Pickup_Adrenaline,
        NULL,
        NULL,
        NULL,
        "items/pkup.wav",
        "models/items/adrenal/tris.md2", EF_ROTATE,
        NULL,
        /* icon */      "p_adrenaline",
        /* pickup */    "Adrenaline",
        /* width */     2,
        60,
        NULL,
        0,
        0,
        NULL,
        0,
        /* precache */ ""
    },

    /*QUAKED item_bandolier (.3 .3 1) (-16 -16 -16) (16 16 16)
    */
    {
        "item_bandolier",
        Pickup_Bandolier,
        NULL,
        NULL,
        NULL,
        "items/pkup.wav",
        "models/items/band/tris.md2", EF_ROTATE,
        NULL,
        /* icon */      "p_bandolier",
        /* pickup */    "Bandolier",
        /* width */     2,
        60,
        NULL,
        0,
        0,
        NULL,
        0,
        /* precache */ ""
    },

    /*QUAKED item_pack (.3 .3 1) (-16 -16 -16) (16 16 16)
    */
    {
        "item_pack",
        Pickup_Pack,
        NULL,
        NULL,
        NULL,
        "items/pkup.wav",
        "models/items/pack/tris.md2", EF_ROTATE,
        NULL,
        /* icon */      "i_pack",
        /* pickup */    "Ammo Pack",
        /* width */     2,
        180,
        NULL,
        0,
        0,
        NULL,
        0,
        /* precache */ ""
    },

    //
    // KEYS
    //
    /*QUAKED key_data_cd (0 .5 .8) (-16 -16 -16) (16 16 16)
    key for computer centers
    */
    {
        "key_data_cd",
        Pickup_Key,
        NULL,
        Drop_General,
        NULL,
        "items/pkup.wav",
        "models/items/keys/data_cd/tris.md2", EF_ROTATE,
        NULL,
        "k_datacd",
        "Data CD",
        2,
        0,
        NULL,
        IT_STAY_COOP | IT_KEY,
        0,
        NULL,
        0,
        /* precache */ ""
    },

    /*QUAKED key_power_cube (0 .5 .8) (-16 -16 -16) (16 16 16) TRIGGER_SPAWN NO_TOUCH
    warehouse circuits
    */
    {
        "key_power_cube",
        Pickup_Key,
        NULL,
        Drop_General,
        NULL,
        "items/pkup.wav",
        "models/items/keys/power/tris.md2", EF_ROTATE,
        NULL,
        "k_powercube",
        "Power Cube",
        2,
        0,
        NULL,
        IT_STAY_COOP | IT_KEY,
        0,
        NULL,
        0,
        /* precache */ ""
    },

    /*QUAKED key_pyramid (0 .5 .8) (-16 -16 -16) (16 16 16)
    key for the entrance of jail3
    */
    {
        "key_pyramid",
        Pickup_Key,
        NULL,
        Drop_General,
        NULL,
        "items/pkup.wav",
        "models/items/keys/pyramid/tris.md2", EF_ROTATE,
        NULL,
        "k_pyramid",
        "Pyramid Key",
        2,
        0,
        NULL,
        IT_STAY_COOP | IT_KEY,
        0,
        NULL,
        0,
        /* precache */ ""
    },

    /*QUAKED key_data_spinner (0 .5 .8) (-16 -16 -16) (16 16 16)
    key for the city computer
    */
    {
        "key_data_spinner",
        Pickup_Key,
        NULL,
        Drop_General,
        NULL,
        "items/pkup.wav",
        "models/items/keys/spinner/tris.md2", EF_ROTATE,
        NULL,
        "k_dataspin",
        "Data Spinner",
        2,
        0,
        NULL,
        IT_STAY_COOP | IT_KEY,
        0,
        NULL,
        0,
        /* precache */ ""
    },

    /*QUAKED key_pass (0 .5 .8) (-16 -16 -16) (16 16 16)
    security pass for the security level
    */
    {
        "key_pass",
        Pickup_Key,
        NULL,
        Drop_General,
        NULL,
        "items/pkup.wav",
        "models/items/keys/pass/tris.md2", EF_ROTATE,
        NULL,
        "k_security",
        "Security Pass",
        2,
        0,
        NULL,
        IT_STAY_COOP | IT_KEY,
        0,
        NULL,
        0,
        /* precache */ ""
    },

    /*QUAKED key_blue_key (0 .5 .8) (-16 -16 -16) (16 16 16)
    normal door key - blue
    */
    {
        "key_blue_key",
        Pickup_Key,
        NULL,
        Drop_General,
        NULL,
        "items/pkup.wav",
        "models/items/keys/key/tris.md2", EF_ROTATE,
        NULL,
        "k_bluekey",
        "Blue Key",
        2,
        0,
        NULL,
        IT_STAY_COOP | IT_KEY,
        0,
        NULL,
        0,
        /* precache */ ""
    },

    /*QUAKED key_red_key (0 .5 .8) (-16 -16 -16) (16 16 16)
    normal door key - red
    */
    {
        "key_red_key",
        Pickup_Key,
        NULL,
        Drop_General,
        NULL,
        "items/pkup.wav",
        "models/items/keys/red_key/tris.md2", EF_ROTATE,
        NULL,
        "k_redkey",
        "Red Key",
        2,
        0,
        NULL,
        IT_STAY_COOP | IT_KEY,
        0,
        NULL,
        0,
        /* precache */ ""
    },

    /*QUAKED key_commander_head (0 .5 .8) (-16 -16 -16) (16 16 16)
    tank commander's head
    */
    {
        "key_commander_head",
        Pickup_Key,
        NULL,
        Drop_General,
        NULL,
        "items/pkup.wav",
        "models/monsters/commandr/head/tris.md2", EF_GIB,
        NULL,
        /* icon */      "k_comhead",
        /* pickup */    "Commander's Head",
        /* width */     2,
        0,
        NULL,
        IT_STAY_COOP | IT_KEY,
        0,
        NULL,
        0,
        /* precache */ ""
    },

    /*QUAKED key_airstrike_target (0 .5 .8) (-16 -16 -16) (16 16 16)
    tank commander's head
    */
    {
        "key_airstrike_target",
        Pickup_Key,
        NULL,
        Drop_General,
        NULL,
        "items/pkup.wav",
        "models/items/keys/target/tris.md2", EF_ROTATE,
        NULL,
        /* icon */      "i_airstrike",
        /* pickup */    "Airstrike Marker",
        /* width */     2,
        0,
        NULL,
        IT_STAY_COOP | IT_KEY,
        0,
        NULL,
        0,
        /* precache */ ""
    },
#endif

    {
        .pickup = Pickup_Health,
        .pickup_sound = "items/pkup.wav",
        .icon = "i_health",
        .pickup_name = "Health"
    },

    // end of list marker
    {NULL}
};


/*QUAKED item_health (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
void SP_item_health(edict_t *self)
{
    if (deathmatch.integer && (dmflags.integer & DF_NO_HEALTH)) {
        G_FreeEdict(self);
        return;
    }

    self->model = "models/items/healing/medium/tris.md2";
    self->count = 10;
    SpawnItem(self, FindItem("Health"));
    SV_SoundIndex("items/n_health.wav");
}

/*QUAKED item_health_small (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
void SP_item_health_small(edict_t *self)
{
    if (deathmatch.integer && (dmflags.integer & DF_NO_HEALTH)) {
        G_FreeEdict(self);
        return;
    }

    self->model = "models/items/healing/stimpack/tris.md2";
    self->count = 2;
    SpawnItem(self, FindItem("Health"));
    self->style = HEALTH_IGNORE_MAX;
    SV_SoundIndex("items/s_health.wav");
}

/*QUAKED item_health_large (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
void SP_item_health_large(edict_t *self)
{
    if (deathmatch.integer && (dmflags.integer & DF_NO_HEALTH)) {
        G_FreeEdict(self);
        return;
    }

    self->model = "models/items/healing/large/tris.md2";
    self->count = 25;
    SpawnItem(self, FindItem("Health"));
    SV_SoundIndex("items/l_health.wav");
}

/*QUAKED item_health_mega (.3 .3 1) (-16 -16 -16) (16 16 16)
*/
void SP_item_health_mega(edict_t *self)
{
    if (deathmatch.integer && (dmflags.integer & DF_NO_HEALTH)) {
        G_FreeEdict(self);
        return;
    }

    self->model = "models/items/mega_h/tris.md2";
    self->count = 100;
    SpawnItem(self, FindItem("Health"));
    SV_SoundIndex("items/m_health.wav");
    self->style = HEALTH_IGNORE_MAX | HEALTH_TIMED;
}


void InitItems(void)
{
    game.num_items = sizeof(itemlist) / sizeof(itemlist[0]) - 1;
}



/*
===============
SetItemNames

Called by worldspawn
===============
*/
void SetItemNames(void)
{
    int     i;
    gitem_t *it;

    for (i = 0 ; i < game.num_items ; i++) {
        it = &itemlist[i];
        SV_SetConfigString(CS_ITEMS + i, it->pickup_name);
    }

    jacket_armor_index = ITEM_INDEX(FindItem("Jacket Armor"));
    combat_armor_index = ITEM_INDEX(FindItem("Combat Armor"));
    body_armor_index   = ITEM_INDEX(FindItem("Body Armor"));
    power_screen_index = ITEM_INDEX(FindItem("Power Screen"));
    power_shield_index = ITEM_INDEX(FindItem("Power Shield"));
}
