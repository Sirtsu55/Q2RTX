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

static void SuperShotgun_Fire(edict_t *ent)
{
    vec3_t      start;
    vec3_t      forward, right;
    vec3_t      offset;
    int         damage = 2;
    int         kick = 1;

    if (is_quad) {
        damage *= 4;
        kick *= 4;
    }

    AngleVectors(ent->client->v_angle, forward, right, NULL);

    ent->client->kick_angles[0] = -2;

    vec3_t angle;
    VectorCopy(ent->client->v_angle, angle);
    angle[1] = ent->client->v_angle[1] + 3;

    AngleVectors(angle, forward, right, NULL);
    VectorSet(offset, 1, -1, ent->viewheight);
    P_ProjectSource(ent->client, ent->s.origin, offset, forward, right, start);

    fire_shotgun(ent, start, forward, damage, kick, 550, 225, DEFAULT_SHOTGUN_COUNT, MOD_SHOTGUN);
    angle[1] = ent->client->v_angle[1] - 3;

    AngleVectors(angle, forward, right, NULL);
    VectorSet(offset, 1, 1, ent->viewheight);
    P_ProjectSource(ent->client, ent->s.origin, offset, forward, right, start);

    fire_shotgun(ent, start, forward, damage, kick, 550, 225, DEFAULT_SHOTGUN_COUNT, MOD_SHOTGUN);

    // send muzzle flash
    SV_WriteByte(svc_muzzleflash);
    SV_WriteEntity(ent);
    SV_WriteByte(MZ_SHOTGUN);
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
    ANIM_ATTACK_FIRST   = 68,
    ANIM_ATTACK_LAST    = 83,
    ANIM_PUTAWAY_FIRST  = 84,
    ANIM_PUTAWAY_LAST   = 86,
    ANIM_INSPECT_FIRST  = 87,
    ANIM_INSPECT_LAST   = 157
};

static bool SuperShotgun_PickIdle(edict_t *ent);
static bool SuperShotgun_Idle(edict_t *ent);

const weapon_animation_t weap_sshotgun_activate = {
    .start = ANIM_EQUIP_FIRST, .end = ANIM_EQUIP_LAST,
    .finished = SuperShotgun_PickIdle
};

const weapon_animation_t weap_sshotgun_deactivate = {
    .start = ANIM_PUTAWAY_FIRST, .end = ANIM_PUTAWAY_LAST,
    .finished = ChangeWeapon
};

const weapon_animation_t weap_sshotgun_idle = {
    .start = ANIM_IDLE_FIRST, .end = ANIM_IDLE_LAST,
    .frame = SuperShotgun_Idle, .finished = SuperShotgun_PickIdle
};

const weapon_animation_t weap_sshotgun_inspect = {
    .start = ANIM_INSPECT_FIRST, .end = ANIM_INSPECT_LAST,
    .frame = SuperShotgun_Idle, .finished = SuperShotgun_PickIdle
};

const weapon_animation_t weap_sshotgun_fire = {
    .start = ANIM_ATTACK_FIRST, .end = ANIM_ATTACK_LAST, .next = &weap_sshotgun_idle
};

static bool SuperShotgun_PickIdle(edict_t *ent)
{
    if (ent->client->weapanim[WEAPID_GUN] == &weap_sshotgun_inspect ||
        (!ent->client->inspect || random() < WEAPON_RANDOM_INSPECT_CHANCE))
    {
        Weapon_SetAnimation(ent, &weap_sshotgun_idle);
        return false;
    }

    Weapon_SetAnimation(ent, &weap_sshotgun_inspect);

    ent->client->inspect = false;

    return false;
}

static bool SuperShotgun_Idle(edict_t *ent)
{
    if (ent->client->newweapon || !ent->client->pers.inventory[ent->client->pers.weapon->id])
    {
        Weapon_SetAnimation(ent, &weap_sshotgun_deactivate);
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

        SuperShotgun_Fire(ent);

        Weapon_SetAnimation(ent, &weap_sshotgun_fire);
        return false;
    }

    // check explicit inspect last
    if (ent->client->inspect)
    {
        if (ent->client->weapanim[WEAPID_GUN] == &weap_sshotgun_idle)
        {
            SuperShotgun_PickIdle(ent);
            return false;
        }

        ent->client->inspect = false;
    }

    return true;
}