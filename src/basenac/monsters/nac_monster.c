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

Monster routines for NAC

==============================================================================
*/

#include "../g_local.h"
#include <shared/iqm.h>

static void *G_ModelAllocate(void *arg, size_t s)
{
    return Z_TagMallocz(s, TAG_MODEL);
}

typedef struct m_iqm_s {
    iqm_model_t model;
    int32_t root_id;
} m_iqm_t;

// TODO: check for errors
m_iqm_t *M_InitializeIQM(const char *name)
{
    static m_iqm_t iqm;
    void *buf;
    int ret = G_LoadFile(name, &buf);
    int iqmret = MOD_LoadIQM_Base(&iqm.model, buf, ret, name, G_ModelAllocate, NULL);
    G_FreeFile(buf);

    // find the root joint
    iqm.root_id = -1;

    for (uint32_t i = 0; i < iqm.model.num_joints; i++)
    {
        if (strlen(iqm.model.jointNames[i]) >= 4 && Q_strcasecmp(iqm.model.jointNames[i] + strlen(iqm.model.jointNames[i]) - 4, "root") == 0)
        {
            iqm.root_id = i;
            break;
        }
    }

    if (iqm.root_id == -1)
        Com_Errorf(ERR_DROP, "Missing required model data for %s", name);

    return &iqm;
}

void M_FreeIQM(m_iqm_t *iqm)
{
    Z_FreeTags(TAG_MODEL);
}

void M_SetupIQMAnimation(const m_iqm_t *iqm, mmove_t *anim, bool absolute)
{
    vec3_t offsetFrom = { 0, 0, 0 };
    mframe_t *frame = anim->frame;

    for (int32_t i = anim->firstframe; i < anim->lastframe; i++, frame++)
    {
        const iqm_transform_t *pose = &iqm->model.poses[iqm->root_id + (i * iqm->model.num_poses)];
        if (absolute) {
            VectorCopy(pose->translate, frame->translate);
        } else {
            VectorSubtract(offsetFrom, pose->translate, frame->translate);
            VectorCopy(pose->translate, offsetFrom);
            frame->dist = VectorLength(frame->translate);
        }
    }
}

void M_SetupIQMAnimations(const m_iqm_t *iqm, mmove_t *animations[])
{
    for (mmove_t **anim = animations; *anim; anim++) {
        M_SetupIQMAnimation(iqm, *anim, false);
    }
}

static const vec3_t vec3_up = { 0, 0, 1 };

// LICENSE
//
//   This software is dual-licensed to the public domain and under the following
//   license: you are granted a perpetual, irrevocable license to copy, modify,
//   publish, and distribute this file as you see fit.
//
// VERSION 
//   0.1.0  (2016-06-01)  Initial release
//
// AUTHOR
//   Forrest Smith
//
// ADDITIONAL READING
//   https://medium.com/@ForrestTheWoods/solving-ballistic-trajectories-b0165523348c
//
// API
//    int solve_ballistic_arc(Vector3 proj_pos, float proj_speed, Vector3 target, float gravity, out Vector3 low, out Vector3 high);
//    int solve_ballistic_arc(Vector3 proj_pos, float proj_speed, Vector3 target, Vector3 target_velocity, float gravity, out Vector3 s0, out Vector3 s1, out Vector3 s2, out Vector3 s3);
//    bool solve_ballistic_arc_lateral(Vector3 proj_pos, float lateral_speed, Vector3 target, float max_height, out float vertical_speed, out float gravity);
//    bool solve_ballistic_arc_lateral(Vector3 proj_pos, float lateral_speed, Vector3 target, Vector3 target_velocity, float max_height_offset, out Vector3 fire_velocity, out float gravity, out Vector3 impact_point);
//
//    float ballistic_range(float speed, float gravity, float initial_height);
//
//    bool IsZero(double d);
//    int SolveQuadric(double c0, double c1, double c2, out double s0, out double s1);
//    int SolveCubic(double c0, double c1, double c2, double c3, out double s0, out double s1, out double s2);
//    int SolveQuartic(double c0, double c1, double c2, double c3, double c4, out double s0, out double s1, out double s2, out double s3);

// SolveQuadric, SolveCubic, and SolveQuartic were ported from C as written for Graphics Gems I
// Original Author: Jochen Schwarze (schwarze@isa.de)
// https://github.com/erich666/GraphicsGems/blob/240a34f2ad3fa577ef57be74920db6c4b00605e4/gems/Roots3And4.c

// Utility function used by SolveQuadratic, SolveCubic, and SolveQuartic
static bool IsZero(double d) {
    const double eps = 1e-9;
    return d > -eps && d < eps;
}

static double GetCubicRoot(double value)
{   
    if (value > 0.0) {
        return pow(value, 1.0 / 3.0);
    } else if (value < 0) {
        return -pow(-value, 1.0 / 3.0);
    } else {
        return 0.0;
    }
}

// Solve quadratic equation: c0*x^2 + c1*x + c2. 
// Returns number of solutions.
static int SolveQuadric(double c0, double c1, double c2, double *s0, double *s1) {
    *s0 = NAN;
    *s1 = NAN;

    double p, q, D;

    /* normal form: x^2 + px + q = 0 */
    p = c1 / (2 * c0);
    q = c2 / c0;

    D = p * p - q;

    if (IsZero(D)) {
        *s0 = -p;
        return 1;
    }
    else if (D < 0) {
        return 0;
    }
    else /* if (D > 0) */ {
        double sqrt_D = sqrt(D);

        *s0 =  sqrt_D - p;
        *s1 = -sqrt_D - p;
        return 2;
    }
}

// Solve cubic equation: c0*x^3 + c1*x^2 + c2*x + c3. 
// Returns number of solutions.
static int SolveCubic(double c0, double c1, double c2, double c3, double *s0, double *s1, double *s2)
{
    *s0 = NAN;
    *s1 = NAN;
    *s2 = NAN;

    int     num;
    double  sub;
    double  A, B, C;
    double  sq_A, p, q;
    double  cb_p, D;

    /* normal form: x^3 + Ax^2 + Bx + C = 0 */
    A = c1 / c0;
    B = c2 / c0;
    C = c3 / c0;

    /*  substitute x = y - A/3 to eliminate quadric term:  x^3 +px + q = 0 */
    sq_A = A * A;
    p = 1.0/3 * (- 1.0/3 * sq_A + B);
    q = 1.0/2 * (2.0/27 * A * sq_A - 1.0/3 * A * B + C);

    /* use Cardano's formula */
    cb_p = p * p * p;
    D = q * q + cb_p;

    if (IsZero(D)) {
        if (IsZero(q)) /* one triple solution */ {
            *s0 = 0;
            num = 1;
        }
        else /* one single and one double solution */ {
            double u = GetCubicRoot(-q);
            *s0 = 2 * u;
            *s1 = - u;
            num = 2;
        }
    }
    else if (D < 0) /* Casus irreducibilis: three real solutions */ {
        double phi = 1.0/3 * acos(-q / sqrt(-cb_p));
        double t = 2 * sqrt(-p);

        *s0 =   t * cos(phi);
        *s1 = - t * cos(phi + M_PI / 3);
        *s2 = - t * cos(phi - M_PI / 3);
        num = 3;
    }
    else /* one real solution */ {
        double sqrt_D = sqrt(D);
        double u = GetCubicRoot(sqrt_D - q);
        double v = -GetCubicRoot(sqrt_D + q);

        *s0 = u + v;
        num = 1;
    }

    /* resubstitute */
    sub = 1.0/3 * A;

    if (num > 0)    *s0 -= sub;
    if (num > 1)    *s1 -= sub;
    if (num > 2)    *s2 -= sub;

    return num;
}

// Solve quartic function: c0*x^4 + c1*x^3 + c2*x^2 + c3*x + c4. 
// Returns number of solutions.
static int SolveQuartic(double c0, double c1, double c2, double c3, double c4, double *s0, double *s1, double *s2, double *s3) {
    *s0 = NAN;
    *s1 = NAN;
    *s2 = NAN;
    *s3 = NAN;

    double  coeffs[4];
    double  z, u, v, sub;
    double  A, B, C, D;
    double  sq_A, p, q, r;
    int     num;

    /* normal form: x^4 + Ax^3 + Bx^2 + Cx + D = 0 */
    A = c1 / c0;
    B = c2 / c0;
    C = c3 / c0;
    D = c4 / c0;

    /*  substitute x = y - A/4 to eliminate cubic term: x^4 + px^2 + qx + r = 0 */
    sq_A = A * A;
    p = - 3.0/8 * sq_A + B;
    q = 1.0/8 * sq_A * A - 1.0/2 * A * B + C;
    r = - 3.0/256*sq_A*sq_A + 1.0/16*sq_A*B - 1.0/4*A*C + D;

    if (IsZero(r)) {
        /* no absolute term: y(y^3 + py + q) = 0 */

        coeffs[ 3 ] = q;
        coeffs[ 2 ] = p;
        coeffs[ 1 ] = 0;
        coeffs[ 0 ] = 1;

        num = SolveCubic(coeffs[0], coeffs[1], coeffs[2], coeffs[3], s0, s1, s2);
    }
    else {
        /* solve the resolvent cubic ... */
        coeffs[ 3 ] = 1.0/2 * r * p - 1.0/8 * q * q;
        coeffs[ 2 ] = - r;
        coeffs[ 1 ] = - 1.0/2 * p;
        coeffs[ 0 ] = 1;

        SolveCubic(coeffs[0], coeffs[1], coeffs[2], coeffs[3], s0, s1, s2);

        /* ... and take the one real solution ... */
        z = *s0;

        /* ... to build two quadric equations */
        u = z * z - r;
        v = 2 * z - p;

        if (IsZero(u))
            u = 0;
        else if (u > 0)
            u = sqrt(u);
        else
            return 0;

        if (IsZero(v))
            v = 0;
        else if (v > 0)
            v = sqrt(v);
        else
            return 0;

        coeffs[ 2 ] = z - u;
        coeffs[ 1 ] = q < 0 ? -v : v;
        coeffs[ 0 ] = 1;

        num = SolveQuadric(coeffs[0], coeffs[1], coeffs[2], s0, s1);

        coeffs[ 2 ]= z + u;
        coeffs[ 1 ] = q < 0 ? v : -v;
        coeffs[ 0 ] = 1;

        if (num == 0) num += SolveQuadric(coeffs[0], coeffs[1], coeffs[2], s0, s1);
        else if (num == 1) num += SolveQuadric(coeffs[0], coeffs[1], coeffs[2], s1, s2);
        else if (num == 2) num += SolveQuadric(coeffs[0], coeffs[1], coeffs[2], s2, s3);
    }

    /* resubstitute */
    sub = 1.0/4 * A;

    if (num > 0)    *s0 -= sub;
    if (num > 1)    *s1 -= sub;
    if (num > 2)    *s2 -= sub;
    if (num > 3)    *s3 -= sub;

    return num;
}


// Calculate the maximum range that a ballistic projectile can be fired on given speed and gravity.
//
// speed (float): projectile velocity
// gravity (float): force of gravity, positive is down
// initial_height (float): distance above flat terrain
//
// return (float): maximum range
static float ballistic_range(float speed, float gravity, float initial_height) {

    // Handling these cases is up to your project's coding standards
    //Debug.Assert(speed > 0 && gravity > 0 && initial_height >= 0, "fts.ballistic_range called with invalid data");

    // Derivation
    //   (1) x = speed * time * cos O
    //   (2) y = initial_height + (speed * time * sin O) - (.5 * gravity*time*time)
    //   (3) via quadratic: t = (speed*sin O)/gravity + sqrt(speed*speed*sin O + 2*gravity*initial_height)/gravity    [ignore smaller root]
    //   (4) solution: range = x = (speed*cos O)/gravity * sqrt(speed*speed*sin O + 2*gravity*initial_height)    [plug t back into x=speed*time*cos O]
    float angle = DEG2RAD(45); // no air resistence, so 45 degrees provides maximum range
    float ccos = cos(angle);
    float ssin = sin(angle);

    float range = (speed*ccos/gravity) * (speed*ssin + sqrt(speed*speed*ssin*ssin + 2*gravity*initial_height));
    return range;
}


// Solve firing angles for a ballistic projectile with speed and gravity to hit a fixed position.
//
// proj_pos (Vector3): point projectile will fire from
// proj_speed (float): scalar speed of projectile
// target (Vector3): point projectile is trying to hit
// gravity (float): force of gravity, positive down
//
// s0 (out Vector3): firing solution (low angle) 
// s1 (out Vector3): firing solution (high angle)
//
// return (int): number of unique solutions found: 0, 1, or 2.
int solve_ballistic_arc1(vec3_t proj_pos, float proj_speed, vec3_t target, float gravity, vec3_t s0, vec3_t s1) {

    // Handling these cases is up to your project's coding standards
    //Debug.Assert(proj_pos != target && proj_speed > 0 && gravity > 0, "fts.solve_ballistic_arc called with invalid data");

    // C# requires out variables be set
    VectorClear(s0);
    VectorClear(s1);

    // Derivation
    //   (1) x = v*t*cos O
    //   (2) y = v*t*sin O - .5*g*t^2
    // 
    //   (3) t = x/(cos O*v)                                        [solve t from (1)]
    //   (4) y = v*x*sin O/(cos O * v) - .5*g*x^2/(cos^2 O*v^2)     [plug t into y=...]
    //   (5) y = x*tan O - g*x^2/(2*v^2*cos^2 O)                    [reduce; cos/sin = tan]
    //   (6) y = x*tan O - (g*x^2/(2*v^2))*(1+tan^2 O)              [reduce; 1+tan O = 1/cos^2 O]
    //   (7) 0 = ((-g*x^2)/(2*v^2))*tan^2 O + x*tan O - (g*x^2)/(2*v^2) - y    [re-arrange]
    //   Quadratic! a*p^2 + b*p + c where p = tan O
    //
    //   (8) let gxv = -g*x*x/(2*v*v)
    //   (9) p = (-x +- sqrt(x*x - 4gxv*(gxv - y)))/2*gxv           [quadratic formula]
    //   (10) p = (v^2 +- sqrt(v^4 - g(g*x^2 + 2*y*v^2)))/gx        [multiply top/bottom by -2*v*v/x; move 4*v^4/x^2 into root]
    //   (11) O = atan(p)

    vec3_t diff;
    VectorSubtract(target, proj_pos, diff);
    vec3_t diffXZ = { diff[0] * (1.0f - vec3_up[0]), diff[1] * (1.0f - vec3_up[1]), diff[2] * (1.0f - vec3_up[2]) };
    float groundDist = VectorLength(diffXZ);

    float speed2 = proj_speed*proj_speed;
    float speed4 = proj_speed*proj_speed*proj_speed*proj_speed;
    float y = diff[2];
    float x = groundDist;
    float gx = gravity*x;

    float root = speed4 - gravity*(gravity*x*x + 2*y*speed2);

    // No solution
    if (root < 0)
        return 0;

    root = sqrt(root);

    float lowAng = atan2(speed2 - root, gx);
    float highAng = atan2(speed2 + root, gx);
    int numSolutions = lowAng != highAng ? 2 : 1;

    vec3_t groundDir;
    VectorNormalize2(diffXZ, groundDir);

    float lowAngCos = cos(lowAng), lowAngSin = sin(lowAng);
    float highAngCos = cos(highAng), highAngSin = sin(highAng);

    for (int i = 0; i < 3; i++)
        s0[i] = groundDir[i]*lowAngCos*proj_speed + (i == 2 ? 1 : 0)*lowAngSin*proj_speed;
    if (numSolutions > 1)
        for (int i = 0; i < 3; i++)
            s1[i] = groundDir[i]*highAngCos*proj_speed + (i == 2 ? 1 : 0)*highAngSin*proj_speed;

    return numSolutions;
}

static int compare_double(const void *a, const void *b)
{
    const double *da = a;
    const double *db = b;

    return *da == *db ? 0 : *da < *db ? -1 : 1;
}

// Solve firing angles for a ballistic projectile with speed and gravity to hit a target moving with constant, linear velocity.
//
// proj_pos (Vector3): point projectile will fire from
// proj_speed (float): scalar speed of projectile
// target (Vector3): point projectile is trying to hit
// target_velocity (Vector3): velocity of target
// gravity (float): force of gravity, positive down
//
// s0 (out Vector3): firing solution (fastest time impact) 
// s1 (out Vector3): firing solution (next impact)
// s2 (out Vector3): firing solution (next impact)
// s3 (out Vector3): firing solution (next impact)
//
// return (int): number of unique solutions found: 0, 1, 2, 3, or 4.
int solve_ballistic_arc2(vec3_t proj_pos, float proj_speed, vec3_t target_pos, vec3_t target_velocity, float gravity, vec3_t s0, vec3_t s1) {

    // Initialize output parameters
    VectorClear(s0);
    VectorClear(s1);

    // Derivation 
    //
    //  For full derivation see: blog.forrestthewoods.com
    //  Here is an abbreviated version.
    //
    //  Four equations, four unknowns (solution.x, solution.y, solution.z, time):
    //
    //  (1) proj_pos.x + solution.x*time = target_pos.x + target_vel.x*time
    //  (2) proj_pos.y + solution.y*time + .5*G*t = target_pos.y + target_vel.y*time
    //  (3) proj_pos.z + solution.z*time = target_pos.z + target_vel.z*time
    //  (4) proj_speed^2 = solution.x^2 + solution.y^2 + solution.z^2
    //
    //  (5) Solve for solution.x and solution.z in equations (1) and (3)
    //  (6) Square solution.x and solution.z from (5)
    //  (7) Solve solution.y^2 by plugging (6) into (4)
    //  (8) Solve solution.y by rearranging (2)
    //  (9) Square (8)
    //  (10) Set (8) = (7). All solution.xyz terms should be gone. Only time remains.
    //  (11) Rearrange 10. It will be of the form a*^4 + b*t^3 + c*t^2 + d*t * e. This is a quartic.
    //  (12) Solve the quartic using SolveQuartic.
    //  (13) If there are no positive, real roots there is no solution.
    //  (14) Each positive, real root is one valid solution
    //  (15) Plug each time value into (1) (2) and (3) to calculate solution.xyz
    //  (16) The end.

    double G = gravity;

    double A = proj_pos[0];
    double B = proj_pos[1];
    double C = proj_pos[2];
    double M = target_pos[0];
    double N = target_pos[1];
    double O = target_pos[2];
    double P = target_velocity[0];
    double Q = target_velocity[1];
    double R = target_velocity[2];
    double S = proj_speed;

    double H = M - A;
    double J = O - C;
    double K = N - B;
    double L = -.5f * G;

    // Quartic Coeffecients
    double c0 = L*L;
    double c1 = -2*Q*L;
    double c2 = Q*Q - 2*K*L - S*S + P*P + R*R;
    double c3 = 2*K*Q + 2*H*P + 2*J*R;
    double c4 = K*K + H*H + J*J;

    // Solve quartic
    double times[4];
    int numTimes = SolveQuartic(c0, c1, c2, c3, c4, &times[0], &times[1], &times[2], &times[3]);

    // Sort so faster collision is found first
    qsort(times, 4, sizeof(*times), compare_double);
    //System.Array.Sort(times);

    // Plug quartic solutions into base equations
    // There should never be more than 2 positive, real roots.
    vec3_t solutions[2];
    int numSolutions = 0;

    for (int i = 0; i < q_countof(times) && numSolutions < 2; ++i) {
        double t = times[i];
        if (t <= 0 || isnan(t))
            continue;

        solutions[numSolutions][0] = (float)((H+P*t)/t);
        solutions[numSolutions][1] = (float)((K+Q*t-L*t*t)/ t);
        solutions[numSolutions][2] = (float)((J+R*t)/t);
        ++numSolutions;
    }

    // Write out solutions
    if (numSolutions > 0)   VectorCopy(solutions[0], s0);
    if (numSolutions > 1)   VectorCopy(solutions[1], s1);

    return numSolutions;
}



// Solve the firing arc with a fixed lateral speed. Vertical speed and gravity varies. 
// This enables a visually pleasing arc.
//
// proj_pos (Vector3): point projectile will fire from
// lateral_speed (float): scalar speed of projectile along XZ plane
// target_pos (Vector3): point projectile is trying to hit
// max_height (float): height above Max(proj_pos, impact_pos) for projectile to peak at
//
// fire_velocity (out Vector3): firing velocity
// gravity (out float): gravity necessary to projectile to hit precisely max_height
//
// return (bool): true if a valid solution was found
bool solve_ballistic_arc_lateral1(vec3_t proj_pos, float lateral_speed, vec3_t target_pos, float max_height, vec3_t fire_velocity, float *gravity) {

    // Handling these cases is up to your project's coding standards
    //Debug.Assert(proj_pos != target_pos && lateral_speed > 0 && max_height > proj_pos.y, "fts.solve_ballistic_arc called with invalid data");

    VectorClear(fire_velocity);
    *gravity = NAN;

    vec3_t diff;
    VectorSubtract(target_pos, proj_pos, diff);
    vec3_t diffXZ = { diff[0] * (1.0f - vec3_up[0]), diff[1] * (1.0f - vec3_up[1]), diff[2] * (1.0f - vec3_up[2]) };
    float lateralDist = VectorLength(diffXZ);

    if (lateralDist == 0)
        return false;

    float time = lateralDist / lateral_speed;

    VectorNormalize2(diffXZ, fire_velocity);
    VectorScale(fire_velocity, lateral_speed, fire_velocity);

    // System of equations. Hit max_height at t=.5*time. Hit target at t=time.
    //
    // peak = y0 + vertical_speed*halfTime + .5*gravity*halfTime^2
    // end = y0 + vertical_speed*time + .5*gravity*time^s
    // Wolfram Alpha: solve b = a + .5*v*t + .5*g*(.5*t)^2, c = a + vt + .5*g*t^2 for g, v
    float a = proj_pos[2];       // initial
    float b = max_height;       // peak
    float c = target_pos[2];     // final

    *gravity = -4*(a - 2*b + c) / (time* time);
    fire_velocity[2] = -(3*a - 4*b + c) / time;

    return true;
}

// Solve the firing arc with a fixed lateral speed. Vertical speed and gravity varies. 
// This enables a visually pleasing arc.
//
// proj_pos (Vector3): point projectile will fire from
// lateral_speed (float): scalar speed of projectile along XZ plane
// target_pos (Vector3): point projectile is trying to hit
// max_height (float): height above Max(proj_pos, impact_pos) for projectile to peak at
//
// fire_velocity (out Vector3): firing velocity
// gravity (out float): gravity necessary to projectile to hit precisely max_height
// impact_point (out Vector3): point where moving target will be hit
//
// return (bool): true if a valid solution was found
bool solve_ballistic_arc_lateral2(vec3_t proj_pos, float lateral_speed, vec3_t target, vec3_t target_velocity, float max_height_offset, vec3_t fire_velocity, float *gravity, vec3_t impact_point) {

    // Handling these cases is up to your project's coding standards
    //Debug.Assert(proj_pos != target && lateral_speed > 0, "fts.solve_ballistic_arc_lateral called with invalid data");

    // Initialize output variables
    VectorClear(fire_velocity);
    *gravity = 0.f;
    VectorClear(impact_point);

    // Ground plane terms
    vec3_t targetVelXZ = { target_velocity[0] * (1.0f - vec3_up[0]), target_velocity[1] * (1.0f - vec3_up[1]), target_velocity[2] * (1.0f - vec3_up[2]) };
    vec3_t diffXZ;
    VectorSubtract(target, proj_pos, diffXZ);
    diffXZ[2] = 0;

    // Derivation
    //   (1) Base formula: |P + V*t| = S*t
    //   (2) Substitute variables: |diffXZ + targetVelXZ*t| = S*t
    //   (3) Square both sides: Dot(diffXZ,diffXZ) + 2*Dot(diffXZ, targetVelXZ)*t + Dot(targetVelXZ, targetVelXZ)*t^2 = S^2 * t^2
    //   (4) Quadratic: (Dot(targetVelXZ,targetVelXZ) - S^2)t^2 + (2*Dot(diffXZ, targetVelXZ))*t + Dot(diffXZ, diffXZ) = 0
    float c0 = DotProduct(targetVelXZ, targetVelXZ) - lateral_speed*lateral_speed;
    float c1 = 2.f * DotProduct(diffXZ, targetVelXZ);
    float c2 = DotProduct(diffXZ, diffXZ);
    double t0, t1;
    int n = SolveQuadric(c0, c1, c2, &t0, &t1);

    // pick smallest, positive time
    bool valid0 = n > 0 && t0 > 0;
    bool valid1 = n > 1 && t1 > 0;

    float t;
    if (!valid0 && !valid1)
        return false;
    else if (valid0 && valid1)
        t = min((float)t0, (float)t1);
    else
        t = valid0 ? (float)t0 : (float)t1;

    // Calculate impact point
    //impact_point = target + (target_velocity*t);
    VectorMA(target, t, target_velocity, impact_point);

    // Calculate fire velocity along XZ plane
    vec3_t dir;
    VectorSubtract(impact_point, proj_pos, dir);
    VectorSet(fire_velocity, dir[0] * (1.0f - vec3_up[0]), dir[1] * (1.0f - vec3_up[1]), dir[2] * (1.0f - vec3_up[2]));
    VectorNormalize(fire_velocity);
    VectorScale(fire_velocity, lateral_speed, fire_velocity);

    // Solve system of equations. Hit max_height at t=.5*time. Hit target at t=time.
    //
    // peak = y0 + vertical_speed*halfTime + .5*gravity*halfTime^2
    // end = y0 + vertical_speed*time + .5*gravity*time^s
    // Wolfram Alpha: solve b = a + .5*v*t + .5*g*(.5*t)^2, c = a + vt + .5*g*t^2 for g, v
    float a = proj_pos[2];       // initial
    float b = max(proj_pos[2], impact_point[2]) + max_height_offset;  // peak
    float c = impact_point[2];   // final

    *gravity = -4*(a - 2*b + c) / (t* t);
    fire_velocity[2] = -(3*a - 4*b + c) / t;

    return true;
}