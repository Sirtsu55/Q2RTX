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

void R_ComputeIQMRelativeJoints(const iqm_model_t* model, int32_t frame, int32_t oldframe, float lerp, float backlerp, iqm_transform_t *relativeJoints)
{
	iqm_transform_t* relativeJoint = relativeJoints;

	frame = model->num_frames ? (frame % (int) model->num_frames) : 0;
	oldframe = model->num_frames ? (oldframe % (int) model->num_frames) : 0;

	// copy or lerp animation frame pose
	if (oldframe == frame)
	{
		const iqm_transform_t* pose = &model->poses[frame * model->num_poses];
		for (uint32_t pose_idx = 0; pose_idx < model->num_poses; pose_idx++, pose++, relativeJoint++)
		{
			if (pose_idx == model->root_id)
			{
				VectorClear(relativeJoint->translate);
				VectorSet(relativeJoint->scale, 1, 1, 1);
				QuatCopy(pose->rotate, relativeJoint->rotate);
				continue;
			}

			VectorCopy(pose->translate, relativeJoint->translate);
			VectorCopy(pose->scale, relativeJoint->scale);
			QuatCopy(pose->rotate, relativeJoint->rotate);

			//if (pose_idx == spin_id)
			//	QuatMultiply(relativeJoint->rotate, relativeJoint->rotate, spin_quat);
		}
	}
	else
	{
		const iqm_transform_t* pose = &model->poses[frame * model->num_poses];
		const iqm_transform_t* oldpose = &model->poses[oldframe * model->num_poses];
		for (uint32_t pose_idx = 0; pose_idx < model->num_poses; pose_idx++, oldpose++, pose++, relativeJoint++)
		{
			if (pose_idx == model->root_id)
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

			//if (pose_idx == spin_id)
			//	QuatMultiply(relativeJoint->rotate, relativeJoint->rotate, spin_quat);
		}
	}
}

void R_ComputeIQMLocalSpaceMatricesFromRelative(const iqm_model_t *model, const iqm_transform_t *relativeJoints, float *pose_matrices)
{
	// multiply by inverse of bind pose and parent 'pose mat' (bind pose transform matrix)
	const iqm_transform_t *relativeJoint = relativeJoints;
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
}

void R_ComputeIQMWorldSpaceMatricesFromRelative(const iqm_model_t *model, const iqm_transform_t *relativeJoints, float *pose_matrices)
{
	R_ComputeIQMLocalSpaceMatricesFromRelative(model, relativeJoints, pose_matrices);

	float *poseMat = model->bindJoints;
	float *outPose = pose_matrices;

	for (size_t i = 0; i < model->num_poses; i++, poseMat += 12, outPose += 12) {
		float inPose[12];
		memcpy(inPose, outPose, sizeof(inPose));
		Matrix34Multiply(inPose, poseMat, outPose);
	}
}

