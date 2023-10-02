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

static bool G_HasWeaponAndAmmo(edict_t *ent, gitem_id_t weapon)
{
    if (!ent->client->pers.inventory[weapon])
        return false;

    const gitem_t *it = GetItemByIndex(weapon);

    if (it->ammo && ent->client->pers.inventory[it->ammo] < it->quantity)
        return false;

    return true;
}

/*
=================
NoAmmoWeaponChange
=================
*/
bool NoAmmoWeaponChange(edict_t *ent)
{
    if (G_HasWeaponAndAmmo(ent, ITEM_PERFORATOR)) {
        ent->client->newweapon = GetItemByIndex(ITEM_PERFORATOR);
    } else if (G_HasWeaponAndAmmo(ent, ITEM_THUNDERBOLT)) {
        ent->client->newweapon = GetItemByIndex(ITEM_THUNDERBOLT);
    } else if (G_HasWeaponAndAmmo(ent, ITEM_ROCKET_LAUNCHER)) {
        ent->client->newweapon = GetItemByIndex(ITEM_ROCKET_LAUNCHER);
    } else if (G_HasWeaponAndAmmo(ent, ITEM_GRENADE_LAUNCHER)) {
        ent->client->newweapon = GetItemByIndex(ITEM_GRENADE_LAUNCHER);
    } else if (G_HasWeaponAndAmmo(ent, ITEM_SHOTGUN)) {
        ent->client->newweapon = GetItemByIndex(ITEM_SHOTGUN);
    } else if (G_HasWeaponAndAmmo(ent, ITEM_AXE)) {
        ent->client->newweapon = GetItemByIndex(ITEM_AXE);
    } else if (ent->client->pers.weapon) {
        ent->client->newweapon = NULL;
    } else {
        return false;
    }

    return true;
}

// not animation function; returns true if
// we can keep firing
bool Weapon_AmmoCheck(edict_t *ent)
{
    if (ent->client->newweapon || !ent->client->pers.inventory[ent->client->pers.weapon->id])
        return false;

    if (ent->client->pers.inventory[ent->client->ammo_index] >= ent->client->pers.weapon->quantity) {
        return true;
    }

    if (NoAmmoWeaponChange(ent)) {
        SV_StartSound(ent, CHAN_VOICE, SV_SoundIndex(ASSET_SOUND_OUT_OF_AMMO), 1, ATTN_NORM, 0);
    }

    return false;
}

/**
 * @brief Start activation of our `newweapon`.
 * @param ent 
*/
void Weapon_Activate(edict_t *ent, bool switched)
{
    if (ent->client->pers.weapon == ent->client->newweapon)
        return;

    if (switched)
    {
        // set entity weapon properties
        ent->client->pers.lastweapon = ent->client->pers.weapon;
        ent->client->pers.weapon = ent->client->newweapon;
    }

    // set visible model
    if (ent->s.modelindex == 255)
    {
        int32_t i;

        if (ent->client->pers.weapon)
            i = ((ent->client->pers.weapon->weapmodel & 0xff) << 8);
        else
            i = 0;
        ent->s.skinnum = (ent - globals.entities - 1) | i;
    }

    // set ammo index
    if (ent->client->pers.weapon && ent->client->pers.weapon->ammo)
        ent->client->ammo_index = ent->client->pers.weapon->ammo;
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
    Weapon_SetAnimation(ent, ent->client->pers.weapon->weapid, ent->client->pers.weapon->animation);

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
bool ChangeWeapon(edict_t *ent, weapon_id_t weapon_id)
{
    if (ent->client->newweapon)
    {
        Weapon_Activate(ent, true);
        ent->client->newweapon = NULL;
    }
    else
    {
        ent->client->ps.gun[weapon_id].index = 0;
        ent->client->weapanim[weapon_id] = NULL;
    }

    return false;
}

/*
======================================================================

WEAPON ANIMATIONS

======================================================================
*/

void Weapon_SetAnimationFrame(edict_t *ent, weapon_id_t weapon_id, const weapon_animation_t *animation, int32_t frame)
{
    ent->client->weapanim[weapon_id] = animation;

    if (frame < animation->start || frame > animation->end)
    {
        Com_Print("Bad start frame\n");
        ent->client->ps.gun[weapon_id].frame = animation->start;
    }
    else
        ent->client->ps.gun[weapon_id].frame = frame;

    int32_t currentFrame = ent->client->ps.gun[weapon_id].frame;

    // run events
    for (const weapon_event_t *event = animation->events; event && event->func; event++)
        if ((event->start == WEAPON_EVENT_MINMAX || currentFrame >= event->start) &&
            (event->end == WEAPON_EVENT_MINMAX || currentFrame <= event->end))
            if (!event->func(ent, weapon_id))
                return;

    //Com_Printf("(%f) change to frame %i\n", G_MsToSec(level.time), frame);
}

void Weapon_SetAnimation(edict_t *ent, weapon_id_t weapon_id, const weapon_animation_t *animation)
{
    Weapon_SetAnimationFrame(ent, weapon_id, animation, animation->start);
}

static void Weapon_RunAnimation(edict_t *ent, weapon_id_t currentWeaponId)
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

    //Com_Printf("(%f) running frame %i\n", G_MsToSec(level.time), currentFrame);
   
    // run frame func first
    if (animation->frame && !animation->frame(ent, currentWeaponId))
        return; 
    
    // will the next frame bring us to the end?
    if (currentFrame + 1 > animation->end)
    {
        if (animation->finished && !animation->finished(ent, currentWeaponId))
            return;

        if (animation->next)
            Weapon_SetAnimation(ent, currentWeaponId, animation->next);
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
            if (!event->func(ent, currentWeaponId))
                return;

    // copy over to visual model
    ent->client->ps.gun[currentWeaponId].frame = currentFrame;
}

bool Weapon_CheckChange(edict_t *ent, const weapon_animation_t *deact_anim, weapon_id_t id)
{
    if (ent->client->newweapon && ent->client->newweapon->id != ent->client->pers.weapon->id)
    {
        Weapon_SetAnimation(ent, id, deact_anim);

        if (!ent->client->pers.inventory[ent->client->pers.weapon->id] || !ent->client->newweapon || ent->client->newweapon->weapid != id)
            Weapon_Activate(ent, true);

        return true;
    }

    return false;
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

        for (weapon_id_t currentWeaponId = 0; currentWeaponId < WEAPID_TOTAL; currentWeaponId++) {
            Weapon_RunAnimation(ent, currentWeaponId);
        }
    } else {
        if (ent->client->newweapon) {
            Weapon_Activate(ent, true);
        }
    }
}
