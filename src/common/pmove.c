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

#include "shared/shared.h"
#include "common/pmove.h"

// all of the locals will be zeroed before each
// pmove, just to make damn sure we don't have
// any differences when running on client or server

typedef struct {
    vec3_t      origin;         // full float precision
    vec3_t      velocity;       // full float precision

    vec3_t      forward, right, up;
    float       frametime;

    csurface_t  *groundsurface;
    cplane_t    groundplane;
    int         groundcontents;

    vec3_t      previous_origin;
    bool        ladder;

    float       xyspeed;
    float       bobmove, bobtime, bobfracsin;
    int         bobcycle;
} pml_t;

static pmove_t      *pm;
static pml_t        pml;

static pmoveParams_t    *pmp;

// movement parameters
static const float  pm_stopspeed = 100;
static const float  pm_duckspeed = 100;
static const float  pm_accelerate = 10;
static const float  pm_wateraccelerate = 10;
static const float  pm_waterspeed = 400;

/*
==================
PM_StepSlideMove

Each intersection will try to step over the obstruction instead of
sliding along it.

Returns a new origin, velocity, and contact entity
Does not modify any world state?
==================
*/
static void Pm_StepSlideMove_Trace(trace_t *tr, vec3_t origin, vec3_t mins, vec3_t maxs, vec3_t end, void *arg)
{
    pm->trace(tr, origin, mins, maxs, end);
}

static bool Pm_StepSlideMove_Impact(trace_t *tr, void *arg)
{
    // save entity for contact
    if (pm->numtouch < MAXTOUCH && tr->ent) {
        pm->touchents[pm->numtouch] = tr->ent;
        pm->numtouch++;
    }

    return false;
}

/*
==================
PM_StepSlideMove
==================
*/
static void PM_StepSlideMove(void)
{
    trace_t tr;
    StepSlideMove(pml.origin, pm->mins, pm->maxs, pml.velocity, pml.frametime, !!pm->s.pm_time, Pm_StepSlideMove_Trace, Pm_StepSlideMove_Impact, NULL, &tr);
}

/*
==================
PM_Friction

Handles both ground friction and water friction
==================
*/
static void PM_Friction(void)
{
    float   *vel;
    float   speed, newspeed, control;
    float   friction;
    float   drop;

    vel = pml.velocity;

    speed = VectorLength(vel);
    if (speed < 1) {
        vel[0] = 0;
        vel[1] = 0;
        return;
    }

    drop = 0;

// apply ground friction
    if (!(pm->s.pm_flags & PMF_TIME_LAND) && ((pm->groundentity && pml.groundsurface && !(pml.groundsurface->flags & SURF_SLICK)) || (pml.ladder))) {
        friction = pmp->friction;
        control = speed < pm_stopspeed ? pm_stopspeed : speed;
        drop += control * friction * pml.frametime;
    }

// apply water friction
    if (pm->waterlevel && !pml.ladder)
        drop += speed * pmp->waterfriction * pm->waterlevel * pml.frametime;

// scale the velocity
    newspeed = speed - drop;
    if (newspeed < 0)
        newspeed = 0;
    newspeed /= speed;

    vel[0] = vel[0] * newspeed;
    vel[1] = vel[1] * newspeed;
    vel[2] = vel[2] * newspeed;
}

/*
==============
PM_Accelerate

Handles user intended acceleration
==============
*/
static void PM_Accelerate(vec3_t wishdir, float wishspeed, float accel)
{
    int         i;
    float       addspeed, accelspeed, currentspeed;

    currentspeed = DotProduct(pml.velocity, wishdir);
    addspeed = wishspeed - currentspeed;
    if (addspeed <= 0)
        return;
    accelspeed = accel * pml.frametime * wishspeed;
    if (accelspeed > addspeed)
        accelspeed = addspeed;

    for (i = 0; i < 3; i++)
        pml.velocity[i] += accelspeed * wishdir[i];
}

static void PM_AirAccelerate(vec3_t wishdir, float wishspeed, float accel)
{
    int         i;
    float       addspeed, accelspeed, currentspeed, wishspd = wishspeed;

    if (wishspd > 30)
        wishspd = 30;
    currentspeed = DotProduct(pml.velocity, wishdir);
    addspeed = wishspd - currentspeed;
    if (addspeed <= 0)
        return;
    accelspeed = accel * wishspeed * pml.frametime;
    if (accelspeed > addspeed)
        accelspeed = addspeed;

    for (i = 0; i < 3; i++)
        pml.velocity[i] += accelspeed * wishdir[i];
}

/*
=============
PM_AddCurrents
=============
*/
static void PM_AddCurrents(vec3_t wishvel)
{
    vec3_t  v;
    float   s;

    //
    // account for ladders
    //

    if (pml.ladder && fabsf(pml.velocity[2]) <= 200) {
        if ((pm->viewangles[PITCH] <= -15) && (pm->cmd.forwardmove > 0))
            wishvel[2] = 200;
        else if ((pm->viewangles[PITCH] >= 15) && (pm->cmd.forwardmove > 0))
            wishvel[2] = -200;
        else if (pm->cmd.upmove > 0)
            wishvel[2] = 200;
        else if (pm->cmd.upmove < 0)
            wishvel[2] = -200;
        else
            wishvel[2] = 0;

        // limit horizontal speed when on a ladder
        clamp(wishvel[0], -25, 25);
        clamp(wishvel[1], -25, 25);
    }

    //
    // add water currents
    //

    if (pm->watertype & MASK_CURRENT) {
        VectorClear(v);

        if (pm->watertype & CONTENTS_CURRENT_0)
            v[0] += 1;
        if (pm->watertype & CONTENTS_CURRENT_90)
            v[1] += 1;
        if (pm->watertype & CONTENTS_CURRENT_180)
            v[0] -= 1;
        if (pm->watertype & CONTENTS_CURRENT_270)
            v[1] -= 1;
        if (pm->watertype & CONTENTS_CURRENT_UP)
            v[2] += 1;
        if (pm->watertype & CONTENTS_CURRENT_DOWN)
            v[2] -= 1;

        s = pm_waterspeed;
        if ((pm->waterlevel == 1) && (pm->groundentity))
            s /= 2;

        VectorMA(wishvel, s, v, wishvel);
    }

    //
    // add conveyor belt velocities
    //

    if (pm->groundentity) {
        VectorClear(v);

        if (pml.groundcontents & CONTENTS_CURRENT_0)
            v[0] += 1;
        if (pml.groundcontents & CONTENTS_CURRENT_90)
            v[1] += 1;
        if (pml.groundcontents & CONTENTS_CURRENT_180)
            v[0] -= 1;
        if (pml.groundcontents & CONTENTS_CURRENT_270)
            v[1] -= 1;
        if (pml.groundcontents & CONTENTS_CURRENT_UP)
            v[2] += 1;
        if (pml.groundcontents & CONTENTS_CURRENT_DOWN)
            v[2] -= 1;

        VectorMA(wishvel, 100 /* pm->groundentity->speed */, v, wishvel);
    }
}

/*
===================
PM_WaterMove

===================
*/
static void PM_WaterMove(void)
{
    int     i;
    vec3_t  wishvel;
    float   wishspeed;
    vec3_t  wishdir;

//
// user intentions
//
    for (i = 0; i < 3; i++)
        wishvel[i] = pml.forward[i] * pm->cmd.forwardmove + pml.right[i] * pm->cmd.sidemove;

    if (!pm->cmd.forwardmove && !pm->cmd.sidemove && !pm->cmd.upmove)
        wishvel[2] -= 60;       // drift towards bottom
    else
        wishvel[2] += pm->cmd.upmove;

    PM_AddCurrents(wishvel);

    VectorCopy(wishvel, wishdir);
    wishspeed = VectorNormalize(wishdir);

    if (wishspeed > pmp->maxspeed) {
        VectorScale(wishvel, pmp->maxspeed / wishspeed, wishvel);
        wishspeed = pmp->maxspeed;
    }
    wishspeed *= pmp->watermult;

    PM_Accelerate(wishdir, wishspeed, pm_wateraccelerate);

    PM_StepSlideMove();
}

/*
===================
PM_AirMove

===================
*/
static void PM_AirMove(void)
{
    int         i;
    vec3_t      wishvel;
    float       fmove, smove;
    vec3_t      wishdir;
    float       wishspeed;
    float       maxspeed;

    fmove = pm->cmd.forwardmove;
    smove = pm->cmd.sidemove;

    for (i = 0; i < 2; i++)
        wishvel[i] = pml.forward[i] * fmove + pml.right[i] * smove;
    wishvel[2] = 0;

    PM_AddCurrents(wishvel);

    VectorCopy(wishvel, wishdir);
    wishspeed = VectorNormalize(wishdir);

//
// clamp to server defined max speed
//
    maxspeed = (pm->s.pm_flags & PMF_DUCKED) ? pm_duckspeed : pmp->maxspeed;

    if (wishspeed > maxspeed) {
        VectorScale(wishvel, maxspeed / wishspeed, wishvel);
        wishspeed = maxspeed;
    }

    if (pml.ladder) {
        PM_Accelerate(wishdir, wishspeed, pm_accelerate);
        if (!wishvel[2]) {
            if (pml.velocity[2] > 0) {
                pml.velocity[2] -= pm->s.gravity * pml.frametime;
                if (pml.velocity[2] < 0)
                    pml.velocity[2] = 0;
            } else {
                pml.velocity[2] += pm->s.gravity * pml.frametime;
                if (pml.velocity[2] > 0)
                    pml.velocity[2] = 0;
            }
        }
        PM_StepSlideMove();
    } else if (pm->groundentity) {
        // walking on ground
        pml.velocity[2] = 0; //!!! this is before the accel
        PM_Accelerate(wishdir, wishspeed, pm_accelerate);

// PGM  -- fix for negative trigger_gravity fields
//      pml.velocity[2] = 0;
        if (pm->s.gravity > 0)
            pml.velocity[2] = 0;
        else
            pml.velocity[2] -= pm->s.gravity * pml.frametime;
// PGM

        if (!pml.velocity[0] && !pml.velocity[1])
            return;
        PM_StepSlideMove();
    } else {
        // not on ground, so little effect on velocity
         PM_AirAccelerate(wishdir, wishspeed, pm_accelerate);
        // add gravity
        pml.velocity[2] -= pm->s.gravity * pml.frametime;
        PM_StepSlideMove();
    }
}

/*
=============
PM_CategorizePosition
=============
*/
static void PM_CategorizePosition(void)
{
    vec3_t      point;
    int         cont;
    trace_t     trace;
    int         sample1;
    int         sample2;

// if the player hull point one unit down is solid, the player
// is on ground

// see if standing on something solid
    point[0] = pml.origin[0];
    point[1] = pml.origin[1];
    point[2] = pml.origin[2] - 0.25f;
    if (pml.velocity[2] > 180 || ((pm->s.pm_flags & PMF_TIME_LAND) && pm->s.pm_time > 33)) { //!!ZOID changed from 100 to 180 (ramp accel)
        pm->s.pm_flags &= ~PMF_ON_GROUND;
        pm->groundentity = NULL;
    } else {
        pm->trace(&trace, pml.origin, pm->mins, pm->maxs, point);
        pml.groundplane = trace.plane;
        pml.groundsurface = trace.surface;
        pml.groundcontents = trace.contents;

        if (!trace.ent || (trace.plane.normal[2] < 0.7f && !trace.startsolid)) {
            pm->groundentity = NULL;
            pm->s.pm_flags &= ~PMF_ON_GROUND;
        } else {
            pm->groundentity = trace.ent;

            // hitting solid ground will end a waterjump
            if (pm->s.pm_flags & PMF_TIME_WATERJUMP) {
                pm->s.pm_flags &= ~(PMF_TIME_WATERJUMP | PMF_TIME_TELEPORT);
                pm->s.pm_time = 0;
            }

            // just hit the ground
            pm->s.pm_flags |= PMF_ON_GROUND;
        }

        if (pm->numtouch < MAXTOUCH && trace.ent) {
            pm->touchents[pm->numtouch] = trace.ent;
            pm->numtouch++;
        }
    }

//
// get waterlevel, accounting for ducking
//
    pm->waterlevel = 0;
    pm->watertype = 0;

    sample2 = pm->viewheight - pm->mins[2];
    sample1 = sample2 / 2;

    point[2] = pml.origin[2] + pm->mins[2] + 1;
    cont = pm->pointcontents(point);

    if (cont & MASK_WATER) {
        pm->watertype = cont;
        pm->waterlevel = 1;
        point[2] = pml.origin[2] + pm->mins[2] + sample1;
        cont = pm->pointcontents(point);
        if (cont & MASK_WATER) {
            pm->waterlevel = 2;
            point[2] = pml.origin[2] + pm->mins[2] + sample2;
            cont = pm->pointcontents(point);
            if (cont & MASK_WATER)
                pm->waterlevel = 3;
        }
    }
}

/*
=============
PM_CheckJump
=============
*/
static void PM_CheckJump(void)
{
    if (pm->cmd.upmove < 10) {
        // not holding jump
        pm->s.pm_flags &= ~PMF_JUMP_HELD;
        return;
    }

    // must wait for jump to be released
    if (pm->s.pm_flags & PMF_JUMP_HELD)
        return;

    if (pm->s.pm_type == PM_DEAD)
        return;

    if (pm->waterlevel >= 2) {
        // swimming, not jumping
        pm->groundentity = NULL;
        return;
    }

    if (pm->groundentity == NULL)
        return;     // in air, so no effect

    pm->s.pm_flags |= PMF_JUMP_HELD;

    pm->groundentity = NULL;
    pm->s.pm_flags &= ~PMF_ON_GROUND;
    pml.velocity[2] += 270;
    if (pml.velocity[2] < 270)
        pml.velocity[2] = 270;
}

/*
=============
PM_CheckSpecialMovement
=============
*/
static void PM_CheckSpecialMovement(void)
{
    vec3_t  spot;
    int     cont;
    vec3_t  flatforward;
    trace_t trace;

    if (pm->s.pm_time)
        return;

    pml.ladder = false;

    // check for ladder
    flatforward[0] = pml.forward[0];
    flatforward[1] = pml.forward[1];
    flatforward[2] = 0;
    VectorNormalize(flatforward);

    VectorMA(pml.origin, 1, flatforward, spot);
    pm->trace(&trace, pml.origin, pm->mins, pm->maxs, spot);
    if ((trace.fraction < 1) && (trace.contents & CONTENTS_LADDER))
        pml.ladder = true;

    // check for water jump
    if (pm->waterlevel != 2)
        return;

    VectorMA(pml.origin, 30, flatforward, spot);
    spot[2] += 4;
    cont = pm->pointcontents(spot);
    if (!(cont & CONTENTS_SOLID))
        return;

    spot[2] += 16;
    cont = pm->pointcontents(spot);
    if (cont)
        return;
    // jump out of water
    VectorScale(flatforward, 50, pml.velocity);
    pml.velocity[2] = 350;

    pm->s.pm_flags |= PMF_TIME_WATERJUMP;
    pm->s.pm_time = 255;
}

/*
===============
PM_FlyMove
===============
*/
static void PM_FlyMove(void)
{
    float   speed, drop, friction, control, newspeed;
    float   currentspeed, addspeed, accelspeed;
    int         i;
    vec3_t      wishvel;
    float       fmove, smove;
    vec3_t      wishdir;
    float       wishspeed;

    pm->viewheight = 22;

    // friction
    speed = VectorLength(pml.velocity);
    if (speed < 1) {
        VectorClear(pml.velocity);
    } else {
        drop = 0;

        friction = pmp->flyfriction;
        control = speed < pm_stopspeed ? pm_stopspeed : speed;
        drop += control * friction * pml.frametime;

        // scale the velocity
        newspeed = speed - drop;
        if (newspeed < 0)
            newspeed = 0;
        newspeed /= speed;

        VectorScale(pml.velocity, newspeed, pml.velocity);
    }

    // accelerate
    fmove = pm->cmd.forwardmove;
    smove = pm->cmd.sidemove;

    VectorNormalize(pml.forward);
    VectorNormalize(pml.right);

    for (i = 0; i < 3; i++)
        wishvel[i] = pml.forward[i] * fmove + pml.right[i] * smove;
    wishvel[2] += pm->cmd.upmove;

    VectorCopy(wishvel, wishdir);
    wishspeed = VectorNormalize(wishdir);

    //
    // clamp to server defined max speed
    //
    if (wishspeed > pmp->maxspeed) {
        VectorScale(wishvel, pmp->maxspeed / wishspeed, wishvel);
        wishspeed = pmp->maxspeed;
    }

    currentspeed = DotProduct(pml.velocity, wishdir);
    addspeed = wishspeed - currentspeed;
    if (addspeed > 0) {
        accelspeed = pm_accelerate * pml.frametime * wishspeed;
        if (accelspeed > addspeed)
            accelspeed = addspeed;

        for (i = 0; i < 3; i++)
            pml.velocity[i] += accelspeed * wishdir[i];
    }

    // move
    VectorMA(pml.origin, pml.frametime, pml.velocity, pml.origin);
}

/*
==============
PM_CheckDuck

Sets mins, maxs, and pm->viewheight
==============
*/
static void PM_CheckDuck(void)
{
    trace_t trace;

    pm->mins[0] = -16;
    pm->mins[1] = -16;

    pm->maxs[0] = 16;
    pm->maxs[1] = 16;

    if (pm->s.pm_type == PM_GIB) {
        pm->mins[2] = 0;
        pm->maxs[2] = 16;
        pm->viewheight = 8;
        return;
    }

    pm->mins[2] = -24;

    if (pm->s.pm_type == PM_DEAD) {
        pm->s.pm_flags |= PMF_DUCKED;
    } else if (pm->cmd.upmove < 0 && (pm->s.pm_flags & PMF_ON_GROUND)) {
        // duck
        pm->s.pm_flags |= PMF_DUCKED;
    } else {
        // stand up if possible
        if (pm->s.pm_flags & PMF_DUCKED) {
            // try to stand up
            pm->maxs[2] = 32;
            pm->trace(&trace, pml.origin, pm->mins, pm->maxs, pml.origin);
            if (!trace.allsolid)
                pm->s.pm_flags &= ~PMF_DUCKED;
        }
    }

    if (pm->s.pm_flags & PMF_DUCKED) {
        pm->maxs[2] = 4;
        pm->viewheight = -2;
    } else {
        pm->maxs[2] = 32;
        pm->viewheight = 22;
    }
}

/*
==============
PM_DeadMove
==============
*/
static void PM_DeadMove(void)
{
    float   forward;

    if (!pm->groundentity)
        return;

    // extra friction
    forward = VectorLength(pml.velocity);
    forward -= 20;
    if (forward <= 0) {
        VectorClear(pml.velocity);
    } else {
        VectorNormalize(pml.velocity);
        VectorScale(pml.velocity, forward, pml.velocity);
    }
}

static bool PM_GoodPosition(void)
{
    trace_t trace;

    if (pm->s.pm_type == PM_SPECTATOR)
        return true;

    pm->trace(&trace, pm->s.origin, pm->mins, pm->maxs, pm->s.origin);

    return !trace.allsolid;
}

/*
================
PM_SnapPosition

On exit, the origin will have a value that is pre-quantized to the COORDSCALE
precision of the network channel and in a valid position.
================
*/
static void PM_SnapPosition(void)
{
    vec3_t  sign;
    int     i, j, bits;
    vec3_t  base;
    // try all single bits first
    static const byte jitterbits[8] = {0, 4, 1, 2, 3, 5, 6, 7};

    // snap velocity to eigths
    VectorSnapCoord(pml.velocity, pm->s.velocity);
    VectorSnapCoord(pml.origin, pm->s.origin);

    for (i = 0; i < 3; i++) {
        if (pml.origin[i] > 0)
            sign[i] = SHORT2COORD(1);
        else if (pml.origin[i] < 0)
            sign[i] = SHORT2COORD(-1);
        else
            sign[i] = 0;
    }
    VectorCopy(pm->s.origin, base);

    // try all combinations
    for (j = 0; j < 8; j++) {
        bits = jitterbits[j];
        VectorCopy(base, pm->s.origin);
        for (i = 0; i < 3; i++)
            if (bits & (1 << i))
                pm->s.origin[i] += sign[i];

        if (PM_GoodPosition()) {
            return;
        }
    }

    // go back to the last position
    VectorCopy(pml.previous_origin, pm->s.origin);
}

/*
================
PM_InitialSnapPosition

================
*/
static void PM_InitialSnapPosition(void)
{
    int        x, y, z;
    vec3_t     base;
    static const vec3_t offset = { 0, SHORT2COORD(-1), SHORT2COORD(1) };

    VectorCopy(pm->s.origin, base);

    for (z = 0; z < 3; z++) {
        pm->s.origin[2] = base[2] + offset[z];
        for (y = 0; y < 3; y++) {
            pm->s.origin[1] = base[1] + offset[y];
            for (x = 0; x < 3; x++) {
                pm->s.origin[0] = base[0] + offset[x];
                if (PM_GoodPosition()) {
                    VectorCopy(pm->s.origin, pml.previous_origin);
                    return;
                }
            }
        }
    }
}

/*
================
PM_ClampAngles

================
*/
static void PM_ClampAngles(void)
{
    if (pm->s.pm_flags & PMF_TIME_TELEPORT) {
        pm->viewangles[YAW] = pm->cmd.angles[YAW] + pm->s.delta_angles[YAW];
        pm->viewangles[PITCH] = 0;
        pm->viewangles[ROLL] = 0;
    } else {
        VectorAdd(pm->cmd.angles, pm->s.delta_angles, pm->viewangles);

        // don't let the player look up or down more than 90 degrees
        clamp(pm->viewangles[PITCH], -89, 89);
    }
    AngleVectors(pm->viewangles, pml.forward, pml.right, pml.up);
}

static void PM_CalculateGunAngles(void)
{
    // calculate speed and cycle to be used for
    // all cyclic walking effects 
    pml.xyspeed = sqrtf(pml.velocity[0] * pml.velocity[0] + pml.velocity[1] * pml.velocity[1]);

    if (pml.xyspeed < 5) {
        pml.bobmove = 0;
        pm->bobtime = 0;    // start at beginning of cycle again
    } else if (pm->s.pm_flags & PMF_ON_GROUND) {
        // so bobbing only cycles when on ground
        if (pml.xyspeed > 210)
            pml.bobmove = 0.25f;
        else if (pml.xyspeed > 100)
            pml.bobmove = 0.125f;
        else
            pml.bobmove = 0.0625f;
    }

    pml.bobmove *= pml.frametime;

    pml.bobtime = (pm->bobtime += pml.bobmove);

    if (pm->s.pm_flags & PMF_DUCKED)
        pml.bobtime *= 4;

    pml.bobcycle = (int) pml.bobtime;
    pml.bobfracsin = fabsf(sinf(pml.bobtime * M_PI));

    // gun angles from bobbing
    pm->gunangles[ROLL] = pml.xyspeed * pml.bobfracsin * 0.005f;
    pm->gunangles[YAW] = pml.xyspeed * pml.bobfracsin * 0.01f;
    if (pml.bobcycle & 1) {
        pm->gunangles[ROLL] = -pm->gunangles[ROLL];
        pm->gunangles[YAW] = -pm->gunangles[YAW];
    }

    pm->gunangles[PITCH] = pml.xyspeed * pml.bobfracsin * 0.005f;
}

/*
================
Pmove

Can be called by either the server or the client
================
*/
void Pmove(pmove_t *pmove, pmoveParams_t *params)
{
    pm = pmove;
    pmp = params;

    // clear results
    pm->numtouch = 0;
    VectorClear(pm->viewangles);
    pm->viewheight = 0;
    pm->groundentity = NULL;
    pm->watertype = 0;
    pm->waterlevel = 0;

    // clear all pmove local vars
    memset(&pml, 0, sizeof(pml));

    // convert origin and velocity to float values
    VectorCopy(pm->s.origin, pml.origin);
    VectorCopy(pm->s.velocity, pml.velocity);

    // save old org in case we get stuck
    VectorCopy(pm->s.origin, pml.previous_origin);

    PM_ClampAngles();

    if (pm->s.pm_type == PM_SPECTATOR) {
        pml.frametime = pmp->speedmult * pm->cmd.msec * 0.001f;
        PM_FlyMove();
        PM_SnapPosition();
        return;
    }

    pml.frametime = pm->cmd.msec * 0.001f;

    if (pm->s.pm_type >= PM_DEAD) {
        pm->cmd.forwardmove = 0;
        pm->cmd.sidemove = 0;
        pm->cmd.upmove = 0;
    }

    if (pm->s.pm_type == PM_FREEZE)
        return;     // no movement at all

    // set mins, maxs, and viewheight
    PM_CheckDuck();

    if (pm->snapinitial)
        PM_InitialSnapPosition();

    // set groundentity, watertype, and waterlevel
    PM_CategorizePosition();

    if (pm->s.pm_type == PM_DEAD)
        PM_DeadMove();

    PM_CheckSpecialMovement();

    // drop timing counter
    if (pm->s.pm_time) {
        int     msec;

        msec = pm->cmd.msec >> 3;
        if (!msec)
            msec = 1;
        if (msec >= pm->s.pm_time) {
            pm->s.pm_flags &= ~(PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT);
            pm->s.pm_time = 0;
        } else
            pm->s.pm_time -= msec;
    }

    if (pm->s.pm_flags & PMF_TIME_TELEPORT) {
        // teleport pause stays exactly in place
    } else if (pm->s.pm_flags & PMF_TIME_WATERJUMP) {
        // waterjump has no control, but falls
        pml.velocity[2] -= pm->s.gravity * pml.frametime;
        if (pml.velocity[2] < 0) {
            // cancel as soon as we are falling down again
            pm->s.pm_flags &= ~(PMF_TIME_WATERJUMP | PMF_TIME_LAND | PMF_TIME_TELEPORT);
            pm->s.pm_time = 0;
        }

        PM_StepSlideMove();
    } else {
        PM_CheckJump();

        PM_Friction();

        if (pm->waterlevel >= 2)
            PM_WaterMove();
        else {
            vec3_t  angles;

            VectorCopy(pm->viewangles, angles);
            if (angles[PITCH] > 180)
                angles[PITCH] = angles[PITCH] - 360;
            angles[PITCH] /= 3;

            AngleVectors(angles, pml.forward, pml.right, pml.up);

            PM_AirMove();
        }
    }

    // set groundentity, watertype, and waterlevel for final spot
    PM_CategorizePosition();

    PM_SnapPosition();

    PM_CalculateGunAngles();
}

void PmoveInit(pmoveParams_t *pmp)
{
    // set up default pmove parameters
    memset(pmp, 0, sizeof(*pmp));

    pmp->speedmult = 2;
    pmp->flyfriction = 9;

    pmp->watermult = 0.7f;
    pmp->maxspeed = 320;
    pmp->friction = 4;
    pmp->waterfriction = 4;
}
