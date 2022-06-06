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
// g_phys.c

#include "g_local.h"

/*


pushmove objects do not obey gravity, and do not interact with each other or trigger fields, but block normal movement and push normal objects when they move.

onground is set for toss objects when they come to a complete rest.  it is set for steping or walking objects

doors, plats, etc are SOLID_BSP, and MOVETYPE_PUSH
bonus items are SOLID_TRIGGER touch, and MOVETYPE_TOSS
corpses are SOLID_NOT and MOVETYPE_TOSS
crates are SOLID_BBOX and MOVETYPE_TOSS
walking monsters are SOLID_SLIDEBOX and MOVETYPE_STEP
flying/floating monsters are SOLID_SLIDEBOX and MOVETYPE_FLY

solid_edge items only clip against bsp models.

*/


/*
============
SV_TestEntityPosition

============
*/
static edict_t *SV_TestEntityPosition(edict_t *ent)
{
    trace_t trace;
    int     mask;

    if (ent->clipmask)
        mask = ent->clipmask;
    else
        mask = MASK_SOLID;
    trace = SV_Trace(ent->s.origin, ent->mins, ent->maxs, ent->s.origin, ent, mask);

    if (trace.startsolid)
        return game.world;

    return NULL;
}


/*
================
SV_CheckVelocity
================
*/
static void SV_CheckVelocity(edict_t *ent)
{
//
// bound velocity
//
    float len = VectorLength(ent->velocity);

    if (len > sv_maxvelocity.value) {
        VectorScale(ent->velocity, (1.0f / len) * sv_maxvelocity.value, ent->velocity);
    }
}

/*
=============
SV_RunThink

Runs thinking code for this frame if necessary
=============
*/
static bool SV_RunThink(edict_t *ent)
{
    gtime_t thinktime = ent->nextthink;
    if (thinktime <= 0)
        return true;
    if (thinktime > level.time)
        return true;

    ent->nextthink = 0;
    if (!ent->think)
        Com_Error(ERR_DROP, "NULL ent->think");
    ent->think(ent);

    return false;
}

/*
==================
SV_Impact

Two entities have touched, so run their touch functions
==================
*/
void SV_Impact(edict_t *e1, trace_t *trace)
{
    edict_t     *e2;
//  cplane_t    backplane;

    e2 = trace->ent;

    if (e1->touch && e1->solid != SOLID_NOT)
        e1->touch(e1, e2, &trace->plane, trace->surface);

    if (e2->touch && e2->solid != SOLID_NOT)
        e2->touch(e2, e1, NULL, NULL);
}

/*
============
SV_FlyMove

The basic solid body movement clip that slides along multiple planes
Returns the clipflags if the velocity was modified (hit something solid)
1 = floor
2 = wall / step
4 = dead stop
============
*/
typedef struct {
    int mask;
    edict_t *self;
} fly_move_args;

void SV_FlyMove_Trace(trace_t *tr, vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, void *arg)
{
    fly_move_args *args = arg;
    *tr = SV_Trace(start, mins, maxs, end, args->self, args->mask);
}

bool SV_FlyMove_Impact(trace_t *tr, void *arg)
{
    fly_move_args *args = arg;
    SV_Impact(args->self, tr);
    return !args->self->inuse; // removed by the impact function?
}

static inline void SV_FlyMove(edict_t *ent, float time, int mask)
{
    fly_move_args args = { mask, ent };
    SlideMove(ent->s.origin, ent->mins, ent->maxs, ent->velocity, time, false, SV_FlyMove_Trace, SV_FlyMove_Impact, &args);
}


/*
============
SV_AddGravity

============
*/
static void SV_AddGravity(edict_t *ent)
{
    ent->velocity[2] -= ent->gravity * level.gravity * BASE_FRAMETIME_S;
}

/*
===============================================================================

PUSHMOVE

===============================================================================
*/

/*
============
SV_PushEntity

Does not change the entities velocity at all
============
*/
static trace_t SV_PushEntity(edict_t *ent, vec3_t push)
{
    trace_t trace;
    vec3_t  start;
    vec3_t  end;
    int     mask;

    VectorCopy(ent->s.origin, start);
    VectorAdd(start, push, end);

retry:
    if (ent->clipmask)
        mask = ent->clipmask;
    else
        mask = MASK_SOLID;

    trace = SV_Trace(start, ent->mins, ent->maxs, end, ent, mask);

    VectorCopy(trace.endpos, ent->s.origin);
    SV_LinkEntity(ent);

    if (trace.fraction != 1.0f) {
        SV_Impact(ent, &trace);

        // if the pushed entity went away and the pusher is still there
        if (!trace.ent->inuse && ent->inuse) {
            // move the pusher back and try again
            VectorCopy(start, ent->s.origin);
            SV_LinkEntity(ent);
            goto retry;
        }
    }

    if (ent->inuse)
        G_TouchTriggers(ent);

    return trace;
}


typedef struct {
    edict_t *ent;
    vec3_t  origin;
    vec3_t  angles;
#if USE_SMOOTH_DELTA_ANGLES
    int     deltayaw;
#endif
} pushed_t;
pushed_t    pushed[MAX_EDICTS], *pushed_p;

edict_t *obstacle;

/*
============
SV_Push

Objects need to be moved back on a failed push,
otherwise riders would continue to slide.
============
*/
static bool SV_Push(edict_t *pusher, vec3_t move, vec3_t amove)
{
    int         i;
    edict_t     *check, *block;
    vec3_t      mins, maxs;
    pushed_t    *p;
    vec3_t      org, org2, move2, forward, right, up;

    // clamp the move to COORDSCALE units, so the position will
    // be accurate for client side prediction
    for (i = 0 ; i < 3 ; i++) {
        float   temp = COORD2SHORT(move[i]);
        if (temp > 0.f)
            temp += 0.5f;
        else if (temp < 0.f)
            temp -= 0.5f;
        move[i] = SHORT2COORD((int)temp);
    }

    // find the bounding box
    for (i = 0 ; i < 3 ; i++) {
        mins[i] = pusher->absmin[i] + move[i];
        maxs[i] = pusher->absmax[i] + move[i];
    }

// we need this for pushing things later
    VectorNegate(amove, org);
    AngleVectors(org, forward, right, up);

// save the pusher's original position
    pushed_p->ent = pusher;
    VectorCopy(pusher->s.origin, pushed_p->origin);
    VectorCopy(pusher->s.angles, pushed_p->angles);
#if USE_SMOOTH_DELTA_ANGLES
    if (pusher->client)
        pushed_p->deltayaw = pusher->client->ps.pmove.delta_angles[YAW];
#endif
    pushed_p++;

// move the pusher to it's final position
    VectorAdd(pusher->s.origin, move, pusher->s.origin);
    VectorAdd(pusher->s.angles, amove, pusher->s.angles);
    SV_LinkEntity(pusher);

// see if any solid entities are inside the final position
    for (check = G_NextEnt(game.world); check; check = G_NextEnt(check)) {
        if (check->movetype == MOVETYPE_PUSH
            || check->movetype == MOVETYPE_STOP
            || check->movetype == MOVETYPE_NONE
            || check->movetype == MOVETYPE_NOCLIP)
            continue;

        if (!SV_EntityLinked(check))
            continue;       // not linked in anywhere

        // if the entity is standing on the pusher, it will definitely be moved
        if (check->groundentity != pusher) {
            // see if the ent needs to be tested
            if (check->absmin[0] >= maxs[0]
                || check->absmin[1] >= maxs[1]
                || check->absmin[2] >= maxs[2]
                || check->absmax[0] <= mins[0]
                || check->absmax[1] <= mins[1]
                || check->absmax[2] <= mins[2])
                continue;

            // see if the ent's bbox is inside the pusher's final position
            if (!SV_TestEntityPosition(check))
                continue;
        }

        if ((pusher->movetype == MOVETYPE_PUSH) || (check->groundentity == pusher)) {
            // move this entity
            pushed_p->ent = check;
            VectorCopy(check->s.origin, pushed_p->origin);
            VectorCopy(check->s.angles, pushed_p->angles);
#if USE_SMOOTH_DELTA_ANGLES
            if (check->client)
                pushed_p->deltayaw = check->client->ps.pmove.delta_angles[YAW];
#endif
            pushed_p++;

            // try moving the contacted entity
            VectorAdd(check->s.origin, move, check->s.origin);
#if USE_SMOOTH_DELTA_ANGLES
            if (check->client) {
                // FIXME: doesn't rotate monsters?
                // FIXME: skuller: needs client side interpolation
                check->client->ps.pmove.delta_angles[YAW] += ANGLE2SHORT(amove[YAW]);
            }
#endif

            // figure movement due to the pusher's amove
            VectorSubtract(check->s.origin, pusher->s.origin, org);
            org2[0] = DotProduct(org, forward);
            org2[1] = -DotProduct(org, right);
            org2[2] = DotProduct(org, up);
            VectorSubtract(org2, org, move2);
            VectorAdd(check->s.origin, move2, check->s.origin);

            // may have pushed them off an edge
            if (check->groundentity != pusher)
                check->groundentity = NULL;

            block = SV_TestEntityPosition(check);
            if (!block) {
                // pushed ok
                SV_LinkEntity(check);
                // impact?
                continue;
            }

            // if it is ok to leave in the old position, do it
            // this is only relevent for riding entities, not pushed
            // FIXME: this doesn't acount for rotation
            VectorSubtract(check->s.origin, move, check->s.origin);
            block = SV_TestEntityPosition(check);
            if (!block) {
                pushed_p--;
                continue;
            }
        }

        // save off the obstacle so we can call the block function
        obstacle = check;

        // move back any entities we already moved
        // go backwards, so if the same entity was pushed
        // twice, it goes back to the original position
        for (p = pushed_p - 1 ; p >= pushed ; p--) {
            VectorCopy(p->origin, p->ent->s.origin);
            VectorCopy(p->angles, p->ent->s.angles);
#if USE_SMOOTH_DELTA_ANGLES
            if (p->ent->client) {
                p->ent->client->ps.pmove.delta_angles[YAW] = p->deltayaw;
            }
#endif
            SV_LinkEntity(p->ent);
        }
        return false;
    }

//FIXME: is there a better way to handle this?
    // see if anything we moved has touched a trigger
    for (p = pushed_p - 1 ; p >= pushed ; p--)
        G_TouchTriggers(p->ent);

    return true;
}

/*
================
SV_Physics_Pusher

Bmodel objects don't interact with each other, but
push all box objects
================
*/
static void SV_Physics_Pusher(edict_t *ent)
{
    vec3_t      move, amove;
    edict_t     *part, *mv;

    // if not a team captain, so movement will be handled elsewhere
    if (ent->flags & FL_TEAMSLAVE)
        return;

    // make sure all team slaves can move before commiting
    // any moves or calling any think functions
    // if the move is blocked, all moved objects will be backed out
//retry:
    pushed_p = pushed;
    for (part = ent ; part ; part = part->teamchain) {
        if (!VectorEmpty(part->velocity) || !VectorEmpty(part->avelocity)) {
            // object is moving
            VectorScale(part->velocity, BASE_FRAMETIME_S, move);
            VectorScale(part->avelocity, BASE_FRAMETIME_S, amove);

            if (!SV_Push(part, move, amove))
                break;  // move was blocked
        }
    }
    if (pushed_p > &pushed[MAX_EDICTS])
        Com_Error(ERR_DROP, "pushed_p > &pushed[MAX_EDICTS], memory corrupted");

    if (part) {
        // the move failed, bump all nextthink times and back out moves
        for (mv = ent ; mv ; mv = mv->teamchain) {
            if (mv->nextthink > 0)
                mv->nextthink += BASE_FRAMETIME;
        }

        // if the pusher has a "blocked" function, call it
        // otherwise, just stay in place until the obstacle is gone
        if (part->blocked)
            part->blocked(part, obstacle);
#if 0
        // if the pushed entity went away and the pusher is still there
        if (!obstacle->inuse && part->inuse)
            goto retry;
#endif
    } else {
        // the move succeeded, so call all think functions
        for (part = ent ; part ; part = part->teamchain) {
            SV_RunThink(part);
        }
    }
}

//==================================================================

/*
=============
SV_Physics_None

Non moving objects can only think
=============
*/
static void SV_Physics_None(edict_t *ent)
{
// regular thinking
    SV_RunThink(ent);
}

/*
=============
SV_Physics_Noclip

A moving object that doesn't obey physics
=============
*/
static void SV_Physics_Noclip(edict_t *ent)
{
// regular thinking
    SV_RunThink(ent);

    if (!ent->inuse)
        return;

    VectorMA(ent->s.angles, BASE_FRAMETIME_S, ent->avelocity, ent->s.angles);
    VectorMA(ent->s.origin, BASE_FRAMETIME_S, ent->velocity, ent->s.origin);

    SV_LinkEntity(ent);
}

/*
==============================================================================

TOSS / BOUNCE

==============================================================================
*/

/*
=============
SV_Physics_Toss

Toss, bounce, and fly movement.  When onground, do nothing.
=============
*/
static void SV_Physics_Toss(edict_t *ent)
{
    trace_t     trace;
    vec3_t      move;
    float       backoff;
    edict_t     *slave;
    int         wasinwater;
    int         isinwater;
    vec3_t      old_origin;

// regular thinking
    SV_RunThink(ent);
    if (!ent->inuse)
        return;

    // if not a team captain, so movement will be handled elsewhere
    if (ent->flags & FL_TEAMSLAVE)
        return;

    if (ent->velocity[2] > 0)
        ent->groundentity = NULL;

// check for the groundentity going away
    if (ent->groundentity)
        if (!ent->groundentity->inuse)
            ent->groundentity = NULL;

// if onground, return without moving
    if (ent->groundentity)
        return;

    VectorCopy(ent->s.origin, old_origin);

    SV_CheckVelocity(ent);

// add gravity
    if (ent->movetype != MOVETYPE_FLY
        && ent->movetype != MOVETYPE_FLYMISSILE)
        SV_AddGravity(ent);

// move angles
    VectorMA(ent->s.angles, BASE_FRAMETIME_S, ent->avelocity, ent->s.angles);

// move origin
    VectorScale(ent->velocity, BASE_FRAMETIME_S, move);
    trace = SV_PushEntity(ent, move);
    if (!ent->inuse)
        return;

    if (trace.fraction < 1) {
        if (ent->movetype == MOVETYPE_BOUNCE)
            backoff = 0.5f;
        else
            backoff = 0.25f;

        ClipVelocity(ent->velocity, trace.plane.normal, ent->velocity, backoff);
        
        // stop if on ground
        if (trace.plane.normal[2] > 0.7f || trace.allsolid) {
            if (ent->velocity[2] < 60/* || ent->movetype != MOVETYPE_BOUNCE*/) {
                ent->groundentity = trace.ent;
                ent->groundentity_linkcount = trace.ent->linkcount;
                VectorClear(ent->velocity);
                VectorClear(ent->avelocity);
            }
        }

//      if (ent->touch)
//          ent->touch (ent, trace.ent, &trace.plane, trace.surface);
    }

// check for water transition
    wasinwater = (ent->watertype & MASK_WATER);
    ent->watertype = SV_PointContents(ent->s.origin);
    isinwater = ent->watertype & MASK_WATER;

    if (isinwater)
        ent->waterlevel = 1;
    else
        ent->waterlevel = 0;

    if (!wasinwater && isinwater)
        SV_PositionedSound(old_origin, game.world, CHAN_AUTO, SV_SoundIndex(ASSET_SOUND_WATER_IMPACT), 1, ATTN_NORM, 0);
    else if (wasinwater && !isinwater)
        SV_PositionedSound(ent->s.origin, game.world, CHAN_AUTO, SV_SoundIndex(ASSET_SOUND_WATER_IMPACT), 1, ATTN_NORM, 0);

// move teamslaves
    for (slave = ent->teamchain; slave; slave = slave->teamchain) {
        VectorCopy(ent->s.origin, slave->s.origin);
        SV_LinkEntity(slave);
    }
}

/*
===============================================================================

STEPPING MOVEMENT

===============================================================================
*/

/*
=============
SV_Physics_Step

Monsters freefall when they don't have a ground entity, otherwise
all movement is done with discrete steps.

This is also used for objects that have become still on the ground, but
will fall if the floor is pulled out from under them.
FIXME: is this true?
=============
*/

//FIXME: hacked in for E3 demo
#define sv_stopspeed        100
#define sv_friction         6
#define sv_waterfriction    1

static void SV_AddRotationalFriction(edict_t *ent)
{
    int     n;
    float   adjustment;

    VectorMA(ent->s.angles, BASE_FRAMETIME_S, ent->avelocity, ent->s.angles);
    adjustment = BASE_FRAMETIME_S * sv_stopspeed * sv_friction;
    for (n = 0; n < 3; n++) {
        if (ent->avelocity[n] > 0) {
            ent->avelocity[n] -= adjustment;
            if (ent->avelocity[n] < 0)
                ent->avelocity[n] = 0;
        } else {
            ent->avelocity[n] += adjustment;
            if (ent->avelocity[n] > 0)
                ent->avelocity[n] = 0;
        }
    }
}

static void SV_Physics_Step(edict_t *ent)
{
    bool        wasonground;
    bool        hitsound = false;
    float       *vel;
    float       speed, newspeed, control;
    float       friction;
    edict_t     *groundentity;
    int         mask;

    // airborn monsters should always check for ground
    if (!ent->groundentity)
        M_CheckGround(ent);

    groundentity = ent->groundentity;

    SV_CheckVelocity(ent);

    if (groundentity)
        wasonground = true;
    else
        wasonground = false;

    if (!VectorEmpty(ent->avelocity))
        SV_AddRotationalFriction(ent);

    // add gravity except:
    //   flying monsters
    //   swimming monsters who are in the water
    if (! wasonground)
        if (!(ent->flags & FL_FLY))
            if (!((ent->flags & FL_SWIM) && (ent->waterlevel > 2))) {
                if (ent->velocity[2] < level.gravity * -0.1f)
                    hitsound = true;
                if (ent->waterlevel == 0)
                    SV_AddGravity(ent);
            }

    // friction for flying monsters that have been given vertical velocity
    if ((ent->flags & FL_FLY) && (ent->velocity[2] != 0)) {
        speed = fabsf(ent->velocity[2]);
        control = speed < sv_stopspeed ? sv_stopspeed : speed;
        friction = sv_friction / 3;
        newspeed = speed - (BASE_FRAMETIME_S * control * friction);
        if (newspeed < 0)
            newspeed = 0;
        newspeed /= speed;
        ent->velocity[2] *= newspeed;
    }

    // friction for flying monsters that have been given vertical velocity
    if ((ent->flags & FL_SWIM) && (ent->velocity[2] != 0)) {
        speed = fabsf(ent->velocity[2]);
        control = speed < sv_stopspeed ? sv_stopspeed : speed;
        newspeed = speed - (BASE_FRAMETIME_S * control * sv_waterfriction * ent->waterlevel);
        if (newspeed < 0)
            newspeed = 0;
        newspeed /= speed;
        ent->velocity[2] *= newspeed;
    }

    if (ent->velocity[2] || ent->velocity[1] || ent->velocity[0]) {
        // apply friction
        // let dead monsters who aren't completely onground slide
        if ((wasonground) || (ent->flags & (FL_SWIM | FL_FLY)))
            if (!(ent->health <= 0.0f && !M_CheckBottom(ent))) {
                vel = ent->velocity;
                speed = sqrtf(vel[0] * vel[0] + vel[1] * vel[1]);
                if (speed) {
                    friction = sv_friction;

                    control = speed < sv_stopspeed ? sv_stopspeed : speed;
                    newspeed = speed - BASE_FRAMETIME_S * control * friction;

                    if (newspeed < 0)
                        newspeed = 0;
                    newspeed /= speed;

                    vel[0] *= newspeed;
                    vel[1] *= newspeed;
                }
            }

        if (ent->svflags & SVF_MONSTER)
            mask = MASK_MONSTERSOLID;
        else
            mask = MASK_SOLID;
        SV_FlyMove(ent, BASE_FRAMETIME_S, mask);
        if (!ent->groundentity)
            M_CheckGround(ent);

        SV_LinkEntity(ent);
        G_TouchTriggers(ent);
        if (!ent->inuse)
            return;

        if (ent->groundentity)
            if (!wasonground)
                if (hitsound)
                    SV_StartSound(ent, 0, SV_SoundIndex(ASSET_SOUND_MONSTER_LAND), 1, ATTN_NORM, 0);
    }

// regular thinking
    SV_RunThink(ent);
}

// Paril - generic entity animation
static void G_RunAnimation(edict_t *ent)
{
    if (!ent->anim.animating)
        return;

    // time to move to next frame
    if (!ent->anim.next_frame) {
        if (ent->s.frame + 1 > ent->anim.end) {

            if (ent->anim.count) {
                if (!--ent->anim.count_left) {
                    ent->anim.animating = false;
                    char *old_target = ent->target;
                    ent->target = ent->anim.finished_target;
                    G_UseTargets(ent, ent->activator);
                    ent->target = old_target;
                    return;
                }
            } else {
                char *old_target = ent->target;
                ent->target = ent->anim.finished_target;
                G_UseTargets(ent, ent->activator);
                ent->target = old_target;
            }

            ent->s.frame = ent->anim.start;
        }
        else
            ent->s.frame++;

        ent->anim.next_frame = ent->anim.frame_delay;
    } else {
        ent->anim.next_frame--;
    }
}

// Paril - call to "enable" animations on entity
void G_InitAnimation(edict_t *ent)
{
    ent->anim.is_active = true;
    ent->s.frame = ent->anim.start;
    ent->anim.next_frame = ent->anim.frame_delay;
    ent->anim.count_left = ent->anim.count;
}

//============================================================================
/*
================
G_RunEntity

================
*/
void G_RunEntity(edict_t *ent)
{
    if (ent->prethink)
        ent->prethink(ent);

    switch ((int)ent->movetype) {
    case MOVETYPE_PUSH:
    case MOVETYPE_STOP:
        SV_Physics_Pusher(ent);
        break;
    case MOVETYPE_NONE:
        SV_Physics_None(ent);
        break;
    case MOVETYPE_NOCLIP:
        SV_Physics_Noclip(ent);
        break;
    case MOVETYPE_STEP:
        SV_Physics_Step(ent);
        break;
    case MOVETYPE_TOSS:
    case MOVETYPE_BOUNCE:
    case MOVETYPE_FLY:
    case MOVETYPE_FLYMISSILE:
        SV_Physics_Toss(ent);
        break;
    default:
        Com_Errorf(ERR_DROP, "SV_Physics: bad movetype %i", (int)ent->movetype);
    }

    // Paril - generic entity animation
    if (ent->anim.is_active) {
        G_RunAnimation(ent);
    }
}
