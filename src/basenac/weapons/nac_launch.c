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
// g_ai.c

#include "../g_local.h"
#include "../m_player.h"

extern bool     is_quad;

static void Launch_Fire(edict_t* ent)
{
    vec3_t      start;
    vec3_t      forward, right;
    vec3_t      offset;
    int         damage = 100;

    AngleVectors(ent->client->v_angle, forward, right, NULL);

    ent->client->kick_angles[0] = -2;

    VectorSet(offset, 4, 0, ent->viewheight - 8);
    P_ProjectSource(ent->client, ent->s.origin, offset, forward, right, start);

    if (is_quad) {
        damage *= 4;
    }

    fire_grenade(ent, start, forward, damage, 750, 2.5, damage + 40);

    // send muzzle flash
    SV_WriteByte(svc_muzzleflash);
    SV_WriteEntity(ent);
    SV_WriteByte(MZ_GRENADE);
    SV_Multicast(ent->s.origin, MULTICAST_PVS, false);

    PlayerNoise(ent, ent->s.origin, PNOISE_WEAPON);

    if (!(dmflags.integer & DF_INFINITE_AMMO))
        ent->client->pers.inventory[ent->client->ammo_index] -= ent->client->pers.weapon->quantity;
}

enum {
    ANIM_EQUIP_FIRST = 0,
    ANIM_EQUIP_LAST = 8,
    ANIM_IDLE_FIRST = 9,
    ANIM_IDLE_LAST = 67,
    ANIM_ATTACK_FIRST = 68,
    ANIM_ATTACK_LAST = 83,
    ANIM_PUTAWAY_FIRST = 84,
    ANIM_PUTAWAY_LAST = 86,
    ANIM_INSPECT_FIRST = 87,
    ANIM_INSPECT_LAST = 157
};

static bool Launch_PickIdle(edict_t* ent, weapon_id_t id);
static bool Launch_Idle(edict_t* ent, weapon_id_t id);

const weapon_animation_t weap_launch_activate = {
    .start = ANIM_EQUIP_FIRST, .end = ANIM_EQUIP_LAST,
    .finished = Launch_PickIdle
};

const weapon_animation_t weap_launch_deactivate = {
    .start = ANIM_PUTAWAY_FIRST, .end = ANIM_PUTAWAY_LAST,
    .finished = ChangeWeapon
};

const weapon_animation_t weap_launch_idle = {
    .start = ANIM_IDLE_FIRST, .end = ANIM_IDLE_LAST,
    .frame = Launch_Idle, .finished = Launch_PickIdle
};

const weapon_animation_t weap_launch_inspect = {
    .start = ANIM_INSPECT_FIRST, .end = ANIM_INSPECT_LAST,
    .frame = Launch_Idle, .finished = Launch_PickIdle
};

const weapon_animation_t weap_launch_fire = {
    .start = ANIM_ATTACK_FIRST, .end = ANIM_ATTACK_LAST, .next = &weap_launch_idle
};

static bool Launch_PickIdle(edict_t* ent, weapon_id_t id)
{
    if (ent->client->weapanim[id] == &weap_launch_inspect ||
        (!ent->client->inspect && random() < WEAPON_RANDOM_INSPECT_CHANCE))
    {
        Weapon_SetAnimation(ent, id, &weap_launch_idle);
        return false;
    }

    Weapon_SetAnimation(ent, id, &weap_launch_inspect);

    ent->client->inspect = false;

    return false;
}

static bool Launch_Idle(edict_t* ent, weapon_id_t id)
{
    // check weapon change
    if (Weapon_CheckChange(ent, &weap_launch_deactivate, id))
        return false;

    if (ent->client->buttons & BUTTON_ATTACK)
    {
        if (!Weapon_AmmoCheck(ent))
            return false;

        ent->client->anim_priority = ANIM_ATTACK;
        if (ent->client->ps.pmove.pm_flags & PMF_DUCKED) {
            ent->s.frame = FRAME_crattak1 - 1;
            ent->client->anim_end = FRAME_crattak9;
        }
        else {
            ent->s.frame = FRAME_attack1 - 1;
            ent->client->anim_end = FRAME_attack8;
        }

        Launch_Fire(ent);

        Weapon_SetAnimation(ent, id, &weap_launch_fire);
        return false;
    }

    // check explicit inspect last
    if (ent->client->inspect)
    {
        if (ent->client->weapanim[WEAPID_GUN] == &weap_launch_idle)
        {
            Launch_PickIdle(ent, id);
            return false;
        }

        ent->client->inspect = false;
    }

    return true;
}