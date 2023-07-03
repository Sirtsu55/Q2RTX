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

enum {
    ANIMATION(IDLE, 14),
    ANIMATION(WALK, 32),
    ANIMATION(RUN, 16),
    ANIMATION(ATTACK, 20),
    ANIMATION(DEATH1, 28),
    ANIMATION(DEATH2, 22),
    ANIMATION(PAIN1, 9),
    ANIMATION(PAIN2, 10),
    ANIMATION(PAIN3, 15),
    ANIMATION(PAIN4, 38)
};

static int sound_death;
static int sound_idle;
static int sound_pain1;
static int sound_pain2;
static int sound_sight1;
static int sound_sight2;
static int sound_sight3;
static int sound_sight4;

void enforcer_sight(edict_t *self, edict_t *other)
{
    switch (Q_rand_uniform(4))
    {
    case 0:
        SV_StartSound(self, CHAN_VOICE, sound_sight1, 1, ATTN_NORM, 0);
        break;
    case 1:
        SV_StartSound(self, CHAN_VOICE, sound_sight2, 1, ATTN_NORM, 0);
        break;
    case 2:
        SV_StartSound(self, CHAN_VOICE, sound_sight3, 1, ATTN_NORM, 0);
        break;
    case 3:
        SV_StartSound(self, CHAN_VOICE, sound_sight4, 1, ATTN_NORM, 0);
        break;
    }
}

void enforcer_search(edict_t *self)
{
    if (frand() < 0.2)
        SV_StartSound(self, CHAN_VOICE, sound_idle, 1, ATTN_NORM, 0);
}

mmove_t enforcer_move_stand = {
    .firstframe = FRAME_IDLE_FIRST,
    .lastframe = FRAME_IDLE_LAST,
    .frame = (mframe_t [FRAME_IDLE_COUNT]) { 0 },
    .default_aifunc = ai_stand
};

void enforcer_stand(edict_t *self)
{
    self->monsterinfo.currentmove = &enforcer_move_stand;
}

mmove_t enforcer_move_walk = {
    .firstframe = FRAME_WALK_FIRST,
    .lastframe = FRAME_WALK_LAST,
    .frame = (mframe_t [FRAME_WALK_COUNT]) { 0 },
    .default_aifunc = ai_walk
};

void enforcer_walk(edict_t *self)
{
    self->monsterinfo.currentmove = &enforcer_move_walk;
}

mmove_t enforcer_move_run = {
    .firstframe = FRAME_RUN_FIRST,
    .lastframe = FRAME_RUN_LAST,
    .frame = (mframe_t [FRAME_RUN_COUNT]) { 0 },
    .default_aifunc = ai_run
};

void enforcer_run(edict_t *self)
{
    if (self->monsterinfo.aiflags & AI_STAND_GROUND)
        self->monsterinfo.currentmove = &enforcer_move_stand;
    else
        self->monsterinfo.currentmove = &enforcer_move_run;
}

void enforcer_fire(edict_t *self)
{
    vec3_t  start;
    vec3_t  forward, right, aim, end;

    AngleVectors(self->s.angles, forward, right, NULL);

    vec3_t o;
    VectorCopy(monster_flash_offset[MZ2_SOLDIER_BLASTER_8], o);
    o[2] += 26.f;
    o[0] -= 20.0f;

    G_ProjectSource(self->s.origin, o, forward, right, start);

    VectorCopy(self->enemy->s.origin, end);
    end[2] += self->enemy->viewheight;
    VectorSubtract(end, start, aim);
    VectorNormalize(aim);

    monster_fire_blaster(self, start, aim, 15, 800, MZ2_SOLDIER_BLASTER_8, EF_HYPERBLASTER);
}

void enforcer_refire(edict_t *self)
{
    if (self->count)
        return;

    self->monsterinfo.nextframe = FRAME_ATTACK_FIRST + 7;
    self->count = 1;
}

mmove_t enforcer_move_attack = {
    .firstframe = FRAME_ATTACK_FIRST,
    .lastframe = FRAME_ATTACK_LAST, // last frame was unused in vanilla
    .frame = (mframe_t [FRAME_ATTACK_COUNT]) {
        [10] = { .thinkfunc = enforcer_fire },
        [15] = { .thinkfunc = enforcer_refire },
    },
    .endfunc = enforcer_run,
    .default_aifunc = ai_charge
};

void enforcer_attack(edict_t *self)
{
    self->monsterinfo.currentmove = &enforcer_move_attack;
    self->count = 0;
}

mmove_t enforcer_move_pain1 = {
    .firstframe = FRAME_PAIN1_FIRST,
    .lastframe = FRAME_PAIN1_LAST,
    .frame = (mframe_t [FRAME_PAIN1_COUNT]) { 0 },
    .endfunc = enforcer_run,
    .default_aifunc = ai_move
};

mmove_t enforcer_move_pain2 = {
    .firstframe = FRAME_PAIN2_FIRST,
    .lastframe = FRAME_PAIN2_LAST,
    .frame = (mframe_t [FRAME_PAIN2_COUNT]) { 0 },
    .endfunc = enforcer_run,
    .default_aifunc = ai_move
};

mmove_t enforcer_move_pain3 = {
    .firstframe = FRAME_PAIN3_FIRST,
    .lastframe = FRAME_PAIN3_LAST,
    .frame = (mframe_t [FRAME_PAIN3_COUNT]) { 0 },
    .endfunc = enforcer_run,
    .default_aifunc = ai_move
};

mmove_t enforcer_move_pain4 = {
    .firstframe = FRAME_PAIN4_FIRST,
    .lastframe = FRAME_PAIN4_LAST,
    .frame = (mframe_t [FRAME_PAIN4_COUNT]) { 0 },
    .endfunc = enforcer_run,
    .default_aifunc = ai_move
};

void enforcer_pain(edict_t *self, edict_t *other, float kick, int damage)
{
    //if (self->health < (self->max_health / 2))
    //    self->s.skinnum = 1;

    if (level.time < self->pain_debounce_time)
        return;

    self->pain_debounce_time = level.time + 3000;

    if (Q_rand_uniform(2) == 0)
        SV_StartSound(self, CHAN_VOICE, sound_pain1, 1, ATTN_NORM, 0);
    else
        SV_StartSound(self, CHAN_VOICE, sound_pain2, 1, ATTN_NORM, 0);

    if (skill.integer == 3)
        return;     // no pain anims in nightmare

    int r = Q_rand_uniform(4);

    if (r == 0)
        self->monsterinfo.currentmove = &enforcer_move_pain1;
    else if (r == 1)
        self->monsterinfo.currentmove = &enforcer_move_pain2;
    else if (r == 2)
        self->monsterinfo.currentmove = &enforcer_move_pain3;
    else
        self->monsterinfo.currentmove = &enforcer_move_pain4;
}

void enforcer_dead(edict_t *self)
{
    VectorSet(self->mins, -16, -16, -0);
    VectorSet(self->maxs, 16, 16, 8);
    self->movetype = MOVETYPE_TOSS;
    self->nextthink = 0;
    SV_LinkEntity(self);
}

mmove_t enforcer_move_death1 = {
    .firstframe = FRAME_DEATH1_FIRST,
    .lastframe = FRAME_DEATH1_LAST,
    .frame = (mframe_t [FRAME_DEATH1_COUNT]) { 0 },
    .endfunc = enforcer_dead,
    .default_aifunc = ai_move
};

mmove_t enforcer_move_death2 = {
    .firstframe = FRAME_DEATH2_FIRST,
    .lastframe = FRAME_DEATH2_LAST,
    .frame = (mframe_t [FRAME_DEATH2_COUNT]) { 0 },
    .endfunc = enforcer_dead,
    .default_aifunc = ai_move
};

void enforcer_die(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
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

    SV_StartSound(self, CHAN_VOICE, sound_death, 1, ATTN_NORM, 0);
    self->deadflag = DEAD_DEAD;
    self->takedamage = DAMAGE_YES;
    self->svflags |= SVF_DEADMONSTER;
    SV_LinkEntity(self);

    if (frand() < 0.5f)
        self->monsterinfo.currentmove = &enforcer_move_death1;
    else
        self->monsterinfo.currentmove = &enforcer_move_death2;
}

static bool enforcer_inititalized = false;

void enforcer_load(edict_t *self)
{
    if (enforcer_inititalized)
        return;

    m_iqm_t *iqm = M_InitializeIQM(ASSET_MODEL_ENFORCER);

    M_SetupIQMAnimations(iqm, (mmove_t *[]) {
        &enforcer_move_stand, &enforcer_move_walk, &enforcer_move_run,
            &enforcer_move_attack, &enforcer_move_pain1, &enforcer_move_pain2, &enforcer_move_pain3,
            &enforcer_move_pain4, &enforcer_move_death1, &enforcer_move_death2, NULL
    });

    M_FreeIQM(iqm);

    enforcer_inititalized = true;
}

/*QUAKED monster_enforcer (1 .5 0) (-16 -16 -24) (16 16 32) Ambush Trigger_Spawn Sight
*/
void SP_monster_enforcer(edict_t *self)
{
    if (deathmatch.integer) {
        G_FreeEdict(self);
        return;
    }

    sound_death = SV_SoundIndex(ASSET_SOUND_ENFORCER_DEATH);
    sound_idle = SV_SoundIndex(ASSET_SOUND_ENFORCER_IDLE);
    sound_pain1 = SV_SoundIndex(ASSET_SOUND_ENFORCER_PAIN1);
    sound_pain2 = SV_SoundIndex(ASSET_SOUND_ENFORCER_PAIN2);
    sound_sight1 = SV_SoundIndex(ASSET_SOUND_ENFORCER_SIGHT1);
    sound_sight2 = SV_SoundIndex(ASSET_SOUND_ENFORCER_SIGHT2);
    sound_sight3 = SV_SoundIndex(ASSET_SOUND_ENFORCER_SIGHT3);
    sound_sight4 = SV_SoundIndex(ASSET_SOUND_ENFORCER_SIGHT4);

    self->s.modelindex = SV_ModelIndex(ASSET_MODEL_ENFORCER);
    VectorSet(self->mins, -16, -16, -0);
    VectorSet(self->maxs, 16, 16, 56);
    self->movetype = MOVETYPE_STEP;
    self->solid = SOLID_BBOX;
    self->viewheight = 48;

    self->health = 80;
    self->gib_health = -35;
    self->mass = 175;
    self->monsterinfo.aiflags |= AI_HIGH_TICK_RATE;
    self->yaw_speed = 25;

    self->pain = enforcer_pain;
    self->die = enforcer_die;

    self->monsterinfo.stand = enforcer_stand;
    self->monsterinfo.walk = enforcer_walk;
    self->monsterinfo.run = enforcer_run;
    self->monsterinfo.attack = enforcer_attack;
    self->monsterinfo.sight = enforcer_sight;
    self->monsterinfo.search = enforcer_search;
    self->monsterinfo.idle = enforcer_search;

    self->monsterinfo.currentmove = &enforcer_move_stand;
    self->monsterinfo.load = enforcer_load;

    SV_LinkEntity(self);

    walkmonster_start(self);
}
