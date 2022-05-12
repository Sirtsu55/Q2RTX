/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2008 Andrey Nazarov
Copyright (C) 2019, NVIDIA CORPORATION. All rights reserved.

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

#pragma once

typedef struct
{
	vec3_t translate;
	quat_t rotate;
	vec3_t scale;
} iqm_transform_t;

typedef struct
{
	char name[MAX_QPATH];
	uint32_t first_frame;
	uint32_t num_frames;
	bool loop;
} iqm_anim_t;

typedef char joint_name_t[MAX_QPATH];

// inter-quake-model
typedef struct
{
	uint32_t num_vertexes;
	uint32_t num_triangles;
	uint32_t num_frames;
	uint32_t num_meshes;
	uint32_t num_joints;
	uint32_t num_poses;
	uint32_t num_animations;
	struct iqm_mesh_s *meshes; // only used for renderer

	uint32_t *indices;

	// vertex arrays
	float *positions;
	float *texcoords;
	float *normals;
	float *tangents;
	byte *colors;
	byte *blend_indices; // byte4 per vertex
	byte *blend_weights; // byte4 per vertex

	joint_name_t *jointNames; // [num_joints * MAX_QPATH]
	int *jointParents;
	float *bindJoints; // [num_joints * 12]
	float *invBindJoints; // [num_joints * 12]
	iqm_transform_t *poses; // [num_frames * num_poses]
	float *bounds;

	iqm_anim_t *animations;

	// NAC stuff
	// Root bone ID
	int32_t root_id;
} iqm_model_t;

// inter-quake-model mesh
typedef struct iqm_mesh_s
{
	char name[MAX_QPATH];
	char material[MAX_QPATH];
	iqm_model_t *data;
	uint32_t first_vertex, num_vertexes;
	uint32_t first_triangle, num_triangles;
	uint32_t first_influence, num_influences;
} iqm_mesh_t;

int MOD_LoadIQM_Base(iqm_model_t *mod, const void *rawdata, size_t length, const char *mod_name, void *(*allocate) (void *arg, size_t size), void *allocate_arg);