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
// g_combat.c

#include "g_local.h"

/*
============
CanDamage

Returns true if the inflictor can directly damage the target.  Used for
explosions and melee attacks.
============
*/
bool CanDamage(edict_t *targ, edict_t *inflictor)
{
    vec3_t  dest;
    trace_t trace;

// bmodels need special checking because their origin is 0,0,0
    if (targ->movetype == MOVETYPE_PUSH) {
        VectorAdd(targ->absmin, targ->absmax, dest);
        VectorScale(dest, 0.5f, dest);
        trace = SV_Trace(inflictor->s.origin, vec3_origin, vec3_origin, dest, inflictor, MASK_SOLID);
        if (trace.fraction == 1.0f)
            return true;
        if (trace.ent == targ)
            return true;
        return false;
    }

    trace = SV_Trace(inflictor->s.origin, vec3_origin, vec3_origin, targ->s.origin, inflictor, MASK_SOLID);
    if (trace.fraction == 1.0f)
        return true;

    VectorCopy(targ->s.origin, dest);
    dest[0] += 15.0f;
    dest[1] += 15.0f;
    trace = SV_Trace(inflictor->s.origin, vec3_origin, vec3_origin, dest, inflictor, MASK_SOLID);
    if (trace.fraction == 1.0f)
        return true;

    VectorCopy(targ->s.origin, dest);
    dest[0] += 15.0f;
    dest[1] -= 15.0f;
    trace = SV_Trace(inflictor->s.origin, vec3_origin, vec3_origin, dest, inflictor, MASK_SOLID);
    if (trace.fraction == 1.0f)
        return true;

    VectorCopy(targ->s.origin, dest);
    dest[0] -= 15.0f;
    dest[1] += 15.0f;
    trace = SV_Trace(inflictor->s.origin, vec3_origin, vec3_origin, dest, inflictor, MASK_SOLID);
    if (trace.fraction == 1.0f)
        return true;

    VectorCopy(targ->s.origin, dest);
    dest[0] -= 15.0f;
    dest[1] -= 15.0f;
    trace = SV_Trace(inflictor->s.origin, vec3_origin, vec3_origin, dest, inflictor, MASK_SOLID);
    if (trace.fraction == 1.0f)
        return true;


    return false;
}


/*
============
Killed
============
*/
void Killed(edict_t *targ, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
    if (targ->health < -999)
        targ->health = -999;

    targ->enemy = attacker;

    if ((targ->svflags & SVF_MONSTER) && (targ->deadflag != DEAD_DEAD)) {
//      targ->svflags |= SVF_DEADMONSTER;   // now treat as a different content type
        if (!(targ->monsterinfo.aiflags & AI_GOOD_GUY)) {
            level.killed_monsters++;
            if (coop.integer && attacker->client)
                attacker->client->resp.score++;
            // medics won't heal monsters that they kill themselves
            if (strcmp(attacker->classname, "monster_medic") == 0)
                targ->owner = attacker;
        }
    }

    if (targ->movetype == MOVETYPE_PUSH || targ->movetype == MOVETYPE_STOP || targ->movetype == MOVETYPE_NONE) {
        // doors, triggers, etc
        targ->die(targ, inflictor, attacker, damage, point);
        return;
    }

    if ((targ->svflags & SVF_MONSTER) && (targ->deadflag != DEAD_DEAD)) {
        targ->touch = NULL;
        monster_death_use(targ);
    }

    targ->die(targ, inflictor, attacker, damage, point);
}


/*
================
SpawnDamage
================
*/
void SpawnDamage(int type, const vec3_t origin, const vec3_t normal, int damage)
{
    if (damage > 255)
        damage = 255;
    SV_WriteByte(svc_temp_entity);
    SV_WriteByte(type);
//  SV_WriteByte (damage);
    SV_WritePos(origin);
    SV_WriteDir(normal);
    SV_Multicast(origin, MULTICAST_PVS, false);
}


/*
============
T_Damage

targ        entity that is being damaged
inflictor   entity that is causing the damage
attacker    entity that caused the inflictor to damage targ
    example: targ=monster, inflictor=rocket, attacker=player

dir         direction of the attack
point       point at which the damage is being inflicted
normal      normal vector from that point
damage      amount of damage being inflicted
knockback   force to be applied against targ as a result of the damage

dflags      these flags are used to control how T_Damage works
    DAMAGE_RADIUS           damage was indirect (from a nearby explosion)
    DAMAGE_NO_ARMOR         armor does not protect from this damage
    DAMAGE_ENERGY           damage is from an energy based weapon
    DAMAGE_NO_KNOCKBACK     do not affect velocity, just view angles
    DAMAGE_BULLET           damage is from a bullet (used for ricochets)
    DAMAGE_NO_PROTECTION    kills godmode, armor, everything
============
*/
static int CheckArmor(edict_t *ent, const vec3_t point, const vec3_t normal, int damage, int te_sparks, int dflags)
{
    gclient_t   *client;
    int         save;
    int         index;
    gitem_t     *armor;

    if (!damage)
        return 0;

    client = ent->client;

    if (!client)
        return 0;

    if (dflags & DAMAGE_NO_ARMOR)
        return 0;

    index = ArmorIndex(ent);
    if (!index)
        return 0;

    armor = GetItemByIndex(index);

    if (dflags & DAMAGE_ENERGY)
        save = ceil(armor->armor->energy_protection * damage);
    else
        save = ceil(armor->armor->normal_protection * damage);

    if (save >= client->pers.inventory[index])
        save = client->pers.inventory[index];

    if (!save)
        return 0;

    client->pers.inventory[index] -= save;
    SpawnDamage(te_sparks, point, normal, save);

    return save;
}

void M_ReactToDamage(edict_t *targ, edict_t *attacker)
{
    if (!(attacker->client) && !(attacker->svflags & SVF_MONSTER))
        return;

    if (attacker == targ || attacker == targ->enemy)
        return;

    // dead monsters, like misc_deadsoldier, don't have AI functions, but 
    // M_ReactToDamage might still be called on them
    if (targ->svflags & SVF_DEADMONSTER)
        return;
    // don't react if we're going to a combat point
    else if (targ->monsterinfo.aiflags & AI_COMBAT_POINT)
        return;

    // if we are a good guy monster and our attacker is a player
    // or another good guy, do not get mad at them
    if (targ->monsterinfo.aiflags & AI_GOOD_GUY) {
        if (attacker->client || (attacker->monsterinfo.aiflags & AI_GOOD_GUY))
            return;
    }

    // we now know that we are not both good guys

    // if attacker is a client, get mad at them because he's good and we're not
    if (attacker->client) {
        targ->monsterinfo.aiflags &= ~AI_SOUND_TARGET;

        // this can only happen in coop (both new and old enemies are clients)
        // only switch if can't see the current enemy
        if (targ->enemy && targ->enemy->client) {
            if (visible(targ, targ->enemy)) {
                targ->oldenemy = attacker;
                return;
            }
            targ->oldenemy = targ->enemy;
        }
        targ->enemy = attacker;
        if (!(targ->monsterinfo.aiflags & AI_DUCKED))
            FoundTarget(targ);
        return;
    }

    // it's the same base (walk/swim/fly) type and a different classname and it's not a tank
    // (they spray too much), get mad at them
    if (((targ->flags & (FL_FLY | FL_SWIM)) == (attacker->flags & (FL_FLY | FL_SWIM))) &&
        (strcmp(targ->classname, attacker->classname) != 0) &&
        (strcmp(attacker->classname, "monster_tank") != 0) &&
        (strcmp(attacker->classname, "monster_supertank") != 0) &&
        (strcmp(attacker->classname, "monster_makron") != 0) &&
        (strcmp(attacker->classname, "monster_jorg") != 0)) {
        if (targ->enemy && targ->enemy->client)
            targ->oldenemy = targ->enemy;
        targ->enemy = attacker;
        if (!(targ->monsterinfo.aiflags & AI_DUCKED))
            FoundTarget(targ);
    }
    // if they *meant* to shoot us, then shoot back
    else if (attacker->enemy == targ) {
        if (targ->enemy && targ->enemy->client)
            targ->oldenemy = targ->enemy;
        targ->enemy = attacker;
        if (!(targ->monsterinfo.aiflags & AI_DUCKED))
            FoundTarget(targ);
    }
    // otherwise get mad at whoever they are mad at (help our buddy) unless it is us!
    else if (attacker->enemy && attacker->enemy != targ) {
        if (targ->enemy && targ->enemy->client)
            targ->oldenemy = targ->enemy;
        targ->enemy = attacker->enemy;
        if (!(targ->monsterinfo.aiflags & AI_DUCKED))
            FoundTarget(targ);
    }
}

bool CheckTeamDamage(edict_t *targ, edict_t *attacker)
{
    //FIXME make the next line real and uncomment this block
    // if ((ability to damage a teammate == OFF) && (targ's team == attacker's team))
    return false;
}

void T_Damage(edict_t *targ, edict_t *inflictor, edict_t *attacker, const vec3_t dir, vec3_t point, const vec3_t normal, int damage, int knockback, int dflags, int mod)
{
    gclient_t   *client;
    int         take;
    int         save;
    int         asave;
    int         te_sparks;

    if (!targ->takedamage)
        return;

    // easy mode takes half damage
    if (skill.integer == 0 && deathmatch.integer == 0 && targ->client) {
        damage *= 0.5f;
        if (!damage)
            damage = 1;
    }

    // friendly fire avoidance
    // if enabled you can't hurt teammates (but you can hurt yourself)
    // knockback still occurs
    if ((targ != attacker) && ((deathmatch.integer && (dmflags.integer & (DF_MODELTEAMS | DF_SKINTEAMS))) || (coop.integer && targ->client))) {
        if (OnSameTeam(targ, attacker)) {
            if (dmflags.integer & DF_NO_FRIENDLY_FIRE)
                damage = 0;
            else
                mod |= MOD_FRIENDLY_FIRE;
        }
    }
    meansOfDeath = mod;

    client = targ->client;

    if (dflags & DAMAGE_BULLET)
        te_sparks = TE_BULLET_SPARKS;
    else
        te_sparks = TE_SPARKS;

// bonus damage for suprising a monster
    if (!(dflags & DAMAGE_RADIUS) && (targ->svflags & SVF_MONSTER) && (attacker->client) && (!targ->enemy) && (targ->health > 0))
        damage *= 2;

    if (targ->flags & FL_NO_KNOCKBACK)
        knockback = 0;

// figure momentum add
    if (!(dflags & DAMAGE_NO_KNOCKBACK)) {
        if ((knockback) && (targ->movetype != MOVETYPE_NONE) && (targ->movetype != MOVETYPE_BOUNCE) && (targ->movetype != MOVETYPE_PUSH) && (targ->movetype != MOVETYPE_STOP)) {
            vec3_t  kvel;
            float   mass;

            if (targ->mass < 50)
                mass = 50;
            else
                mass = targ->mass;

            VectorNormalize2(dir, kvel);

            if (targ->client  && attacker == targ)
                VectorScale(kvel, 1600.0f * (float)knockback / mass, kvel);  // the rocket jump hack...
            else
                VectorScale(kvel, 500.0f * (float)knockback / mass, kvel);

            VectorAdd(targ->velocity, kvel, targ->velocity);
        }
    }

    take = damage;
    save = 0;

    // check for godmode
    if ((targ->flags & FL_GODMODE) && !(dflags & DAMAGE_NO_PROTECTION)) {
        take = 0;
        save = damage;
        SpawnDamage(te_sparks, point, normal, save);
    }

    // check for invincibility
    if ((client && client->invincible_time > level.time) && !(dflags & DAMAGE_NO_PROTECTION)) {
        if (targ->pain_debounce_time < level.time) {
            SV_StartSound(targ, CHAN_ITEM, SV_SoundIndex(ASSET_SOUND_PENT_HIT), 1, ATTN_NORM, 0);
            targ->pain_debounce_time = level.time + 2000;
        }
        take = 0;
        save = damage;
    }

    asave = CheckArmor(targ, point, normal, take, te_sparks, dflags);
    take -= asave;

    //treat cheat/powerup savings the same as armor
    asave += save;

    // team damage avoidance
    if (!(dflags & DAMAGE_NO_PROTECTION) && CheckTeamDamage(targ, attacker))
        return;

// do the damage
    if (take) {
        if ((targ->svflags & SVF_MONSTER) || (client))
        {
            // SpawnDamage(TE_BLOOD, point, normal, take);
            SpawnDamage(TE_BLOOD, point, dir, take);
        }
        else
            SpawnDamage(te_sparks, point, normal, take);


        targ->health = targ->health - take;

        if (targ->health <= 0) {
            if ((targ->svflags & SVF_MONSTER) || (client))
                targ->flags |= FL_NO_KNOCKBACK;
            Killed(targ, inflictor, attacker, take, point);
            return;
        }
    }

    if (targ->svflags & SVF_MONSTER) {
        M_ReactToDamage(targ, attacker);
        if (!(targ->monsterinfo.aiflags & AI_DUCKED) && (take)) {
            targ->pain(targ, attacker, knockback, take);
            // nightmare mode monsters don't go into pain frames often
            if (skill.integer == 3)
                targ->pain_debounce_time = level.time + 5000;
        }
    } else if (client) {
        if (!(targ->flags & FL_GODMODE) && (take))
            targ->pain(targ, attacker, knockback, take);
    } else if (take) {
        if (targ->pain)
            targ->pain(targ, attacker, knockback, take);
    }

    // add to the damage inflicted on a player this frame
    // the total will be turned into screen blends and view angle kicks
    // at the end of the frame
    if (client) {
        client->damage_armor += asave;
        client->damage_blood += take;
        client->damage_knockback += knockback;
        VectorCopy(point, client->damage_from);
    }
}


/*
============
T_RadiusDamage
============
*/
void T_RadiusDamage(edict_t *inflictor, edict_t *attacker, float damage, edict_t *ignore, float radius, int mod)
{
    float   points;
    edict_t *ent = NULL;
    vec3_t  v;
    vec3_t  dir;

    while ((ent = findradius(ent, inflictor->s.origin, radius)) != NULL) {
        if (ent == ignore)
            continue;
        if (!ent->takedamage)
            continue;

        VectorClosestPointToBox(inflictor->s.origin, ent->absmin, ent->absmax, v);
        VectorSubtract(inflictor->s.origin, v, v);
        points = damage - 0.5f * VectorLength(v);
        if (ent == attacker)
            points = points * 0.5f;
        if (points > 0) {
            if (CanDamage(ent, inflictor)) {
                VectorSubtract(ent->s.origin, inflictor->s.origin, dir);
                T_Damage(ent, inflictor, attacker, dir, inflictor->s.origin, vec3_origin, (int)points, (int)points, DAMAGE_RADIUS, mod);
            }
        }
    }
}
