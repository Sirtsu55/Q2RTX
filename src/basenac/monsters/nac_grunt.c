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
    ANIMATION(IDLE, 16),
    ANIMATION(DEATH1, 20),
    ANIMATION(DEATH2, 22),
    ANIMATION(RELOAD, 21),
    ANIMATION(PAIN1, 12),
    ANIMATION(PAIN2, 28),
    ANIMATION(PAIN3, 27),
    ANIMATION(RUN, 15),
    ANIMATION(ATTACK, 19),
    ANIMATION(WALK, 47)
};

mmove_t grunt_move_stand = {
    .firstframe = FRAME_IDLE_FIRST,
    .lastframe = FRAME_IDLE_LAST,
    .frame = (mframe_t [FRAME_IDLE_COUNT]) { 0 },
    .default_aifunc = ai_stand
};

static int sound_idle;
static int sound_sight;
static int sound_pain1;
static int sound_pain2;
static int sound_death;

void grunt_sight(edict_t *self)
{
    SV_StartSound(self, CHAN_VOICE, sound_sight, 1, ATTN_NORM, 0);
}

void grunt_search(edict_t *self)
{
    if (frand() < 0.2)
        SV_StartSound(self, CHAN_VOICE, sound_idle, 1, ATTN_NORM, 0);
}

void grunt_stand(edict_t *self)
{
    self->monsterinfo.currentmove = &grunt_move_stand;
}

mmove_t grunt_move_walk = {
    .firstframe = FRAME_WALK_FIRST,
    .lastframe = FRAME_WALK_LAST,
    .frame = (mframe_t [FRAME_WALK_COUNT]) { 0 },
    .default_aifunc = ai_walk
};

void grunt_walk(edict_t *self)
{
    self->monsterinfo.currentmove = &grunt_move_walk;
}

mmove_t grunt_move_run = {
    .firstframe = FRAME_RUN_FIRST,
    .lastframe = FRAME_RUN_LAST,
    .frame = (mframe_t [FRAME_RUN_COUNT]) { 0 },
    .default_aifunc = ai_run
};

void grunt_run(edict_t *self)
{
    if (self->monsterinfo.aiflags & AI_STAND_GROUND)
        self->monsterinfo.currentmove = &grunt_move_stand;
    else
        self->monsterinfo.currentmove = &grunt_move_run;
}

void grunt_fire(edict_t *self)
{
    vec3_t  start;
    vec3_t  forward, right;

    AngleVectors(self->s.angles, forward, right, NULL);
    G_ProjectSource(self->s.origin, monster_flash_offset[MZ2_SOLDIER_SHOTGUN_1], forward, right, start);

    monster_fire_shotgun(self, start, forward, 2, 1, DEFAULT_SHOTGUN_HSPREAD, DEFAULT_SHOTGUN_VSPREAD, DEFAULT_SHOTGUN_COUNT, MZ2_SOLDIER_SHOTGUN_1);
}

mmove_t grunt_move_attack = {
    .firstframe = FRAME_ATTACK_FIRST,
    .lastframe = FRAME_ATTACK_LAST, // last frame was unused in vanilla
    .frame = (mframe_t [FRAME_ATTACK_COUNT]) {
        [10] = { .thinkfunc = grunt_fire },
    },
    .endfunc = grunt_run,
    .default_aifunc = ai_charge
};

void grunt_attack(edict_t *self)
{
    self->monsterinfo.currentmove = &grunt_move_attack;
}

mmove_t grunt_move_pain1 = {
    .firstframe = FRAME_PAIN1_FIRST,
    .lastframe = FRAME_PAIN1_LAST,
    .frame = (mframe_t [FRAME_PAIN1_COUNT]) { 0 },
    .endfunc = grunt_run,
    .default_aifunc = ai_move
};

mmove_t grunt_move_pain2 = {
    .firstframe = FRAME_PAIN2_FIRST,
    .lastframe = FRAME_PAIN2_LAST,
    .frame = (mframe_t [FRAME_PAIN2_COUNT]) { 0 },
    .endfunc = grunt_run,
    .default_aifunc = ai_move
};

mmove_t grunt_move_pain3 = {
    .firstframe = FRAME_PAIN3_FIRST,
    .lastframe = FRAME_PAIN3_LAST,
    .frame = (mframe_t [FRAME_PAIN3_COUNT]) { 0 },
    .endfunc = grunt_run,
    .default_aifunc = ai_move
};

void grunt_pain(edict_t *self, edict_t *other, float kick, int damage)
{
    if (self->health < (self->max_health / 2))
        self->s.skinnum = 1;

    if (level.time < self->pain_debounce_time)
        return;

    self->pain_debounce_time = level.time + 3000;

    if (Q_rand_uniform(2) == 0)
        SV_StartSound(self, CHAN_VOICE, sound_pain1, 1, ATTN_NORM, 0);
    else
        SV_StartSound(self, CHAN_VOICE, sound_pain2, 1, ATTN_NORM, 0);

    if (skill.integer == 3)
        return;     // no pain anims in nightmare

    int r = Q_rand_uniform(3);

    if (r == 0)
        self->monsterinfo.currentmove = &grunt_move_pain1;
    else if (r == 1)
        self->monsterinfo.currentmove = &grunt_move_pain2;
    else
        self->monsterinfo.currentmove = &grunt_move_pain3;
}

void grunt_dead(edict_t *self)
{
    VectorSet(self->mins, -16, -16, -0);
    VectorSet(self->maxs, 16, 16, 8);
    self->movetype = MOVETYPE_TOSS;
    self->nextthink = 0;
    SV_LinkEntity(self);
}

mmove_t grunt_move_death1 = {
    .firstframe = FRAME_DEATH1_FIRST,
    .lastframe = FRAME_DEATH1_LAST,
    .frame = (mframe_t [FRAME_DEATH1_COUNT]) { 0 },
    .endfunc = grunt_dead,
    .default_aifunc = ai_move
};

mmove_t grunt_move_death2 = {
    .firstframe = FRAME_DEATH2_FIRST,
    .lastframe = FRAME_DEATH2_LAST,
    .frame = (mframe_t [FRAME_DEATH2_COUNT]) { 0 },
    .endfunc = grunt_dead,
    .default_aifunc = ai_move
};

void grunt_die(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
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
        self->monsterinfo.currentmove = &grunt_move_death1;
    else
        self->monsterinfo.currentmove = &grunt_move_death2;
}

static bool grunt_inititalized = false;

void grunt_load(edict_t *self)
{
    if (grunt_inititalized)
        return;

    m_iqm_t *iqm = M_InitializeIQM(ASSET_MODEL_GRUNT);

    M_SetupIQMAnimations(iqm, (mmove_t *[]) {
        &grunt_move_stand, &grunt_move_walk, &grunt_move_run,
            &grunt_move_attack, &grunt_move_pain1, &grunt_move_pain2, &grunt_move_pain3,
            &grunt_move_death1, &grunt_move_death2, NULL
    });

    M_FreeIQM(iqm);

    grunt_inititalized = true;
}

/*QUAKED monster_grunt (1 .5 0) (-16 -16 -24) (16 16 32) Ambush Trigger_Spawn Sight
*/
void SP_monster_grunt(edict_t *self)
{
    if (deathmatch.integer) {
        G_FreeEdict(self);
        return;
    }

    sound_idle = SV_SoundIndex(ASSET_SOUND_GRUNT_IDLE);
    sound_sight = SV_SoundIndex(ASSET_SOUND_GRUNT_SIGHT);
    sound_pain1 = SV_SoundIndex(ASSET_SOUND_GRUNT_PAIN1);
    sound_pain2 = SV_SoundIndex(ASSET_SOUND_GRUNT_PAIN2);
    sound_death = SV_SoundIndex(ASSET_SOUND_GRUNT_DEATH);

    self->s.modelindex = SV_ModelIndex(ASSET_MODEL_GRUNT);
    VectorSet(self->mins, -16, -16, -0);
    VectorSet(self->maxs, 16, 16, 56);
    self->movetype = MOVETYPE_STEP;
    self->solid = SOLID_BBOX;
    self->viewheight = 48;

    self->health = 30;
    self->gib_health = -20;
    self->mass = 100;
    self->monsterinfo.aiflags |= AI_HIGH_TICK_RATE;
    self->yaw_speed = 25;

    self->pain = grunt_pain;
    self->die = grunt_die;

    self->monsterinfo.stand = grunt_stand;
    self->monsterinfo.walk = grunt_walk;
    self->monsterinfo.run = grunt_run;
    self->monsterinfo.attack = grunt_attack;
    self->monsterinfo.sight = grunt_sight;
    self->monsterinfo.search = grunt_search;
    self->monsterinfo.idle = grunt_search;

    self->monsterinfo.currentmove = &grunt_move_stand;
    self->monsterinfo.load = grunt_load;

    SV_LinkEntity(self);

    walkmonster_start(self);
}
