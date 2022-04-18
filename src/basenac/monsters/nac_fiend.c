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
static int sound_hit;
static int sound_sight;
static int sound_jump;
static int sound_land;

enum {
    ANIMATION(STAND, 25),
    ANIMATION(WALK, 16),
    ANIMATION(RUN, 12),
    ANIMATION(LEAP, 24),
    ANIMATION(PAIN, 12),
    ANIMATION(DEATH, 18),
    ANIMATION(ATTACKA, 29)
};

void fiend_sight(edict_t *self, edict_t *other)
{
    SV_StartSound(self, CHAN_VOICE, sound_sight, 1, ATTN_NORM, 0);
    self->monsterinfo.idle_time = level.time + 1000;
}

void fiend_search(edict_t *self)
{
    if (frand() < 0.2 && level.time > self->monsterinfo.idle_time)
    {
        SV_StartSound(self, CHAN_VOICE, sound_idle, 1, ATTN_NORM, 0);
        self->monsterinfo.idle_time = level.time + 500;
    }
}

mmove_t fiend_move_stand = {
    .firstframe = FRAME_STAND_FIRST,
    .lastframe = FRAME_STAND_LAST,
    .frame = (mframe_t [FRAME_STAND_COUNT]) { 0 },
    .default_aifunc = ai_stand
};

void fiend_stand(edict_t *self)
{
    self->monsterinfo.currentmove = &fiend_move_stand;
}

mmove_t fiend_move_walk = {
    .firstframe = FRAME_WALK_FIRST,
    .lastframe = FRAME_WALK_LAST,
    .frame = (mframe_t [FRAME_WALK_COUNT]) { 0 },
    .default_aifunc = ai_walk
};

void fiend_walk(edict_t *self)
{
    self->monsterinfo.currentmove = &fiend_move_walk;
}

mmove_t fiend_move_run1 = {
    .firstframe = FRAME_RUN_FIRST,
    .lastframe = FRAME_RUN_LAST,
    .frame = (mframe_t [FRAME_RUN_COUNT]) {
        [0] = { .thinkfunc = fiend_search },
        [6] = { .thinkfunc = fiend_search }
    },
    .default_aifunc = ai_run
};

void fiend_run(edict_t *self)
{
    if (self->monsterinfo.aiflags & AI_STAND_GROUND)
        self->monsterinfo.currentmove = &fiend_move_stand;
    else
        self->monsterinfo.currentmove = &fiend_move_run1;
}

void fiend_attack_claw(edict_t *self)
{
    static  vec3_t  aim = {MELEE_DISTANCE * 1.25f, 0, 0};
    fire_hit(self, aim, 25, 5);
}

mmove_t fiend_move_attack_claw = {
    .firstframe = FRAME_ATTACKA_FIRST,
    .lastframe = FRAME_ATTACKA_LAST,
    .frame = (mframe_t [FRAME_ATTACKA_COUNT]) {
        [10] = { .thinkfunc = fiend_attack_claw },
        [22] = { .thinkfunc = fiend_attack_claw }
    },
    .endfunc = fiend_run,
    .default_aifunc = ai_charge
};

void fiend_melee(edict_t *self)
{
    self->monsterinfo.currentmove = &fiend_move_attack_claw;
}

void fiend_leap_early_land(edict_t *self)
{
    if (self->groundentity)
        self->monsterinfo.nextframe = FRAME_LEAP_FIRST + 20;
}

void fiend_leap_wait(edict_t *self)
{
    if (self->groundentity)
        self->monsterinfo.nextframe = FRAME_LEAP_FIRST + 20;
    else
        self->monsterinfo.nextframe = self->s.frame;
}

void fiend_touch(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
    if (self->health <= 0) {
        self->touch = NULL;
        return;
    }

    if (other->takedamage) {
        if (VectorLength(self->velocity) > 200) {
            vec3_t  point;
            vec3_t  normal;
            int     damage;

            VectorCopy(self->velocity, normal);
            VectorNormalize(normal);
            VectorMA(self->s.origin, self->maxs[0], normal, point);
            damage = 40 + 10 * random();
            T_Damage(other, self, self, self->velocity, point, normal, damage, damage, 0, MOD_UNKNOWN);
        }
    }

    M_CheckBottom(self);

    self->touch = NULL;
    VectorClear(self->velocity);
}

void fiend_leap(edict_t *self)
{
    VectorCopy(self->pos1, self->velocity);
    vec3_t dir;
    VectorNormalize2(self->velocity, dir);
    self->velocity[2] += 24.f;
    self->groundentity = NULL;
    self->s.origin[2] += 1;
    self->touch = fiend_touch;

    SV_LinkEntity(self);
}

mmove_t fiend_move_leap = {
    .firstframe = FRAME_LEAP_FIRST,
    .lastframe = FRAME_LEAP_LAST,
    .frame = (mframe_t [FRAME_LEAP_COUNT]) {
        [6] = { .thinkfunc = fiend_leap },
        [7] = { .thinkfunc = fiend_leap_early_land, .aifunc = ai_move },
        [8] = { .thinkfunc = fiend_leap_early_land, .aifunc = ai_move },
        [9] = { .thinkfunc = fiend_leap_early_land, .aifunc = ai_move },
        [10] = { .thinkfunc = fiend_leap_early_land, .aifunc = ai_move },
        [11] = { .thinkfunc = fiend_leap_early_land, .aifunc = ai_move },
        [12] = { .thinkfunc = fiend_leap_early_land, .aifunc = ai_move },
        [13] = { .thinkfunc = fiend_leap_early_land, .aifunc = ai_move },
        [14] = { .thinkfunc = fiend_leap_early_land, .aifunc = ai_move },
        [15] = { .thinkfunc = fiend_leap_early_land, .aifunc = ai_move },
        [16] = { .thinkfunc = fiend_leap_early_land, .aifunc = ai_move },
        [17] = { .thinkfunc = fiend_leap_early_land, .aifunc = ai_move },
        [18] = { .thinkfunc = fiend_leap_early_land, .aifunc = ai_move },
        [19] = { .thinkfunc = fiend_leap_wait, .aifunc = ai_move }
    },
    .endfunc = fiend_run,
    .default_aifunc = ai_charge
};

void fiend_attack(edict_t *self)
{
    if (!self->enemy || !self->enemy->inuse)
        return;

    vec3_t low, high;
    int solutions = solve_ballistic_arc1(self->s.origin, 600.f, self->enemy->s.origin, 800.f, low, high);

    if (!solutions && self->enemy->s.origin[2] > self->s.origin[2])
    {
        int solutions = solve_ballistic_arc1(self->s.origin, 750.f, self->enemy->s.origin, 800.f, low, high);

        if (!solutions)
            return;
    }

    vec3_t o;
    VectorMA(self->s.origin, BASE_FRAMETIME_S, low, o);

    if (SV_Trace(self->s.origin, self->mins, self->maxs, o, self, MASK_SHOT).fraction != 1.0f)
    {
        if (solutions != 2)
            return;

        VectorCopy(high, self->pos1);
    }
    else
        VectorCopy(low, self->pos1);

    self->monsterinfo.currentmove = &fiend_move_leap;
    SV_StartSound(self, CHAN_VOICE, sound_jump, 1, ATTN_NORM, 0);
}

mmove_t fiend_move_pain = {
    .firstframe = FRAME_PAIN_FIRST,
    .lastframe = FRAME_PAIN_LAST,
    .frame = (mframe_t [FRAME_PAIN_COUNT]) { 0 },
    .endfunc = fiend_run,
    .default_aifunc = ai_move
};

void fiend_pain(edict_t *self, edict_t *other, float kick, int damage)
{
    //if (self->health < (self->max_health / 2))
    //    self->s.skinnum = 1;
    if (!self->groundentity)
        return;

    if (level.time < self->pain_debounce_time)
        return;

    self->pain_debounce_time = level.time + 3000;
    SV_StartSound(self, CHAN_VOICE, sound_pain, 1, ATTN_NORM, 0);

    if (skill.integer == 3)
        return;     // no pain anims in nightmare

    self->monsterinfo.currentmove = &fiend_move_pain;
}

void fiend_dead(edict_t *self)
{
    VectorSet(self->mins, -16, -16, -0);
    VectorSet(self->maxs, 16, 16, 8);
    self->movetype = MOVETYPE_TOSS;
    self->nextthink = 0;
    SV_LinkEntity(self);
}

mmove_t fiend_move_death = {
    .firstframe = FRAME_DEATH_FIRST,
    .lastframe = FRAME_DEATH_LAST,
    .frame = (mframe_t [FRAME_DEATH_COUNT]) { 0 },
    .endfunc = fiend_dead,
    .default_aifunc = ai_move
};

void fiend_die(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
    int     n;

    if (self->health <= self->gib_health) {
        SV_StartSound(self, CHAN_VOICE, SV_SoundIndex("misc/udeath.wav"), 1, ATTN_NORM, 0);
        for (n = 0; n < 2; n++)
            ThrowGib(self, "models/objects/gibs/bone/tris.md2", damage, GIB_ORGANIC);
        for (n = 0; n < 4; n++)
            ThrowGib(self, "models/objects/gibs/sm_meat/tris.md2", damage, GIB_ORGANIC);
        ThrowHead(self, "models/objects/gibs/head2/tris.md2", damage, GIB_ORGANIC);
        self->deadflag = DEAD_DEAD;
        return;
    }

    if (self->deadflag == DEAD_DEAD)
        return;

    SV_StartSound(self, CHAN_VOICE, sound_die, 1, ATTN_NORM, 0);
    self->deadflag = DEAD_DEAD;
    self->takedamage = DAMAGE_YES;
    self->svflags |= SVF_DEADMONSTER;
    self->touch = NULL;
    SV_LinkEntity(self);

    self->monsterinfo.currentmove = &fiend_move_death;
}

static bool fiend_inititalized = false;

void fiend_load(edict_t *self)
{
    if (fiend_inititalized)
        return;

    m_iqm_t *iqm = M_InitializeIQM("models/monsters/fiend/fiend.iqm");

    M_SetupIQMDists(iqm, (mmove_t *[]) {
        &fiend_move_stand, &fiend_move_walk, &fiend_move_run1, /*&fiend_move_leap,*/ &fiend_move_pain,
        &fiend_move_death, &fiend_move_attack_claw, NULL
    });

    M_FreeIQM(iqm);

    fiend_inititalized = true;
}

/*QUAKED monster_knight (1 .5 0) (-16 -16 -24) (16 16 32) Ambush Trigger_Spawn Sight
*/
void SP_monster_fiend(edict_t *self)
{
    if (deathmatch.integer) {
        G_FreeEdict(self);
        return;
    }

    // pre-caches
    sound_pain  = SV_SoundIndex("nac_fiend/dpain1.wav");
    sound_die   = SV_SoundIndex("nac_fiend/ddeath.wav");
    sound_idle  = SV_SoundIndex("nac_fiend/idle1.wav");
    sound_hit   = SV_SoundIndex("nac_fiend/dhit2.wav");
    sound_jump  = SV_SoundIndex("nac_fiend/djump.wav");
    sound_land  = SV_SoundIndex("nac_fiend/dland2.wav");
    sound_sight = SV_SoundIndex("nac_fiend/dsight.wav");

    self->s.modelindex = SV_ModelIndex("models/monsters/fiend/fiend.iqm");
    VectorSet(self->mins, -16, -16, -0);
    VectorSet(self->maxs, 16, 16, 56);
    self->movetype = MOVETYPE_STEP;
    self->solid = SOLID_BBOX;

    self->health = 300;
    self->gib_health = -60;
    self->mass = 145;
    self->monsterinfo.aiflags |= AI_HIGH_TICK_RATE;

    self->pain = fiend_pain;
    self->die = fiend_die;

    self->monsterinfo.stand = fiend_stand;
    self->monsterinfo.walk = fiend_walk;
    self->monsterinfo.run = fiend_run;
    self->monsterinfo.attack = fiend_attack;
    self->monsterinfo.melee = fiend_melee;
    self->monsterinfo.sight = fiend_sight;
    self->monsterinfo.search = fiend_search;
    self->monsterinfo.idle = fiend_search;

    self->monsterinfo.currentmove = &fiend_move_stand;
    self->monsterinfo.load = fiend_load;

    SV_LinkEntity(self);

    walkmonster_start(self);
}
