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

#include <common/error.h>
#include <shared/shared.h>
#include <format/iqm.h>
#include <shared/iqm.h>
#include <assert.h>

#define CHECK(x)    if (!(x)) { ret = Q_ERR(ENOMEM); goto fail; }

static bool IQM_CheckRange(const iqmHeader_t *header, uint32_t offset, uint32_t count, size_t size)
{
	// return true if the range specified by offset, count and size
	// doesn't fit into the file
	return (count == 0 ||
		offset > header->filesize ||
		offset + count * size > header->filesize);
}

static vec_t QuatNormalize2(const quat_t v, quat_t out)
{
	float length = v[0] * v[0] + v[1] * v[1] + v[2] * v[2] + v[3] * v[3];

	if (length > 0.f)
	{
		/* writing it this way allows gcc to recognize that rsqrt can be used */
		float ilength = 1 / sqrtf(length);
		/* sqrt(length) = length * (1 / sqrt(length)) */
		length *= ilength;
		out[0] = v[0] * ilength;
		out[1] = v[1] * ilength;
		out[2] = v[2] * ilength;
		out[3] = v[3] * ilength;
	}
	else
	{
		out[0] = out[1] = out[2] = 0;
		out[3] = -1;
	}

	return length;
}

static void Matrix34Invert(const float *inMat, float *outMat)
{
	outMat[0] = inMat[0]; outMat[1] = inMat[4]; outMat[2] = inMat[8];
	outMat[4] = inMat[1]; outMat[5] = inMat[5]; outMat[6] = inMat[9];
	outMat[8] = inMat[2]; outMat[9] = inMat[6]; outMat[10] = inMat[10];

	float invSqrLen, *v;
	v = outMat + 0; invSqrLen = 1.0f / DotProduct(v, v); VectorScale(v, invSqrLen, v);
	v = outMat + 4; invSqrLen = 1.0f / DotProduct(v, v); VectorScale(v, invSqrLen, v);
	v = outMat + 8; invSqrLen = 1.0f / DotProduct(v, v); VectorScale(v, invSqrLen, v);

	vec3_t trans;
	trans[0] = inMat[3];
	trans[1] = inMat[7];
	trans[2] = inMat[11];

	outMat[3] = -DotProduct(outMat + 0, trans);
	outMat[7] = -DotProduct(outMat + 4, trans);
	outMat[11] = -DotProduct(outMat + 8, trans);
}

// ReSharper disable CppClangTidyClangDiagnosticCastAlign

/*
=================
MOD_LoadIQM_Base

Load an IQM model and compute the joint poses for every frame.
=================
*/
int MOD_LoadIQM_Base(iqm_model_t *iqmData, const void *rawdata, size_t length, const char *mod_name, void *(*allocate) (void *arg, size_t size), void *allocate_arg)
{
	iqm_transform_t *transform;
	float *mat, *matInv;
	char meshName[MAX_QPATH];
	int vertexArrayFormat[IQM_COLOR + 1];
	int ret;

	if (length < sizeof(iqmHeader_t))
	{
		return Q_ERR_FILE_TOO_SMALL;
	}

	const iqmHeader_t *header = rawdata;
	if (strncmp(header->magic, IQM_MAGIC, sizeof(header->magic)) != 0)
	{
		return Q_ERR_INVALID_FORMAT;
	}

	if (header->version != IQM_VERSION)
	{
		Com_WPrintf("R_LoadIQM: %s is a unsupported IQM version (%d), only version %d is supported.\n",
			mod_name, header->version, IQM_VERSION);
		return Q_ERR_UNKNOWN_FORMAT;
	}

	if (header->filesize > length || header->filesize > 16 << 20)
	{
		return Q_ERR_FILE_TOO_SMALL;
	}

	// check ioq3 joint limit
	if (header->num_joints > IQM_MAX_JOINTS)
	{
		Com_WPrintf("R_LoadIQM: %s has more than %d joints (%d).\n",
			mod_name, IQM_MAX_JOINTS, header->num_joints);
		return Q_ERR_INVALID_FORMAT;
	}

	for (uint32_t vertexarray_idx = 0; vertexarray_idx < q_countof(vertexArrayFormat); vertexarray_idx++)
	{
		vertexArrayFormat[vertexarray_idx] = -1;
	}

	if (header->num_meshes)
	{
		// check vertex arrays
		if (IQM_CheckRange(header, header->ofs_vertexarrays, header->num_vertexarrays, sizeof(iqmVertexArray_t)))
		{
			return Q_ERR_BAD_EXTENT;
		}
		const iqmVertexArray_t *vertexarray = (const iqmVertexArray_t *) ((const byte *) header + header->ofs_vertexarrays);
		for (uint32_t vertexarray_idx = 0; vertexarray_idx < header->num_vertexarrays; vertexarray_idx++, vertexarray++)
		{
			if (vertexarray->size <= 0 || vertexarray->size > 4)
			{
				return Q_ERR_INVALID_FORMAT;
			}

			uint32_t num_values = header->num_vertexes * vertexarray->size;

			switch (vertexarray->format)
			{
				case IQM_BYTE:
				case IQM_UBYTE:
					// 1-byte
					if (IQM_CheckRange(header, vertexarray->offset, num_values, sizeof(byte)))
					{
						return Q_ERR_BAD_EXTENT;
					}
					break;
				case IQM_INT:
				case IQM_UINT:
				case IQM_FLOAT:
					// 4-byte
					if (IQM_CheckRange(header, vertexarray->offset, num_values, sizeof(float)))
					{
						return Q_ERR_BAD_EXTENT;
					}
					break;
				default:
					// not supported
					return Q_ERR_INVALID_FORMAT;
			}

			if (vertexarray->type < q_countof(vertexArrayFormat))
			{
				vertexArrayFormat[vertexarray->type] = (int) vertexarray->format;
			}

			switch (vertexarray->type)
			{
				case IQM_POSITION:
				case IQM_NORMAL:
					if (vertexarray->format != IQM_FLOAT ||
						vertexarray->size != 3)
					{
						return Q_ERR_INVALID_FORMAT;
					}
					break;
				case IQM_TANGENT:
					if (vertexarray->format != IQM_FLOAT ||
						vertexarray->size != 4)
					{
						return Q_ERR_INVALID_FORMAT;
					}
					break;
				case IQM_TEXCOORD:
					if (vertexarray->format != IQM_FLOAT ||
						vertexarray->size != 2)
					{
						return Q_ERR_INVALID_FORMAT;
					}
					break;
				case IQM_BLENDINDEXES:
					if ((vertexarray->format != IQM_INT &&
						vertexarray->format != IQM_UBYTE) ||
						vertexarray->size != 4)
					{
						return Q_ERR_INVALID_FORMAT;
					}
					break;
				case IQM_BLENDWEIGHTS:
					if ((vertexarray->format != IQM_FLOAT &&
						vertexarray->format != IQM_UBYTE) ||
						vertexarray->size != 4)
					{
						return Q_ERR_INVALID_FORMAT;
					}
					break;
				case IQM_COLOR:
					if (vertexarray->format != IQM_UBYTE ||
						vertexarray->size != 4)
					{
						return Q_ERR_INVALID_FORMAT;
					}
					break;
				default:
					break;
			}
		}

		// check for required vertex arrays
		if (vertexArrayFormat[IQM_POSITION] == -1 || vertexArrayFormat[IQM_NORMAL] == -1 || vertexArrayFormat[IQM_TEXCOORD] == -1)
		{
			Com_WPrintf("R_LoadIQM: %s is missing IQM_POSITION, IQM_NORMAL, and/or IQM_TEXCOORD array.\n", mod_name);
			return Q_ERR_INVALID_FORMAT;
		}

		if (header->num_joints)
		{
			if (vertexArrayFormat[IQM_BLENDINDEXES] == -1 || vertexArrayFormat[IQM_BLENDWEIGHTS] == -1)
			{
				Com_WPrintf("R_LoadIQM: %s is missing IQM_BLENDINDEXES and/or IQM_BLENDWEIGHTS array.\n", mod_name);
				return Q_ERR_INVALID_FORMAT;
			}
		}
		else
		{
			// ignore blend arrays if present
			vertexArrayFormat[IQM_BLENDINDEXES] = -1;
			vertexArrayFormat[IQM_BLENDWEIGHTS] = -1;
		}

		// check triangles
		if (IQM_CheckRange(header, header->ofs_triangles, header->num_triangles, sizeof(iqmTriangle_t)))
		{
			return Q_ERR_BAD_EXTENT;
		}
		const iqmTriangle_t *triangle = (const iqmTriangle_t *) ((const byte *) header + header->ofs_triangles);
		for (uint32_t triangle_idx = 0; triangle_idx < header->num_triangles; triangle_idx++, triangle++)
		{
			if (triangle->vertex[0] > header->num_vertexes ||
				triangle->vertex[1] > header->num_vertexes ||
				triangle->vertex[2] > header->num_vertexes)
			{
				return Q_ERR_INVALID_FORMAT;
			}
		}

		// check meshes
		if (IQM_CheckRange(header, header->ofs_meshes, header->num_meshes, sizeof(iqmMesh_t)))
		{
			return Q_ERR_BAD_EXTENT;
		}

		const iqmMesh_t *mesh = (const iqmMesh_t *) ((const byte *) header + header->ofs_meshes);
		for (uint32_t mesh_idx = 0; mesh_idx < header->num_meshes; mesh_idx++, mesh++)
		{
			if (mesh->name < header->num_text)
			{
				strncpy(meshName, (const char *) header + header->ofs_text + mesh->name, sizeof(meshName) - 1);
			}
			else
			{
				meshName[0] = '\0';
			}

			if (mesh->first_vertex >= header->num_vertexes ||
				mesh->first_vertex + mesh->num_vertexes > header->num_vertexes ||
				mesh->first_triangle >= header->num_triangles ||
				mesh->first_triangle + mesh->num_triangles > header->num_triangles ||
				mesh->name >= header->num_text ||
				mesh->material >= header->num_text)
			{
				return Q_ERR_INVALID_FORMAT;
			}
		}
	}

	if (header->num_poses != header->num_joints && header->num_poses != 0)
	{
		Com_WPrintf("R_LoadIQM: %s has %d poses and %d joints, must have the same number or 0 poses\n",
			mod_name, header->num_poses, header->num_joints);
		return Q_ERR_INVALID_FORMAT;
	}

	if (header->num_joints)
	{
		// check joints
		if (IQM_CheckRange(header, header->ofs_joints, header->num_joints, sizeof(iqmJoint_t)))
		{
			return Q_ERR_BAD_EXTENT;
		}

		const iqmJoint_t *joint = (const iqmJoint_t *) ((const byte *) header + header->ofs_joints);
		for (uint32_t joint_idx = 0; joint_idx < header->num_joints; joint_idx++, joint++)
		{
			if (joint->parent < -1 ||
				joint->parent >= (int) header->num_joints ||
				joint->name >= header->num_text)
			{
				return Q_ERR_INVALID_FORMAT;
			}
		}
	}

	if (header->num_poses)
	{
		// check poses
		if (IQM_CheckRange(header, header->ofs_poses, header->num_poses, sizeof(iqmPose_t)))
		{
			return Q_ERR_BAD_EXTENT;
		}
	}

	if (header->ofs_bounds)
	{
		// check model bounds
		if (IQM_CheckRange(header, header->ofs_bounds, header->num_frames, sizeof(iqmBounds_t)))
		{
			return Q_ERR_BAD_EXTENT;
		}
	}

	if (header->num_anims)
	{
		// check animations
		const iqmAnim_t *anim = (const iqmAnim_t *) ((const byte *) header + header->ofs_anims);
		for (uint32_t anim_idx = 0; anim_idx < header->num_anims; anim_idx++, anim++)
		{
			if (anim->first_frame + anim->num_frames > header->num_frames)
			{
				return Q_ERR_INVALID_FORMAT;
			}
		}
	}

	// fill header
	iqmData->num_vertexes = (header->num_meshes > 0) ? header->num_vertexes : 0;
	iqmData->num_triangles = (header->num_meshes > 0) ? header->num_triangles : 0;
	iqmData->num_frames = header->num_frames;
	iqmData->num_meshes = header->num_meshes;
	iqmData->num_joints = header->num_joints;
	iqmData->num_poses = header->num_poses;

	if (header->num_meshes)
	{
		CHECK(iqmData->meshes = allocate(allocate_arg, header->num_meshes * sizeof(iqm_mesh_t)));
		CHECK(iqmData->indices = allocate(allocate_arg, header->num_triangles * 3 * sizeof(int)));
		CHECK(iqmData->positions = allocate(allocate_arg, header->num_vertexes * 3 * sizeof(float)));
		CHECK(iqmData->texcoords = allocate(allocate_arg, header->num_vertexes * 2 * sizeof(float)));
		CHECK(iqmData->normals = allocate(allocate_arg, header->num_vertexes * 3 * sizeof(float)));

		if (vertexArrayFormat[IQM_TANGENT] != -1)
		{
			CHECK(iqmData->tangents = allocate(allocate_arg, header->num_vertexes * 4 * sizeof(float)));
		}

		if (vertexArrayFormat[IQM_COLOR] != -1)
		{
			CHECK(iqmData->colors = allocate(allocate_arg, header->num_vertexes * 4 * sizeof(byte)));
		}

		if (vertexArrayFormat[IQM_BLENDINDEXES] != -1)
		{
			CHECK(iqmData->blend_indices = allocate(allocate_arg, header->num_vertexes * 4 * sizeof(byte)));
		}

		if (vertexArrayFormat[IQM_BLENDWEIGHTS] != -1)
		{
			CHECK(iqmData->blend_weights = allocate(allocate_arg, header->num_vertexes * 4 * sizeof(byte)));
		}
	}

	if (header->num_joints)
	{
		CHECK(iqmData->jointNames = allocate(allocate_arg, header->num_joints * sizeof(joint_name_t)));
		CHECK(iqmData->jointParents = allocate(allocate_arg, header->num_joints * sizeof(int)));
		CHECK(iqmData->bindJoints = allocate(allocate_arg, header->num_joints * 12 * sizeof(float))); // bind joint matricies
		CHECK(iqmData->invBindJoints = allocate(allocate_arg, header->num_joints * 12 * sizeof(float))); // inverse bind joint matricies
	}

	if (header->num_poses)
	{
		CHECK(iqmData->poses = allocate(allocate_arg, header->num_poses * header->num_frames * sizeof(iqm_transform_t))); // pose transforms
	}

	if (header->ofs_bounds)
	{
		CHECK(iqmData->bounds = allocate(allocate_arg, header->num_frames * 6 * sizeof(float))); // model bounds
	}
	else if (header->num_meshes && header->num_frames == 0)
	{
		CHECK(iqmData->bounds = allocate(allocate_arg, 6 * sizeof(float))); // model bounds
	}

	if (header->num_meshes)
	{
		const iqmMesh_t *mesh = (const iqmMesh_t *) ((const byte *) header + header->ofs_meshes);
		iqm_mesh_t *surface = iqmData->meshes;
		const char *str = (const char *) header + header->ofs_text;
		for (uint32_t mesh_idx = 0; mesh_idx < header->num_meshes; mesh_idx++, mesh++, surface++)
		{
			strncpy(surface->name, str + mesh->name, sizeof(surface->name) - 1);
			Q_strlwr(surface->name); // lowercase the surface name so skin compares are faster
			strncpy(surface->material, str + mesh->material, sizeof(surface->material) - 1);
			Q_strlwr(surface->material);
			surface->data = iqmData;
			surface->first_vertex = mesh->first_vertex;
			surface->num_vertexes = mesh->num_vertexes;
			surface->first_triangle = mesh->first_triangle;
			surface->num_triangles = mesh->num_triangles;
		}

		// copy triangles
		const iqmTriangle_t *triangle = (const iqmTriangle_t *) ((const byte *) header + header->ofs_triangles);
		for (uint32_t i = 0; i < header->num_triangles; i++, triangle++)
		{
			iqmData->indices[3 * i + 0] = triangle->vertex[0];
			iqmData->indices[3 * i + 1] = triangle->vertex[1];
			iqmData->indices[3 * i + 2] = triangle->vertex[2];
		}

		// copy vertexarrays and indexes
		const iqmVertexArray_t *vertexarray = (const iqmVertexArray_t *) ((const byte *) header + header->ofs_vertexarrays);
		for (uint32_t vertexarray_idx = 0; vertexarray_idx < header->num_vertexarrays; vertexarray_idx++, vertexarray++)
		{
			// skip disabled arrays
			if (vertexarray->type < q_countof(vertexArrayFormat)
				&& vertexArrayFormat[vertexarray->type] == -1)
				continue;

			// total number of values
			uint32_t n = header->num_vertexes * vertexarray->size;

			switch (vertexarray->type)
			{
				case IQM_POSITION:
					memcpy(iqmData->positions,
						(const byte *) header + vertexarray->offset,
						n * sizeof(float));
					break;
				case IQM_NORMAL:
					memcpy(iqmData->normals,
						(const byte *) header + vertexarray->offset,
						n * sizeof(float));
					break;
				case IQM_TANGENT:
					memcpy(iqmData->tangents,
						(const byte *) header + vertexarray->offset,
						n * sizeof(float));
					break;
				case IQM_TEXCOORD:
					memcpy(iqmData->texcoords,
						(const byte *) header + vertexarray->offset,
						n * sizeof(float));
					break;
				case IQM_BLENDINDEXES:
					if (vertexArrayFormat[IQM_BLENDINDEXES] == IQM_UBYTE)
					{
						memcpy(iqmData->blend_indices,
							(const byte *) header + vertexarray->offset,
							n * sizeof(byte));
					}
					else if (vertexArrayFormat[IQM_BLENDINDEXES] == IQM_INT)
					{
						const int *indices = (const int *) ((const byte *) header + vertexarray->offset);

						// Convert blend indices from int to byte
						for (uint32_t index_idx = 0; index_idx < n; index_idx++)
						{
							int index = indices[index_idx];
							iqmData->blend_indices[index_idx] = (byte) index;
						}
					}
					else
					{
						// The formats are validated before loading the data.
						assert(!"Unsupported IQM_BLENDINDEXES format");
						memset(iqmData->blend_indices, 0, n * sizeof(byte));
					}
					break;
				case IQM_BLENDWEIGHTS:
					if (vertexArrayFormat[IQM_BLENDWEIGHTS] == IQM_UBYTE)
					{
						memcpy(iqmData->blend_weights,
							(const byte *) header + vertexarray->offset,
							n * sizeof(byte));
					}
					else if (vertexArrayFormat[IQM_BLENDWEIGHTS] == IQM_FLOAT)
					{
						const float *weights = (const float *) ((const byte *) header + vertexarray->offset);

						// Convert blend weights from float to byte
						for (uint32_t weight_idx = 0; weight_idx < n; weight_idx++)
						{
							float integer_weight = weights[weight_idx] * 255.f;
							clamp(integer_weight, 0.f, 255.f);
							iqmData->blend_weights[weight_idx] = (byte) integer_weight;
						}
					}
					else
					{
						// The formats are validated before loading the data.
						assert(!"Unsupported IQM_BLENDWEIGHTS format");
						memset(iqmData->blend_weights, 0, n * sizeof(byte));
					}
					break;
				case IQM_COLOR:
					memcpy(iqmData->colors,
						(const byte *) header + vertexarray->offset,
						n * sizeof(byte));
					break;
				default:
					break;
			}
		}
	}

	if (header->num_joints)
	{
		// copy joint names
		joint_name_t *str = iqmData->jointNames;
		const iqmJoint_t *joint = (const iqmJoint_t *) ((const byte *) header + header->ofs_joints);
		for (uint32_t joint_idx = 0; joint_idx < header->num_joints; joint_idx++, joint++)
		{
			const char *name = (const char *) header + header->ofs_text + joint->name;
			Q_strlcpy((char *) str, name, sizeof(*str));
			str++;
		}

		// copy joint parents
		joint = (const iqmJoint_t *) ((const byte *) header + header->ofs_joints);
		for (uint32_t joint_idx = 0; joint_idx < header->num_joints; joint_idx++, joint++)
		{
			iqmData->jointParents[joint_idx] = joint->parent;
		}

		// calculate bind joint matrices and their inverses
		mat = iqmData->bindJoints;
		matInv = iqmData->invBindJoints;
		joint = (const iqmJoint_t *) ((const byte *) header + header->ofs_joints);
		for (uint32_t joint_idx = 0; joint_idx < header->num_joints; joint_idx++, joint++)
		{
			float baseFrame[12], invBaseFrame[12];

			quat_t rotate;
			QuatNormalize2(joint->rotate, rotate);

			JointToMatrix(rotate, joint->scale, joint->translate, baseFrame);
			Matrix34Invert(baseFrame, invBaseFrame);

			if (joint->parent >= 0)
			{
				Matrix34Multiply(iqmData->bindJoints + 12 * joint->parent, baseFrame, mat);
				mat += 12;
				Matrix34Multiply(invBaseFrame, iqmData->invBindJoints + 12 * joint->parent, matInv);
				matInv += 12;
			}
			else
			{
				memcpy(mat, baseFrame, sizeof(baseFrame));
				mat += 12;
				memcpy(matInv, invBaseFrame, sizeof(invBaseFrame));
				matInv += 12;
			}
		}
	}

	if (header->num_poses)
	{
		// calculate pose transforms
		transform = iqmData->poses;
		const uint16_t *framedata = (const uint16_t *) ((const byte *) header + header->ofs_frames);
		for (uint32_t frame_idx = 0; frame_idx < header->num_frames; frame_idx++)
		{
			const iqmPose_t *pose = (const iqmPose_t *) ((const byte *) header + header->ofs_poses);
			for (uint32_t pose_idx = 0; pose_idx < header->num_poses; pose_idx++, pose++, transform++)
			{
				vec3_t translate;
				quat_t rotate;
				vec3_t scale;

				translate[0] = pose->channeloffset[0]; if (pose->mask & 0x001) translate[0] += (float) *framedata++ * pose->channelscale[0];
				translate[1] = pose->channeloffset[1]; if (pose->mask & 0x002) translate[1] += (float) *framedata++ * pose->channelscale[1];
				translate[2] = pose->channeloffset[2]; if (pose->mask & 0x004) translate[2] += (float) *framedata++ * pose->channelscale[2];

				rotate[0] = pose->channeloffset[3]; if (pose->mask & 0x008) rotate[0] += (float) *framedata++ * pose->channelscale[3];
				rotate[1] = pose->channeloffset[4]; if (pose->mask & 0x010) rotate[1] += (float) *framedata++ * pose->channelscale[4];
				rotate[2] = pose->channeloffset[5]; if (pose->mask & 0x020) rotate[2] += (float) *framedata++ * pose->channelscale[5];
				rotate[3] = pose->channeloffset[6]; if (pose->mask & 0x040) rotate[3] += (float) *framedata++ * pose->channelscale[6];

				scale[0] = pose->channeloffset[7]; if (pose->mask & 0x080) scale[0] += (float) *framedata++ * pose->channelscale[7];
				scale[1] = pose->channeloffset[8]; if (pose->mask & 0x100) scale[1] += (float) *framedata++ * pose->channelscale[8];
				scale[2] = pose->channeloffset[9]; if (pose->mask & 0x200) scale[2] += (float) *framedata++ * pose->channelscale[9];

				VectorCopy(translate, transform->translate);
				QuatNormalize2(rotate, transform->rotate);
				VectorCopy(scale, transform->scale);
			}
		}
	}

	// copy model bounds
	if (header->ofs_bounds)
	{
		mat = iqmData->bounds;
		const iqmBounds_t *bounds = (const iqmBounds_t *) ((const byte *) header + header->ofs_bounds);
		for (uint32_t frame_idx = 0; frame_idx < header->num_frames; frame_idx++)
		{
			mat[0] = bounds->bbmin[0];
			mat[1] = bounds->bbmin[1];
			mat[2] = bounds->bbmin[2];
			mat[3] = bounds->bbmax[0];
			mat[4] = bounds->bbmax[1];
			mat[5] = bounds->bbmax[2];

			mat += 6;
			bounds++;
		}
	}
	else if (header->num_meshes && header->num_frames == 0)
	{
		mat = iqmData->bounds;

		ClearBounds(&iqmData->bounds[0], &iqmData->bounds[3]);
		for (uint32_t vertex_idx = 0; vertex_idx < header->num_vertexes; vertex_idx++)
		{
			AddPointToBounds(&iqmData->positions[vertex_idx * 3], &iqmData->bounds[0], &iqmData->bounds[3]);
		}
	}

	if (header->num_anims)
	{
		iqmData->num_animations = header->num_anims;
		CHECK(iqmData->animations = allocate(allocate_arg, header->num_anims * sizeof(iqm_anim_t)));

		const iqmAnim_t *src = (const iqmAnim_t *) ((const byte *) header + header->ofs_anims);
		iqm_anim_t *dst = iqmData->animations;
		for (uint32_t anim_idx = 0; anim_idx < header->num_anims; anim_idx++, src++, dst++)
		{
			const char *name = (const char *) header + header->ofs_text + src->name;
			strncpy(dst->name, name, sizeof(dst->name));
			dst->name[sizeof(dst->name) - 1] = 0;

			dst->first_frame = src->first_frame;
			dst->num_frames = src->num_frames;
			dst->loop = (src->flags & IQM_LOOP) != 0;
		}
	}

	return Q_ERR_SUCCESS;

fail:
	return ret;
}