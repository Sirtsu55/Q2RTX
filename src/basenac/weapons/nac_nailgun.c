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

static void NailGun_Fire(edict_t *ent, bool left)
{
    vec3_t      start;
    vec3_t      forward, right;
    vec3_t      offset;
    int         damage = 12;

    AngleVectors(ent->client->v_angle, forward, right, NULL);

    ent->client->kick_angles[0] = -0.8;

    VectorSet(offset, 16, 0, ent->viewheight - 6);

    if (left)
        offset[1] = -2.7f;
    else
        offset[1] = 2.7f;

    P_ProjectSource(ent->client, ent->s.origin, offset, forward, right, start);

    if (is_quad) {
        damage *= 4;
    }

    fire_nail(ent, start, forward, damage, 1250);

    // send muzzle flash
    SV_WriteByte(svc_muzzleflash);
    SV_WriteEntity(ent);
    SV_WriteByte(MZ_MACHINEGUN);
    SV_Multicast(ent->s.origin, MULTICAST_PVS, false);

    PlayerNoise(ent, ent->s.origin, PNOISE_WEAPON);

    if (!(dmflags.integer & DF_INFINITE_AMMO))
        ent->client->pers.inventory[ent->client->ammo_index] -= ent->client->pers.weapon->quantity;
}

enum {
    ANIM_EQUIP_FIRST    = 0,
    ANIM_EQUIP_LAST     = 8,
    ANIM_IDLE_FIRST     = 9,
    ANIM_IDLE_LAST      = 67,
    ANIM_LEFT_FIRST     = 68,
    ANIM_LEFT_LAST      = 70,
    ANIM_RIGHT_FIRST    = 71,
    ANIM_RIGHT_LAST     = 73,
    ANIM_PUTAWAY_FIRST  = 74,
    ANIM_PUTAWAY_LAST   = 77,
    ANIM_INSPECT_FIRST  = 78,
    ANIM_INSPECT_LAST   = 132
};

static bool NailGun_PickIdle(edict_t *ent, weapon_id_t id);
static bool NailGun_Idle(edict_t *ent, weapon_id_t id);

const weapon_animation_t weap_nailgun_activate = {
    .start = ANIM_EQUIP_FIRST, .end = ANIM_EQUIP_LAST,
    .finished = NailGun_PickIdle
};

const weapon_animation_t weap_nailgun_deactivate = {
    .start = ANIM_PUTAWAY_FIRST, .end = ANIM_PUTAWAY_LAST,
    .finished = ChangeWeapon
};

const weapon_animation_t weap_nailgun_idle = {
    .start = ANIM_IDLE_FIRST, .end = ANIM_IDLE_LAST,
    .frame = NailGun_Idle, .finished = NailGun_PickIdle
};

const weapon_animation_t weap_nailgun_inspect = {
    .start = ANIM_INSPECT_FIRST, .end = ANIM_INSPECT_LAST,
    .frame = NailGun_Idle, .finished = NailGun_PickIdle
};

const weapon_animation_t weap_nailgun_fire_left = {
    .start = ANIM_LEFT_FIRST, .end = ANIM_LEFT_LAST - 1, .next = &weap_nailgun_idle
};

const weapon_animation_t weap_nailgun_fire_right = {
    .start = ANIM_RIGHT_FIRST, .end = ANIM_RIGHT_LAST - 1, .next = &weap_nailgun_idle
};

static bool NailGun_PickIdle(edict_t *ent, weapon_id_t id)
{
    if (ent->client->weapanim[WEAPID_GUN] == &weap_nailgun_inspect ||
        (!ent->client->inspect && random() < WEAPON_RANDOM_INSPECT_CHANCE))
    {
        Weapon_SetAnimation(ent, id, &weap_nailgun_idle);
        return false;
    }

    Weapon_SetAnimation(ent, id, &weap_nailgun_inspect);

    ent->client->inspect = false;

    return false;
}

static bool NailGun_Idle(edict_t *ent, weapon_id_t id)
{
    if (ent->client->newweapon)
    {
        Weapon_SetAnimation(ent, id, &weap_nailgun_deactivate);

        if (!ent->client->pers.inventory[ent->client->pers.weapon->id] || !ent->client->newweapon || ent->client->newweapon->weapid != id)
            Weapon_Activate(ent, true);

        return false;
    }

    if (ent->client->buttons & BUTTON_ATTACK)
    {
        if (!Weapon_AmmoCheck(ent))
            return false;

        ent->client->anim_priority = ANIM_ATTACK;
        if (ent->client->ps.pmove.pm_flags & PMF_DUCKED) {
            ent->s.frame = FRAME_crattak1 - 1;
            ent->client->anim_end = FRAME_crattak9;
        } else {
            ent->s.frame = FRAME_attack1 - 1;
            ent->client->anim_end = FRAME_attack8;
        }

        NailGun_Fire(ent, ent->client->nail_left);

        if (ent->client->nail_left)
            Weapon_SetAnimation(ent, id, &weap_nailgun_fire_left);
        else
            Weapon_SetAnimation(ent, id, &weap_nailgun_fire_right);

        ent->client->nail_left = !ent->client->nail_left;
        return false;
    }

    // check explicit inspect last
    if (ent->client->inspect)
    {
        if (ent->client->weapanim[WEAPID_GUN] == &weap_nailgun_idle)
        {
            NailGun_PickIdle(ent, id);
            return false;
        }

        ent->client->inspect = false;
    }

    return true;
}