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
/*
==============================================================================

Knight

==============================================================================
*/

#include "../g_local.h"

static int sound_pain;
static int sound_die;
static int sound_idle;
static int sound_fire;
static int sound_sight;
static int sound_splat;

enum {
    ANIMATION(STAND, 32),
    ANIMATION(RUN, 28),
    ANIMATION(ATTACK, 26),
    ANIMATION(PAIN, 8),
    ANIMATION(DEATH, 15)
};

void scrag_sight(edict_t* self, edict_t* other)
{
    SV_StartSound(self, CHAN_VOICE, sound_sight, 1, ATTN_NORM, 0);
}

void scrag_search(edict_t* self)
{
    if (frand() < 0.2)
        SV_StartSound(self, CHAN_VOICE, sound_idle, 1, ATTN_NORM, 0);
}

mmove_t scrag_move_stand = {
    .firstframe = FRAME_STAND_FIRST,
    .lastframe = FRAME_STAND_LAST,
    .frame = (mframe_t[FRAME_STAND_COUNT]) { 0 },
    .default_aifunc = ai_stand
};

void scrag_stand(edict_t* self)
{
    self->monsterinfo.currentmove = &scrag_move_stand;
}

mmove_t scrag_move_walk = {
    .firstframe = FRAME_RUN_FIRST,
    .lastframe = FRAME_RUN_LAST,
    .frame = (mframe_t[FRAME_RUN_COUNT]) { 0 },
    .default_aifunc = ai_walk
};

void scrag_walk(edict_t* self)
{
    self->monsterinfo.currentmove = &scrag_move_walk;
}

mmove_t scrag_move_run = {
    .firstframe = FRAME_RUN_FIRST,
    .lastframe = FRAME_RUN_LAST,
    .frame = (mframe_t[FRAME_RUN_COUNT]) {
        [0] = {.thinkfunc = scrag_search },
        [8] = {.thinkfunc = scrag_search },
        [16] = {.thinkfunc = scrag_search }
    },
    .default_aifunc = ai_run
};

void scrag_run(edict_t* self)
{
    if (self->monsterinfo.aiflags & AI_STAND_GROUND)
        self->monsterinfo.currentmove = &scrag_move_stand;
    else
        self->monsterinfo.currentmove = &scrag_move_run;
}


/*
=================
scrag_bolt_touch

Fires a single blaster bolt.  Used by the blaster and hyper blaster.
=================
*/
void scrag_bolt_touch(edict_t* self, edict_t* other, cplane_t* plane, csurface_t* surf)
{
    if (other == self->owner)
        return;

    if (surf && (surf->flags & SURF_SKY)) {
        G_FreeEdict(self);
        return;
    }

    if (self->owner->client)
        PlayerNoise(self->owner, self->s.origin, PNOISE_IMPACT);

    if (other->takedamage) {
        T_Damage(other, self, self->owner, self->velocity, self->s.origin, plane->normal, self->dmg, 1, DAMAGE_ENERGY, MOD_UNKNOWN);
    }

    self->spawnflags |= 1;
    self->s.frame = 11;
    self->solid = SOLID_NOT;
    self->nextthink = level.time + 50;
    VectorClear(self->velocity);
    self->movetype = MOVETYPE_NONE;
    vec3_t normal;
    if (plane)
        VectorCopy(plane->normal, normal);
    else
        VectorSet(normal, 0, 0, 1);
    VectorInverse(normal);
    vectoangles(normal, self->s.angles);
    VectorMA(self->s.origin, 12.f, plane->normal, self->s.origin);
    SV_LinkEntity(self);
}

void scrag_bolt_animate(edict_t* self)
{
    self->nextthink = level.time + 50;

    if (self->spawnflags & 1)
    {
        self->s.frame++;

        if (self->s.frame == 16)
            G_FreeEdict(self);

        return;
    }

    self->s.frame++;

    if (self->s.frame > 10)
        self->s.frame = 4;
}

void fire_scrag(edict_t* self, vec3_t start, vec3_t dir, int damage, int speed)
{
    edict_t* bolt;
    trace_t tr;

    VectorNormalize(dir);

    bolt = G_Spawn();
    bolt->svflags = SVF_DEADMONSTER;
    // yes, I know it looks weird that projectiles are deadmonsters
    // what this means is that when prediction is used against the object
    // (blaster/hyperblaster shots), the player won't be solid clipped against
    // the object.  Right now trying to run into a firing hyperblaster
    // is very jerky since you are predicted 'against' the shots.
    VectorCopy(start, bolt->s.origin);
    VectorCopy(start, bolt->s.old_origin);
    vectoangles(dir, bolt->s.angles);
    VectorScale(dir, speed, bolt->velocity);
    bolt->movetype = MOVETYPE_FLYMISSILE;
    bolt->clipmask = MASK_SHOT;
    bolt->solid = SOLID_BBOX;
    bolt->s.effects = EF_GREENGIB;
    VectorClear(bolt->mins);
    VectorClear(bolt->maxs);
    bolt->s.modelindex = SV_ModelIndex(ASSET_MODEL_SCRAG_BALL);
    bolt->owner = self;
    bolt->touch = scrag_bolt_touch;
    bolt->nextthink = level.time + 50;
    bolt->think = scrag_bolt_animate;
    bolt->dmg = damage;
    bolt->classname = "scragbolt";
    SV_LinkEntity(bolt);

    tr = SV_Trace(self->s.origin, NULL, NULL, bolt->s.origin, bolt, MASK_SHOT);
    if (tr.fraction < 1.0f) {
        VectorMA(bolt->s.origin, -10, dir, bolt->s.origin);
        bolt->touch(bolt, tr.ent, &tr.plane, tr.surface);
    }
}

void scrag_attack_spike(edict_t* self)
{
    vec3_t  start;
    vec3_t  forward, right;
    vec3_t  end;
    vec3_t  dir;

    AngleVectors(self->s.angles, forward, right, NULL);
    G_ProjectSource(self->s.origin, (vec3_t) { 0.f, 0.f, (self->s.frame > FRAME_ATTACK_FIRST + 9) ? 29.f : 24.f }, forward, right, start);

    VectorCopy(self->enemy->s.origin, end);
    end[2] += self->enemy->viewheight;
    VectorSubtract(end, start, dir);

    fire_scrag(self, start, dir, 9, 650);
}

void scrag_swing(edict_t* self)
{
    SV_StartSound(self, CHAN_WEAPON, sound_fire, 1, ATTN_NORM, 0);
}

mmove_t scrag_move_attack = {
    .firstframe = FRAME_ATTACK_FIRST,
    .lastframe = FRAME_ATTACK_LAST,
    .frame = (mframe_t[FRAME_ATTACK_COUNT]) {
        [0] = {.thinkfunc = scrag_swing },
        [7] = {.thinkfunc = scrag_attack_spike },
        [14] = {.thinkfunc = scrag_attack_spike }
    },
    .endfunc = scrag_run,
    .default_aifunc = ai_charge
};

void scrag_attack(edict_t* self)
{
    if (!self->enemy || !self->enemy->inuse)
        return;

    if (range(self, self->enemy) <= RANGE_MID)
        self->monsterinfo.currentmove = &scrag_move_attack;
}

mmove_t scrag_move_pain = {
    .firstframe = FRAME_PAIN_FIRST,
    .lastframe = FRAME_PAIN_LAST,
    .frame = (mframe_t[FRAME_PAIN_COUNT]) { 0 },
    .endfunc = scrag_run,
    .default_aifunc = ai_move
};

void scrag_pain(edict_t* self, edict_t* other, float kick, int damage)
{
    if (self->health < (self->max_health / 2))
        self->s.skinnum = 1;

    if (level.time < self->pain_debounce_time)
        return;

    self->pain_debounce_time = level.time + 3000;
    SV_StartSound(self, CHAN_VOICE, sound_pain, 1, ATTN_NORM, 0);

    if (skill.integer == 3)
        return;     // no pain anims in nightmare

    self->monsterinfo.currentmove = &scrag_move_pain;
}

void scrag_dead(edict_t* self)
{
    VectorSet(self->mins, -16, -16, -32);
    VectorSet(self->maxs, 16, 16, 0);
    self->movetype = MOVETYPE_TOSS;
    self->nextthink = 0;
    SV_LinkEntity(self);
}

mmove_t scrag_move_death = {
    .firstframe = FRAME_DEATH_FIRST,
    .lastframe = FRAME_DEATH_LAST,
    .frame = (mframe_t[FRAME_DEATH_COUNT]) { 0 },
    .endfunc = scrag_dead,
    .default_aifunc = ai_move
};

static gib_def_t scrag_gibs[] = {
    { ASSET_MODEL_SCRAG_GIB_ARML },
    { ASSET_MODEL_SCRAG_GIB_ARMR },
    { ASSET_MODEL_SCRAG_GIB_CHEST },
    { ASSET_MODEL_SCRAG_GIB_HEAD, .head = true },
    { ASSET_MODEL_SCRAG_GIB_NUB },
    { ASSET_MODEL_SCRAG_GIB_SHOULDER },
    { ASSET_MODEL_SCRAG_GIB_TAIL },
    { ASSET_MODEL_SCRAG_GIB_TAIL_BASE }
};

void scrag_die(edict_t* self, edict_t* inflictor, edict_t* attacker, int damage, vec3_t point)
{
    if (self->health <= self->gib_health) {
        SV_StartSound(self, CHAN_VOICE, SV_SoundIndex(ASSET_SOUND_GIB), 1, ATTN_NORM, 0);
        SpawnGibs(self, damage, scrag_gibs, q_countof(scrag_gibs));
        self->deadflag = DEAD_DEAD;
        return;
    }

    if (self->deadflag == DEAD_DEAD)
        return;

    self->movetype = MOVETYPE_TOSS;

    SV_StartSound(self, CHAN_VOICE, sound_die, 1, ATTN_NORM, 0);
    self->deadflag = DEAD_DEAD;
    self->takedamage = DAMAGE_YES;
    self->svflags |= SVF_DEADMONSTER;
    SV_LinkEntity(self);

    self->monsterinfo.currentmove = &scrag_move_death;
}

static bool scrag_inititalized = false;

void scrag_load(edict_t* self)
{
    if (scrag_inititalized)
        return;

    for (int32_t i = scrag_move_walk.firstframe; i < scrag_move_walk.lastframe; i++)
        scrag_move_walk.frame[i - scrag_move_walk.firstframe].dist = 3;
    for (int32_t i = scrag_move_run.firstframe; i < scrag_move_run.lastframe; i++)
        scrag_move_run.frame[i - scrag_move_run.firstframe].dist = 8;

    scrag_inititalized = true;
}

/*QUAKED monster_scrag (1 .5 0) (-16 -16 -24) (16 16 32) Ambush Trigger_Spawn Sight
*/
void SP_monster_scrag(edict_t* self)
{
    if (deathmatch.integer) {
        G_FreeEdict(self);
        return;
    }

    // pre-caches
    sound_pain = SV_SoundIndex(ASSET_SOUND_SCRAG_HURT);
    sound_die = SV_SoundIndex(ASSET_SOUND_SCRAG_DEATH);
    sound_idle = SV_SoundIndex(ASSET_SOUND_SCRAG_IDLE);
    sound_fire = SV_SoundIndex(ASSET_SOUND_SCRAG_FIRE);
    sound_sight = SV_SoundIndex(ASSET_SOUND_SCRAG_SIGHT);
    sound_splat = SV_SoundIndex(ASSET_SOUND_SCRAG_SPLAT);

    self->s.modelindex = SV_ModelIndex(ASSET_MODEL_SCRAG);
    VectorSet(self->mins, -16, -16, -38);
    VectorSet(self->maxs, 16, 16, 36);
    self->movetype = MOVETYPE_STEP;
    self->solid = SOLID_BBOX;
    self->viewheight = 28;

    self->health = 80;
    self->gib_health = -40;
    self->mass = 70;
    self->monsterinfo.aiflags |= AI_HIGH_TICK_RATE;
    self->yaw_speed = 20;

    self->pain = scrag_pain;
    self->die = scrag_die;

    self->monsterinfo.stand = scrag_stand;
    self->monsterinfo.walk = scrag_walk;
    self->monsterinfo.run = scrag_run;
    self->monsterinfo.attack = scrag_attack;
    self->monsterinfo.sight = scrag_sight;
    self->monsterinfo.search = scrag_search;
    self->monsterinfo.idle = scrag_search;

    self->monsterinfo.currentmove = &scrag_move_stand;
    self->monsterinfo.load = scrag_load;

    SV_ModelIndex(ASSET_MODEL_SCRAG_BALL);

    PrecacheGibs(scrag_gibs, q_countof(scrag_gibs));

    SV_LinkEntity(self);

    flymonster_start(self);
}
