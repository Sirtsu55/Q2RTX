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
// g_weapon.c

#include "g_local.h"
#include "m_player.h"

bool     is_quad;
byte     is_silenced;

void G_ProjectSource2(const vec3_t point, const vec3_t distance, const vec3_t forward, const vec3_t right, const vec3_t up, vec3_t result)
{
    result[0] = point[0] + forward[0] * distance[0] + right[0] * distance[1] + up[0] * distance[2];
    result[1] = point[1] + forward[1] * distance[0] + right[1] * distance[1] + up[1] * distance[2];
    result[2] = point[2] + forward[2] * distance[0] + right[2] * distance[1] + up[2] * distance[2];
}

void P_ProjectSource(gclient_t *client, vec3_t point, vec3_t distance, vec3_t forward, vec3_t right, vec3_t result)
{
    vec3_t  _distance;

    VectorCopy(distance, _distance);
    if (client->pers.hand == LEFT_HANDED)
        _distance[1] *= -1;
    else if (client->pers.hand == CENTER_HANDED)
        _distance[1] = 0;
    G_ProjectSource(point, _distance, forward, right, result);
}

void P_ProjectSource2(gclient_t *client, vec3_t point, vec3_t distance, vec3_t forward, vec3_t right, vec3_t up, vec3_t result)
{
    vec3_t  _distance;

    VectorCopy(distance, _distance);
    if (client->pers.hand == LEFT_HANDED)
        _distance[1] *= -1;
    else if (client->pers.hand == CENTER_HANDED)
        _distance[1] = 0;
    G_ProjectSource2(point, _distance, forward, right, up, result);
}


/*
===============
PlayerNoise

Each player can have two noise objects associated with it:
a personal noise (jumping, pain, weapon firing), and a weapon
target noise (bullet wall impacts)

Monsters that don't directly see the player can move
to a noise in hopes of seeing the player from there.
===============
*/
void PlayerNoise(edict_t *who, vec3_t where, int type)
{
    edict_t     *noise;

    if (type == PNOISE_WEAPON) {
        if (who->client->silencer_shots) {
            who->client->silencer_shots--;
            return;
        }
    }

    if (deathmatch.integer)
        return;

    if (who->flags & FL_NOTARGET)
        return;


    if (!who->mynoise) {
        noise = G_Spawn();
        noise->classname = "player_noise";
        VectorSet(noise->mins, -8, -8, -8);
        VectorSet(noise->maxs, 8, 8, 8);
        noise->owner = who;
        noise->svflags = SVF_NOCLIENT;
        who->mynoise = noise;

        noise = G_Spawn();
        noise->classname = "player_noise";
        VectorSet(noise->mins, -8, -8, -8);
        VectorSet(noise->maxs, 8, 8, 8);
        noise->owner = who;
        noise->svflags = SVF_NOCLIENT;
        who->mynoise2 = noise;
    }

    if (type == PNOISE_SELF || type == PNOISE_WEAPON) {
        noise = who->mynoise;
        level.sound_entity = noise;
        level.sound_entity_time = level.time;
    } else { // type == PNOISE_IMPACT
        noise = who->mynoise2;
        level.sound2_entity = noise;
        level.sound2_entity_time = level.time;
    }

    VectorCopy(where, noise->s.origin);
    VectorSubtract(where, noise->maxs, noise->absmin);
    VectorAdd(where, noise->maxs, noise->absmax);
    noise->last_sound_time = level.time;
    SV_LinkEntity(noise);
}


bool Pickup_Weapon(edict_t *ent, edict_t *other)
{
    int         index;
    gitem_t     *ammo;

    index = ITEM_INDEX(ent->item);

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
        // give them some ammo with it
        ammo = FindItem(ent->item->ammo);
        if (dmflags.integer & DF_INFINITE_AMMO)
            Add_Ammo(other, ammo, 1000);
        else
            Add_Ammo(other, ammo, ammo->quantity);

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
        (!deathmatch.integer || other->client->pers.weapon == FindItem("Axe")))
        other->client->newweapon = ent->item;

    return true;
}

/*
=================
NoAmmoWeaponChange
=================
*/
static void NoAmmoWeaponChange(edict_t *ent)
{
    if (ent->client->pers.inventory[ITEM_INDEX(FindItem("nails"))]
        &&  ent->client->pers.inventory[ITEM_INDEX(FindItem("perforator"))]) {
        ent->client->newweapon = FindItem("perforator");
        return;
    }
    if (ent->client->pers.inventory[ITEM_INDEX(FindItem("shells"))]
        &&  ent->client->pers.inventory[ITEM_INDEX(FindItem("shotgun"))]) {
        ent->client->newweapon = FindItem("shotgun");
        return;
    }
    ent->client->newweapon = FindItem("axe");
}

// not animation function; returns true if
// we can keep firing
bool Weapon_AmmoCheck(edict_t *ent)
{
    if (ent->client->newweapon)
        return false;

    if (ent->client->pers.inventory[ent->client->ammo_index] >= ent->client->pers.weapon->quantity) {
        return true;
    }

    SV_StartSound(ent, CHAN_VOICE, SV_SoundIndex("weapons/noammo.wav"), 1, ATTN_NORM, 0);

    NoAmmoWeaponChange(ent);

    return false;
}

static weapon_id_t currentWeaponId;

/**
 * @brief Start activation of our `newweapon`.
 * @param ent 
*/
void Weapon_Activate(edict_t *ent, bool switched)
{
    if (switched)
    {
        // set entity weapon properties
        ent->client->pers.lastweapon = ent->client->pers.weapon;
        ent->client->pers.weapon = ent->client->newweapon;
        ent->client->newweapon = NULL;
    }

    // set visible model
    if (ent->s.modelindex == 255)
    {
        int32_t i;

        if (ent->client->pers.weapon)
            i = ((ent->client->pers.weapon->weapmodel & 0xff) << 8);
        else
            i = 0;
        ent->s.skinnum = (ent - g_edicts - 1) | i;
    }

    // set ammo index
    if (ent->client->pers.weapon && ent->client->pers.weapon->ammo)
        ent->client->ammo_index = ITEM_INDEX(FindItem(ent->client->pers.weapon->ammo));
    else
        ent->client->ammo_index = 0;

    // clear entity parameters
    ent->client->weapon_sound = 0;
    ent->s.sound_pitch = 0;

    // dead
    if (!ent->client->pers.weapon)
    {
        ent->client->ps.gun[0].index = ent->client->ps.gun[1].index = 0;
        return;
    }

    // set parameters for new gun
    ent->client->ps.gun[ent->client->pers.weapon->weapid] = (player_gun_t) {
        .index = SV_ModelIndex(ent->client->pers.weapon->view_model),
        .spin = 0
    };

    // begin animation
    currentWeaponId = ent->client->pers.weapon->weapid;
    Weapon_SetAnimation(ent, ent->client->pers.weapon->animation);

    // player animation
    ent->client->anim_priority = ANIM_PAIN;
    if (ent->client->ps.pmove.pm_flags & PMF_DUCKED) {
        ent->s.frame = FRAME_crpain1;
        ent->client->anim_end = FRAME_crpain4;
    } else {
        ent->s.frame = FRAME_pain301;
        ent->client->anim_end = FRAME_pain304;
    }
}

/*
===============
ChangeWeapon

The old weapon has been dropped all the way, so remove it
from view.
===============
*/
bool ChangeWeapon(edict_t *ent)
{
    ent->client->ps.gun[currentWeaponId].index = 0;
    return false;
}

/*
======================================================================

WEAPON ANIMATIONS

======================================================================
*/

void Weapon_SetAnimationFrame(edict_t *ent, const weapon_animation_t *animation, int32_t frame)
{
    ent->client->weapanim[currentWeaponId] = animation;

    if (frame < animation->start || frame > animation->end)
    {
        Com_Print("Bad start frame\n");
        ent->client->ps.gun[currentWeaponId].frame = animation->start;
    }
    else
        ent->client->ps.gun[currentWeaponId].frame = frame;
}

void Weapon_SetAnimation(edict_t *ent, const weapon_animation_t *animation)
{
    Weapon_SetAnimationFrame(ent, animation, animation->start);
}

void Weapon_RunAnimation(edict_t *ent)
{
    const weapon_animation_t *animation = ent->client->weapanim[currentWeaponId];

    if (!animation) {
        return;
    }

    int32_t currentFrame = ent->client->ps.gun[currentWeaponId].frame;

    // assertion...
    if (currentFrame < animation->start || currentFrame > animation->end)
    {
        currentFrame = animation->start;
        Com_Print("animation out of range; restarting\n");
    }
   
    // run frame func first
    if (animation->frame && !animation->frame(ent))
        return; 
    
    // will the next frame bring us to the end?
    if (currentFrame + 1 > animation->end)
    {
        if (animation->finished && !animation->finished(ent))
            return;

        if (animation->next)
            Weapon_SetAnimation(ent, animation->next);
        else
            ent->client->ps.gun[currentWeaponId].frame = animation->start;

        return;
    }
    // regular frame, so we're going up by 1
    else
        currentFrame++;

    // run events
    for (const weapon_event_t *event = animation->events; event && event->func; event++)
        if ((event->start == WEAPON_EVENT_MINMAX || currentFrame >= event->start) &&
            (event->end == WEAPON_EVENT_MINMAX || currentFrame <= event->end))
            if (!event->func(ent))
                return;

    // copy over to visual model
    ent->client->ps.gun[currentWeaponId].frame = currentFrame;
}

/*
=================
Think_Weapon

Called by ClientBeginServerFrame and ClientThink
=================
*/
void Think_Weapon(edict_t *ent)
{
    // if just died, put the weapon away
    if (ent->health < 1) {
        ent->client->newweapon = NULL;
        Weapon_Activate(ent, true);
        return;
    }

    // call active weapon think routine
    if (ent->client->pers.weapon) {
        is_quad = (ent->client->quad_time > level.time);
        if (ent->client->silencer_shots)
            is_silenced = MZ_SILENCED;
        else
            is_silenced = 0;

        for (currentWeaponId = 0; currentWeaponId < WEAPID_TOTAL; currentWeaponId++) {
            Weapon_RunAnimation(ent);
        }
    }
}


/*
================
Use_Weapon

Make the weapon ready if there is ammo
================
*/
void Use_Weapon(edict_t *ent, gitem_t *item)
{
    int         ammo_index;
    gitem_t     *ammo_item;

    // see if we're already using it
    if (item == ent->client->pers.weapon)
        return;

    if (item->ammo && !g_select_empty.integer && !(item->flags & IT_AMMO)) {
        ammo_item = FindItem(item->ammo);
        ammo_index = ITEM_INDEX(ammo_item);

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
void Drop_Weapon(edict_t *ent, gitem_t *item)
{
    int     index;

    if (dmflags.integer & DF_WEAPONS_STAY)
        return;

    index = ITEM_INDEX(item);
    // see if we're already using it
    if (((item == ent->client->pers.weapon) || (item == ent->client->newweapon)) && (ent->client->pers.inventory[index] == 1)) {
        SV_ClientPrint(ent, PRINT_HIGH, "Can't drop current weapon\n");
        return;
    }

    Drop_Item(ent, item);
    ent->client->pers.inventory[index]--;
}
