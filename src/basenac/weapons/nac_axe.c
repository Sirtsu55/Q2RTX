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

static bool Axe_PickIdle(edict_t *ent);

const weapon_animation_t weap_axe_activate = {
    0, 8, NULL,
    NULL, Axe_PickIdle,
    NULL
};

const weapon_animation_t weap_axe_deactivate = {
    163, 168, NULL,
    NULL, ChangeWeapon,
    NULL
};

extern const weapon_animation_t weap_axe_idle;

static void Axe_Attack(edict_t *ent, int damage)
{
    if (!ent->client->axe_attack)
        return;

    vec3_t forward, right, start, end, offset;

    AngleVectors(ent->client->v_angle, forward, right, NULL);
    VectorSet(offset, 0, 0, ent->viewheight - 8);
    P_ProjectSource(ent->client, ent->s.origin, offset, forward, right, start);
    VectorMA(start, 48.f, forward, end);
            
    trace_t tr = SV_Trace(ent->s.origin, vec3_origin, vec3_origin, start, ent, MASK_SHOT);

    if (tr.fraction == 1.f)
        tr = SV_Trace(start, vec3_origin, vec3_origin, end, ent, MASK_SHOT);

    if (tr.fraction < 1.f)
    {
        if (tr.ent && tr.ent->takedamage)
        {
            T_Damage(tr.ent, ent, ent, forward, tr.endpos, tr.plane.normal, damage, 0, 0, MOD_BLASTER);
            SV_StartSound(ent, CHAN_AUTO, SV_SoundIndex("makron/brain1.wav"), 1.f, ATTN_NORM, 0);
        }
        else if (!(tr.surface->flags & SURF_SKY))
        {
            SV_WriteByte(svc_temp_entity);
            SV_WriteByte(TE_GUNSHOT);
            SV_WritePos(tr.endpos);
            SV_WriteDir(tr.plane.normal);
            SV_Multicast(tr.endpos, MULTICAST_PVS, false);
        }

        ent->client->axe_attack = false;
    }
}

static bool Axe_RegularAttack(edict_t *ent)
{
    Axe_Attack(ent, 8);
    return true;
}

static bool Axe_RestoreCharge(edict_t *ent)
{
    ent->client->can_charge_axe = true;
    return true;
}

static bool Axe_NextAttack(edict_t *ent);
static bool Axe_QuickAttack(edict_t *ent);
static bool Axe_EnsureCharge(edict_t *ent);
static bool Axe_TransitionIntoCharge(edict_t *ent);
static bool Axe_InitialCharge(edict_t *ent);

const weapon_animation_t weap_axe_attack1 = {
    116, 131, &weap_axe_idle,
    NULL, Axe_NextAttack,
    (const weapon_event_t []) {
        { Axe_EnsureCharge, WEAPON_EVENT_MINMAX, 118 },
        { Axe_InitialCharge, 119, 119 },
        { Axe_RestoreCharge, 120, 120 },
        { Axe_RegularAttack, 120, 123 },
        { Axe_EnsureCharge, 119, 121 },
        { Axe_TransitionIntoCharge, 122, 122 },
        { Axe_QuickAttack, 123, WEAPON_EVENT_MINMAX },
        { NULL }
    }
};

const weapon_animation_t weap_axe_attack2 = {
    132, 147, &weap_axe_idle,
    NULL, Axe_NextAttack,
    (const weapon_event_t []) {
        { Axe_RegularAttack, 136, 139 },
        { Axe_EnsureCharge, 135, 137 },
        { Axe_TransitionIntoCharge, 138, 138 },
        { Axe_QuickAttack, 139, WEAPON_EVENT_MINMAX },
        { NULL }
    }
};

const weapon_animation_t weap_axe_attack3 = {
    148, 162, &weap_axe_idle,
    NULL, Axe_NextAttack,
    (const weapon_event_t []) {
        { Axe_RegularAttack, 152, 155 },
        { Axe_EnsureCharge, 151, 153 },
        { Axe_TransitionIntoCharge, 154, 154 },
        { Axe_QuickAttack, 155, WEAPON_EVENT_MINMAX },
        { NULL }
    }
};

static bool Axe_ChargedAttack(edict_t *ent)
{
    Axe_Attack(ent, 80);
    return true;
}

const weapon_animation_t weap_axe_attack_charged = {
    190, 214, &weap_axe_idle,
    NULL, NULL,
    (const weapon_event_t []) {
        { Axe_ChargedAttack, 190, 200 },
        { NULL }
    }
};

static bool Axe_ChargeReady(edict_t *ent)
{
    if (!(ent->client->buttons & BUTTON_ATTACK))
    {
        Weapon_SetAnimation(ent, &weap_axe_attack_charged);

        vec3_t forward, right, start, offset;

        AngleVectors(ent->client->v_angle, forward, right, NULL);
        VectorSet(offset, 0, 0, ent->viewheight - 8);
        P_ProjectSource(ent->client, ent->s.origin, offset, forward, right, start);

        if (!(ent->client->ps.pmove.pm_flags & PMF_ON_GROUND))
            ent->velocity[2] = 100.f;
        else
        {
            if (ent->client->v_angle[0] < -2.f)
            {
                ent->s.origin[2] -= 2.f;
                ent->client->ps.pmove.origin[2] -= 2.f;
                ent->velocity[2] = 100.f;
                ent->groundentity = NULL;

                SV_LinkEntity(ent);
            }
        }

        VectorMA(ent->velocity, 250.f, forward, ent->velocity);

        ent->client->ps.pmove.pm_flags &= ~PMF_ON_GROUND;
        ent->client->ps.pmove.pm_flags |= PMF_TIME_LAND;
        ent->client->ps.pmove.pm_time = 42;

        return false;
    }

    return true;
}

const weapon_animation_t weap_axe_charge_hold = {
    175, 189, NULL,
    Axe_ChargeReady, NULL,
    NULL
};

static bool Axe_Uncharge(edict_t *ent)
{
    if (!(ent->client->buttons & BUTTON_ATTACK))
    {
        Weapon_SetAnimationFrame(ent, &weap_axe_attack1, 120);
        return false;
    }

    return true;
}

const weapon_animation_t weap_axe_charge = {
    170, 174, &weap_axe_charge_hold,
    Axe_Uncharge, NULL,
    NULL
};

static bool Axe_InitialCharge(edict_t *ent)
{
    if (!(ent->client->buttons & BUTTON_ATTACK) ||
        !ent->client->can_charge_axe)
    {
        ent->client->can_charge_axe = true;
        return true;
    }

    Weapon_SetAnimation(ent, &weap_axe_charge);
    return false;
}

static bool Axe_AttackTransitionCharge(edict_t *ent)
{
    Weapon_SetAnimation(ent, &weap_axe_charge_hold);
    return false;
}

const weapon_animation_t weap_axe_transition_attack1 = {
    304, 309, NULL,
    Axe_Uncharge, Axe_AttackTransitionCharge,
    NULL
};

const weapon_animation_t weap_axe_transition_attack2 = {
    311, 316, NULL,
    Axe_Uncharge, Axe_AttackTransitionCharge,
    NULL
};

const weapon_animation_t weap_axe_transition_attack3 = {
    318, 323, NULL,
    Axe_Uncharge, Axe_AttackTransitionCharge,
    NULL
};

static bool Axe_TransitionIntoCharge(edict_t *ent)
{
    if (!(ent->client->buttons & BUTTON_ATTACK) ||
        !ent->client->can_charge_axe)
        return true;

    if (ent->client->weaponanimation == &weap_axe_attack1)
        Weapon_SetAnimation(ent, &weap_axe_transition_attack1);
    else if (ent->client->weaponanimation == &weap_axe_attack2)
        Weapon_SetAnimation(ent, &weap_axe_transition_attack2);
    else if (ent->client->weaponanimation == &weap_axe_attack3)
        Weapon_SetAnimation(ent, &weap_axe_transition_attack3);
    return false;
}

static bool Axe_EnsureCharge(edict_t *ent)
{
    if (!(ent->client->buttons & BUTTON_ATTACK))
        ent->client->can_charge_axe = false;

    return true;
}

static bool Axe_NextAttack(edict_t *ent)
{
    if (ent->client->buttons & BUTTON_ATTACK)
    {
        if (ent->client->weaponanimation == &weap_axe_attack1)
            Weapon_SetAnimation(ent, &weap_axe_attack2);
        else if (ent->client->weaponanimation == &weap_axe_attack2)
            Weapon_SetAnimation(ent, &weap_axe_attack3);
        else
            Weapon_SetAnimation(ent, &weap_axe_attack1);
        
        ent->client->axe_attack = ent->client->can_charge_axe = true;
        ent->client->can_release_charge = false;
        return false;
    }

    return true;
}

static bool Axe_QuickAttack(edict_t *ent)
{
    if (ent->client->latched_buttons & BUTTON_ATTACK)
        return Axe_NextAttack(ent);

    return true;
}

static bool Axe_Idle(edict_t *ent)
{
    // check weapon change
    if (ent->client->newweapon)
    {
        Weapon_SetAnimation(ent, &weap_axe_deactivate);
        return false;
    }

    // check attack transition
    if (((ent->client->latched_buttons | ent->client->buttons) & BUTTON_ATTACK))
    {
        ent->client->latched_buttons &= ~BUTTON_ATTACK;
        ent->client->weaponstate = WEAPON_FIRING;
        ent->client->axe_attack = ent->client->can_charge_axe = true;
        ent->client->can_release_charge = false;
        Weapon_SetAnimation(ent, &weap_axe_attack1);
        return false;
    }

    // check explicit inspect
    if (ent->client->inspect)
    {
        if (ent->client->weaponanimation == &weap_axe_idle)
        {
            Axe_PickIdle(ent);
            return false;
        }

        ent->client->inspect = false;
    }

    return true;
}

const weapon_animation_t weap_axe_idle = {
    9, 115, NULL,
    Axe_Idle, Axe_PickIdle,
    NULL
};

const weapon_animation_t weap_axe_inspect1 = {
    215, 244, NULL,
    Axe_Idle, Axe_PickIdle,
    NULL
};

const weapon_animation_t weap_axe_inspect2 = {
    245, 302, NULL,
    Axe_Idle, Axe_PickIdle,
    NULL
};

static bool Axe_PickIdle(edict_t *ent)
{
    // at end of inspect animations always go back to idle
    // otherwise, have a 20% chance of inspecting
    if (ent->client->weaponanimation == &weap_axe_inspect1 ||
        ent->client->weaponanimation == &weap_axe_inspect2 ||
        (!ent->client->inspect && random() < WEAPON_RANDOM_INSPECT_CHANCE))
    {
        Weapon_SetAnimation(ent, &weap_axe_idle);
        return false;
    }

    if (random() < 0.5f)
        Weapon_SetAnimation(ent, &weap_axe_inspect1);
    else
        Weapon_SetAnimation(ent, &weap_axe_inspect2);

    ent->client->inspect = false;
    return false;
}