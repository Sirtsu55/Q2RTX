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
static int sound_punch;
static int sound_sight;

enum {
    ANIMATION(STAND, 18),
    ANIMATION(RUN, 16),
    ANIMATION(RUNATTACK, 22),
    ANIMATION(PAINA, 6),
    ANIMATION(PAINB, 22),
    ANIMATION(ATTACKB, 22),
    ANIMATION(WALK, 28),
    ANIMATION(DEATHA, 20),
    ANIMATION(DEATHB, 22)
};

void knight_sight(edict_t *self, edict_t *other)
{
    SV_StartSound(self, CHAN_VOICE, sound_sight, 1, ATTN_NORM, 0);
}

void knight_search(edict_t *self)
{
    if (frand() < 0.2)
        SV_StartSound(self, CHAN_VOICE, sound_idle, 1, ATTN_NORM, 0);
}

mmove_t knight_move_stand = {
    .firstframe = FRAME_STAND_FIRST,
    .lastframe = FRAME_STAND_LAST,
    .frame = (mframe_t [FRAME_STAND_COUNT]) { 0 },
    .default_aifunc = ai_stand
};

void knight_stand(edict_t *self)
{
    self->monsterinfo.currentmove = &knight_move_stand;
}

mmove_t knight_move_walk = {
    .firstframe = FRAME_WALK_FIRST,
    .lastframe = FRAME_WALK_LAST,
    .frame = (mframe_t [FRAME_WALK_COUNT]) { 0 },
    .default_aifunc = ai_walk
};

void knight_walk(edict_t *self)
{
    self->monsterinfo.currentmove = &knight_move_walk;
}

mmove_t knight_move_run1 = {
    .firstframe = FRAME_RUN_FIRST,
    .lastframe = FRAME_RUN_LAST,
    .frame = (mframe_t [FRAME_RUN_COUNT]) {
        [0] = { .thinkfunc = knight_search },
        [8] = { .thinkfunc = knight_search }
    },
    .default_aifunc = ai_run
};

void knight_run(edict_t *self)
{
    if (self->monsterinfo.aiflags & AI_STAND_GROUND)
        self->monsterinfo.currentmove = &knight_move_stand;
    else
        self->monsterinfo.currentmove = &knight_move_run1;
}

void knight_attack_spike(edict_t *self)
{
    static  vec3_t  aim = {MELEE_DISTANCE, 0, -24};
    fire_hit(self, aim, 2, 0);    //  Faster attack -- upwards and backwards
}

void knight_swing(edict_t *self)
{
    SV_StartSound(self, CHAN_WEAPON, sound_punch, 1, ATTN_NORM, 0);
}

mmove_t knight_move_attack_spike = {
    .firstframe = FRAME_RUNATTACK_FIRST,
    .lastframe = FRAME_RUNATTACK_LAST,
    .frame = (mframe_t [FRAME_RUNATTACK_COUNT]) {
        [0] = { .thinkfunc = knight_swing },
        [8] = { .thinkfunc = knight_attack_spike },
        [9] = { .thinkfunc = knight_attack_spike },
        [10] = { .thinkfunc = knight_attack_spike },
        [11] = { .thinkfunc = knight_attack_spike },
        [12] = { .thinkfunc = knight_attack_spike },
        [13] = { .thinkfunc = knight_attack_spike },
        [14] = { .thinkfunc = knight_attack_spike },
        [15] = { .thinkfunc = knight_attack_spike },
        [16] = { .thinkfunc = knight_attack_spike },
        [17] = { .thinkfunc = knight_attack_spike }
    },
    .endfunc = knight_run,
    .default_aifunc = ai_charge
};

void knight_attack(edict_t *self)
{
    if (!self->enemy || !self->enemy->inuse)
        return;

    if (range(self, self->enemy) <= RANGE_NEAR)
    {
        vec3_t sub;
        VectorSubtract(self->s.origin, self->enemy->s.origin, sub);
        
        if (VectorLength(sub) < 200)
            self->monsterinfo.currentmove = &knight_move_attack_spike;
    }
}

mmove_t knight_move_attack = {
    .firstframe = FRAME_ATTACKB_FIRST,
    .lastframe = FRAME_ATTACKB_LAST - 2, // last frame was unused in vanilla
    .frame = (mframe_t [FRAME_ATTACKB_COUNT]) {
        [0] = { .thinkfunc = knight_swing },
        [10] = { .thinkfunc = knight_attack_spike },
        [11] = { .thinkfunc = knight_attack_spike },
        [12] = { .thinkfunc = knight_attack_spike },
        [13] = { .thinkfunc = knight_attack_spike },
        [14] = { .thinkfunc = knight_attack_spike },
        [15] = { .thinkfunc = knight_attack_spike }
    },
    .endfunc = knight_run,
    .default_aifunc = ai_charge
};

void knight_melee(edict_t *self)
{
    self->monsterinfo.currentmove = &knight_move_attack;
}

mmove_t knight_move_pain1 = {
    .firstframe = FRAME_PAINA_FIRST,
    .lastframe = FRAME_PAINA_LAST,
    .frame = (mframe_t [FRAME_PAINA_COUNT]) { 0 },
    .endfunc = knight_run,
    .default_aifunc = ai_move
};

mmove_t knight_move_pain2 = {
    .firstframe = FRAME_PAINB_FIRST,
    .lastframe = FRAME_PAINB_LAST,
    .frame = (mframe_t [FRAME_PAINB_COUNT]) { 0 },
    .endfunc = knight_run,
    .default_aifunc = ai_move
};

void knight_pain(edict_t *self, edict_t *other, float kick, int damage)
{
    //if (self->health < (self->max_health / 2))
    //    self->s.skinnum = 1;

    if (level.time < self->pain_debounce_time)
        return;

    self->pain_debounce_time = level.time + 3000;
    SV_StartSound(self, CHAN_VOICE, sound_pain, 1, ATTN_NORM, 0);

    if (skill.integer == 3)
        return;     // no pain anims in nightmare

    if ((damage < 20) || (random() < 0.5f))
        self->monsterinfo.currentmove = &knight_move_pain1;
    else
        self->monsterinfo.currentmove = &knight_move_pain2;
}

void knight_dead(edict_t *self)
{
    VectorSet(self->mins, -16, -16, -0);
    VectorSet(self->maxs, 16, 16, 8);
    self->movetype = MOVETYPE_TOSS;
    self->nextthink = 0;
    SV_LinkEntity(self);
}

mmove_t knight_move_death1 = {
    .firstframe = FRAME_DEATHA_FIRST,
    .lastframe = FRAME_DEATHA_LAST,
    .frame = (mframe_t [FRAME_DEATHA_COUNT]) { 0 },
    .endfunc = knight_dead,
    .default_aifunc = ai_move
};

mmove_t knight_move_death2 = {
    .firstframe = FRAME_DEATHB_FIRST,
    .lastframe = FRAME_DEATHB_LAST,
    .frame = (mframe_t [FRAME_DEATHB_COUNT]) { 0 },
    .endfunc = knight_dead,
    .default_aifunc = ai_move
};

void knight_die(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
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

    if (frand() < 0.5f)
        self->monsterinfo.currentmove = &knight_move_death1;
    else
        self->monsterinfo.currentmove = &knight_move_death2;
}

static bool knight_inititalized = false;

void knight_load(edict_t *self)
{
    if (knight_inititalized)
        return;

    m_iqm_t *iqm = M_InitializeIQM(ASSET_MODEL_KNIGHT);

    M_SetupIQMDists(iqm, (mmove_t *[]) {
        &knight_move_stand, &knight_move_walk, &knight_move_run1, &knight_move_attack_spike,
        &knight_move_attack, &knight_move_pain1, &knight_move_pain2, &knight_move_death1,
        &knight_move_death2, NULL
    });

    M_FreeIQM(iqm);

    knight_inititalized = true;
}

/*QUAKED monster_knight (1 .5 0) (-16 -16 -24) (16 16 32) Ambush Trigger_Spawn Sight
*/
void SP_monster_knight(edict_t *self)
{
    if (deathmatch.integer) {
        G_FreeEdict(self);
        return;
    }

    // pre-caches
    sound_pain  = SV_SoundIndex(ASSET_SOUND_KNIGHT_HURT);
    sound_die   = SV_SoundIndex(ASSET_SOUND_KNIGHT_DEATH);
    sound_idle  = SV_SoundIndex(ASSET_SOUND_KNIGHT_IDLE);
    sound_punch = SV_SoundIndex(ASSET_SOUND_KNIGHT_SWORD);
    sound_sight = SV_SoundIndex(ASSET_SOUND_KNIGHT_SIGHT);

    self->s.modelindex = SV_ModelIndex(ASSET_MODEL_KNIGHT);
    VectorSet(self->mins, -16, -16, -0);
    VectorSet(self->maxs, 16, 16, 56);
    self->movetype = MOVETYPE_STEP;
    self->solid = SOLID_BBOX;
    self->viewheight = 48;

    self->health = 90;
    self->gib_health = -60;
    self->mass = 85;
    self->monsterinfo.aiflags |= AI_HIGH_TICK_RATE;
    self->yaw_speed = 25;

    self->pain = knight_pain;
    self->die = knight_die;

    self->monsterinfo.stand = knight_stand;
    self->monsterinfo.walk = knight_walk;
    self->monsterinfo.run = knight_run;
    self->monsterinfo.attack = knight_attack;
    self->monsterinfo.melee = knight_melee;
    self->monsterinfo.sight = knight_sight;
    self->monsterinfo.search = knight_search;
    self->monsterinfo.idle = knight_search;

    self->monsterinfo.currentmove = &knight_move_stand;
    self->monsterinfo.load = knight_load;

    SV_LinkEntity(self);

    walkmonster_start(self);
}
