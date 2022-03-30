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
static int sound_search;

#define ANIMATION(name, length) \
    FRAME_##name##_FIRST, \
    FRAME_##name##_COUNT = length, \
    FRAME_##name##_LAST = (FRAME_##name##_FIRST + FRAME_##name##_COUNT) - 1

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
        SV_StartSound(self, CHAN_VOICE, sound_search, 1, ATTN_NORM, 0);
}

mframe_t knight_frames_stand[FRAME_STAND_COUNT] = {
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL },
    { ai_stand, 0, NULL }
};
mmove_t knight_move_stand = {FRAME_STAND_FIRST, FRAME_STAND_LAST, knight_frames_stand, NULL};

void knight_stand(edict_t *self)
{
    self->monsterinfo.currentmove = &knight_move_stand;
}

mframe_t knight_frames_walk [FRAME_WALK_COUNT] = {
    { ai_walk, 1.5, NULL },
    { ai_walk, 1.5, NULL },
    { ai_walk, 1.0, NULL },
    { ai_walk, 1.0, NULL },
    { ai_walk, 1.5, NULL },
    { ai_walk, 1.5, NULL },
    { ai_walk, 2.0, NULL },
    { ai_walk, 2.0, NULL },
    { ai_walk, 1.5, NULL },
    { ai_walk, 1.5, NULL },
    { ai_walk, 1.5, NULL },
    { ai_walk, 1.5, NULL },
    { ai_walk, 1.5, NULL },
    { ai_walk, 1.5, NULL },
    { ai_walk, 2.0, NULL },
    { ai_walk, 2.0, NULL },
    { ai_walk, 1.5, NULL },
    { ai_walk, 1.5, NULL },
    { ai_walk, 1.5, NULL },
    { ai_walk, 1.5, NULL },
    { ai_walk, 1.0, NULL },
    { ai_walk, 1.0, NULL },
    { ai_walk, 1.5, NULL },
    { ai_walk, 1.5, NULL },
    { ai_walk, 2.0, NULL },
    { ai_walk, 2.0, NULL },
    { ai_walk, 1.5, NULL },
    { ai_walk, 1.5, NULL }
};
mmove_t knight_move_walk = {FRAME_WALK_FIRST, FRAME_WALK_LAST, knight_frames_walk, NULL};

void knight_walk(edict_t *self)
{
    self->monsterinfo.currentmove = &knight_move_walk;
}

mframe_t knight_frames_run1 [FRAME_RUN_COUNT] = {
    { ai_run, 8.0, knight_search },
    { ai_run, 8.0, NULL },
    { ai_run, 10.0, NULL },
    { ai_run, 10.0, NULL },
    { ai_run, 6.5, NULL },
    { ai_run, 6.5, NULL },
    { ai_run, 3.5, NULL },
    { ai_run, 3.5, NULL },
    { ai_run, 8.0, knight_search },
    { ai_run, 8.0, NULL },
    { ai_run, 10.0, NULL },
    { ai_run, 10.0, NULL },
    { ai_run, 7.0, NULL },
    { ai_run, 7.0, NULL },
    { ai_run, 3.0, NULL },
    { ai_run, 3.0, NULL }
};
mmove_t knight_move_run1 = {FRAME_RUN_FIRST, FRAME_RUN_LAST, knight_frames_run1, NULL};

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

mframe_t knight_frames_runattack [FRAME_RUNATTACK_COUNT] = {
    { ai_charge, 10.0, knight_swing },
    { ai_charge, 10.0, NULL },
    { ai_charge, 10.0, NULL },
    { ai_charge, 10.0, NULL },
    { ai_charge, 10.0, NULL },
    { ai_charge, 10.0, NULL },
    { ai_charge, 10.0, NULL },
    { ai_charge, 10.0, NULL },
    { ai_charge, 10.0, knight_attack_spike },
    { ai_charge, 10.0, knight_attack_spike },
    { ai_charge, 10.0, knight_attack_spike },
    { ai_charge, 10.0, knight_attack_spike },
    { ai_charge, 10.0, knight_attack_spike },
    { ai_charge, 10.0, knight_attack_spike },
    { ai_charge, 10.0, knight_attack_spike },
    { ai_charge, 10.0, knight_attack_spike },
    { ai_charge, 10.0, knight_attack_spike },
    { ai_charge, 10.0, knight_attack_spike },
    { ai_charge, 10.0, NULL },
    { ai_charge, 10.0, NULL },
    { ai_charge, 10.0, NULL },
    { ai_charge, 10.0, NULL }
};
mmove_t knight_move_attack_spike = {FRAME_RUNATTACK_FIRST, FRAME_RUNATTACK_LAST, knight_frames_runattack, knight_run};

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

mframe_t knight_frames_attack [FRAME_ATTACKB_COUNT] = {
    { ai_charge, 0, knight_swing },
    { ai_charge, 0, NULL },
    { ai_charge, 3.5, NULL },
    { ai_charge, 3.5, NULL },
    { ai_charge, 2.0, NULL },
    { ai_charge, 2.0, NULL },
    { ai_charge, 0, NULL },
    { ai_charge, 0, NULL },
    { ai_charge, 1.5, NULL },
    { ai_charge, 1.5, NULL },
    { ai_charge, 2.0, knight_attack_spike },
    { ai_charge, 2.0, knight_attack_spike },
    { ai_charge, 0.5, knight_attack_spike },
    { ai_charge, 0.5, knight_attack_spike },
    { ai_charge, 1.5, knight_attack_spike },
    { ai_charge, 1.5, knight_attack_spike },
    { ai_charge, 0.5, NULL },
    { ai_charge, 0.5, NULL },
    { ai_charge, 2.5, NULL },
    { ai_charge, 2.5, NULL }
};
// last frame was unused in vanilla
mmove_t knight_move_attack = {FRAME_ATTACKB_FIRST, FRAME_ATTACKB_LAST - 2, knight_frames_attack, knight_run};


void knight_melee(edict_t *self)
{
    self->monsterinfo.currentmove = &knight_move_attack ;
}


mframe_t knight_frames_pain1 [FRAME_PAINA_COUNT] = {
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL }
};
mmove_t knight_move_pain1 = {FRAME_PAINA_FIRST, FRAME_PAINA_LAST, knight_frames_pain1, knight_run};


mframe_t knight_frames_pain2 [FRAME_PAINB_COUNT] = {
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 1.5, NULL },
    { ai_move, 1.5, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 1.0, NULL },
    { ai_move, 1.0, NULL },
    { ai_move, 2.0, NULL },
    { ai_move, 2.0, NULL },
    { ai_move, 1.0, NULL },
    { ai_move, 1.0, NULL },
    { ai_move, 2.5, NULL },
    { ai_move, 2.5, NULL },
    { ai_move, 2.5, NULL },
    { ai_move, 2.5, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL }
};
mmove_t knight_move_pain2 = {FRAME_PAINB_FIRST, FRAME_PAINB_LAST, knight_frames_pain2, knight_run};

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


mframe_t knight_frames_death1 [FRAME_DEATHA_COUNT] = {
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL }
};
mmove_t knight_move_death1 = {FRAME_DEATHA_FIRST, FRAME_DEATHA_LAST, knight_frames_death1, knight_dead};


mframe_t knight_frames_death2 [FRAME_DEATHB_COUNT] = {
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL },
    { ai_move, 0, NULL }
};
mmove_t knight_move_death2 = {FRAME_DEATHB_FIRST, FRAME_DEATHB_LAST, knight_frames_death2, knight_dead};


void knight_die(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
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
    SV_LinkEntity(self);

    if (frand() < 0.5f)
        self->monsterinfo.currentmove = &knight_move_death1;
    else
        self->monsterinfo.currentmove = &knight_move_death2;
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
    sound_pain  = SV_SoundIndex("nac_knight/khurt.wav");
    sound_die   = SV_SoundIndex("nac_knight/kdeath.wav");
    sound_idle  = SV_SoundIndex("nac_knight/idle.wav");
    sound_punch = SV_SoundIndex("nac_knight/sword1.wav");
    sound_search = SV_SoundIndex("nac_knight/idle.wav");
    sound_sight = SV_SoundIndex("nac_knight/ksight.wav");

    self->s.modelindex = SV_ModelIndex("models/monsters/knight/knight.iqm");
    VectorSet(self->mins, -16, -16, -0);
    VectorSet(self->maxs, 16, 16, 56);
    self->movetype = MOVETYPE_STEP;
    self->solid = SOLID_BBOX;

    self->health = 90;
    self->gib_health = -60;
    self->mass = 85;
    self->monsterinfo.aiflags |= AI_HIGH_TICK_RATE;

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

    SV_LinkEntity(self);

    walkmonster_start(self);
}
