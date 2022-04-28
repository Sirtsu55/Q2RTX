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
    ANIMATION(ATTACKA, 29),
    ANIMATION(LEAP_LOOP, 13),
    ANIMATION(LEAP_CLIMB, 23)
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
    vec3_t  aim = {self->monsterinfo.melee_distance, 0, 0};
    if (fire_hit(self, aim, 25, 5))
        SV_StartSound(self, CHAN_VOICE, sound_hit, 1, ATTN_NORM, 0);
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

void fiend_leap_wait(edict_t *self);

mmove_t fiend_move_leap_loop = {
    .firstframe = FRAME_LEAP_LOOP_FIRST,
    .lastframe = FRAME_LEAP_LOOP_LAST,
    .frame = (mframe_t [FRAME_LEAP_LOOP_COUNT]) {
        [0] = { .thinkfunc = fiend_leap_wait },
        [1] = { .thinkfunc = fiend_leap_wait },
        [2] = { .thinkfunc = fiend_leap_wait },
        [3] = { .thinkfunc = fiend_leap_wait },
        [4] = { .thinkfunc = fiend_leap_wait },
        [5] = { .thinkfunc = fiend_leap_wait },
        [6] = { .thinkfunc = fiend_leap_wait },
        [7] = { .thinkfunc = fiend_leap_wait },
        [8] = { .thinkfunc = fiend_leap_wait },
        [9] = { .thinkfunc = fiend_leap_wait },
        [10] = { .thinkfunc = fiend_leap_wait },
        [11] = { .thinkfunc = fiend_leap_wait },
        [12] = { .thinkfunc = fiend_leap_wait }
    },
    .default_aifunc = ai_move,
};

extern mmove_t fiend_move_leap;

static bool fiend_leap_check_ledge(edict_t *self, vec3_t wall_normal)
{
    // check that we're blocked in front of us
    vec3_t fwd;
    AngleVectors(self->s.angles, fwd, NULL, NULL);

    vec3_t end;
    VectorMA(self->s.origin, 12.f, fwd, end);

    trace_t tr = SV_Trace(self->s.origin, self->mins, self->maxs, end, NULL, MASK_SOLID);

    if (tr.fraction == 1.0f)
        return false;

    VectorCopy(tr.plane.normal, wall_normal);

    // check that the area a full bbox up + forward of us is free
    for (int i = 0; i < 3; i++)
        end[i] = self->s.origin[i] + (i == 2 ? (self->maxs[2] - self->mins[2]) : (self->size[i] * fwd[i]));

    tr = SV_Trace(end, self->mins, self->maxs, end, NULL, MASK_SOLID);

    if (tr.startsolid || tr.allsolid || tr.fraction != 1.0f)
        return false;

    // check that we're free above us
    end[0] = self->s.origin[0];
    end[1] = self->s.origin[1];

    tr = SV_Trace(end, self->mins, self->maxs, end, NULL, MASK_SOLID);

    if (tr.startsolid || tr.allsolid || tr.fraction != 1.0f)
        return false;

    return true;
}

#define FIEND_AIFLAG_CLIMB_WAIT_PUSH (1 << 30)

void fiend_leap_climb_push(edict_t *self)
{
    // already pushed
    if (!(self->monsterinfo.aiflags & FIEND_AIFLAG_CLIMB_WAIT_PUSH)) {
        return;
    }

    // check if we can move forward yet
    vec3_t fwd;
    AngleVectors(self->s.angles, fwd, NULL, NULL);

    vec3_t end;
    for (int i = 0; i < 3; i++)
        end[i] = self->s.origin[i] + (i == 2 ? 0 : (self->size[i] * fwd[i]));

    trace_t tr = SV_Trace(self->s.origin, self->mins, self->maxs, end, NULL, MASK_SOLID);

    if (tr.fraction == 1.f) {
        // good!
        self->monsterinfo.aiflags &= ~FIEND_AIFLAG_CLIMB_WAIT_PUSH;
        VectorMA(self->velocity, 300.f, fwd, self->velocity);

        vec3_t absmin, absmax;
        VectorAdd(end, self->mins, absmin);
        VectorAdd(end, self->maxs, absmax);

        // push away anything that is in front of us, just to ensure
        // we can get up here
        edict_t *ents[8];
        size_t num_ents = SV_AreaEdicts(absmin, absmax, ents, q_countof(ents), AREA_SOLID);

        // add some upwards momentum
        fwd[2] = 0.3f;
        VectorNormalize(fwd);

        for (size_t i = 0; i < num_ents; i++) {
            if (ents[i] == self) {
                continue;
            }

            T_Damage(ents[i], self, self, fwd, ents[i]->s.origin, fwd, 5, 180, DAMAGE_RADIUS, MOD_UNKNOWN);
        }
    }
}

void fiend_leap_climb_end(edict_t *self)
{
    self->monsterinfo.aiflags &= ~FIEND_AIFLAG_CLIMB_WAIT_PUSH;
    fiend_run(self);
}

mmove_t fiend_move_leap_climb = {
    .firstframe = FRAME_LEAP_CLIMB_FIRST,
    .lastframe = FRAME_LEAP_CLIMB_LAST,
    .frame = (mframe_t [FRAME_LEAP_CLIMB_COUNT]) {
        [16] = { .thinkfunc = fiend_attack_claw }
    },
    .thinkfunc = fiend_leap_climb_push,
    .endfunc = fiend_leap_climb_end
};

void fiend_leap_wait(edict_t *self)
{
    if (self->groundentity) {
        self->monsterinfo.nextframe = FRAME_LEAP_FIRST + 20;
        self->monsterinfo.currentmove = &fiend_move_leap;
    } else {
        vec3_t wall_normal;

        if (fiend_leap_check_ledge(self, wall_normal)) {
            self->touch = NULL;

            VectorInverse(wall_normal);
            vectoangles(wall_normal, self->s.angles);
            self->s.angles[0] = self->s.angles[2] = 0;

            self->monsterinfo.aiflags |= FIEND_AIFLAG_CLIMB_WAIT_PUSH;
            VectorSet(self->velocity, 0, 0, 300.f);

            self->monsterinfo.currentmove = &fiend_move_leap_climb;
        } else {
            self->monsterinfo.currentmove = &fiend_move_leap_loop;
        }
    }
}

void fiend_touch(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
    if (self->health <= 0) {
        self->touch = NULL;
        return;
    }

    if (other->takedamage && other == self->enemy) {
        vec3_t  point;
        vec3_t  normal;
        int     damage;

        VectorCopy(self->velocity, normal);
        VectorNormalize(normal);
        VectorMA(self->s.origin, self->maxs[0], normal, point);
        damage = 40 + 10 * random();
        T_Damage(other, self, self, self->velocity, point, normal, damage, damage, 0, MOD_UNKNOWN);
        self->touch = NULL;
    }

    M_CheckBottom(self);

    if (self->groundentity) {
        self->touch = NULL;
        VectorClear(self->velocity);
    }
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
        [3] = { .thinkfunc = fiend_leap },
        [4] = { .thinkfunc = fiend_leap_wait, .aifunc = ai_move }
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

    if (!solutions) {
        float diff = self->enemy->s.origin[2] - self->s.origin[2];

        // if we're above them and we have no solution, give them an extra boost
        if (diff > 24.f) {
            int solutions = solve_ballistic_arc1(self->s.origin, 750.f, self->enemy->s.origin, 800.f, low, high);

            if (!solutions)
                return;
        // if we're below them, just do a jump forward
        } else if (diff < -24.f) {
            solutions = 1;
            AngleVectors(self->s.angles, low, NULL, NULL);
            VectorScale(low, 600.f, low);
        }
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
    self->touch = NULL;
    SV_LinkEntity(self);

    self->monsterinfo.currentmove = &fiend_move_death;
}

static bool fiend_inititalized = false;

void fiend_load(edict_t *self)
{
    if (fiend_inititalized)
        return;

    m_iqm_t *iqm = M_InitializeIQM(ASSET_MODEL_FIEND);

    M_SetupIQMAnimations(iqm, (mmove_t *[]) {
        &fiend_move_stand, &fiend_move_walk, &fiend_move_run1, /*&fiend_move_leap,*/ &fiend_move_pain,
        &fiend_move_death, &fiend_move_attack_claw, NULL
    });

    M_FreeIQM(iqm);

    fiend_inititalized = true;
}

/*QUAKED monster_vore (1 .5 0) (-16 -16 -24) (16 16 32) Ambush Trigger_Spawn Sight
*/
void SP_monster_fiend(edict_t *self)
{
    if (deathmatch.integer) {
        G_FreeEdict(self);
        return;
    }

    // pre-caches
    sound_pain  = SV_SoundIndex(ASSET_SOUND_FIEND_PAIN);
    sound_die   = SV_SoundIndex(ASSET_SOUND_FIEND_DIE);
    sound_idle  = SV_SoundIndex(ASSET_SOUND_FIEND_IDLE);
    sound_hit   = SV_SoundIndex(ASSET_SOUND_FIEND_HIT);
    sound_jump  = SV_SoundIndex(ASSET_SOUND_FIEND_JUMP);
    sound_land  = SV_SoundIndex(ASSET_SOUND_FIEND_LAND);
    sound_sight = SV_SoundIndex(ASSET_SOUND_FIEND_SIGHT);

    self->s.modelindex = SV_ModelIndex(ASSET_MODEL_FIEND);
    VectorSet(self->mins, -16, -16, -0);
    VectorSet(self->maxs, 16, 16, 56);
    self->movetype = MOVETYPE_STEP;
    self->solid = SOLID_BBOX;

    self->health = 300;
    self->gib_health = -60;
    self->mass = 145;
    self->monsterinfo.aiflags |= AI_HIGH_TICK_RATE;
    self->yaw_speed = 40;
    self->viewheight = 48;

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

    self->monsterinfo.melee_distance = 100;

    SV_LinkEntity(self);

    walkmonster_start(self);
}
