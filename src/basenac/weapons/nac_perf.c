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

// important frame #s from model
enum {
    ANIM_EQUIP_FIRST    = 0,
    ANIM_EQUIP_LAST     = 8,
    ANIM_IDLE_FIRST,
    ANIM_IDLE_LAST      = 57,
    ANIM_ATTACK_FIRST   = 62,
    ANIM_ATTACK_LAST    = 69,
    ANIM_PUTAWAY_FIRST  = 58,
    ANIM_PUTAWAY_LAST   = 60,
    ANIM_INSPECT_FIRST  = 70,
    ANIM_INSPECT_LAST   = 126
};

// in radians per second
#define MAX_ROTATION ((float) (M_PI * 4.25f))
#define ROTATION_SPEED ((float) (MAX_ROTATION * BASE_FRAMETIME_S))

static bool Perf_PickIdle(edict_t *ent);

static bool Perf_SetSpinPitch(edict_t *ent)
{
    if (ent->client->ps.gun[WEAPID_GUN].spin <= 0) {
        ent->client->weapon_sound = 0;
        ent->s.sound_pitch = 0;
    } else {
        ent->s.sound_pitch = -63 + ((ent->client->ps.gun[WEAPID_GUN].spin / MAX_ROTATION) * 63 * 2);
    }

    return true;
}

static bool Perf_SpinDown(edict_t *ent)
{
    ent->client->ps.gun[WEAPID_GUN].spin = max(0.f, ent->client->ps.gun[WEAPID_GUN].spin - ROTATION_SPEED);
    Perf_SetSpinPitch(ent);
    return true;
}

static bool Perf_Idle(edict_t *ent);
static bool Perf_Firing(edict_t *ent);

const weapon_animation_t weap_perf_activate = {
    .start = ANIM_EQUIP_FIRST, .end = ANIM_EQUIP_LAST,
    .finished = Perf_PickIdle
};

const weapon_animation_t weap_perf_deactivate = {
    .start = ANIM_PUTAWAY_FIRST, .end = ANIM_PUTAWAY_LAST,
    .frame = Perf_SpinDown, .finished = ChangeWeapon
};

const weapon_animation_t weap_perf_idle = {
    .start = ANIM_IDLE_FIRST, .end = ANIM_IDLE_LAST,
    .frame = Perf_Idle, .finished = Perf_PickIdle
};

const weapon_animation_t weap_perf_inspect = {
    .start = ANIM_INSPECT_FIRST, .end = ANIM_INSPECT_LAST,
    .frame = Perf_Idle, .finished = Perf_PickIdle
};

const weapon_animation_t weap_perf_firing = {
    .start = ANIM_ATTACK_FIRST, .end = ANIM_ATTACK_LAST,
    .frame = Perf_Firing
};

static void Perf_Fire(edict_t *ent)
{
    vec3_t forward, right, up, start;

    // get start / end positions
    AngleVectors(ent->client->v_angle, forward, right, up);
    VectorCopy(ent->s.origin, start);
    start[2] += ent->viewheight;
    VectorMA(start, 32, forward, start);
    VectorMA(start, -10, up, start);

    float rotation = fmodf(G_MsToSec(level.time) * 10.0f, M_PI * 2);

    VectorMA(start, -4 * cosf(rotation), right, start);
    VectorMA(start, 4 * sinf(rotation), up, start);

    for (int i = 0; i < 3; i++) {
        ent->client->kick_angles[i] += crandom() * 0.5f;
    }

    fire_nail(ent, start, forward, 12, 2000);

    SV_WriteByte(svc_muzzleflash);
    SV_WriteEntity(ent);
    SV_WriteByte(MZ_HYPERBLASTER);
    SV_Multicast(ent->s.origin, MULTICAST_PVS, false);

    if (!(dmflags.integer & DF_INFINITE_AMMO)) {
        ent->client->pers.inventory[ent->client->ammo_index]--;
    }
}

static bool Perf_CheckSpin(edict_t *ent)
{
    // can't fire, so we're gonna switch
    if (!Weapon_AmmoCheck(ent)) {

        // if we're in firing, switch to idle animation
        if (ent->client->weapanim[WEAPID_GUN] == &weap_perf_firing) {
            Weapon_SetAnimation(ent, &weap_perf_idle);
        }

        return false;
    }
    
    // attack is being held, so start spin sound
    ent->client->weapon_sound = SV_SoundIndex(ASSET_SOUND_PERFORATOR_SPIN);

    // calculate spin value
    ent->client->ps.gun[WEAPID_GUN].spin = min(MAX_ROTATION, ent->client->ps.gun[WEAPID_GUN].spin + ROTATION_SPEED);

    Perf_SetSpinPitch(ent);

    // check if we're firing nails 
    if (ent->client->ps.gun[WEAPID_GUN].spin < MAX_ROTATION) {

        // if we're in firing, switch to idle animation
        if (ent->client->weapanim[WEAPID_GUN] == &weap_perf_firing) {
            Weapon_SetAnimation(ent, &weap_perf_idle);
            return false;
        }

        return true;
    }
    
    if (ent->client->ps.gun[WEAPID_GUN].frame < ANIM_ATTACK_FIRST) {
        ent->client->ps.gun[WEAPID_GUN].frame = ANIM_ATTACK_FIRST;
    }

    Perf_Fire(ent);

    ent->client->anim_priority = ANIM_ATTACK;
    if (ent->client->ps.pmove.pm_flags & PMF_DUCKED) {
        ent->s.frame = (FRAME_crattak1 - 1) + (level.time % (int)(BASE_FRAMETIME * 2));
        ent->client->anim_end = FRAME_crattak9;
    } else {
        ent->s.frame = (FRAME_attack1 - 1) + (level.time % (int)(BASE_FRAMETIME * 2));
        ent->client->anim_end = FRAME_attack8;
    }

    // if we're in idle, switch to firing animation
    if (ent->client->weapanim[WEAPID_GUN] != &weap_perf_firing) {
        Weapon_SetAnimation(ent, &weap_perf_firing);
        return false;
    }

    return true;
}

static bool Perf_Firing(edict_t *ent)
{
    if (!(ent->client->buttons & BUTTON_ATTACK)) {
        Perf_SpinDown(ent);

        if (ent->client->weapanim[WEAPID_GUN] == &weap_perf_firing) {
            Weapon_SetAnimation(ent, &weap_perf_idle);
            return false;
        }

        return false;
    }
    
    return Perf_CheckSpin(ent);
}

static bool Perf_Idle(edict_t *ent)
{
    if (ent->client->newweapon || !ent->client->pers.inventory[ent->client->pers.weapon->id])
    {
        Weapon_SetAnimation(ent, &weap_perf_deactivate);
        Weapon_Activate(ent, true);
        return false;
    }

    if (!Perf_Firing(ent)) {

        if (ent->client->weapanim[WEAPID_GUN] == &weap_perf_inspect) {
            return true;
        } else if (ent->client->inspect) {
            if (ent->client->weapanim[WEAPID_GUN] == &weap_perf_idle)
            {
                Perf_PickIdle(ent);
                return false;
            }

            ent->client->inspect = false;
        }

        return false;
    }

    return true;
}


static bool Perf_PickIdle(edict_t *ent)
{
    if (ent->client->weapanim[WEAPID_GUN] == &weap_perf_inspect ||
        (!ent->client->inspect || random() < WEAPON_RANDOM_INSPECT_CHANCE))
    {
        Weapon_SetAnimation(ent, &weap_perf_idle);
        return false;
    }
    
    Weapon_SetAnimation(ent, &weap_perf_inspect);

    ent->client->inspect = false;

    return false;
}
