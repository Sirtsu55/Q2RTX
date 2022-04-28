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

Vore

==============================================================================
*/

#include "../g_local.h"

static int sound_pain;
static int sound_die;
static int sound_idle;
static int sound_attack;
static int sound_attack2;
static int sound_sight;

enum {
    ANIMATION(ATTACK, 22),
    ANIMATION(PAIN, 10),
    ANIMATION(DEATH, 14),
    ANIMATION(WALK, 25),
    ANIMATION(STAND, 44)
};

void vore_sight(edict_t *self, edict_t *other)
{
    SV_StartSound(self, CHAN_VOICE, sound_sight, 1, ATTN_NORM, 0);
}

void vore_search(edict_t *self)
{
    if (frand() < 0.2)
        SV_StartSound(self, CHAN_VOICE, sound_idle, 1, ATTN_NORM, 0);
}

mmove_t vore_move_stand = {
    .firstframe = FRAME_STAND_FIRST,
    .lastframe = FRAME_STAND_LAST,
    .frame = (mframe_t [FRAME_STAND_COUNT]) { 0 },
    .default_aifunc = ai_stand
};

void vore_stand(edict_t *self)
{
    self->monsterinfo.currentmove = &vore_move_stand;
}

mmove_t vore_move_walk = {
    .firstframe = FRAME_WALK_FIRST,
    .lastframe = FRAME_WALK_LAST,
    .frame = (mframe_t [FRAME_WALK_COUNT]) { 0 },
    .default_aifunc = ai_walk
};

void vore_walk(edict_t *self)
{
    self->monsterinfo.currentmove = &vore_move_walk;
}

mmove_t vore_move_run = {
    .firstframe = FRAME_WALK_FIRST,
    .lastframe = FRAME_WALK_LAST,
    .frame = (mframe_t [FRAME_WALK_COUNT]) {
        [0] = { .thinkfunc = vore_search },
        [12] = { .thinkfunc = vore_search }
    },
    .default_aifunc = ai_run
};

void vore_run(edict_t *self)
{
    if (self->monsterinfo.aiflags & AI_STAND_GROUND)
        self->monsterinfo.currentmove = &vore_move_stand;
    else
        self->monsterinfo.currentmove = &vore_move_run;
}

enum {
    VORE_BALL_ANIM_START = 7,
    VORE_BALL_ANIM_END = 15,
    VORE_BALL_ANIM_COUNT = (VORE_BALL_ANIM_END - VORE_BALL_ANIM_START) + 1
};

mmove_t vore_ball_anim = {
    .firstframe = VORE_BALL_ANIM_START,
    .lastframe = VORE_BALL_ANIM_END,
    .frame = (mframe_t [VORE_BALL_ANIM_COUNT]) { 0 }
};

void vore_ball_touch(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
    vec3_t d;
    VectorMA(self->s.origin, -0.01f, self->velocity, d);

    SV_WriteByte(svc_temp_entity);
    SV_WriteByte(TE_EXPLOSION1);
    SV_WritePos(d);
    SV_Multicast(d, MULTICAST_PVS, false);

    G_FreeEdict(self);
}

#define VORE_BALL_CHASE_SPEED 300
#define VORE_BALL_CORRECTION 0.35

void vore_ball_chase(edict_t *self)
{
    // vore or enemy dead, so just keep going forward
    if (!self->enemy || !self->enemy->inuse || !self->owner || !self->owner->inuse || self->owner->health <= 0 || self->enemy->health <= 0) {
        return;
    }

    self->nextthink = level.time + 1;

    vec3_t chase_origin;
    VectorMA(self->enemy->absmin, 0.5, self->enemy->size, chase_origin);
    chase_origin[2] += self->enemy->viewheight;
    trace_t tr = SV_Trace(self->s.origin, vec3_origin, vec3_origin, self->enemy->s.origin, self, MASK_OPAQUE);

    // target not in view, so just drift
    if (tr.fraction != 1.f) {
        return;
    }

    // don't allow us to get too close to another vore ball; push the other balls away
    edict_t *other = NULL;
    vec3_t dir;
    
    while ((other = findradius(other, self->s.origin, 24.f))) {
        if (other != self && strcmp(other->classname, self->classname) == 0) {
            VectorSubtract(other->s.origin, self->s.origin, dir);
            VectorNormalize(dir);

            VectorScale(dir, VORE_BALL_CHASE_SPEED, other->movedir);
            VectorAdd(other->velocity, other->movedir, other->movedir);
            VectorNormalize(other->movedir);
            VectorScale(other->movedir, VORE_BALL_CHASE_SPEED, other->movedir);
        }
    }

    // target in view, apply correction
    VectorSubtract(chase_origin, self->s.origin, dir);
    VectorNormalize(dir);

    VectorScale(dir, VORE_BALL_CORRECTION, dir);
    VectorAdd(dir, self->movedir, dir);

    VectorNormalize(dir);

    VectorCopy(dir, self->movedir);

    VectorScale(dir, VORE_BALL_CHASE_SPEED, self->velocity);
}

void vore_ball_think(edict_t *self)
{
    self->s.frame++;
    self->nextthink = level.time + 1;

    if (self->s.frame == VORE_BALL_ANIM_END)
    {
        self->movetype = MOVETYPE_FLYMISSILE;
        self->solid = SOLID_BBOX;
        self->think = vore_ball_chase;
        self->touch = vore_ball_touch;
        self->s.sound = SV_SoundIndex(ASSET_SOUND_VORE_BALL_CHASE);
        self->s.sound_pitch = 83;
        self->owner->mynoise = NULL;
        VectorSet(self->avelocity, crandom() * 300, crandom() * 300, crandom() * 300);

        vec3_t d;
        VectorSubtract(self->enemy->s.origin, self->s.origin, d);
        VectorNormalize(d);
        VectorCopy(d, self->movedir);
        VectorScale(d, VORE_BALL_CHASE_SPEED, self->velocity);
        return;
    }

    vec3_t axis[3];
    vec3_t pt;
    VectorCopy(vore_ball_anim.frame[self->s.frame - VORE_BALL_ANIM_START].translate, pt);

    AnglesToAxis((const vec3_t) { 0, -self->owner->s.angles[1], 0 }, axis);
    RotatePoint(pt, axis);

    VectorAdd(self->owner->s.origin, pt, self->s.origin);
    SV_LinkEntity(self);
}

static void vore_spawn_ball(edict_t *self)
{
    edict_t *ball = G_Spawn();
    ball->s.modelindex = SV_ModelIndex(ASSET_MODEL_VORE_BALL);
    VectorCopy(self->s.origin, ball->s.origin);
    VectorCopy(self->s.angles, ball->s.angles);
    ball->svflags = SVF_DEADMONSTER;
    ball->clipmask = MASK_SHOT;
    ball->s.effects = EF_PLASMA;
    ball->owner = self;
    ball->enemy = self->enemy;
    ball->s.frame = 6;
    vore_ball_think(ball);
    ball->think = vore_ball_think;
    ball->nextthink = level.time + 1;
    ball->classname = "vore_ball";
    self->mynoise = ball;
    SV_LinkEntity(ball);
}

mmove_t vore_move_attack = {
    .firstframe = FRAME_ATTACK_FIRST,
    .lastframe = FRAME_ATTACK_LAST,
    .frame = (mframe_t [FRAME_ATTACK_COUNT]) {
        [7] = { .thinkfunc = vore_spawn_ball }
    },
    .endfunc = vore_run,
    .default_aifunc = ai_charge
};

void vore_attack(edict_t *self)
{
    if (!self->enemy || !self->enemy->inuse)
        return;

    self->monsterinfo.currentmove = &vore_move_attack;
}

mmove_t vore_move_pain = {
    .firstframe = FRAME_PAIN_FIRST,
    .lastframe = FRAME_PAIN_LAST,
    .frame = (mframe_t [FRAME_PAIN_COUNT]) { 0 },
    .endfunc = vore_run,
    .default_aifunc = ai_move
};

void vore_pain(edict_t *self, edict_t *other, float kick, int damage)
{
    //if (self->health < (self->max_health / 2))
    //    self->s.skinnum = 1;

    if (level.time < self->pain_debounce_time)
        return;

    self->pain_debounce_time = level.time + 3000;
    SV_StartSound(self, CHAN_VOICE, sound_pain, 1, ATTN_NORM, 0);

    if (skill.integer == 3)
        return;     // no pain anims in nightmare

    if (self->mynoise) {
        G_FreeEdict(self);
    }

    self->monsterinfo.currentmove = &vore_move_pain;
}

void vore_dead(edict_t *self)
{
    VectorSet(self->mins, -16, -16, -0);
    VectorSet(self->maxs, 16, 16, 8);
    self->movetype = MOVETYPE_TOSS;
    self->nextthink = 0;
    SV_LinkEntity(self);
}

mmove_t vore_move_death = {
    .firstframe = FRAME_DEATH_FIRST,
    .lastframe = FRAME_DEATH_LAST,
    .frame = (mframe_t [FRAME_DEATH_COUNT]) { 0 },
    .endfunc = vore_dead,
    .default_aifunc = ai_move
};

void vore_die(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
    int     n;

    if (self->health <= self->gib_health) {
        SV_StartSound(self, CHAN_VOICE, SV_SoundIndex(ASSET_SOUND_GIB), 1, ATTN_NORM, 0);
        for (n = 0; n < 2; n++)
            ThrowGib(self, ASSET_MODEL_GIB_BONE, damage, GIB_ORGANIC);
        for (n = 0; n < 4; n++)
            ThrowGib(self, ASSET_MODEL_GIB_MEAT, damage, GIB_ORGANIC);
        ThrowHead(self, ASSET_MODEL_GIB_HEAD, damage, GIB_ORGANIC);
        self->deadflag = DEAD_DEAD;
        return;
    }

    if (self->deadflag == DEAD_DEAD)
        return;

    SV_StartSound(self, CHAN_VOICE, sound_die, 1, ATTN_NORM, 0);
    self->deadflag = DEAD_DEAD;
    self->takedamage = DAMAGE_YES;
    self->svflags |= SVF_DEADMONSTER;
    SV_LinkEntity(self);

    if (self->mynoise) {
        G_FreeEdict(self->mynoise);
        self->mynoise = NULL;
    }

    self->monsterinfo.currentmove = &vore_move_death;
}

static bool vore_inititalized = false;

void vore_load(edict_t *self)
{
    if (vore_inititalized)
        return;

    m_iqm_t *iqm = M_InitializeIQM(ASSET_MODEL_VORE);

    M_SetupIQMAnimations(iqm, (mmove_t *[]) {
        &vore_move_stand, &vore_move_walk, &vore_move_run, &vore_move_attack,
        &vore_move_pain, &vore_move_death, NULL
    });

    M_FreeIQM(iqm);

    iqm = M_InitializeIQM(ASSET_MODEL_VORE_BALL);

    M_SetupIQMAnimation(iqm, &vore_ball_anim, true);

    M_FreeIQM(iqm);

    vore_inititalized = true;
}

/*QUAKED monster_vore (1 .5 0) (-16 -16 -24) (16 16 32) Ambush Trigger_Spawn Sight
*/
void SP_monster_vore(edict_t *self)
{
    if (deathmatch.integer) {
        G_FreeEdict(self);
        return;
    }

    // pre-caches
    sound_pain    = SV_SoundIndex(ASSET_SOUND_VORE_HURT);
    sound_die     = SV_SoundIndex(ASSET_SOUND_VORE_DEATH);
    sound_idle    = SV_SoundIndex(ASSET_SOUND_VORE_IDLE);
    sound_attack  = SV_SoundIndex(ASSET_SOUND_VORE_ATTACK);
    sound_attack2 = SV_SoundIndex(ASSET_SOUND_VORE_ATTACK2);
    sound_sight   = SV_SoundIndex(ASSET_SOUND_VORE_SIGHT);

    self->s.modelindex = SV_ModelIndex(ASSET_MODEL_VORE);
    VectorSet(self->mins, -16, -16, -0);
    VectorSet(self->maxs, 16, 16, 56);
    self->movetype = MOVETYPE_STEP;
    self->solid = SOLID_BBOX;

    self->health = 400;
    self->gib_health = -60;
    self->mass = 165;
    self->monsterinfo.aiflags |= AI_HIGH_TICK_RATE;
    self->yaw_speed = 25;
    self->viewheight = 48;

    self->pain = vore_pain;
    self->die = vore_die;

    self->monsterinfo.stand = vore_stand;
    self->monsterinfo.walk = vore_walk;
    self->monsterinfo.run = vore_run;
    self->monsterinfo.attack = vore_attack;
    self->monsterinfo.sight = vore_sight;
    self->monsterinfo.search = vore_search;
    self->monsterinfo.idle = vore_search;

    self->monsterinfo.currentmove = &vore_move_stand;
    self->monsterinfo.load = vore_load;

    SV_ModelIndex(ASSET_MODEL_VORE_BALL);
    SV_SoundIndex(ASSET_SOUND_VORE_BALL_CHASE);

    SV_LinkEntity(self);

    walkmonster_start(self);
}
