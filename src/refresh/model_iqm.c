/*
===========================================================================
Copyright (C) 2011 Thilo Schulz <thilo@tjps.eu>
Copyright (C) 2011 Matthias Bentrup <matthias.bentrup@googlemail.com>
Copyright (C) 2011-2019 Zack Middleton <zturtleman@gmail.com>

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include <assert.h>
#include <shared/shared.h>
#include <format/iqm.h>
#include <refresh/models.h>
#include <refresh/refresh.h>

static void QuatSlerp(const quat_t from, const quat_t _to, float fraction, quat_t out)
{
	// cos() of angle
	float cosAngle = from[0] * _to[0] + from[1] * _to[1] + from[2] * _to[2] + from[3] * _to[3];

	// negative handling is needed for taking shortest path (required for model joints)
	quat_t to;
	if (cosAngle < 0.0f)
	{
		cosAngle = -cosAngle;
		to[0] = -_to[0];
		to[1] = -_to[1];
		to[2] = -_to[2];
		to[3] = -_to[3];
	}
	else
	{
		QuatCopy(_to, to);
	}

	float backlerp, lerp;
	if (cosAngle < 0.999999f)
	{
		// spherical lerp (slerp)
		const float angle = acosf(cosAngle);
		const float sinAngle = sinf(angle);
		backlerp = sinf((1.0f - fraction) * angle) / sinAngle;
		lerp = sinf(fraction * angle) / sinAngle;
	}
	else
	{
		// linear lerp
		backlerp = 1.0f - fraction;
		lerp = fraction;
	}

	out[0] = from[0] * backlerp + to[0] * lerp;
	out[1] = from[1] * backlerp + to[1] * lerp;
	out[2] = from[2] * backlerp + to[2] * lerp;
	out[3] = from[3] * backlerp + to[3] * lerp;
}

static inline void QuatRotateX(quat_t out, quat_t a, float rad)
{
	rad *= 0.5;

	float ax = a[0],
		ay = a[1],
		az = a[2],
		aw = a[3];
	float bx = sinf(rad),
		bw = cosf(rad);

	out[0] = ax * bw + aw * bx;
	out[1] = ay * bw + az * bx;
	out[2] = az * bw - ay * bx;
	out[3] = aw * bw - ax * bx;
}

static inline void QuatRotateY(quat_t out, quat_t a, float rad) {
	rad *= 0.5;

	float ax = a[0],
		ay = a[1],
		az = a[2],
		aw = a[3];
	float by = sinf(rad),
		bw = cosf(rad);

	out[0] = ax * bw - az * by;
	out[1] = ay * bw + aw * by;
	out[2] = az * bw + ax * by;
	out[3] = aw * bw - ay * by;
}

static inline void QuatRotateZ(quat_t out, quat_t a, float rad) {
	rad *= 0.5;

	float ax = a[0],
		ay = a[1],
		az = a[2],
		aw = a[3];
	float bz = sinf(rad),
		bw = cosf(rad);

	out[0] = ax * bw + ay * bz;
	out[1] = ay * bw - ax * bz;
	out[2] = az * bw + aw * bz;
	out[3] = aw * bw - az * bz;
}

static inline void QuatMultiply(quat_t out, quat_t a, quat_t b)
{
	float ax = a[0],
		ay = a[1],
		az = a[2],
		aw = a[3];
	float bx = b[0],
		by = b[1],
		bz = b[2],
		bw = b[3];

	out[0] = ax * bw + aw * bx + ay * bz - az * by;
	out[1] = ay * bw + aw * by + az * bx - ax * bz;
	out[2] = az * bw + aw * bz + ax * by - ay * bx;
	out[3] = aw * bw - ax * bx - ay * by - az * bz;
}

/*
=================
R_ComputeIQMTransforms

Compute matrices for this model, returns [model->num_poses] 3x4 matrices in the (pose_matrices) array
=================
*/
bool R_ComputeIQMTransforms(const iqm_model_t* model, const entity_t* entity, float* pose_matrices, refdef_t *fd)
{
	iqm_transform_t relativeJoints[IQM_MAX_JOINTS];

	iqm_transform_t* relativeJoint = relativeJoints;

	const int frame = model->num_frames ? entity->frame % (int)model->num_frames : 0;
	const int oldframe = model->num_frames ? entity->oldframe % (int)model->num_frames : 0;
	const float backlerp = entity->backlerp;

	// SPIN
	int32_t spin_id = -1, skip_id = -1;
	static float spin_angle = 0;
	
	for (uint32_t i = 0; i < model->num_joints; i++)
	{
		if (Q_strcasecmp(model->jointNames[i], "spin") == 0)
			spin_id = i;
		// SKIP
		else if (strlen(model->jointNames[i]) >= 4 && Q_strcasecmp(model->jointNames[i] + strlen(model->jointNames[i]) - 4, "root") == 0)
			skip_id = i;
	}

	quat_t spin_quat = { 0, 0, 0, 1 };

	if (spin_id != -1 && entity->spin_angle)
		QuatRotateY(spin_quat, spin_quat, entity->spin_angle);
	// SPIN

	// copy or lerp animation frame pose
	if (oldframe == frame)
	{
		const iqm_transform_t* pose = &model->poses[frame * model->num_poses];
		for (uint32_t pose_idx = 0; pose_idx < model->num_poses; pose_idx++, pose++, relativeJoint++)
		{
			if (pose_idx == skip_id)
			{
				VectorClear(relativeJoint->translate);
				VectorSet(relativeJoint->scale, 1, 1, 1);
				QuatCopy(pose->rotate, relativeJoint->rotate);
				continue;
			}

			VectorCopy(pose->translate, relativeJoint->translate);
			VectorCopy(pose->scale, relativeJoint->scale);
			QuatCopy(pose->rotate, relativeJoint->rotate);

			if (pose_idx == spin_id)
				QuatMultiply(relativeJoint->rotate, relativeJoint->rotate, spin_quat);
		}
	}
	else
	{
		const float lerp = 1.0f - backlerp;
		const iqm_transform_t* pose = &model->poses[frame * model->num_poses];
		const iqm_transform_t* oldpose = &model->poses[oldframe * model->num_poses];
		for (uint32_t pose_idx = 0; pose_idx < model->num_poses; pose_idx++, oldpose++, pose++, relativeJoint++)
		{
			if (pose_idx == skip_id)
			{
				VectorClear(relativeJoint->translate);
				VectorSet(relativeJoint->scale, 1, 1, 1);
				QuatCopy(pose->rotate, relativeJoint->rotate);
				continue;
			}

			relativeJoint->translate[0] = oldpose->translate[0] * backlerp + pose->translate[0] * lerp;
			relativeJoint->translate[1] = oldpose->translate[1] * backlerp + pose->translate[1] * lerp;
			relativeJoint->translate[2] = oldpose->translate[2] * backlerp + pose->translate[2] * lerp;

			relativeJoint->scale[0] = oldpose->scale[0] * backlerp + pose->scale[0] * lerp;
			relativeJoint->scale[1] = oldpose->scale[1] * backlerp + pose->scale[1] * lerp;
			relativeJoint->scale[2] = oldpose->scale[2] * backlerp + pose->scale[2] * lerp;

			QuatSlerp(oldpose->rotate, pose->rotate, lerp, relativeJoint->rotate);

			if (pose_idx == spin_id)
				QuatMultiply(relativeJoint->rotate, relativeJoint->rotate, spin_quat);
		}
	}

	// multiply by inverse of bind pose and parent 'pose mat' (bind pose transform matrix)
	relativeJoint = relativeJoints;
	const int* jointParent = model->jointParents;
	const float* invBindMat = model->invBindJoints;
	float* poseMat = pose_matrices;
	for (uint32_t pose_idx = 0; pose_idx < model->num_poses; pose_idx++, relativeJoint++, jointParent++, invBindMat += 12, poseMat += 12)
	{
		float mat1[12], mat2[12];

		JointToMatrix(relativeJoint->rotate, relativeJoint->scale, relativeJoint->translate, mat1);

		if (*jointParent >= 0)
		{
			Matrix34Multiply(&model->bindJoints[(*jointParent) * 12], mat1, mat2);
			Matrix34Multiply(mat2, invBindMat, mat1);
			Matrix34Multiply(&pose_matrices[(*jointParent) * 12], mat1, poseMat);
		}
		else
		{
			Matrix34Multiply(mat1, invBindMat, poseMat);
		}
	}

	return true;
}
