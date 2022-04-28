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

typedef struct m_iqm_s m_iqm_t;

m_iqm_t *M_InitializeIQM(const char *name);
void M_FreeIQM(m_iqm_t *iqm);
void M_SetupIQMAnimation(const m_iqm_t *iqm, mmove_t *animations, bool absolute);
void M_SetupIQMAnimations(const m_iqm_t *iqm, mmove_t *animations[]);

#define ANIMATION(name, length) \
    FRAME_##name##_FIRST, \
    FRAME_##name##_COUNT = length, \
    FRAME_##name##_LAST = (FRAME_##name##_FIRST + FRAME_##name##_COUNT) - 1

bool solve_ballistic_arc_lateral2(vec3_t proj_pos, float lateral_speed, vec3_t target, vec3_t target_velocity, float max_height_offset, vec3_t fire_velocity, float *gravity, vec3_t impact_point);
bool solve_ballistic_arc_lateral1(vec3_t proj_pos, float lateral_speed, vec3_t target_pos, float max_height, vec3_t fire_velocity, float *gravity);
int solve_ballistic_arc2(vec3_t proj_pos, float proj_speed, vec3_t target_pos, vec3_t target_velocity, float gravity, vec3_t s0, vec3_t s1);
int solve_ballistic_arc1(vec3_t proj_pos, float proj_speed, vec3_t target, float gravity, vec3_t s0, vec3_t s1);
