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

/*
=================
fire_rail
=================
*/
void fire_lightning(edict_t* self, vec3_t start, vec3_t aimdir, int damage, int kick)
{
    vec3_t      from;
    vec3_t      end;
    trace_t     tr;
    edict_t* ignore;
    int         mask;
    bool        water;
    float       lastfrac;

    VectorMA(start, 768, aimdir, end);
    VectorCopy(start, from);
    ignore = self;
    water = false;
    mask = MASK_SHOT | CONTENTS_SLIME | CONTENTS_LAVA;
    lastfrac = 1;
    while (ignore) {
        tr = SV_Trace(from, NULL, NULL, end, ignore, mask);

        if (tr.contents & (CONTENTS_SLIME | CONTENTS_LAVA)) {
            mask &= ~(CONTENTS_SLIME | CONTENTS_LAVA);
            water = true;
        }
        else {
            //ZOID--added so rail goes through SOLID_BBOX entities (gibs, etc)
            if (((tr.ent->svflags & SVF_MONSTER) || (tr.ent->client) ||
                (tr.ent->solid == SOLID_BBOX)) && (lastfrac + tr.fraction > 0))
                ignore = tr.ent;
            else
                ignore = NULL;

            if ((tr.ent != self) && (tr.ent->takedamage))
                T_Damage(tr.ent, self, self, aimdir, tr.endpos, tr.plane.normal, damage, kick, 0, MOD_RAILGUN);
        }

        VectorCopy(tr.endpos, from);
        lastfrac = tr.fraction;
    }

    // send gun puff / flash
    SV_WriteByte(svc_temp_entity);
    SV_WriteByte(TE_LIGHTNING);
    SV_WriteShort(self->s.number);
    SV_WriteShort(0);
    SV_WritePos(start);
    SV_WritePos(tr.endpos);
    SV_Multicast(self->s.origin, MULTICAST_PHS, false);
    if (water) {
        SV_WriteByte(svc_temp_entity);
        SV_WriteByte(TE_LIGHTNING);
        SV_WriteShort(self->s.number);
        SV_WriteShort(0);
        SV_WritePos(start);
        SV_WritePos(tr.endpos);
        SV_Multicast(tr.endpos, MULTICAST_PHS, false);
    }

    if (self->client)
        PlayerNoise(self, tr.endpos, PNOISE_IMPACT);
}

static void Thunder_Fire(edict_t* ent)
{
    vec3_t      start;
    vec3_t      forward, right;
    vec3_t      offset;
    int         damage = 10;
    int         kick = 5;

    AngleVectors(ent->client->v_angle, forward, right, NULL);

    //ent->client->kick_angles[0] = -2;

    VectorSet(offset, 0, 0, ent->viewheight - 6);
    P_ProjectSource(ent->client, ent->s.origin, offset, forward, right, start);

    if (is_quad) {
        damage *= 4;
        kick *= 4;
    }

    fire_lightning(ent, start, forward, damage, kick);

    // send muzzle flash
    /*SV_WriteByte(svc_muzzleflash);
    SV_WriteEntity(ent);
    SV_WriteByte(MZ_SHOTGUN);
    SV_Multicast(ent->s.origin, MULTICAST_PVS, false);*/

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
    ANIM_ATTACK_LAST = 94,
    ANIM_PUTAWAY_FIRST = 95,
    ANIM_PUTAWAY_LAST = 97,
    ANIM_INSPECT_FIRST = 98,
    ANIM_INSPECT_LAST = 157
};

static bool Thunder_PickIdle(edict_t* ent, weapon_id_t id);
static bool Thunder_Idle(edict_t* ent, weapon_id_t id);

const weapon_animation_t weap_thunder_activate = {
    .start = ANIM_EQUIP_FIRST, .end = ANIM_EQUIP_LAST,
    .finished = Thunder_PickIdle
};

const weapon_animation_t weap_thunder_deactivate = {
    .start = ANIM_PUTAWAY_FIRST, .end = ANIM_PUTAWAY_LAST,
    .finished = ChangeWeapon
};

const weapon_animation_t weap_thunder_idle = {
    .start = ANIM_IDLE_FIRST, .end = ANIM_IDLE_LAST,
    .frame = Thunder_Idle, .finished = Thunder_PickIdle
};

const weapon_animation_t weap_thunder_inspect = {
    .start = ANIM_INSPECT_FIRST, .end = ANIM_INSPECT_LAST,
    .frame = Thunder_Idle, .finished = Thunder_PickIdle
};

static bool Thunder_Firing(edict_t* ent, weapon_id_t id);

const weapon_animation_t weap_thunder_fire = {
    .start = ANIM_ATTACK_FIRST, .end = ANIM_ATTACK_LAST,
    .frame = Thunder_Firing
};

static bool Thunder_Firing(edict_t* ent, weapon_id_t id)
{
    if (!(ent->client->buttons & BUTTON_ATTACK) || !Weapon_AmmoCheck(ent)) {
        Weapon_SetAnimation(ent, id, &weap_thunder_idle);
        return false;
    }

    Thunder_Fire(ent);
    return true;
}

static bool Thunder_PickIdle(edict_t* ent, weapon_id_t id)
{
    if (ent->client->weapanim[id] == &weap_thunder_inspect ||
        (!ent->client->inspect && random() < WEAPON_RANDOM_INSPECT_CHANCE))
    {
        Weapon_SetAnimation(ent, id, &weap_thunder_idle);
        return false;
    }

    Weapon_SetAnimation(ent, id, &weap_thunder_inspect);

    ent->client->inspect = false;

    return false;
}

static bool Thunder_Idle(edict_t* ent, weapon_id_t id)
{
    // check weapon change
    if (Weapon_CheckChange(ent, &weap_thunder_deactivate, id))
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

        Thunder_Fire(ent);

        Weapon_SetAnimation(ent, id, &weap_thunder_fire);
        return false;
    }

    // check explicit inspect last
    if (ent->client->inspect)
    {
        if (ent->client->weapanim[id] == &weap_thunder_idle)
        {
            Thunder_PickIdle(ent, id);
            return false;
        }

        ent->client->inspect = false;
    }

    return true;
}