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
// g_ai.c

#include "../g_local.h"

void env_reverb_touch(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
	if (other->client)
		other->client->ps.reverb = self->sounds;
}

void env_reverb_use(edict_t *self, edict_t *other, edict_t *activator)
{
	if (self->solid == SOLID_TRIGGER) {
		self->solid = SOLID_NOT;
	} else if (self->model && *self->model) {
		self->solid = SOLID_TRIGGER;
	} else if (self->flags) {
		edict_t **reverbHead = &level.reverb_entities;

		while (*reverbHead && *reverbHead != self)
			reverbHead = &(*reverbHead)->next_reverb;

		if (!reverbHead || !*reverbHead) {
			Com_EPrintf("Reverb error @ %f %f %f, not linked properly\n", self->s.origin[0], self->s.origin[1], self->s.origin[2]);
		} else {
			*reverbHead = (*reverbHead)->next_reverb;
			self->flags = 0;
		}
	} else {
		self->next_reverb = level.reverb_entities;
		level.reverb_entities = self;
		self->flags = 1;
	}
}

void SP_env_reverb(edict_t *self)
{
	if (!self->sounds) {
		self->sounds = level.default_reverb;
	} else if (self->sounds == -1) {
		self->sounds = 0;
	}

	self->svflags |= SVF_NOCLIENT;

	if (self->model && *self->model) {
		self->solid = (self->spawnflags & 1) ? SOLID_NOT : SOLID_TRIGGER;
		SV_SetBrushModel(self, self->model);
		self->touch = env_reverb_touch;
	} else {
		if (!self->radius) {
			self->radius = 64;
		}

		self->radius *= self->radius;

		if (!(self->spawnflags & 1)) {
			self->next_reverb = level.reverb_entities;
			level.reverb_entities = self;
			self->flags = 1;
		}
	}

	self->use = env_reverb_use;

	SV_LinkEntity(self);
}