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

static bool Blaster_Fire(edict_t *ent)
{
    vec3_t      start;
    vec3_t      forward, right;
    vec3_t      offset;
    int         damage = 50;

    AngleVectors(ent->client->v_angle, forward, right, NULL);

    ent->client->kick_angles[0] = -2;

    VectorSet(offset, 0, 0, ent->viewheight - 6);
    P_ProjectSource(ent->client, ent->s.origin, offset, forward, right, start);

    if (is_quad) {
        damage *= 4;
    }

    fire_blaster(ent, start, forward, damage, 800, EF_HYPERBLASTER, true);

    // send muzzle flash
    SV_WriteByte(svc_muzzleflash);
    SV_WriteEntity(ent);
    SV_WriteByte(MZ_BLASTER);
    SV_Multicast(ent->s.origin, MULTICAST_PVS, false);

    PlayerNoise(ent, ent->s.origin, PNOISE_WEAPON);

    if (!(dmflags.integer & DF_INFINITE_AMMO))
        ent->client->pers.inventory[ent->client->ammo_index] -= ent->client->pers.weapon->quantity;

    return true;
}

enum {
    ANIM_EQUIP_FIRST    = 0,
    ANIM_EQUIP_LAST     = 8,
    ANIM_IDLE_FIRST     = 9,
    ANIM_IDLE_LAST      = 67,
    ANIM_ATTACK_FIRST   = 68,
    ANIM_ATTACK_LAST    = 91,
    ANIM_PUTAWAY_FIRST  = 92,
    ANIM_PUTAWAY_LAST   = 95,
    ANIM_INSPECT_FIRST  = 96,
    ANIM_INSPECT_LAST   = 176
};

static bool Blaster_PickIdle(edict_t *ent);
static bool Blaster_Idle(edict_t *ent);

const weapon_animation_t weap_blaster_activate = {
    .start = ANIM_EQUIP_FIRST, .end = ANIM_EQUIP_LAST,
    .finished = Blaster_PickIdle
};

const weapon_animation_t weap_blaster_deactivate = {
    .start = ANIM_PUTAWAY_FIRST, .end = ANIM_PUTAWAY_LAST,
    .finished = ChangeWeapon
};

const weapon_animation_t weap_blaster_idle = {
    .start = ANIM_IDLE_FIRST, .end = ANIM_IDLE_LAST,
    .frame = Blaster_Idle, .finished = Blaster_PickIdle
};

const weapon_animation_t weap_blaster_inspect = {
    .start = ANIM_INSPECT_FIRST, .end = ANIM_INSPECT_LAST,
    .frame = Blaster_Idle, .finished = Blaster_PickIdle
};

const weapon_animation_t weap_blaster_fire = {
    .start = ANIM_ATTACK_FIRST, .end = ANIM_ATTACK_LAST, .next = &weap_blaster_idle,
    .events = (const weapon_event_t []) {
        { Blaster_Fire, 75, 75 },
        { NULL }
    }
};

static bool Blaster_PickIdle(edict_t *ent)
{
    if (ent->client->weapanim[WEAPID_GUN] == &weap_blaster_inspect ||
        (!ent->client->inspect || random() < WEAPON_RANDOM_INSPECT_CHANCE))
    {
        Weapon_SetAnimation(ent, &weap_blaster_idle);
        return false;
    }

    Weapon_SetAnimation(ent, &weap_blaster_inspect);

    ent->client->inspect = false;

    return false;
}

static bool Blaster_Idle(edict_t *ent)
{
    if (ent->client->newweapon || !ent->client->pers.inventory[ent->client->pers.weapon->id])
    {
        Weapon_SetAnimation(ent, &weap_blaster_deactivate);
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

        Blaster_Fire(ent);

        Weapon_SetAnimation(ent, &weap_blaster_fire);
        return false;
    }

    // check explicit inspect last
    if (ent->client->inspect)
    {
        if (ent->client->weapanim[WEAPID_GUN] == &weap_blaster_idle)
        {
            Blaster_PickIdle(ent);
            return false;
        }

        ent->client->inspect = false;
    }

    return true;
}