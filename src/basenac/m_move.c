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
// m_move.c -- monster movement

#include "g_local.h"

/*
=============
M_CheckBottom

Returns false if any part of the bottom of the entity is off an edge that
is not a staircase.

=============
*/
int c_yes, c_no;

bool M_CheckBottom(edict_t *ent)
{
    vec3_t  mins, maxs, start, stop;
    trace_t trace;
    int     x, y;
    float   mid, bottom;

    VectorAdd(ent->s.origin, ent->mins, mins);
    VectorAdd(ent->s.origin, ent->maxs, maxs);

// if all of the points under the corners are solid world, don't bother
// with the tougher checks
// the corners must be within 16 of the midpoint
    start[2] = mins[2] - 1;
    for (x = 0 ; x <= 1 ; x++)
        for (y = 0 ; y <= 1 ; y++) {
            start[0] = x ? maxs[0] : mins[0];
            start[1] = y ? maxs[1] : mins[1];
            if (SV_PointContents(start) != CONTENTS_SOLID)
                goto realcheck;
        }

    c_yes++;
    return true;        // we got out easy

realcheck:
    c_no++;
//
// check it for real...
//
    start[2] = mins[2];

// the midpoint must be within 16 of the bottom
    start[0] = stop[0] = (mins[0] + maxs[0]) * 0.5f;
    start[1] = stop[1] = (mins[1] + maxs[1]) * 0.5f;
    stop[2] = start[2] - 2 * STEPSIZE;
    trace = SV_Trace(start, vec3_origin, vec3_origin, stop, ent, MASK_MONSTERSOLID);

    if (trace.fraction == 1.0f)
        return false;
    mid = bottom = trace.endpos[2];

// the corners must be within 16 of the midpoint
    for (x = 0 ; x <= 1 ; x++)
        for (y = 0 ; y <= 1 ; y++) {
            start[0] = stop[0] = x ? maxs[0] : mins[0];
            start[1] = stop[1] = y ? maxs[1] : mins[1];

            trace = SV_Trace(start, vec3_origin, vec3_origin, stop, ent, MASK_MONSTERSOLID);

            if (trace.fraction != 1.0f && trace.endpos[2] > bottom)
                bottom = trace.endpos[2];
            if (trace.fraction == 1.0f || mid - trace.endpos[2] > STEPSIZE)
                return false;
        }

    c_yes++;
    return true;
}

static void SV_movestep_StepSlideMoveTrace (trace_t *tr, vec3_t origin, vec3_t mins, vec3_t maxs, vec3_t end, void *arg)
{
    edict_t *ent = arg;
    *tr = SV_Trace(origin, mins, maxs, end, ent, MASK_MONSTERSOLID);
}

static bool SV_movestep_StepSlideMoveImpact (trace_t *ent, void *arg)
{
    edict_t *self = arg;
    SV_Impact(self, ent);
    return false;
}

/*
=============
SV_movestep

Called by monster program code.
The move will be adjusted for slopes and stairs, but if the move isn't
possible, no move is done, false is returned
=============
*/
bool SV_movestep(edict_t *ent, vec3_t move, bool relink)
{
    vec3_t oldorg;
    VectorCopy(ent->s.origin, oldorg);

    // flying monsters don't step up
    if (ent->flags & (FL_SWIM | FL_FLY)) {
        vec3_t neworg;

        // try the move
        VectorAdd(ent->s.origin, move, neworg);

        // try one move with vertical motion, then one without
        for (int i = 0 ; i < 2 ; i++) {
            VectorAdd(ent->s.origin, move, neworg);
            if (i == 0 && ent->enemy) {
                if (!ent->goalentity)
                    ent->goalentity = ent->enemy;
                float dz = ent->s.origin[2] - ent->goalentity->s.origin[2];
                if (ent->goalentity->client) {
                    if (dz > 120)
                        neworg[2] -= 4;
                    if (!((ent->flags & FL_SWIM) && (ent->waterlevel < 2)))
                        if (dz < 30)
                            neworg[2] += 4;
                } else {
                    if (dz > 4)
                        neworg[2] -= 4;
                    else if (dz > 0)
                        neworg[2] -= dz;
                    else if (dz < -4)
                        neworg[2] += 4;
                    else
                        neworg[2] += dz;
                }
            }
            trace_t trace = SV_Trace(ent->s.origin, ent->mins, ent->maxs, neworg, ent, MASK_MONSTERSOLID);

            // fly monsters don't enter water voluntarily
            if (ent->flags & FL_FLY) {
                if (!ent->waterlevel) {
                    vec3_t test;
                    test[0] = trace.endpos[0];
                    test[1] = trace.endpos[1];
                    test[2] = trace.endpos[2] + ent->mins[2] + 1;
                    int contents = SV_PointContents(test);
                    if (contents & MASK_WATER)
                        return false;
                }
            }

            // swim monsters don't exit water voluntarily
            if (ent->flags & FL_SWIM) {
                if (ent->waterlevel < 2) {
                    vec3_t test;
                    test[0] = trace.endpos[0];
                    test[1] = trace.endpos[1];
                    test[2] = trace.endpos[2] + ent->mins[2] + 1;
                    int contents = SV_PointContents(test);
                    if (!(contents & MASK_WATER))
                        return false;
                }
            }

            if (trace.fraction == 1) {
                VectorCopy(trace.endpos, ent->s.origin);
                if (relink) {
                    SV_LinkEntity(ent);
                    G_TouchTriggers(ent);
                }
                return true;
            }

            if (!ent->enemy)
                break;
        }

        return false;
    }

    vec3_t neworg;
    VectorCopy(ent->s.origin, neworg);

    trace_t tr;
    if (!StepSlideMove(neworg, ent->mins, ent->maxs, move, 1.0f, false, !!ent->groundentity, SV_movestep_StepSlideMoveTrace, SV_movestep_StepSlideMoveImpact, ent, &tr)) {
        return false;
    }

    VectorCopy(neworg, ent->s.origin);
    
    M_CatagorizePosition(ent);

    VectorCopy(oldorg, ent->s.origin);

    // don't go under water
    if (ent->waterlevel == 3) {
        return false;
    }

    if (tr.fraction == 1) {
        // if monster had the ground pulled out, go ahead and fall
        if (ent->flags & FL_PARTIALGROUND) {
            VectorAdd(ent->s.origin, move, ent->s.origin);
            if (relink) {
                SV_LinkEntity(ent);
                G_TouchTriggers(ent);
            }
            ent->groundentity = NULL;
            return true;
        }

        return false;       // walked off an edge
    }

// check point traces down for dangling corners
    VectorCopy(neworg, ent->s.origin);

    if (!M_CheckBottom(ent)) {
        if (ent->flags & FL_PARTIALGROUND) {
            // entity had floor mostly pulled out from underneath it
            // and is trying to correct
            if (relink) {
                SV_LinkEntity(ent);
                G_TouchTriggers(ent);
            }
            return true;
        }
        VectorCopy(oldorg, ent->s.origin);
        ent->monsterinfo.aiflags |= AI_JUMP_IMMEDIATELY;
        return false;
    }

    if (ent->flags & FL_PARTIALGROUND) {
        ent->flags &= ~FL_PARTIALGROUND;
    }
    ent->groundentity = tr.ent;
    ent->groundentity_linkcount = tr.ent->linkcount;

// the move is ok
    if (relink) {
        SV_LinkEntity(ent);
        G_TouchTriggers(ent);
    }
    return true;
}


//============================================================================

/*
===============
M_ChangeYaw

===============
*/
void M_ChangeYaw(edict_t *ent)
{
    float   ideal;
    float   current;
    float   move;
    float   speed;

    current = anglemod(ent->s.angles[YAW]);
    ideal = ent->ideal_yaw;

    if (current == ideal)
        return;

    move = ideal - current;
    speed = ent->yaw_speed;
    if (ideal > current) {
        if (move >= 180)
            move = move - 360;
    } else {
        if (move <= -180)
            move = move + 360;
    }
    if (move > 0) {
        if (move > speed)
            move = speed;
    } else {
        if (move < -speed)
            move = -speed;
    }

    ent->s.angles[YAW] = anglemod(current + move);
}


/*
======================
SV_StepDirection

Turns to the movement direction, and walks the current distance if
facing it.

======================
*/
bool SV_StepDirection(edict_t *ent, float yaw, float dist)
{
    vec3_t      move, oldorigin;
    float       delta;

    ent->ideal_yaw = yaw;
    M_ChangeYaw(ent);

    yaw = DEG2RAD(yaw);
    move[0] = cos(yaw) * dist;
    move[1] = sin(yaw) * dist;
    move[2] = 0;

    VectorCopy(ent->s.origin, oldorigin);
    if (SV_movestep(ent, move, false)) {

        // if we didn't move at least a 16th of the distance,
        // we're probably stuck
        if (!dist || VectorDistance(ent->s.origin, oldorigin) >= dist * (1.f / 16.f)) {
            delta = ent->s.angles[YAW] - ent->ideal_yaw;
            if (delta > 45 && delta < 315) {
                // not turned far enough, so don't take the step
                VectorCopy(oldorigin, ent->s.origin);
            }
            SV_LinkEntity(ent);
            G_TouchTriggers(ent);
            return true;
        }
    }
    SV_LinkEntity(ent);
    G_TouchTriggers(ent);
    return false;
}

/*
======================
SV_FixCheckBottom

======================
*/
void SV_FixCheckBottom(edict_t *ent)
{
    ent->flags |= FL_PARTIALGROUND;
}



/*
================
SV_NewChaseDir

================
*/
#define DI_NODIR    -1
void SV_NewChaseDir(edict_t *actor, edict_t *enemy, float dist)
{
    float   deltax, deltay;
    float   d[3];
    float   tdir, olddir, turnaround;

    //FIXME: how did we get here with no enemy
    if (!enemy)
        return;

    olddir = anglemod((int)(actor->ideal_yaw / 45) * 45);
    turnaround = anglemod(olddir - 180);

    deltax = enemy->s.origin[0] - actor->s.origin[0];
    deltay = enemy->s.origin[1] - actor->s.origin[1];
    if (deltax > 10)
        d[1] = 0;
    else if (deltax < -10)
        d[1] = 180;
    else
        d[1] = DI_NODIR;
    if (deltay < -10)
        d[2] = 270;
    else if (deltay > 10)
        d[2] = 90;
    else
        d[2] = DI_NODIR;

// try direct route
    if (d[1] != DI_NODIR && d[2] != DI_NODIR) {
        if (d[1] == 0)
            tdir = d[2] == 90 ? 45 : 315;
        else
            tdir = d[2] == 90 ? 135 : 215;

        if (tdir != turnaround && SV_StepDirection(actor, tdir, dist))
            return;
    }

// try other directions
    if (((Q_rand() & 3) & 1) || fabsf(deltay) > fabsf(deltax)) {
        tdir = d[1];
        d[1] = d[2];
        d[2] = tdir;
    }

    if (d[1] != DI_NODIR && d[1] != turnaround
        && SV_StepDirection(actor, d[1], dist))
        return;

    if (d[2] != DI_NODIR && d[2] != turnaround
        && SV_StepDirection(actor, d[2], dist))
        return;

    /* there is no direct path to the player, so pick another direction */

    if (olddir != DI_NODIR && SV_StepDirection(actor, olddir, dist))
        return;

    if (Q_rand() & 1) { /*randomly determine direction of search*/
        for (tdir = 0 ; tdir <= 315 ; tdir += 45)
            if (tdir != turnaround && SV_StepDirection(actor, tdir, dist))
                return;
    } else {
        for (tdir = 315 ; tdir >= 0 ; tdir -= 45)
            if (tdir != turnaround && SV_StepDirection(actor, tdir, dist))
                return;
    }

    if (turnaround != DI_NODIR && SV_StepDirection(actor, turnaround, dist))
        return;

    actor->ideal_yaw = olddir;      // can't move

// if a bridge was pulled out from underneath a monster, it may not have
// a valid standing position at all

    if (!M_CheckBottom(actor))
        SV_FixCheckBottom(actor);
}

/*
======================
SV_CloseEnough

======================
*/
bool SV_CloseEnough(edict_t *ent, edict_t *goal, float dist)
{
    int     i;

    for (i = 0 ; i < 3 ; i++) {
        if (goal->absmin[i] > ent->absmax[i] + dist)
            return false;
        if (goal->absmax[i] < ent->absmin[i] - dist)
            return false;
    }
    return true;
}


/*
======================
M_MoveToGoal
======================
*/
void M_MoveToGoal(edict_t *ent, float dist)
{
    edict_t     *goal;

    goal = ent->goalentity;

    if (!ent->groundentity && !(ent->flags & (FL_FLY | FL_SWIM)))
        return;

// if the next step hits the enemy, return immediately
    if (ent->enemy &&  SV_CloseEnough(ent, ent->enemy, dist))
        return;

// bump around...
    if ((Q_rand() & 3) == 1 || !SV_StepDirection(ent, ent->ideal_yaw, dist)) {
        if (ent->inuse)
            SV_NewChaseDir(ent, goal, dist);
    }
}


/*
===============
M_walkmove
===============
*/
bool M_walkmove(edict_t *ent, float yaw, float dist)
{
    vec3_t  move;

    if (!ent->groundentity && !(ent->flags & (FL_FLY | FL_SWIM)))
        return false;

    yaw = DEG2RAD(yaw);
    move[0] = cos(yaw) * dist;
    move[1] = sin(yaw) * dist;
    move[2] = 0;

    return SV_movestep(ent, move, true);
}
