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
#include "g_local.h"

enum TriggerSpawnflags
{
    SPAWNFLAG_MONSTER = 1 << 0,
    SPAWNFLAG_NOT_PLAYER = 1 << 1,
    SPAWNFLAG_START_OFF = 1 << 2,
    SPAWNFLAG_SHOOTABLE = 1 << 3,
    SPAWNFLAG_TOGGLE = 1 << 4
};

void InitTrigger(edict_t *self)
{
    if (!VectorEmpty(self->s.angles))
        G_SetMovedir(self->s.angles, self->movedir);

    self->solid = SOLID_TRIGGER;
    self->movetype = MOVETYPE_NONE;
    self->svflags = SVF_NOCLIENT;

    if (self->model && *self->model) {
        SV_SetBrushModel(self, self->model);
    }

    SV_LinkEntity(self);
}


// the wait time has passed, so set back up for another activation
void multi_wait(edict_t *ent)
{
    ent->nextthink = 0;

    if (ent->spawnflags & SPAWNFLAG_SHOOTABLE) {
        ent->solid = SOLID_BBOX;
        ent->takedamage = DAMAGE_YES;
    }
}


// the trigger was just activated
// ent->activator should be set to the activator so it can be held through a delay
// so wait for the delay time before firing
void multi_trigger(edict_t *ent)
{
    if (ent->nextthink)
        return;     // already been triggered

    G_UseTargets(ent, ent->activator);

    if (ent->wait > 0) {
        ent->think = multi_wait;
        ent->nextthink = level.time + G_SecToMs(ent->wait);
    } else {
        // we can't just remove (self) here, because this is a touch function
        // called while looping through area links...
        ent->touch = NULL;
        ent->nextthink = level.time + 1;
        ent->think = G_FreeEdict;
    }
}

void Use_Multi(edict_t *ent, edict_t *other, edict_t *activator)
{
    ent->activator = activator;
    multi_trigger(ent);
}

void Touch_Multi(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
    if (other->client) {
        if (self->spawnflags & SPAWNFLAG_NOT_PLAYER)
            return;
    } else if (other->svflags & SVF_MONSTER) {
        if (!(self->spawnflags & SPAWNFLAG_MONSTER))
            return;
    } else {
        return;
    }

    if (!VectorEmpty(self->movedir)) {
        vec3_t  forward;

        AngleVectors(other->s.angles, forward, NULL, NULL);
        if (DotProduct(forward, self->movedir) < 0)
            return;
    }

    self->activator = other;
    multi_trigger(self);
}

/*QUAKED trigger_multiple (.5 .5 .5) ? MONSTER NOT_PLAYER START_OFF SHOOTABLE TOGGLE
Variable sized repeatable trigger.  Must be targeted at one or more entities.
If "delay" is set, the trigger waits some time after activating before firing.
"wait" : Seconds between triggerings. (.2 default)
sounds
1)  secret
2)  beep beep
3)  large switch
4)
set "message" to text string
*/
void trigger_toggle(edict_t *self, edict_t *other, edict_t *activator)
{
    if (self->solid == SOLID_NOT) {
        self->solid = (self->spawnflags & SPAWNFLAG_SHOOTABLE) ? SOLID_BBOX : SOLID_TRIGGER;
    } else {
        self->solid = SOLID_NOT;
    }

    if (!(self->spawnflags & SPAWNFLAG_TOGGLE)) {
        self->use = Use_Multi;
    }

    SV_LinkEntity(self);
}

void trigger_die(edict_t *self, edict_t *inflictor, edict_t *attacker, int damage, vec3_t point)
{
    self->activator = attacker;
    multi_trigger(self);
    self->solid = SOLID_NOT;
    self->health = self->max_health;
    self->takedamage = DAMAGE_NO;
}

void SP_trigger_multiple(edict_t *ent)
{
    if (ent->sounds == 0)
        ent->noise_index = SV_SoundIndex(ASSET_SOUND_GAME_MESSAGE);
    else if (ent->sounds == 1)
        ent->noise_index = SV_SoundIndex(ASSET_SOUND_SECRET_FOUND);
    else if (ent->sounds == 2)
        ent->noise_index = SV_SoundIndex(ASSET_SOUND_CHAT);

    if (!ent->wait)
        ent->wait = 0.2f;

    ent->movetype = MOVETYPE_NONE;
    ent->svflags |= SVF_NOCLIENT;

    if (ent->model && *ent->model) {
        SV_SetBrushModel(ent, ent->model);
    }

    if (ent->spawnflags & SPAWNFLAG_SHOOTABLE) {
        if (!ent->health) {
            ent->health = 1;
        }
        ent->max_health = ent->health;
        ent->die = trigger_die;
        ent->takedamage = DAMAGE_YES;
        ent->svflags |= SVF_DEADMONSTER;
    } else {
        ent->touch = Touch_Multi;
    }

    if (ent->spawnflags & SPAWNFLAG_START_OFF) {
        ent->solid = SOLID_NOT;
    } else if (ent->spawnflags & SPAWNFLAG_SHOOTABLE) {
        ent->solid = SOLID_BBOX;
    } else {
        ent->solid = SOLID_TRIGGER;
    }

    if (ent->spawnflags & SPAWNFLAG_TOGGLE) {
        ent->use = trigger_toggle;
    } else {
        ent->use = Use_Multi;
    }

    if (!VectorEmpty(ent->s.angles))
        G_SetMovedir(ent->s.angles, ent->movedir);

    SV_LinkEntity(ent);
}


/*QUAKED trigger_once (.5 .5 .5) ? x x TRIGGERED
Triggers once, then removes itself.
You must set the key "target" to the name of another object in the level that has a matching "targetname".

If TRIGGERED, this trigger must be triggered before it is live.

sounds
 1) secret
 2) beep beep
 3) large switch
 4)

"message"   string to be displayed when triggered
*/

void SP_trigger_once(edict_t *ent)
{
    // make old maps work because I messed up on flag assignments here
    // triggered was on bit 1 when it should have been on bit 4
    if (ent->spawnflags & 1) {
        vec3_t  v;

        VectorMA(ent->mins, 0.5f, ent->size, v);
        ent->spawnflags &= ~1;
        ent->spawnflags |= 4;
        Com_WPrintf("fixed TRIGGERED flag on %s at %s\n", ent->classname, vtos(v));
    }

    ent->wait = -1;
    SP_trigger_multiple(ent);
}

/*QUAKED trigger_relay (.5 .5 .5) (-8 -8 -8) (8 8 8)
This fixed size trigger cannot be touched, it can only be fired by other events.
*/
void trigger_relay_use(edict_t *self, edict_t *other, edict_t *activator)
{
    // Paril - kill after X many "count"s
    if (self->count) {
        if (self->count < 0) {
            return;
        }

        if (!--self->count) {
            self->count = -1;
            self->think = G_FreeEdict;
            self->nextthink = level.time + 1;
        }
    }

    G_UseTargets(self, activator);
}

void SP_trigger_relay(edict_t *self)
{
    self->use = trigger_relay_use;
}


/*
==============================================================================

trigger_key

==============================================================================
*/

/*QUAKED trigger_key (.5 .5 .5) (-8 -8 -8) (8 8 8)
A relay trigger that only fires it's targets if player has the proper key.
Use "item" to specify the required key, for example "key_data_cd"
*/
void trigger_key_use(edict_t *self, edict_t *other, edict_t *activator)
{
    if (!self->item)
        return;
    if (!activator->client)
        return;

    gitem_id_t index = self->item->id;

    if (!activator->client->pers.inventory[index]) {
        if (level.time < self->touch_debounce_time)
            return;
        self->touch_debounce_time = level.time + 5000;
        SV_CenterPrintf(activator, "You need the %s", self->item->pickup_name);
        SV_StartSound(activator, CHAN_AUTO, SV_SoundIndex(ASSET_SOUND_KEY_TRY), 1, ATTN_NORM, 0);
        return;
    }

    SV_StartSound(activator, CHAN_AUTO, SV_SoundIndex(ASSET_SOUND_KEY_USE), 1, ATTN_NORM, 0);
    if (coop.integer) {
        int     player;
        edict_t *ent;

        for (player = 1; player <= game.maxclients; player++) {
            ent = &globals.entities[player];
            if (!ent->inuse)
                continue;
            if (!ent->client)
                continue;
            if (ent->client->pers.inventory[index]) {
                ent->client->pers.inventory[index]--;
            }
        }
    } else {
        activator->client->pers.inventory[index]--;
    }

    G_UseTargets(self, activator);

    self->use = NULL;
}

void SP_trigger_key(edict_t *self)
{
    if (!st.item) {
        Com_WPrintf("no key item for trigger_key at %s\n", vtos(self->s.origin));
        return;
    }
    self->item = FindItemByClassname(st.item);

    if (!self->item) {
        Com_WPrintf("item %s not found for trigger_key at %s\n", st.item, vtos(self->s.origin));
        return;
    }

    if (!self->target) {
        Com_WPrintf("%s at %s has no target\n", self->classname, vtos(self->s.origin));
        return;
    }

    SV_SoundIndex(ASSET_SOUND_KEY_TRY);
    SV_SoundIndex(ASSET_SOUND_KEY_USE);

    self->use = trigger_key_use;
}


/*
==============================================================================

trigger_counter

==============================================================================
*/

/*QUAKED trigger_counter (.5 .5 .5) ? nomessage
Acts as an intermediary for an action that takes multiple inputs.

If nomessage is not set, t will print "1 more.. " etc when triggered and "sequence complete" when finished.

After the counter has been triggered "count" times (default 2), it will fire all of it's targets and remove itself.
*/

void trigger_counter_use(edict_t *self, edict_t *other, edict_t *activator)
{
    if (self->count == 0)
        return;

    self->count--;

    if (self->count) {
        if (!(self->spawnflags & 1)) {
            SV_CenterPrintf(activator, "%i more to go...", self->count);
            SV_StartSound(activator, CHAN_AUTO, SV_SoundIndex(ASSET_SOUND_GAME_MESSAGE), 1, ATTN_NORM, 0);
        }
        return;
    }

    if (!(self->spawnflags & 1)) {
        SV_CenterPrint(activator, "Sequence completed!");
        SV_StartSound(activator, CHAN_AUTO, SV_SoundIndex(ASSET_SOUND_GAME_MESSAGE), 1, ATTN_NORM, 0);
    }
    self->activator = activator;
    multi_trigger(self);
}

void SP_trigger_counter(edict_t *self)
{
    self->wait = -1;
    if (!self->count)
        self->count = 2;

    self->use = trigger_counter_use;
}


/*
==============================================================================

trigger_always

==============================================================================
*/

/*QUAKED trigger_always (.5 .5 .5) (-8 -8 -8) (8 8 8)
This trigger will always fire.  It is activated by the world.
*/
void SP_trigger_always(edict_t *ent)
{
    // we must have some delay to make sure our use targets are present
    if (ent->delay < 0.2f)
        ent->delay = 0.2f;
    G_UseTargets(ent, ent);
}


/*
==============================================================================

trigger_push

==============================================================================
*/

#define PUSH_ONCE       1
// Paril: extra pushy support
#define PUSH_START_OFF  2
#define PUSH_TOGGLE     4
#define PUSH_SILENT     8

static int windsound;

void trigger_push_touch(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
    if (strcmp(other->classname, "grenade") == 0) {
        VectorScale(self->movedir, self->speed * 10, other->velocity);
    } else if (other->health > 0) {
        VectorScale(self->movedir, self->speed * 10, other->velocity);

        if (other->client) {
            // don't take falling damage immediately from this
            VectorCopy(other->velocity, other->client->oldvelocity);
            if (!(self->spawnflags & PUSH_SILENT)) {
            	if (other->fly_sound_debounce_time < level.time) {
                	other->fly_sound_debounce_time = level.time + 1500;
                    SV_StartSound(other, CHAN_AUTO, windsound, 1, ATTN_NORM, 0);
                }
            }
        }
    }
    if (self->spawnflags & PUSH_ONCE)
        G_FreeEdict(self);
}

void trigger_push_use(edict_t *self, edict_t *other, edict_t *activator)
{
    if (self->solid == SOLID_NOT)
        self->solid = SOLID_TRIGGER;
    else
        self->solid = SOLID_NOT;
    SV_LinkEntity(self);

    if (!(self->spawnflags & PUSH_TOGGLE))
        self->use = NULL;
}

/*QUAKED trigger_push (.5 .5 .5) ? PUSH_ONCE
Pushes the player
"speed"     defaults to 1000
*/
void SP_trigger_push(edict_t *self)
{
    InitTrigger(self);
    if (!(self->spawnflags & PUSH_SILENT))
        windsound = SV_SoundIndex(ASSET_SOUND_PUSH_WIND);

    self->touch = trigger_push_touch;
    if (!self->speed)
        self->speed = 1000;

    if (self->spawnflags & PUSH_START_OFF)
        self->solid = SOLID_NOT;
    else
        self->solid = SOLID_TRIGGER;

    if (self->spawnflags & PUSH_TOGGLE)
        self->use = trigger_push_use;

    SV_LinkEntity(self);
}


/*
==============================================================================

trigger_hurt

==============================================================================
*/

/*QUAKED trigger_hurt (.5 .5 .5) ? START_OFF TOGGLE SILENT NO_PROTECTION SLOW
Any entity that touches this will be hurt.

It does dmg points of damage each server frame

SILENT          supresses playing the sound
SLOW            changes the damage rate to once per second
NO_PROTECTION   *nothing* stops the damage

"dmg"           default 5 (whole numbers only)

*/
void hurt_use(edict_t *self, edict_t *other, edict_t *activator)
{
    if (self->solid == SOLID_NOT)
        self->solid = SOLID_TRIGGER;
    else
        self->solid = SOLID_NOT;
    SV_LinkEntity(self);

    if (!(self->spawnflags & 2))
        self->use = NULL;
}


void hurt_touch(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
    int     dflags;

    if (!other->takedamage)
        return;

    if (self->timestamp > level.time)
        return;

    if (self->spawnflags & 16)
        self->timestamp = level.time + 1000;
    else
        self->timestamp = level.time + 100;

    if (!(self->spawnflags & 4)) {
        if ((level.time % 1000) == 0)
            SV_StartSound(other, CHAN_AUTO, self->noise_index, 1, ATTN_NORM, 0);
    }

    if (self->spawnflags & 8)
        dflags = DAMAGE_NO_PROTECTION;
    else
        dflags = 0;
    T_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, self->dmg, self->dmg, dflags, MOD_TRIGGER_HURT);
}

void SP_trigger_hurt(edict_t *self)
{
    InitTrigger(self);

    if (!(self->spawnflags & 4)) {
        self->noise_index = SV_SoundIndex(ASSET_SOUND_HURT_BURN);
    }
    self->touch = hurt_touch;

    if (!self->dmg)
        self->dmg = 5;

    if (self->spawnflags & 1)
        self->solid = SOLID_NOT;
    else
        self->solid = SOLID_TRIGGER;

    if (self->spawnflags & 2)
        self->use = hurt_use;

    SV_LinkEntity(self);
}


/*
==============================================================================

trigger_gravity

==============================================================================
*/

/*QUAKED trigger_gravity (.5 .5 .5) ?
Changes the touching entites gravity to
the value of "gravity".  1.0 is standard
gravity for the level.
*/

void trigger_gravity_touch(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
    other->gravity = self->gravity;
}

void SP_trigger_gravity(edict_t *self)
{
    if (st.gravity == NULL) {
        Com_WPrintf("trigger_gravity without gravity set at %s\n", vtos(self->s.origin));
        G_FreeEdict(self);
        return;
    }

    InitTrigger(self);
    self->gravity = atoi(st.gravity);
    self->touch = trigger_gravity_touch;
}


/*
==============================================================================

trigger_monsterjump

==============================================================================
*/

/*QUAKED trigger_monsterjump (.5 .5 .5) ?
Walking monsters that touch this will jump in the direction of the trigger's angle
"speed" default to 200, the speed thrown forward
"height" default to 200, the speed thrown upwards
*/

void trigger_monsterjump_touch(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
    if (other->flags & (FL_FLY | FL_SWIM))
        return;
    if (other->svflags & SVF_DEADMONSTER)
        return;
    if (!(other->svflags & SVF_MONSTER))
        return;

// set XY even if not on ground, so the jump will clear lips
    other->velocity[0] = self->movedir[0] * self->speed;
    other->velocity[1] = self->movedir[1] * self->speed;

    if (!other->groundentity)
        return;

    other->groundentity = NULL;
    other->velocity[2] = self->movedir[2];
}

void trigger_monsterjump_toggle(edict_t *self, edict_t *other, edict_t *activator)
{
    self->solid = (self->solid == SOLID_NOT) ? SOLID_TRIGGER : SOLID_NOT;
    SV_LinkEntity(self);
}

void SP_trigger_monsterjump(edict_t *self)
{
    if (!self->speed)
        self->speed = 200;
    if (!st.height)
        st.height = 200;
    if (self->s.angles[YAW] == 0)
        self->s.angles[YAW] = 360;
    InitTrigger(self);

    if (self->spawnflags & 1)
    {
        self->solid = (self->spawnflags & 2) ? SOLID_NOT : SOLID_TRIGGER;
        self->use = trigger_monsterjump_toggle;
        SV_LinkEntity(self);
    }

    self->touch = trigger_monsterjump_touch;
    self->movedir[2] = st.height;
}

