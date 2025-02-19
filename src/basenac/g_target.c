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

/*QUAKED target_temp_entity (1 0 0) (-8 -8 -8) (8 8 8)
Fire an origin based temp entity event to the clients.
"style"     type byte
*/
void Use_Target_Tent(edict_t *ent, edict_t *other, edict_t *activator)
{
    SV_WriteByte(svc_temp_entity);
    SV_WriteByte(ent->style);
    SV_WritePos(ent->s.origin);
    SV_Multicast(ent->s.origin, MULTICAST_PVS, false);
}

void SP_target_temp_entity(edict_t *ent)
{
    ent->use = Use_Target_Tent;
}


//==========================================================

//==========================================================

/*QUAKED target_speaker (1 0 0) (-8 -8 -8) (8 8 8) looped-on looped-off reliable
"noise"     wav file to play
"attenuation"
-1 = none, send to whole level
1 = normal fighting sounds
2 = idle sound level
3 = ambient sound level
"volume"    0.0 to 1.0

Normal sounds play each time the target is used.  The reliable flag can be set for crucial voiceovers.

Looped sounds are always atten 3 / vol 1, and the use function toggles it on/off.
Multiple identical looping sounds will just increase volume without any speed cost.
*/
void Use_Target_Speaker(edict_t *ent, edict_t *other, edict_t *activator)
{
    int     chan;

    if (ent->spawnflags & 3) {
        // looping sound toggles
        if (ent->s.sound)
            ent->s.sound = 0;   // turn it off
        else
            ent->s.sound = ent->noise_index;    // start it
    } else {
        // normal sound
        if (ent->spawnflags & 4)
            chan = CHAN_VOICE | CHAN_RELIABLE;
        else
            chan = CHAN_VOICE;
        // use a positioned_sound, because this entity won't normally be
        // sent to any clients because it is invisible
        SV_PositionedSound(ent->s.origin, ent, chan, ent->noise_index, ent->volume, ent->attenuation, 0);
    }
}

void SP_target_speaker(edict_t *ent)
{
    char    buffer[MAX_QPATH];

    if (!st.noise) {
        Com_WPrintf("target_speaker with no noise set at %s\n", vtos(ent->s.origin));
        return;
    }
    if (!strstr(st.noise, ".wav"))
        Q_snprintf(buffer, sizeof(buffer), "%s.wav", st.noise);
    else
        Q_strlcpy(buffer, st.noise, sizeof(buffer));
    ent->noise_index = SV_SoundIndex(buffer);

    if (!ent->volume)
        ent->volume = 1.0f;

    if (!ent->attenuation)
        ent->attenuation = 1.0f;
    else if (ent->attenuation == -1)    // use -1 so 0 defaults to 1
        ent->attenuation = 0;

    // check for prestarted looping sound
    if (ent->spawnflags & 1)
        ent->s.sound = ent->noise_index;

    ent->use = Use_Target_Speaker;

    // must link the entity so we get areas and clusters so
    // the server can determine who to send updates to
    SV_LinkEntity(ent);
}


//==========================================================

void Use_Target_Help(edict_t *ent, edict_t *other, edict_t *activator)
{
    if (ent->spawnflags & 1)
        Q_strlcpy(game.helpmessage1, ent->message, sizeof(game.helpmessage2));
    else
        Q_strlcpy(game.helpmessage2, ent->message, sizeof(game.helpmessage1));

    game.helpchanged++;
}

/*QUAKED target_help (1 0 1) (-16 -16 -24) (16 16 24) help1
When fired, the "message" key becomes the current personal computer string, and the message light will be set on all clients status bars.
*/
void SP_target_help(edict_t *ent)
{
    if (deathmatch.integer) {
        // auto-remove for deathmatch
        G_FreeEdict(ent);
        return;
    }

    if (!ent->message) {
        Com_WPrintf("%s with no message at %s\n", ent->classname, vtos(ent->s.origin));
        G_FreeEdict(ent);
        return;
    }
    ent->use = Use_Target_Help;
}

//==========================================================

/*QUAKED target_secret (1 0 1) (-8 -8 -8) (8 8 8)
Counts a secret found.
These are single use targets.
*/
void use_target_secret(edict_t *ent, edict_t *other, edict_t *activator)
{
    SV_StartSound(ent, CHAN_VOICE, ent->noise_index, 1, ATTN_NORM, 0);

    level.found_secrets++;

    G_UseTargets(ent, activator);
    G_FreeEdict(ent);
}

void SP_target_secret(edict_t *ent)
{
    if (deathmatch.integer) {
        // auto-remove for deathmatch
        G_FreeEdict(ent);
        return;
    }

    ent->use = use_target_secret;
    if (!st.noise)
        st.noise = ASSET_SOUND_SECRET_FOUND;
    ent->noise_index = SV_SoundIndex(st.noise);
    ent->svflags = SVF_NOCLIENT;
    level.total_secrets++;
    // map bug hack
    if (!Q_stricmp(level.mapname, "mine3") && ent->s.origin[0] == 280 && ent->s.origin[1] == -2048 && ent->s.origin[2] == -624)
        ent->message = "You have found a secret area.";
}

//==========================================================

/*QUAKED target_goal (1 0 1) (-8 -8 -8) (8 8 8)
Counts a goal completed.
These are single use targets.
*/
void use_target_goal(edict_t *ent, edict_t *other, edict_t *activator)
{
    SV_StartSound(ent, CHAN_VOICE, ent->noise_index, 1, ATTN_NORM, 0);

    level.found_goals++;

    if (level.found_goals == level.total_goals)
        SV_SetConfigString(CS_CDTRACK, "0");

    G_UseTargets(ent, activator);
    G_FreeEdict(ent);
}

void SP_target_goal(edict_t *ent)
{
    if (deathmatch.integer) {
        // auto-remove for deathmatch
        G_FreeEdict(ent);
        return;
    }

    ent->use = use_target_goal;
    if (!st.noise)
        st.noise = ASSET_SOUND_SECRET_FOUND;
    ent->noise_index = SV_SoundIndex(st.noise);
    ent->svflags = SVF_NOCLIENT;
    level.total_goals++;
}

//==========================================================


/*QUAKED target_explosion (1 0 0) (-8 -8 -8) (8 8 8)
Spawns an explosion temporary entity when used.

"delay"     wait this long before going off
"dmg"       how much radius damage should be done, defaults to 0
*/
void target_explosion_explode(edict_t *self)
{
    float       save;

    SV_WriteByte(svc_temp_entity);
    SV_WriteByte(TE_EXPLOSION1);
    SV_WritePos(self->s.origin);
    SV_Multicast(self->s.origin, MULTICAST_PHS, false);

    T_RadiusDamage(self, self->activator, self->dmg, NULL, self->dmg + 40, MOD_EXPLOSIVE);

    save = self->delay;
    self->delay = 0;
    G_UseTargets(self, self->activator);
    self->delay = save;
}

void use_target_explosion(edict_t *self, edict_t *other, edict_t *activator)
{
    self->activator = activator;

    if (!self->delay) {
        target_explosion_explode(self);
        return;
    }

    self->think = target_explosion_explode;
    self->nextthink = level.time + G_SecToMs(self->delay);
}

void SP_target_explosion(edict_t *ent)
{
    ent->use = use_target_explosion;
    ent->svflags = SVF_NOCLIENT;
}


//==========================================================

/*QUAKED target_changelevel (1 0 0) (-8 -8 -8) (8 8 8)
Changes level to "map" when fired
*/
void use_target_changelevel(edict_t *self, edict_t *other, edict_t *activator)
{
    if (level.intermission_time)
        return;     // already activated

    if (!deathmatch.integer && !coop.integer) {
        if (globals.entities[1].health <= 0)
            return;
    }

    // if noexit, do a ton of damage to other
    if (deathmatch.integer && !(dmflags.integer & DF_ALLOW_EXIT) && other != game.world) {
        T_Damage(other, self, self, vec3_origin, other->s.origin, vec3_origin, 10 * other->max_health, 1000, 0, MOD_EXIT);
        return;
    }

    // if multiplayer, let everyone know who hit the exit
    if (deathmatch.integer) {
        if (activator && activator->client)
            SV_BroadcastPrintf(PRINT_HIGH, "%s exited the level.\n", activator->client->pers.netname);
    }

    // if going to a new unit, clear cross triggers
    if (strstr(self->map, "*"))
        game.serverflags &= ~(SFL_CROSS_TRIGGER_MASK);

    BeginIntermission(self);
}

void SP_target_changelevel(edict_t *ent)
{
    if (!ent->map) {
        Com_WPrintf("target_changelevel with no map at %s\n", vtos(ent->s.origin));
        G_FreeEdict(ent);
        return;
    }

    // ugly hack because *SOMEBODY* screwed up their map
    if ((Q_stricmp(level.mapname, "fact1") == 0) && (Q_stricmp(ent->map, "fact3") == 0))
        ent->map = "fact3$secret1";

    ent->use = use_target_changelevel;
    ent->svflags = SVF_NOCLIENT;
}


//==========================================================

/*QUAKED target_splash (1 0 0) (-8 -8 -8) (8 8 8)
Creates a particle splash effect when used.

Set "sounds" to one of the following:
  1) sparks
  2) blue water
  3) brown water
  4) slime
  5) lava
  6) blood

"count" how many pixels in the splash
"dmg"   if set, does a radius damage at this location when it splashes
        useful for lava/sparks
*/

void use_target_splash(edict_t *self, edict_t *other, edict_t *activator)
{
    SV_WriteByte(svc_temp_entity);
    SV_WriteByte(TE_SPLASH);
    SV_WriteByte(self->count);
    SV_WritePos(self->s.origin);
    SV_WriteDir(self->movedir);
    SV_WriteByte(self->sounds);
    SV_Multicast(self->s.origin, MULTICAST_PVS, false);

    if (self->dmg)
        T_RadiusDamage(self, activator, self->dmg, NULL, self->dmg + 40, MOD_SPLASH);
}

void SP_target_splash(edict_t *self)
{
    self->use = use_target_splash;
    G_SetMovedir(self->s.angles, self->movedir);

    if (!self->count)
        self->count = 32;

    self->svflags = SVF_NOCLIENT;
}


//==========================================================

/*QUAKED target_spawner (1 0 0) (-8 -8 -8) (8 8 8)
Set target to the type of entity you want spawned.
Useful for spawning monsters and gibs in the factory levels.

For monsters:
    Set direction to the facing you want it to have.

For gibs:
    Set direction if you want it moving and
    speed how fast it should be moving otherwise it
    will just be dropped
*/
void ED_CallSpawn(edict_t *ent);

void use_target_spawner(edict_t *self, edict_t *other, edict_t *activator)
{
    edict_t *ent;

    ent = G_Spawn();
    ent->classname = self->target;
    VectorCopy(self->s.origin, ent->s.origin);
    VectorCopy(self->s.angles, ent->s.angles);
    ED_CallSpawn(ent);
    SV_UnlinkEntity(ent);
    KillBox(ent);
    SV_LinkEntity(ent);
    if (self->speed)
        VectorCopy(self->movedir, ent->velocity);
}

void SP_target_spawner(edict_t *self)
{
    self->use = use_target_spawner;
    self->svflags = SVF_NOCLIENT;
    if (self->speed) {
        G_SetMovedir(self->s.angles, self->movedir);
        VectorScale(self->movedir, self->speed, self->movedir);
    }
}

//==========================================================

/*QUAKED target_blaster (1 0 0) (-8 -8 -8) (8 8 8) NOTRAIL NOEFFECTS NAIL ROCKET GRENADE HOMING
Fires a blaster bolt in the set direction when triggered.

dmg     default is 15
speed   default is 1000
*/

const int SPAWNFLAG_BLASTER_NAIL = 4;
const int SPAWNFLAG_BLASTER_ROCKET = 8;
const int SPAWNFLAG_BLASTER_GRENADE = 16;
const int SPAWNFLAG_BLASTER_HOMING = 32;

void use_target_blaster(edict_t *self, edict_t *other, edict_t *activator)
{
    int effect;

    if (self->spawnflags & 2)
        effect = 0;
    else if (self->spawnflags & 1)
        effect = EF_HYPERBLASTER;
    else
        effect = EF_BLASTER;
    
    vec3_t dir;

    if (self->spawnflags & SPAWNFLAG_BLASTER_HOMING) {
        VectorSubtract(activator->s.origin, self->s.origin, dir);
        VectorNormalize(dir);
    } else {
        VectorCopy(self->movedir, dir);
    }

    if (self->spawnflags & SPAWNFLAG_BLASTER_NAIL) {
        fire_nail(self, self->s.origin, dir, self->dmg, self->speed);
    } else if (self->spawnflags & SPAWNFLAG_BLASTER_ROCKET) {
        fire_rocket(self, self->s.origin, dir, self->dmg, self->speed, self->dmg + 140, self->dmg);
    } else if (self->spawnflags & SPAWNFLAG_BLASTER_GRENADE) {
        fire_grenade(self, self->s.origin, dir, self->dmg, self->speed, 2.5, self->dmg + 140);
    } else {
        fire_blaster(self, self->s.origin, dir, self->dmg, self->speed, effect, MOD_TARGET_BLASTER);
    }
    SV_StartSound(self, CHAN_VOICE, self->noise_index, 1, ATTN_NORM, 0);
}

void SP_target_blaster(edict_t *self)
{
    self->use = use_target_blaster;
    G_SetMovedir(self->s.angles, self->movedir);

    if (st.noise)
        self->noise_index = SV_SoundIndex(st.noise);
    else
        self->noise_index = SV_SoundIndex("weapons/laser2.wav");

    if (!self->dmg)
        self->dmg = 15;
    if (!self->speed)
        self->speed = 1000;

    self->svflags = SVF_NOCLIENT;
}


//==========================================================

/*QUAKED target_crosslevel_trigger (.5 .5 .5) (-8 -8 -8) (8 8 8) trigger1 trigger2 trigger3 trigger4 trigger5 trigger6 trigger7 trigger8
Once this trigger is touched/used, any trigger_crosslevel_target with the same trigger number is automatically used when a level is started within the same unit.  It is OK to check multiple triggers.  Message, delay, target, and killtarget also work.
*/
void trigger_crosslevel_trigger_use(edict_t *self, edict_t *other, edict_t *activator)
{
    game.serverflags |= self->spawnflags;
    G_FreeEdict(self);
}

void SP_target_crosslevel_trigger(edict_t *self)
{
    self->svflags = SVF_NOCLIENT;
    self->use = trigger_crosslevel_trigger_use;
}

/*QUAKED target_crosslevel_target (.5 .5 .5) (-8 -8 -8) (8 8 8) trigger1 trigger2 trigger3 trigger4 trigger5 trigger6 trigger7 trigger8
Triggered by a trigger_crosslevel elsewhere within a unit.  If multiple triggers are checked, all must be true.  Delay, target and
killtarget also work.

"delay"     delay before using targets if the trigger has been activated (default 1)
*/
void target_crosslevel_target_think(edict_t *self)
{
    if (self->spawnflags == (game.serverflags & SFL_CROSS_TRIGGER_MASK & self->spawnflags)) {
        G_UseTargets(self, self);
        G_FreeEdict(self);
    }
}

void SP_target_crosslevel_target(edict_t *self)
{
    if (! self->delay)
        self->delay = 1;
    self->svflags = SVF_NOCLIENT;

    self->think = target_crosslevel_target_think;
    self->nextthink = level.time + G_SecToMs(self->delay);
}

//==========================================================

/*QUAKED target_laser (0 .5 .8) (-8 -8 -8) (8 8 8) START_ON RED GREEN BLUE YELLOW ORANGE FAT
When triggered, fires a laser.  You can either set a target
or a direction.
*/

void target_laser_think(edict_t *self)
{
    edict_t *ignore;
    vec3_t  start;
    vec3_t  end;
    trace_t tr;
    vec3_t  point;
    vec3_t  last_movedir;
    int     count;

    if (self->spawnflags & 0x80000000)
        count = 8;
    else
        count = 4;

    // Paril - origin follow
    if (self->oldenemy) {
        VectorMA(self->oldenemy->absmin, 0.5, self->oldenemy->size, self->s.origin);
        SV_LinkEntity(self);
    }

    if (self->enemy) {
        VectorCopy(self->movedir, last_movedir);
        VectorMA(self->enemy->absmin, 0.5f, self->enemy->size, point);
        VectorSubtract(point, self->s.origin, self->movedir);
        VectorNormalize(self->movedir);
        if (!VectorCompare(self->movedir, last_movedir))
            self->spawnflags |= 0x80000000;
    }

    ignore = self;
    VectorCopy(self->s.origin, start);
    VectorMA(start, 2048, self->movedir, end);
    while (1) {
        tr = SV_Trace(start, NULL, NULL, end, ignore, self->dmg ? (CONTENTS_SOLID | CONTENTS_MONSTER | CONTENTS_DEADMONSTER) : CONTENTS_SOLID);

        if (!tr.ent)
            break;

        if (!self->dmg) {
            break;
        }

        // hurt it if we can
        if ((tr.ent->takedamage) && !(tr.ent->flags & FL_IMMUNE_LASER))
            T_Damage(tr.ent, self, self->activator, self->movedir, tr.endpos, vec3_origin, self->dmg, 1, DAMAGE_ENERGY, MOD_TARGET_LASER);

        // if we hit something that's not a monster or player or is immune to lasers, we're done
        if (!(tr.ent->svflags & SVF_MONSTER) && (!tr.ent->client)) {
            if (self->spawnflags & 0x80000000) {
                self->spawnflags &= ~0x80000000;
                SV_WriteByte(svc_temp_entity);
                SV_WriteByte(TE_LASER_SPARKS);
                SV_WriteByte(count);
                SV_WritePos(tr.endpos);
                SV_WriteDir(tr.plane.normal);
                SV_WriteByte(self->s.skinnum);
                SV_Multicast(tr.endpos, MULTICAST_PVS, false);
            }
            break;
        }

        ignore = tr.ent;
        VectorCopy(tr.endpos, start);
    }

    VectorCopy(tr.endpos, self->s.old_origin);

    self->nextthink = level.time + 1;
}

void target_laser_on(edict_t *self)
{
    if (!self->activator)
        self->activator = self;
    self->spawnflags |= 0x80000001;
    self->svflags &= ~SVF_NOCLIENT;
    target_laser_think(self);
}

void target_laser_off(edict_t *self)
{
    self->spawnflags &= ~1;
    self->svflags |= SVF_NOCLIENT;
    self->nextthink = 0;
}

void target_laser_use(edict_t *self, edict_t *other, edict_t *activator)
{
    self->activator = activator;
    if (self->spawnflags & 1)
        target_laser_off(self);
    else
        target_laser_on(self);
}

void target_laser_start(edict_t *self)
{
    edict_t *ent;

    self->movetype = MOVETYPE_NONE;
    self->solid = SOLID_NOT;
    self->s.renderfx |= self->dmg ? RF_BEAM : (RF_SPOTLIGHT | RF_TRANSLUCENT);
    self->s.modelindex = 1;         // must be non-zero

    // Paril - origin follow
    if (self->combattarget) {
        self->oldenemy = G_Find(NULL, FOFS(targetname), self->combattarget);

        if (self->oldenemy) {
            VectorMA(self->oldenemy->absmin, 0.5, self->oldenemy->size, self->s.origin);
        }
    }

    if (!self->enemy) {
        if (self->target) {
            ent = G_Find(NULL, FOFS(targetname), self->target);
            if (!ent)
                Com_WPrintf("%s at %s: %s is a bad target\n", self->classname, vtos(self->s.origin), self->target);
            self->enemy = ent;
        } else {
            G_SetMovedir(self->s.angles, self->movedir);
        }
    }
    self->use = target_laser_use;
    self->think = target_laser_think;

    VectorSet(self->mins, -8, -8, -8);
    VectorSet(self->maxs, 8, 8, 8);
    SV_LinkEntity(self);

    if (self->spawnflags & 1)
        target_laser_on(self);
    else
        target_laser_off(self);
}

void SP_target_laser(edict_t *self)
{
    if (!self->dmg)
        self->dmg = 1;

    // set the beam diameter
    // Paril: multi-size fatness
    if (self->health) {
        self->s.frame = self->health;
    }
    else if (self->spawnflags & 64)
        self->s.frame = 16;
    else
        self->s.frame = 4;

    // set the color
    // Paril: custom color
    if (self->count || self->style)
        self->s.skinnum = self->count | (self->style << 16);
    else if (self->spawnflags & 2)
        self->s.skinnum = 0xf2f2f0f0;
    else if (self->spawnflags & 4)
        self->s.skinnum = 0xd0d1d2d3;
    else if (self->spawnflags & 8)
        self->s.skinnum = 0xf3f3f1f1;
    else if (self->spawnflags & 16)
        self->s.skinnum = 0xdcdddedf;
    else if (self->spawnflags & 32)
        self->s.skinnum = 0xe0e1e2e3;

    // let everything else get spawned before we start firing
    self->think = target_laser_start;
    self->nextthink = level.time + 1000;
}

void SP_target_spotlight(edict_t *self)
{
    self->dmg = 0;

    if (VectorEmpty(st.color)) {
        self->s.skinnum = MakeRawLong(255, 255, 255, 0);
    } else {
        self->s.skinnum = MakeRawLong((uint8_t) st.color[0], (uint8_t) st.color[1], (uint8_t) st.color[2], 0);
    }
    if (st.intensity) {
        self->s.frame = st.intensity;
    } else {
        self->s.frame = 10000;
    }
    if (st.width_angle) {
        self->s.angles[0] = st.width_angle;
    } else {
        self->s.angles[0] = 30.f;
    }
    if (st.falloff_angle) {
        self->s.angles[1] = st.falloff_angle;
    } else {
        self->s.angles[1] = 15.f;
    }

    // let everything else get spawned before we start firing
    self->think = target_laser_start;
    self->nextthink = level.time + 1000;
}

//==========================================================

/*QUAKED target_lightramp (0 .5 .8) (-8 -8 -8) (8 8 8) TOGGLE
speed       How many seconds the ramping will take
message     two letters; starting lightlevel and ending lightlevel
*/

void target_lightramp_think(edict_t *self)
{
    char    style[2];
    float   diff = G_MsToSec(level.time - self->timestamp);

    style[0] = 'a' + self->movedir[0] + diff * self->movedir[2];
    style[1] = 0;
    SV_SetConfigString(CS_LIGHTS + self->enemy->style, style);

    if (diff < self->speed) {
        self->nextthink = level.time + 1;
    } else if (self->spawnflags & 1) {
        char    temp;

        temp = self->movedir[0];
        self->movedir[0] = self->movedir[1];
        self->movedir[1] = temp;
        self->movedir[2] *= -1;
    }
}

void target_lightramp_use(edict_t *self, edict_t *other, edict_t *activator)
{
    if (!self->enemy) {
        edict_t     *e;

        // check all the targets
        e = NULL;
        while (1) {
            e = G_Find(e, FOFS(targetname), self->target);
            if (!e)
                break;
            if (strcmp(e->classname, "light") != 0) {
                Com_WPrintf("%s at %s: target %s (%s at %s) is not a light\n", self->classname, vtos(self->s.origin), self->target, e->classname, vtos(e->s.origin));
            } else {
                self->enemy = e;
            }
        }

        if (!self->enemy) {
            Com_WPrintf("%s target %s not found at %s\n", self->classname, self->target, vtos(self->s.origin));
            G_FreeEdict(self);
            return;
        }
    }

    self->timestamp = level.time;
    target_lightramp_think(self);
}

void SP_target_lightramp(edict_t *self)
{
    if (!self->message || strlen(self->message) != 2 || self->message[0] < 'a' || self->message[0] > 'z' || self->message[1] < 'a' || self->message[1] > 'z' || self->message[0] == self->message[1]) {
        Com_WPrintf("target_lightramp has bad ramp (%s) at %s\n", self->message, vtos(self->s.origin));
        G_FreeEdict(self);
        return;
    }

    if (deathmatch.integer) {
        G_FreeEdict(self);
        return;
    }

    if (!self->target) {
        Com_WPrintf("%s with no target at %s\n", self->classname, vtos(self->s.origin));
        G_FreeEdict(self);
        return;
    }

    self->svflags |= SVF_NOCLIENT;
    self->use = target_lightramp_use;
    self->think = target_lightramp_think;

    self->movedir[0] = self->message[0] - 'a';
    self->movedir[1] = self->message[1] - 'a';
    self->movedir[2] = (self->movedir[1] - self->movedir[0]) / self->speed;
}

//==========================================================

/*QUAKED target_earthquake (1 0 0) (-8 -8 -8) (8 8 8)
When triggered, this initiates a level-wide earthquake.
All players and monsters are affected.
"speed"     severity of the quake (default:200)
"count"     duration of the quake (default:5)
*/

void target_earthquake_think(edict_t *self)
{
    if (self->last_move_time < level.time) {
        SV_PositionedSound(self->s.origin, game.world, CHAN_BODY, self->noise_index, 1.0f, ATTN_NONE, 0);
        self->last_move_time = level.time + 500;
    }

    for (edict_t *e = G_NextEnt(game.world); e; e = G_NextEnt(e)) {
        if (!e->client)
            continue;
        if (!e->groundentity)
            continue;

        e->groundentity = NULL;
        e->velocity[0] += self->speed * ((crandom() * 150) / e->mass);
        e->velocity[1] += self->speed * ((crandom() * 150) / e->mass);
        e->velocity[2] = self->speed * ((crandom() * 100) / e->mass);
    }

    if (level.time < self->timestamp)
        self->nextthink = level.time + 1;
}

void target_earthquake_use(edict_t *self, edict_t *other, edict_t *activator)
{
    self->timestamp = level.time + G_SecToMs(self->count);
    self->nextthink = level.time + 1;
    self->activator = activator;
    self->last_move_time = 0;
}

void SP_target_earthquake(edict_t *self)
{
    if (!self->targetname)
        Com_WPrintf("untargeted %s at %s\n", self->classname, vtos(self->s.origin));

    if (!self->count)
        self->count = 5;

    if (!self->speed)
        self->speed = 200;

    self->svflags |= SVF_NOCLIENT;
    self->think = target_earthquake_think;
    self->use = target_earthquake_use;

    if (!(self->spawnflags & 1))
        self->noise_index = SV_SoundIndex("world/quake.wav");
}

// Paril: gravity change support
void target_gravity_use(edict_t *self, edict_t *other, edict_t *activator)
{
    level.gravity = self->dmg;
}

void SP_target_gravity(edict_t *self)
{
    if (!st.gravity || !*st.gravity)
        self->dmg = 800;
    else
        self->dmg = atof(st.gravity);

    self->use = target_gravity_use;
}

bool NoAmmoWeaponChange(edict_t *ent);

void target_removeweapons_use(edict_t *self, edict_t *other, edict_t *activator)
{
    for (int i = 0; i < game.maxclients; i++)
    {
        gclient_t *cl = &game.clients[i];

        for (int x = ITEM_AXE; x <= ITEM_THUNDERBOLT; x++)
            if (cl->pers.inventory[x])
                cl->pers.inventory[x] = 0;

        edict_t *e = &globals.entities[i + 1];

        if (e->inuse)
            NoAmmoWeaponChange(e);
    }
}

void SP_target_removeweapons(edict_t *self)
{
    self->use = target_removeweapons_use;
}