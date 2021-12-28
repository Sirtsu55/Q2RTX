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


static bool     is_quad;
static byte     is_silenced;


void G_ProjectSource2(const vec3_t point, const vec3_t distance, const vec3_t forward, const vec3_t right, const vec3_t up, vec3_t result)
{
    result[0] = point[0] + forward[0] * distance[0] + right[0] * distance[1] + up[0] * distance[2];
    result[1] = point[1] + forward[1] * distance[0] + right[1] * distance[1] + up[1] * distance[2];
    result[2] = point[2] + forward[2] * distance[0] + right[2] * distance[1] + up[2] * distance[2];
}
static void P_ProjectSource(gclient_t *client, vec3_t point, vec3_t distance, vec3_t forward, vec3_t right, vec3_t result)
{
    vec3_t  _distance;

    VectorCopy(distance, _distance);
    if (client->pers.hand == LEFT_HANDED)
        _distance[1] *= -1;
    else if (client->pers.hand == CENTER_HANDED)
        _distance[1] = 0;
    G_ProjectSource(point, _distance, forward, right, result);
}
static void P_ProjectSource2(gclient_t *client, vec3_t point, vec3_t distance, vec3_t forward, vec3_t right, vec3_t up, vec3_t result)
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

    if (deathmatch->value)
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
        level.sound_entity_framenum = level.framenum;
    } else { // type == PNOISE_IMPACT
        noise = who->mynoise2;
        level.sound2_entity = noise;
        level.sound2_entity_framenum = level.framenum;
    }

    VectorCopy(where, noise->s.origin);
    VectorSubtract(where, noise->maxs, noise->absmin);
    VectorAdd(where, noise->maxs, noise->absmax);
    noise->last_sound_framenum = level.framenum;
    gi.linkentity(noise);
}


bool Pickup_Weapon(edict_t *ent, edict_t *other)
{
    int         index;
    gitem_t     *ammo;

    index = ITEM_INDEX(ent->item);

    if ((((int)(dmflags->value) & DF_WEAPONS_STAY) || coop->value)
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
        if ((int)dmflags->value & DF_INFINITE_AMMO)
            Add_Ammo(other, ammo, 1000);
        else
            Add_Ammo(other, ammo, ammo->quantity);

        if (!(ent->spawnflags & DROPPED_PLAYER_ITEM)) {
            if (deathmatch->value) {
                if ((int)(dmflags->value) & DF_WEAPONS_STAY)
                    ent->flags |= FL_RESPAWN;
                else
                    SetRespawn(ent, 30);
            }
            if (coop->value)
                ent->flags |= FL_RESPAWN;
        }
    }

    if (other->client->pers.weapon != ent->item &&
        (other->client->pers.inventory[index] == 1) &&
        (!deathmatch->value || other->client->pers.weapon == FindItem("Axe")))
        other->client->newweapon = ent->item;

    return true;
}


/*
===============
ChangeWeapon

The old weapon has been dropped all the way, so make the new one
current
===============
*/
void ChangeWeapon(edict_t *ent)
{
    int i;

    if (ent->client->grenade_framenum) {
        ent->client->grenade_framenum = level.framenum;
        ent->client->grenade_framenum = 0;
    }

    ent->client->pers.lastweapon = ent->client->pers.weapon;
    ent->client->pers.weapon = ent->client->newweapon;
    ent->client->newweapon = NULL;
    ent->client->machinegun_shots = 0;

    // set visible model
    if (ent->s.modelindex == 255) {
        if (ent->client->pers.weapon)
            i = ((ent->client->pers.weapon->weapmodel & 0xff) << 8);
        else
            i = 0;
        ent->s.skinnum = (ent - g_edicts - 1) | i;
    }

    if (ent->client->pers.weapon && ent->client->pers.weapon->ammo)
        ent->client->ammo_index = ITEM_INDEX(FindItem(ent->client->pers.weapon->ammo));
    else
        ent->client->ammo_index = 0;

    ent->client->weapon_sound = 0;
    ent->s.sound_pitch = 0;
    ent->client->ps.gunspin = 0;

    if (!ent->client->pers.weapon) {
        // dead
        ent->client->ps.gunindex = 0;
        return;
    }

    ent->client->weaponstate = WEAPON_ACTIVATING;
    ent->client->ps.gunframe = 0;
    ent->client->ps.gunindex = gi.modelindex(ent->client->pers.weapon->view_model);

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
=================
NoAmmoWeaponChange
=================
*/
void NoAmmoWeaponChange(edict_t *ent)
{
    if (ent->client->pers.inventory[ITEM_INDEX(FindItem("bullets"))]
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
        ChangeWeapon(ent);
    }

    // call active weapon think routine
    if (ent->client->pers.weapon && ent->client->pers.weapon->weaponthink) {
        is_quad = (ent->client->quad_framenum > level.framenum);
        if (ent->client->silencer_shots)
            is_silenced = MZ_SILENCED;
        else
            is_silenced = 0;
        ent->client->pers.weapon->weaponthink(ent);
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

    if (item->ammo && !g_select_empty->value && !(item->flags & IT_AMMO)) {
        ammo_item = FindItem(item->ammo);
        ammo_index = ITEM_INDEX(ammo_item);

        if (!ent->client->pers.inventory[ammo_index]) {
            gi.cprintf(ent, PRINT_HIGH, "No %s for %s.\n", ammo_item->pickup_name, item->pickup_name);
            return;
        }

        if (ent->client->pers.inventory[ammo_index] < item->quantity) {
            gi.cprintf(ent, PRINT_HIGH, "Not enough %s for %s.\n", ammo_item->pickup_name, item->pickup_name);
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

    if ((int)(dmflags->value) & DF_WEAPONS_STAY)
        return;

    index = ITEM_INDEX(item);
    // see if we're already using it
    if (((item == ent->client->pers.weapon) || (item == ent->client->newweapon)) && (ent->client->pers.inventory[index] == 1)) {
        gi.cprintf(ent, PRINT_HIGH, "Can't drop current weapon\n");
        return;
    }

    Drop_Item(ent, item);
    ent->client->pers.inventory[index]--;
}

/*
======================================================================

AXE

======================================================================
*/

void Weapon_Axe(edict_t *ent)
{
    enum {
        ANIM_EQUIP_FIRST    = 0,
        ANIM_EQUIP_LAST     = 8,
        ANIM_IDLE_FIRST,
        ANIM_IDLE_LAST      = 115,
        ANIM_ATTACK1_FIRST,
        ANIM_ATTACK1_LAST   = 131,
        ANIM_ATTACK2_FIRST,
        ANIM_ATTACK2_LAST   = 147,
        ANIM_ATTACK3_FIRST,
        ANIM_ATTACK3_LAST   = 162,
        ANIM_PUTAWAY_FIRST  = 173,
        ANIM_PUTAWAY_LAST   = 178,
        ANIM_HUG_FIRST      = 163,
        ANIM_HUG_WAIT       = 168,
        ANIM_HUG_LAST       = 172
    };
    ent->client->ps.gunframe++;

    switch (ent->client->weaponstate)
    {
    case WEAPON_ACTIVATING:
        if (ent->client->ps.gunframe == ANIM_EQUIP_LAST)
            ent->client->weaponstate = WEAPON_READY;
        break;
    case WEAPON_READY:
        if ((ent->client->newweapon) && (ent->client->weaponstate != WEAPON_FIRING)) {
            ent->client->weaponstate = WEAPON_DROPPING;
            ent->client->ps.gunframe = ANIM_PUTAWAY_FIRST;
        } else if (((ent->client->latched_buttons | ent->client->buttons) & BUTTON_ATTACK)) {
            ent->client->latched_buttons &= ~BUTTON_ATTACK;
            ent->client->ps.gunframe = ANIM_ATTACK1_FIRST;
            ent->client->weaponstate = WEAPON_FIRING;
            ent->client->axe_attack = true;
        } else if (ent->client->ps.gunframe == ANIM_IDLE_LAST + 1 || ent->client->ps.gunframe == ANIM_HUG_LAST + 1) {
            ent->client->ps.gunframe = ANIM_IDLE_FIRST;
        }
        break;
    case WEAPON_FIRING:
        if (ent->client->ps.gunframe == ANIM_ATTACK1_LAST + 1 || ent->client->ps.gunframe == ANIM_ATTACK2_LAST + 1 || ent->client->ps.gunframe == ANIM_ATTACK3_LAST + 1)
        {
            ent->client->axe_attack = true;

            if (!(ent->client->buttons & BUTTON_ATTACK))
            {
                ent->client->ps.gunframe = ANIM_IDLE_FIRST;
                ent->client->weaponstate = WEAPON_READY;
            }
            else if (ent->client->ps.gunframe == ANIM_ATTACK3_LAST + 1)
                ent->client->ps.gunframe = ANIM_ATTACK1_FIRST;
        }

        if (ent->client->axe_attack && (
            (ent->client->ps.gunframe >= 120 && ent->client->ps.gunframe <= 123) ||
            (ent->client->ps.gunframe >= 134 && ent->client->ps.gunframe <= 137) ||
            (ent->client->ps.gunframe >= 151 && ent->client->ps.gunframe <= 154)))
        {
            vec3_t forward, right, start, end, offset;

            AngleVectors(ent->client->v_angle, forward, right, NULL);
            VectorSet(offset, 0, 0, ent->viewheight - 8);
            P_ProjectSource(ent->client, ent->s.origin, offset, forward, right, start);
            VectorMA(start, 48.f, forward, end);
            
            trace_t tr = gi.trace(ent->s.origin, vec3_origin, vec3_origin, start, ent, MASK_SHOT);

            if (tr.fraction == 1.f)
                tr = gi.trace(start, vec3_origin, vec3_origin, end, ent, MASK_SHOT);

            if (tr.fraction < 1.f)
            {
                if (tr.ent && tr.ent->takedamage)
                {
                    T_Damage(tr.ent, ent, ent, forward, tr.endpos, tr.plane.normal, 8, 0, 0, MOD_BLASTER);
                    gi.sound(ent, CHAN_AUTO, gi.soundindex("makron/brain1.wav"), 1.f, ATTN_NORM, 0);
                }
                else if (!(tr.surface->flags & SURF_SKY))
                {
                    gi.WriteByte(svc_temp_entity);
                    gi.WriteByte(TE_GUNSHOT);
                    gi.WritePosition(tr.endpos);
                    gi.WriteDir(tr.plane.normal);
                    gi.multicast(tr.endpos, MULTICAST_PVS);
                }

                ent->client->axe_attack = false;
            }
        }

        if (ent->client->latched_buttons & BUTTON_ATTACK)
        {
            ent->client->latched_buttons &= ~BUTTON_ATTACK;

            if (ent->client->ps.gunframe >= 121 && ent->client->ps.gunframe <= ANIM_ATTACK2_FIRST)
            {
                ent->client->ps.gunframe = ANIM_ATTACK2_FIRST + 1;
                ent->client->axe_attack = true;
            }
            else if (ent->client->ps.gunframe >= 138 && ent->client->ps.gunframe <= ANIM_ATTACK3_FIRST)
            {
                ent->client->ps.gunframe = ANIM_ATTACK3_FIRST + 1;
                ent->client->axe_attack = true;
            }
            else if (ent->client->ps.gunframe >= 154 && ent->client->ps.gunframe <= ANIM_ATTACK3_LAST)
            {
                ent->client->ps.gunframe = ANIM_ATTACK1_FIRST + 1;
                ent->client->axe_attack = true;
            }
        }
        break;
    case WEAPON_DROPPING:
        if (ent->client->ps.gunframe == ANIM_PUTAWAY_LAST)
            ChangeWeapon(ent);
        break;
    }
}

/*
======================================================================

PERFORATOR

======================================================================
*/

// in radians per second
#define MAX_ROTATION ((float) (M_PI * 4.25f))
#define ROTATION_SPEED ((float) (MAX_ROTATION * BASE_FRAMETIME_S))

static void weapon_perforator_fire(edict_t *ent)
{
    vec3_t forward, right, up, start;

    // get start / end positions
    AngleVectors(ent->client->v_angle, forward, right, up);
    VectorCopy(ent->s.origin, start);
    start[2] += ent->viewheight;
    VectorMA(start, 32, forward, start);
    VectorMA(start, -10, up, start);

    float rotation = fmodf(level.time * 10.0f, M_PI * 2);

    VectorMA(start, -4 * cosf(rotation), right, start);
    VectorMA(start, 4 * sinf(rotation), up, start);

    fire_nail(ent, start, forward, 12, 2000);

    gi.WriteByte(svc_muzzleflash);
    gi.WriteShort(ent - g_edicts);
    gi.WriteByte(MZ_HYPERBLASTER | is_silenced);
    gi.multicast(ent->s.origin, MULTICAST_PVS);
}

void Weapon_Perforator(edict_t *ent)
{
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
    ent->client->ps.gunframe++;

    switch (ent->client->weaponstate)
    {
    case WEAPON_ACTIVATING:
        if (ent->client->ps.gunframe == ANIM_EQUIP_LAST)
            ent->client->weaponstate = WEAPON_READY;
        break;
    case WEAPON_READY:
        if (((ent->client->latched_buttons | ent->client->buttons) & BUTTON_ATTACK)) {
            ent->client->latched_buttons &= ~BUTTON_ATTACK;

            if (ent->client->ps.gunframe >= ANIM_INSPECT_FIRST) {
                ent->client->ps.gunframe = ANIM_IDLE_FIRST;
            }

            if (ent->client->pers.inventory[ent->client->ammo_index] >= ent->client->pers.weapon->quantity) {
                ent->client->weaponstate = WEAPON_FIRING;
            } else {
                if (level.framenum >= ent->pain_debounce_framenum) {
                    gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/noammo.wav"), 1, ATTN_NORM, 0);
                    ent->pain_debounce_framenum = level.framenum + 1 * BASE_FRAMERATE;
                }
                NoAmmoWeaponChange(ent);
                goto ready_noammo;
            }

            // intentional fall-through
        } else {
ready_noammo:
            if (ent->client->inspect) {
                ent->client->inspect = false;
                ent->client->ps.gunframe = ANIM_INSPECT_FIRST;
            } else if (ent->client->ps.gunframe == ANIM_IDLE_LAST) {
                if (random() < 0.1) {
                    ent->client->ps.gunframe = ANIM_INSPECT_FIRST;
                } else {
                    ent->client->ps.gunframe = ANIM_IDLE_FIRST;
                }
            } else if (ent->client->ps.gunframe == ANIM_INSPECT_LAST) {
                ent->client->ps.gunframe = ANIM_IDLE_FIRST;
            }

            if (ent->client->newweapon) {
                ent->client->weaponstate = WEAPON_DROPPING;
                ent->client->ps.gunframe = ANIM_PUTAWAY_FIRST;
                break;
            }
        }
    case WEAPON_FIRING:

        ent->client->weapon_sound = gi.soundindex("misc/lasfly.wav");
        if ((ent->client->buttons | ent->client->latched_buttons) & BUTTON_ATTACK) {
            ent->client->latched_buttons &= ~BUTTON_ATTACK;

            if (ent->client->pers.inventory[ent->client->ammo_index] >= ent->client->pers.weapon->quantity) {
                ent->client->ps.gunspin = min(MAX_ROTATION, ent->client->ps.gunspin + ROTATION_SPEED);

                if (ent->client->ps.gunspin >= MAX_ROTATION)
                {
                    if (ent->client->ps.gunframe < ANIM_ATTACK_FIRST) {
                        ent->client->ps.gunframe = ANIM_ATTACK_FIRST;
                    }

                    weapon_perforator_fire(ent);

                    ent->client->anim_priority = ANIM_ATTACK;
                    if (ent->client->ps.pmove.pm_flags & PMF_DUCKED) {
                        ent->s.frame = (FRAME_crattak1 - 1) + (level.framenum % 2);
                        ent->client->anim_end = FRAME_crattak9;
                    } else {
                        ent->s.frame = (FRAME_attack1 - 1) + (level.framenum % 2);
                        ent->client->anim_end = FRAME_attack8;
                    }

                    if (!((int)dmflags->value & DF_INFINITE_AMMO))
                        ent->client->pers.inventory[ent->client->ammo_index]--;
                }
            } else {
                if (level.framenum >= ent->pain_debounce_framenum) {
                    gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/noammo.wav"), 1, ATTN_NORM, 0);
                    ent->pain_debounce_framenum = level.framenum + 1 * BASE_FRAMERATE;
                }
                NoAmmoWeaponChange(ent);
                goto spindown;
            }

            if (ent->client->ps.gunframe == ANIM_IDLE_LAST) {
                ent->client->ps.gunframe = ANIM_IDLE_FIRST;
            } else if (ent->client->ps.gunframe == ANIM_ATTACK_LAST) {
                ent->client->ps.gunframe = ANIM_ATTACK_FIRST;
            }
        } else {
spindown:
            ent->client->ps.gunspin = max(0.f, ent->client->ps.gunspin - ROTATION_SPEED);

            if (ent->client->weaponstate == WEAPON_FIRING) {
                ent->client->ps.gunframe = ANIM_IDLE_FIRST;
                ent->client->weaponstate = WEAPON_READY;
            }
        }

        if (ent->client->ps.gunspin <= 0) {
            ent->client->weapon_sound = 0;
            ent->s.sound_pitch = 0;
        } else {
            ent->s.sound_pitch = -63 + ((ent->client->ps.gunspin / MAX_ROTATION) * 63 * 2);
        }
        break;
    case WEAPON_DROPPING:
        if (ent->client->ps.gunframe == ANIM_PUTAWAY_LAST)
            ChangeWeapon(ent);
        break;
    }
}


/*
======================================================================

SHOTGUN / SUPERSHOTGUN

======================================================================
*/

void weapon_shotgun_fire(edict_t *ent)
{
    vec3_t      start;
    vec3_t      forward, right;
    vec3_t      offset;
    int         damage = 4;
    int         kick = 8;

    AngleVectors(ent->client->v_angle, forward, right, NULL);

    VectorScale(forward, -2, ent->client->kick_origin);
    ent->client->kick_angles[0] = -2;

    VectorSet(offset, 0, 0, ent->viewheight);
    P_ProjectSource(ent->client, ent->s.origin, offset, forward, right, start);

    if (is_quad) {
        damage *= 4;
        kick *= 4;
    }

    if (deathmatch->value)
        fire_shotgun(ent, start, forward, damage, kick, 200, 200, DEFAULT_DEATHMATCH_SHOTGUN_COUNT, MOD_SHOTGUN);
    else
        fire_shotgun(ent, start, forward, damage, kick, 200, 200, DEFAULT_SHOTGUN_COUNT, MOD_SHOTGUN);

    // send muzzle flash
    gi.WriteByte(svc_muzzleflash);
    gi.WriteShort(ent - g_edicts);
    gi.WriteByte(MZ_SHOTGUN | is_silenced);
    gi.multicast(ent->s.origin, MULTICAST_PVS);
}

enum {
    EQUIP_FIRST,
    EQUIP_LAST,
    IDLE_FIRST,
    IDLE_LAST,
    ATTACK_FIRST,
    ATTACK_LAST,
    PUTAWAY_FIRST,
    PUTAWAY_LAST,
    INSPECT_FIRST,
    INSPECT_LAST,

    FRAMES_TOTAL
};

// shared generic entry point for basic weapons
inline void Weapon_Generic(edict_t *ent, const int frames[FRAMES_TOTAL], void (*fire) (edict_t *ent))
{
    ent->client->ps.gunframe++;

    switch (ent->client->weaponstate)
    {
    case WEAPON_ACTIVATING:
        if (ent->client->ps.gunframe == frames[EQUIP_LAST])
            ent->client->weaponstate = WEAPON_READY;
        break;
    case WEAPON_READY:
        if (ent->client->newweapon) {
            ent->client->weaponstate = WEAPON_DROPPING;
            ent->client->ps.gunframe = frames[PUTAWAY_FIRST];
            break;
        } else if (((ent->client->latched_buttons | ent->client->buttons) & BUTTON_ATTACK)) {
            ent->client->latched_buttons &= ~BUTTON_ATTACK;

            if (ent->client->pers.inventory[ent->client->ammo_index] >= ent->client->pers.weapon->quantity) {
                ent->client->anim_priority = ANIM_ATTACK;
                if (ent->client->ps.pmove.pm_flags & PMF_DUCKED) {
                    ent->s.frame = FRAME_crattak1 - 1;
                    ent->client->anim_end = FRAME_crattak9;
                } else {
                    ent->s.frame = FRAME_attack1 - 1;
                    ent->client->anim_end = FRAME_attack8;
                }

                ent->client->weaponstate = WEAPON_FIRING;
                ent->client->ps.gunframe = frames[ATTACK_FIRST];

                fire(ent);

                PlayerNoise(ent, ent->s.origin, PNOISE_WEAPON);

                if (!((int)dmflags->value & DF_INFINITE_AMMO))
                    ent->client->pers.inventory[ent->client->ammo_index] -= ent->client->pers.weapon->quantity;
            } else {
                if (level.framenum >= ent->pain_debounce_framenum) {
                    gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/noammo.wav"), 1, ATTN_NORM, 0);
                    ent->pain_debounce_framenum = level.framenum + 1 * BASE_FRAMERATE;
                }
                NoAmmoWeaponChange(ent);
            }
        } else if (ent->client->ps.gunframe == frames[IDLE_LAST]) {
            ent->client->ps.gunframe = frames[IDLE_FIRST];
        }
        break;
    case WEAPON_FIRING:
        if (ent->client->ps.gunframe == frames[ATTACK_LAST]) {
            ent->client->ps.gunframe = frames[IDLE_FIRST];
            ent->client->weaponstate = WEAPON_READY;
        }
        break;
    case WEAPON_DROPPING:
        if (ent->client->ps.gunframe == frames[PUTAWAY_LAST]) {
            ChangeWeapon(ent);
        }
        break;
    }
}

void Weapon_Shotgun(edict_t *ent)
{
    static const int frames[] = {
        [EQUIP_FIRST]    = 0,
        [EQUIP_LAST]     = 8,
        [IDLE_FIRST]     = 9,
        [IDLE_LAST]      = 68,
        [ATTACK_FIRST]   = 69,
        [ATTACK_LAST]    = 78,
        [PUTAWAY_FIRST]  = 79,
        [PUTAWAY_LAST]   = 81,
        [INSPECT_FIRST]  = -1,
        [INSPECT_LAST]   = -1
    };
    Weapon_Generic(ent, frames, weapon_shotgun_fire);
}

void weapon_supershotgun_fire(edict_t *ent)
{
    vec3_t      start;
    vec3_t      forward, right;
    vec3_t      offset;
    int         damage = 4;
    int         kick = 8;

    if (is_quad) {
        damage *= 4;
        kick *= 4;
    }

    AngleVectors(ent->client->v_angle, forward, right, NULL);

    VectorScale(forward, -4, ent->client->kick_origin);
    ent->client->kick_angles[0] = -4;
    
    VectorSet(offset, 0, 10, ent->viewheight);
    P_ProjectSource(ent->client, ent->s.origin, offset, forward, right, start);

    if (deathmatch->value)
        fire_shotgun(ent, start, forward, damage, kick, 350, 350, DEFAULT_DEATHMATCH_SHOTGUN_COUNT, MOD_SHOTGUN);
    else
        fire_shotgun(ent, start, forward, damage, kick, 350, 350, DEFAULT_SHOTGUN_COUNT, MOD_SHOTGUN);

    VectorSet(offset, 0, -10, ent->viewheight);
    P_ProjectSource(ent->client, ent->s.origin, offset, forward, right, start);

    if (deathmatch->value)
        fire_shotgun(ent, start, forward, damage, kick, 350, 350, DEFAULT_DEATHMATCH_SHOTGUN_COUNT, MOD_SHOTGUN);
    else
        fire_shotgun(ent, start, forward, damage, kick, 350, 350, DEFAULT_SHOTGUN_COUNT, MOD_SHOTGUN);

    // send muzzle flash
    gi.WriteByte(svc_muzzleflash);
    gi.WriteShort(ent - g_edicts);
    gi.WriteByte(MZ_SHOTGUN | is_silenced);
    gi.multicast(ent->s.origin, MULTICAST_PVS);
}

void Weapon_SuperShotgun(edict_t *ent)
{
    static const int frames[] = {
        [EQUIP_FIRST]    = 0,
        [EQUIP_LAST]     = 8,
        [IDLE_FIRST]     = 9,
        [IDLE_LAST]      = 68,
        [ATTACK_FIRST]   = 69,
        [ATTACK_LAST]    = 78,
        [PUTAWAY_FIRST]  = 79,
        [PUTAWAY_LAST]   = 81,
        [INSPECT_FIRST]  = -1,
        [INSPECT_LAST]   = -1
    };
    Weapon_Generic(ent, frames, weapon_supershotgun_fire);
}

void weapon_nailgun_fire(edict_t *ent)
{
    vec3_t      start;
    vec3_t      forward, right;
    vec3_t      offset;

    AngleVectors(ent->client->v_angle, forward, right, NULL);

    VectorScale(forward, -1, ent->client->kick_origin);
    ent->client->kick_angles[0] = -1;
    
    VectorSet(offset, 0, 0, ent->viewheight);
    P_ProjectSource(ent->client, ent->s.origin, offset, forward, right, start);

    fire_nail(ent, start, forward, 12, 2000);

    // send muzzle flash
    gi.WriteByte(svc_muzzleflash);
    gi.WriteShort(ent - g_edicts);
    gi.WriteByte(MZ_SHOTGUN | is_silenced);
    gi.multicast(ent->s.origin, MULTICAST_PVS);
}

void Weapon_Nailgun(edict_t *ent)
{
    static const int frames[] = {
        [EQUIP_FIRST]    = 0,
        [EQUIP_LAST]     = 8,
        [IDLE_FIRST]     = 9,
        [IDLE_LAST]      = 68,
        [ATTACK_FIRST]   = 69,
        [ATTACK_LAST]    = 78,
        [PUTAWAY_FIRST]  = 79,
        [PUTAWAY_LAST]   = 81,
        [INSPECT_FIRST]  = -1,
        [INSPECT_LAST]   = -1
    };
    Weapon_Generic(ent, frames, weapon_nailgun_fire);
}

void weapon_grenadelauncher_fire(edict_t *ent)
{
    vec3_t      start;
    vec3_t      forward, right;
    vec3_t      offset;

    AngleVectors(ent->client->v_angle, forward, right, NULL);

    VectorScale(forward, -1, ent->client->kick_origin);
    ent->client->kick_angles[0] = -1;
    
    VectorSet(offset, 0, 0, ent->viewheight);
    P_ProjectSource(ent->client, ent->s.origin, offset, forward, right, start);

    fire_grenade(ent, start, forward, 100, 800, 2.5f, 75);

    // send muzzle flash
    gi.WriteByte(svc_muzzleflash);
    gi.WriteShort(ent - g_edicts);
    gi.WriteByte(MZ_SHOTGUN | is_silenced);
    gi.multicast(ent->s.origin, MULTICAST_PVS);
}

void Weapon_GrenadeLauncher(edict_t *ent)
{
    static const int frames[] = {
        [EQUIP_FIRST]    = 0,
        [EQUIP_LAST]     = 8,
        [IDLE_FIRST]     = 9,
        [IDLE_LAST]      = 68,
        [ATTACK_FIRST]   = 69,
        [ATTACK_LAST]    = 78,
        [PUTAWAY_FIRST]  = 79,
        [PUTAWAY_LAST]   = 81,
        [INSPECT_FIRST]  = -1,
        [INSPECT_LAST]   = -1
    };
    Weapon_Generic(ent, frames, weapon_grenadelauncher_fire);
}

void weapon_rocketlauncher_fire(edict_t *ent)
{
    vec3_t      start;
    vec3_t      forward, right;
    vec3_t      offset;

    AngleVectors(ent->client->v_angle, forward, right, NULL);

    VectorScale(forward, -1, ent->client->kick_origin);
    ent->client->kick_angles[0] = -1;
    
    VectorSet(offset, 0, 0, ent->viewheight);
    P_ProjectSource(ent->client, ent->s.origin, offset, forward, right, start);

    fire_rocket(ent, start, forward, 100, 800, 120, 75);

    // send muzzle flash
    gi.WriteByte(svc_muzzleflash);
    gi.WriteShort(ent - g_edicts);
    gi.WriteByte(MZ_SHOTGUN | is_silenced);
    gi.multicast(ent->s.origin, MULTICAST_PVS);
}

void Weapon_RocketLauncher(edict_t *ent)
{
    static const int frames[] = {
        [EQUIP_FIRST]    = 0,
        [EQUIP_LAST]     = 8,
        [IDLE_FIRST]     = 9,
        [IDLE_LAST]      = 68,
        [ATTACK_FIRST]   = 69,
        [ATTACK_LAST]    = 78,
        [PUTAWAY_FIRST]  = 79,
        [PUTAWAY_LAST]   = 81,
        [INSPECT_FIRST]  = -1,
        [INSPECT_LAST]   = -1
    };
    Weapon_Generic(ent, frames, weapon_rocketlauncher_fire);
}

void weapon_thunderbolt_fire(edict_t *ent)
{
    vec3_t      start;
    vec3_t      forward, right;
    vec3_t      offset;

    AngleVectors(ent->client->v_angle, forward, right, NULL);

    VectorScale(forward, -1, ent->client->kick_origin);
    ent->client->kick_angles[0] = -1;
    
    VectorSet(offset, 0, 0, ent->viewheight);
    P_ProjectSource(ent->client, ent->s.origin, offset, forward, right, start);

    if (gi.pointcontents(start) & MASK_WATER) {
        T_RadiusDamage(ent, ent, 9999, NULL, 9999, MOD_RAILGUN);
        return;
    }

    vec3_t end;
    VectorMA(start, 8192, forward, end);

    trace_t tr = gi.trace(start, vec3_origin, vec3_origin, end, ent, MASK_SHOT);

    if (tr.ent->takedamage)
        T_Damage(tr.ent, ent, ent, forward, tr.endpos, tr.plane.normal, 7, 15, DAMAGE_ENERGY, MOD_RAILGUN);

    gi.WriteByte(svc_temp_entity);
    gi.WriteByte(TE_LIGHTNING);
    gi.WriteShort(ent->s.number);
    gi.WriteShort(-1);
    gi.WritePosition(start);
    gi.WritePosition(tr.endpos);
    gi.multicast(tr.endpos, MULTICAST_PVS);

    // send muzzle flash
    gi.WriteByte(svc_muzzleflash);
    gi.WriteShort(ent - g_edicts);
    gi.WriteByte(MZ_SHOTGUN | is_silenced);
    gi.multicast(ent->s.origin, MULTICAST_PVS);
}

void Weapon_Thunderbolt(edict_t *ent)
{
    enum {
        ANIM_EQUIP_FIRST      = 0,
        ANIM_EQUIP_LAST       = 8,
        ANIM_IDLE_FIRST       = 9,
        ANIM_IDLE_LAST        = 68,
        ANIM_ATTACK_FIRST     = 69,
        ANIM_ATTACK_LAST      = 78,
        ANIM_PUTAWAY_FIRST    = 79,
        ANIM_PUTAWAY_LAST     = 81,
        ANIM_INSPECT_FIRST    = 9,
        ANIM_INSPECT_LAST     = 68
    };
    
    ent->client->ps.gunframe++;

    switch (ent->client->weaponstate)
    {
    case WEAPON_ACTIVATING:
        if (ent->client->ps.gunframe == ANIM_EQUIP_LAST)
            ent->client->weaponstate = WEAPON_READY;
        break;
    case WEAPON_READY:
        if (((ent->client->latched_buttons | ent->client->buttons) & BUTTON_ATTACK)) {
            ent->client->latched_buttons &= ~BUTTON_ATTACK;

            if (ent->client->ps.gunframe >= ANIM_INSPECT_FIRST) {
                ent->client->ps.gunframe = ANIM_IDLE_FIRST;
            }

            if (ent->client->pers.inventory[ent->client->ammo_index] >= ent->client->pers.weapon->quantity) {
                ent->client->weaponstate = WEAPON_FIRING;
            } else {
                if (level.framenum >= ent->pain_debounce_framenum) {
                    gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/noammo.wav"), 1, ATTN_NORM, 0);
                    ent->pain_debounce_framenum = level.framenum + 1 * BASE_FRAMERATE;
                }
                NoAmmoWeaponChange(ent);
                goto ready_noammo;
            }

            // intentional fall-through
        } else {
ready_noammo:
            if (ent->client->inspect) {
                ent->client->inspect = false;
                ent->client->ps.gunframe = ANIM_INSPECT_FIRST;
            } else if (ent->client->ps.gunframe == ANIM_IDLE_LAST) {
                if (random() < 0.1) {
                    ent->client->ps.gunframe = ANIM_INSPECT_FIRST;
                } else {
                    ent->client->ps.gunframe = ANIM_IDLE_FIRST;
                }
            } else if (ent->client->ps.gunframe == ANIM_INSPECT_LAST) {
                ent->client->ps.gunframe = ANIM_IDLE_FIRST;
            }

            if (ent->client->newweapon) {
                ent->client->weaponstate = WEAPON_DROPPING;
                ent->client->ps.gunframe = ANIM_PUTAWAY_FIRST;
                break;
            }
        }
    case WEAPON_FIRING:
        if ((ent->client->buttons | ent->client->latched_buttons) & BUTTON_ATTACK) {
            ent->client->latched_buttons &= ~BUTTON_ATTACK;

            if (ent->client->pers.inventory[ent->client->ammo_index] >= ent->client->pers.weapon->quantity) {

                ent->client->ps.gunframe = ANIM_ATTACK_FIRST;

                weapon_thunderbolt_fire(ent);

                if (ent->deadflag) {
                    return;
                }

                ent->client->anim_priority = ANIM_ATTACK;
                if (ent->client->ps.pmove.pm_flags & PMF_DUCKED) {
                    ent->s.frame = (FRAME_crattak1 - 1) + (level.framenum % 2);
                    ent->client->anim_end = FRAME_crattak9;
                } else {
                    ent->s.frame = (FRAME_attack1 - 1) + (level.framenum % 2);
                    ent->client->anim_end = FRAME_attack8;
                }

                if (!((int)dmflags->value & DF_INFINITE_AMMO))
                    ent->client->pers.inventory[ent->client->ammo_index]--;
            } else {
                if (level.framenum >= ent->pain_debounce_framenum) {
                    gi.sound(ent, CHAN_VOICE, gi.soundindex("weapons/noammo.wav"), 1, ATTN_NORM, 0);
                    ent->pain_debounce_framenum = level.framenum + 1 * BASE_FRAMERATE;
                }
                NoAmmoWeaponChange(ent);
            }

            if (ent->client->ps.gunframe == ANIM_IDLE_LAST) {
                ent->client->ps.gunframe = ANIM_IDLE_FIRST;
            } else if (ent->client->ps.gunframe == ANIM_ATTACK_LAST) {
                ent->client->ps.gunframe = ANIM_ATTACK_FIRST;
            }
        } else {
            if (ent->client->weaponstate == WEAPON_FIRING) {
                ent->client->ps.gunframe = ANIM_IDLE_FIRST;
                ent->client->weaponstate = WEAPON_READY;
            }
        }
        break;
    case WEAPON_DROPPING:
        if (ent->client->ps.gunframe == ANIM_PUTAWAY_LAST)
            ChangeWeapon(ent);
        break;
    }
}
