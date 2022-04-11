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


/*
=================
check_dodge

This is a support routine used when a client is firing
a non-instant attack weapon.  It checks to see if a
monster's dodge function should be called.
=================
*/
static void check_dodge(edict_t *self, vec3_t start, vec3_t dir, int speed)
{
    vec3_t  end;
    vec3_t  v;
    trace_t tr;
    float   eta;

    // easy mode only ducks one quarter the time
    if (skill.integer == 0) {
        if (random() > 0.25f)
            return;
    }
    VectorMA(start, 8192, dir, end);
    tr = SV_Trace(start, NULL, NULL, end, self, MASK_SHOT);
    if ((tr.ent) && (tr.ent->svflags & SVF_MONSTER) && (tr.ent->health > 0) && (tr.ent->monsterinfo.dodge) && infront(tr.ent, self)) {
        VectorSubtract(tr.endpos, start, v);
        eta = (VectorLength(v) - tr.ent->maxs[0]) / speed;
        tr.ent->monsterinfo.dodge(tr.ent, self, eta);
    }
}


/*
=================
fire_hit

Used for all impact (hit/punch/slash) attacks
=================
*/
bool fire_hit(edict_t *self, vec3_t aim, int damage, int kick)
{
    trace_t     tr;
    vec3_t      forward, right, up;
    vec3_t      v;
    vec3_t      point;
    float       range;
    vec3_t      dir;

    //see if enemy is in range
    VectorSubtract(self->enemy->s.origin, self->s.origin, dir);
    range = VectorLength(dir);
    if (range > aim[0])
        return false;

    if (aim[1] > self->mins[0] && aim[1] < self->maxs[0]) {
        // the hit is straight on so back the range up to the edge of their bbox
        range -= self->enemy->maxs[0];
    } else {
        // this is a side hit so adjust the "right" value out to the edge of their bbox
        if (aim[1] < 0)
            aim[1] = self->enemy->mins[0];
        else
            aim[1] = self->enemy->maxs[0];
    }

    VectorMA(self->s.origin, range, dir, point);

    tr = SV_Trace(self->s.origin, NULL, NULL, point, self, MASK_SHOT);
    if (tr.fraction < 1) {
        if (!tr.ent->takedamage)
            return false;
        // if it will hit any client/monster then hit the one we wanted to hit
        if ((tr.ent->svflags & SVF_MONSTER) || (tr.ent->client))
            tr.ent = self->enemy;
    }

    AngleVectors(self->s.angles, forward, right, up);
    VectorMA(self->s.origin, range, forward, point);
    VectorMA(point, aim[1], right, point);
    VectorMA(point, aim[2], up, point);
    VectorSubtract(point, self->enemy->s.origin, dir);

    // do the damage
    T_Damage(tr.ent, self, self, dir, point, vec3_origin, damage, kick / 2, DAMAGE_NO_KNOCKBACK, MOD_HIT);

    if (!(tr.ent->svflags & SVF_MONSTER) && (!tr.ent->client))
        return false;

    // do our special form of knockback here
    VectorMA(self->enemy->absmin, 0.5f, self->enemy->size, v);
    VectorSubtract(v, point, v);
    VectorNormalize(v);
    VectorMA(self->enemy->velocity, kick, v, self->enemy->velocity);
    if (self->enemy->velocity[2] > 0)
        self->enemy->groundentity = NULL;
    return true;
}


/*
=================
fire_lead

This is an internal support routine used for bullet/pellet based weapons.
=================
*/
static void fire_lead(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int te_impact, int hspread, int vspread, int mod)
{
    trace_t     tr;
    vec3_t      dir;
    vec3_t      forward, right, up;
    vec3_t      end;
    float       r;
    float       u;
    vec3_t      water_start;
    bool        water = false;
    int         content_mask = MASK_SHOT | MASK_WATER;

    tr = SV_Trace(self->s.origin, NULL, NULL, start, self, MASK_SHOT);
    if (!(tr.fraction < 1.0f)) {
        vectoangles(aimdir, dir);
        AngleVectors(dir, forward, right, up);

        r = crandom() * hspread;
        u = crandom() * vspread;
        VectorMA(start, 8192, forward, end);
        VectorMA(end, r, right, end);
        VectorMA(end, u, up, end);

        if (SV_PointContents(start) & MASK_WATER) {
            water = true;
            VectorCopy(start, water_start);
            content_mask &= ~MASK_WATER;
        }

        tr = SV_Trace(start, NULL, NULL, end, self, content_mask);

        // see if we hit water
        if (tr.contents & MASK_WATER) {
            int     color;

            water = true;
            VectorCopy(tr.endpos, water_start);

            if (!VectorCompare(start, tr.endpos)) {
                if (tr.contents & CONTENTS_WATER) {
                    if (strcmp(tr.surface->name, "*brwater") == 0)
                        color = SPLASH_BROWN_WATER;
                    else
                        color = SPLASH_BLUE_WATER;
                } else if (tr.contents & CONTENTS_SLIME)
                    color = SPLASH_SLIME;
                else if (tr.contents & CONTENTS_LAVA)
                    color = SPLASH_LAVA;
                else
                    color = SPLASH_UNKNOWN;

                if (color != SPLASH_UNKNOWN) {
                    SV_WriteByte(svc_temp_entity);
                    SV_WriteByte(TE_SPLASH);
                    SV_WriteByte(8);
                    SV_WritePos(tr.endpos);
                    SV_WriteDir(tr.plane.normal);
                    SV_WriteByte(color);
                    SV_Multicast(tr.endpos, MULTICAST_PVS, false);
                }

                // change bullet's course when it enters water
                VectorSubtract(end, start, dir);
                vectoangles(dir, dir);
                AngleVectors(dir, forward, right, up);
                r = crandom() * hspread * 2;
                u = crandom() * vspread * 2;
                VectorMA(water_start, 8192, forward, end);
                VectorMA(end, r, right, end);
                VectorMA(end, u, up, end);
            }

            // re-trace ignoring water this time
            tr = SV_Trace(water_start, NULL, NULL, end, self, MASK_SHOT);
        }
    }

    // send gun puff / flash
    if (!((tr.surface) && (tr.surface->flags & SURF_SKY))) {
        if (tr.fraction < 1.0f) {
            if (tr.ent->takedamage) {
                T_Damage(tr.ent, self, self, aimdir, tr.endpos, tr.plane.normal, damage, kick, DAMAGE_BULLET, mod);
            } else {
                if (strncmp(tr.surface->name, "sky", 3) != 0) {
                    SV_WriteByte(svc_temp_entity);
                    SV_WriteByte(te_impact);
                    SV_WritePos(tr.endpos);
                    SV_WriteDir(tr.plane.normal);
                    SV_Multicast(tr.endpos, MULTICAST_PVS, false);

                    if (self->client)
                        PlayerNoise(self, tr.endpos, PNOISE_IMPACT);
                }
            }
        }
    }

    // if went through water, determine where the end and make a bubble trail
    if (water) {
        vec3_t  pos;

        VectorSubtract(tr.endpos, water_start, dir);
        VectorNormalize(dir);
        VectorMA(tr.endpos, -2, dir, pos);
        if (SV_PointContents(pos) & MASK_WATER)
            VectorCopy(pos, tr.endpos);
        else
            tr = SV_Trace(pos, NULL, NULL, water_start, tr.ent, MASK_WATER);

        VectorAdd(water_start, tr.endpos, pos);
        VectorScale(pos, 0.5f, pos);

        SV_WriteByte(svc_temp_entity);
        SV_WriteByte(TE_BUBBLETRAIL);
        SV_WritePos(water_start);
        SV_WritePos(tr.endpos);
        SV_Multicast(pos, MULTICAST_PVS, false);
    }
}


/*
=================
fire_bullet

Fires a single round.  Used for machinegun and chaingun.  Would be fine for
pistols, rifles, etc....
=================
*/
void fire_bullet(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int hspread, int vspread, int mod)
{
    fire_lead(self, start, aimdir, damage, kick, TE_GUNSHOT, hspread, vspread, mod);
}


/*
=================
fire_shotgun

Shoots shotgun pellets.  Used by shotgun and super shotgun.
=================
*/
void fire_shotgun(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick, int hspread, int vspread, int count, int mod)
{
    int     i;

    for (i = 0; i < count; i++)
        fire_lead(self, start, aimdir, damage, kick, TE_SHOTGUN, hspread, vspread, mod);
}


/*
=================
fire_blaster

Fires a single blaster bolt.  Used by the blaster and hyper blaster.
=================
*/
void blaster_touch(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
    int     mod;

    if (other == self->owner)
        return;

    if (surf && (surf->flags & SURF_SKY)) {
        G_FreeEdict(self);
        return;
    }

    if (self->owner->client)
        PlayerNoise(self->owner, self->s.origin, PNOISE_IMPACT);

    if (other->takedamage) {
        if (self->spawnflags & 1)
            mod = MOD_HYPERBLASTER;
        else
            mod = MOD_BLASTER;
        T_Damage(other, self, self->owner, self->velocity, self->s.origin, plane->normal, self->dmg, 1, DAMAGE_ENERGY, mod);
    } else {
        SV_WriteByte(svc_temp_entity);
        SV_WriteByte(TE_BLASTER);
        SV_WritePos(self->s.origin);
        if (!plane)
            SV_WriteDir(vec3_origin);
        else
            SV_WriteDir(plane->normal);
        SV_Multicast(self->s.origin, MULTICAST_PVS, false);
    }

    G_FreeEdict(self);
}

void fire_blaster(edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, int effect, bool hyper)
{
    edict_t *bolt;
    trace_t tr;

    VectorNormalize(dir);

    bolt = G_Spawn();
    bolt->svflags = SVF_DEADMONSTER;
    // yes, I know it looks weird that projectiles are deadmonsters
    // what this means is that when prediction is used against the object
    // (blaster/hyperblaster shots), the player won't be solid clipped against
    // the object.  Right now trying to run into a firing hyperblaster
    // is very jerky since you are predicted 'against' the shots.
    VectorCopy(start, bolt->s.origin);
    VectorCopy(start, bolt->s.old_origin);
    vectoangles(dir, bolt->s.angles);
    VectorScale(dir, speed, bolt->velocity);
    bolt->movetype = MOVETYPE_FLYMISSILE;
    bolt->clipmask = MASK_SHOT;
    bolt->solid = SOLID_BBOX;
    bolt->s.effects |= effect;
    VectorClear(bolt->mins);
    VectorClear(bolt->maxs);
    bolt->s.modelindex = SV_ModelIndex("models/objects/laser/tris.md2");
    bolt->s.sound = SV_SoundIndex("misc/lasfly.wav");
    bolt->owner = self;
    bolt->touch = blaster_touch;
    bolt->nextthink = level.time + 2000;
    bolt->think = G_FreeEdict;
    bolt->dmg = damage;
    bolt->classname = "bolt";
    if (hyper)
        bolt->spawnflags = 1;
    SV_LinkEntity(bolt);

    if (self->client)
        check_dodge(self, bolt->s.origin, dir, speed);

    tr = SV_Trace(self->s.origin, NULL, NULL, bolt->s.origin, bolt, MASK_SHOT);
    if (tr.fraction < 1.0f) {
        VectorMA(bolt->s.origin, -10, dir, bolt->s.origin);
        bolt->touch(bolt, tr.ent, NULL, NULL);
    }
}


/*
=================
fire_grenade
=================
*/
void Grenade_Explode(edict_t *ent)
{
    vec3_t      origin;
    int         mod;

    if (ent->owner->client)
        PlayerNoise(ent->owner, ent->s.origin, PNOISE_IMPACT);

    //FIXME: if we are onground then raise our Z just a bit since we are a point?
    if (ent->enemy) {
        float   points;
        vec3_t  v;
        vec3_t  dir;

        VectorAdd(ent->enemy->mins, ent->enemy->maxs, v);
        VectorMA(ent->enemy->s.origin, 0.5f, v, v);
        VectorSubtract(ent->s.origin, v, v);
        points = ent->dmg - 0.5f * VectorLength(v);
        VectorSubtract(ent->enemy->s.origin, ent->s.origin, dir);
        if (ent->spawnflags & 1)
            mod = MOD_HANDGRENADE;
        else
            mod = MOD_GRENADE;
        T_Damage(ent->enemy, ent, ent->owner, dir, ent->s.origin, vec3_origin, (int)points, (int)points, DAMAGE_RADIUS, mod);
    }

    if (ent->spawnflags & 2)
        mod = MOD_HELD_GRENADE;
    else if (ent->spawnflags & 1)
        mod = MOD_HG_SPLASH;
    else
        mod = MOD_G_SPLASH;
    T_RadiusDamage(ent, ent->owner, ent->dmg, ent->enemy, ent->dmg_radius, mod);

    VectorMA(ent->s.origin, -0.02f, ent->velocity, origin);
    SV_WriteByte(svc_temp_entity);
    if (ent->waterlevel) {
        if (ent->groundentity)
            SV_WriteByte(TE_GRENADE_EXPLOSION_WATER);
        else
            SV_WriteByte(TE_ROCKET_EXPLOSION_WATER);
    } else {
        if (ent->groundentity)
            SV_WriteByte(TE_GRENADE_EXPLOSION);
        else
            SV_WriteByte(TE_ROCKET_EXPLOSION);
    }
    SV_WritePos(origin);
    SV_Multicast(ent->s.origin, MULTICAST_PHS, false);

    G_FreeEdict(ent);
}

void Grenade_Touch(edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf)
{
    if (other == ent->owner)
        return;

    if (surf && (surf->flags & SURF_SKY)) {
        G_FreeEdict(ent);
        return;
    }

    if (!other->takedamage) {
        if (ent->spawnflags & 1) {
            if (random() > 0.5f)
                SV_StartSound(ent, CHAN_VOICE, SV_SoundIndex("weapons/hgrenb1a.wav"), 1, ATTN_NORM, 0);
            else
                SV_StartSound(ent, CHAN_VOICE, SV_SoundIndex("weapons/hgrenb2a.wav"), 1, ATTN_NORM, 0);
        } else {
            SV_StartSound(ent, CHAN_VOICE, SV_SoundIndex("weapons/grenlb1b.wav"), 1, ATTN_NORM, 0);
        }
        return;
    }

    ent->enemy = other;
    Grenade_Explode(ent);
}

void fire_grenade(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed, float timer, float damage_radius)
{
    edict_t *grenade;
    vec3_t  dir;
    vec3_t  forward, right, up;
    float   scale;

    vectoangles(aimdir, dir);
    AngleVectors(dir, forward, right, up);

    grenade = G_Spawn();
    VectorCopy(start, grenade->s.origin);
    VectorScale(aimdir, speed, grenade->velocity);
    scale = 200 + crandom() * 10.0f;
    VectorMA(grenade->velocity, scale, up, grenade->velocity);
    scale = crandom() * 10.0f;
    VectorMA(grenade->velocity, scale, right, grenade->velocity);
    VectorSet(grenade->avelocity, 300, 300, 300);
    grenade->movetype = MOVETYPE_BOUNCE;
    grenade->clipmask = MASK_SHOT;
    grenade->solid = SOLID_BBOX;
    grenade->s.effects |= EF_GRENADE;
    VectorClear(grenade->mins);
    VectorClear(grenade->maxs);
    grenade->s.modelindex = SV_ModelIndex("models/objects/grenade/tris.md2");
    grenade->owner = self;
    grenade->touch = Grenade_Touch;
    grenade->nextthink = level.time + G_SecToMs(timer);
    grenade->think = Grenade_Explode;
    grenade->dmg = damage;
    grenade->dmg_radius = damage_radius;
    grenade->classname = "grenade";

    SV_LinkEntity(grenade);
}

void fire_grenade2(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int speed, float timer, float damage_radius, bool held)
{
    edict_t *grenade;
    vec3_t  dir;
    vec3_t  forward, right, up;
    float   scale;

    vectoangles(aimdir, dir);
    AngleVectors(dir, forward, right, up);

    grenade = G_Spawn();
    VectorCopy(start, grenade->s.origin);
    VectorScale(aimdir, speed, grenade->velocity);
    scale = 200 + crandom() * 10.0f;
    VectorMA(grenade->velocity, scale, up, grenade->velocity);
    scale = crandom() * 10.0f;
    VectorMA(grenade->velocity, scale, right, grenade->velocity);
    VectorSet(grenade->avelocity, 300, 300, 300);
    grenade->movetype = MOVETYPE_BOUNCE;
    grenade->clipmask = MASK_SHOT;
    grenade->solid = SOLID_BBOX;
    grenade->s.effects |= EF_GRENADE;
    VectorClear(grenade->mins);
    VectorClear(grenade->maxs);
    grenade->s.modelindex = SV_ModelIndex("models/objects/grenade2/tris.md2");
    grenade->owner = self;
    grenade->touch = Grenade_Touch;
    grenade->nextthink = level.time + G_SecToMs(timer);
    grenade->think = Grenade_Explode;
    grenade->dmg = damage;
    grenade->dmg_radius = damage_radius;
    grenade->classname = "hgrenade";
    if (held)
        grenade->spawnflags = 3;
    else
        grenade->spawnflags = 1;
    grenade->s.sound = SV_SoundIndex("weapons/hgrenc1b.wav");

    if (timer <= 0.0f)
        Grenade_Explode(grenade);
    else {
        SV_StartSound(self, CHAN_WEAPON, SV_SoundIndex("weapons/hgrent1a.wav"), 1, ATTN_NORM, 0);
        SV_LinkEntity(grenade);
    }
}


/*
=================
fire_rocket
=================
*/
void rocket_touch(edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf)
{
    vec3_t      origin;
    int         n;

    if (other == ent->owner)
        return;

    if (surf && (surf->flags & SURF_SKY)) {
        G_FreeEdict(ent);
        return;
    }

    if (ent->owner->client)
        PlayerNoise(ent->owner, ent->s.origin, PNOISE_IMPACT);

    // calculate position for the explosion entity
    VectorMA(ent->s.origin, -0.02f, ent->velocity, origin);

    if (other->takedamage) {
        T_Damage(other, ent, ent->owner, ent->velocity, ent->s.origin, plane->normal, ent->dmg, 0, 0, MOD_ROCKET);
    } else {
        // don't throw any debris in net games
        if (!deathmatch.integer && !coop.integer) {
            if ((surf) && !(surf->flags & (SURF_WARP | SURF_TRANS33 | SURF_TRANS66 | SURF_FLOWING))) {
                n = Q_rand() % 5;
                while (n--)
                    ThrowDebris(ent, "models/objects/debris2/tris.md2", 2, ent->s.origin);
            }
        }
    }

    T_RadiusDamage(ent, ent->owner, ent->radius_dmg, other, ent->dmg_radius, MOD_R_SPLASH);

    SV_WriteByte(svc_temp_entity);
    if (ent->waterlevel)
        SV_WriteByte(TE_ROCKET_EXPLOSION_WATER);
    else
        SV_WriteByte(TE_ROCKET_EXPLOSION);
    SV_WritePos(origin);
    SV_Multicast(ent->s.origin, MULTICAST_PHS, false);

    G_FreeEdict(ent);
}

void fire_rocket(edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, float damage_radius, int radius_damage)
{
    edict_t *rocket;

    rocket = G_Spawn();
    VectorCopy(start, rocket->s.origin);
    VectorCopy(dir, rocket->movedir);
    vectoangles(dir, rocket->s.angles);
    VectorScale(dir, speed, rocket->velocity);
    rocket->movetype = MOVETYPE_FLYMISSILE;
    rocket->clipmask = MASK_SHOT;
    rocket->solid = SOLID_BBOX;
    rocket->s.effects |= EF_ROCKET;
    VectorClear(rocket->mins);
    VectorClear(rocket->maxs);
    rocket->s.modelindex = SV_ModelIndex("models/objects/rocket/tris.md2");
    rocket->owner = self;
    rocket->touch = rocket_touch;
    rocket->nextthink = level.time + 4000;
    rocket->think = G_FreeEdict;
    rocket->dmg = damage;
    rocket->radius_dmg = radius_damage;
    rocket->dmg_radius = damage_radius;
    rocket->s.sound = SV_SoundIndex("weapons/rockfly.wav");
    rocket->classname = "rocket";

    if (self->client)
        check_dodge(self, rocket->s.origin, dir, speed);

    SV_LinkEntity(rocket);
}


/*
=================
fire_nail
=================
*/
void nail_touch(edict_t *ent, edict_t *other, cplane_t *plane, csurface_t *surf)
{
    if (other == ent->owner)
        return;

    if (surf && (surf->flags & SURF_SKY)) {
        G_FreeEdict(ent);
        return;
    }

    if (ent->owner->client)
        PlayerNoise(ent->owner, ent->s.origin, PNOISE_IMPACT);

    if (other->takedamage) {
        T_Damage(other, ent, ent->owner, ent->velocity, ent->s.origin, plane->normal, ent->dmg, 0, 0, MOD_ROCKET);
    } else {
        SV_WriteByte(svc_temp_entity);
        SV_WriteByte(TE_GUNSHOT);
        SV_WritePos(ent->s.origin);
        SV_WriteDir(plane->normal);
        SV_Multicast(ent->s.origin, MULTICAST_PHS, false);
    }

    G_FreeEdict(ent);
}

void fire_nail(edict_t *self, vec3_t start, vec3_t dir, int damage, int speed)
{
    edict_t *nail = G_Spawn();
    VectorCopy(start, nail->s.origin);
    vectoangles(dir, nail->s.angles);
    VectorScale(dir, speed, nail->velocity);
    nail->movetype = MOVETYPE_FLYMISSILE;
    nail->clipmask = MASK_SHOT;
    nail->solid = SOLID_BBOX;
    nail->s.modelindex = SV_ModelIndex("models/objects/nail/nail.iqm");
    nail->owner = self;
    nail->touch = nail_touch;
    nail->nextthink = level.time + 2000;
    nail->think = G_FreeEdict;
    nail->dmg = damage;
    nail->classname = "nail";

    if (self->client)
        check_dodge(self, nail->s.origin, dir, speed);

    SV_LinkEntity(nail);
}


/*
=================
fire_rail
=================
*/
void fire_rail(edict_t *self, vec3_t start, vec3_t aimdir, int damage, int kick)
{
    vec3_t      from;
    vec3_t      end;
    trace_t     tr;
    edict_t     *ignore;
    int         mask;
    bool        water;
    float       lastfrac;

    VectorMA(start, 8192, aimdir, end);
    VectorCopy(start, from);
    ignore = self;
    water = false;
    mask = MASK_SHOT | CONTENTS_SLIME | CONTENTS_LAVA;
    lastfrac = 1;
    while (ignore) {
        tr = SV_Trace(from, NULL, NULL, end, ignore, mask);

        if (tr.contents & (CONTENTS_SLIME | CONTENTS_LAVA)) {
            mask &= ~(CONTENTS_SLIME | CONTENTS_LAVA);
            water = true;
        } else {
            //ZOID--added so rail goes through SOLID_BBOX entities (gibs, etc)
            if (((tr.ent->svflags & SVF_MONSTER) || (tr.ent->client) ||
                (tr.ent->solid == SOLID_BBOX)) && (lastfrac + tr.fraction > 0))
                ignore = tr.ent;
            else
                ignore = NULL;

            if ((tr.ent != self) && (tr.ent->takedamage))
                T_Damage(tr.ent, self, self, aimdir, tr.endpos, tr.plane.normal, damage, kick, 0, MOD_RAILGUN);
        }

        VectorCopy(tr.endpos, from);
        lastfrac = tr.fraction;
    }

    // send gun puff / flash
    SV_WriteByte(svc_temp_entity);
    SV_WriteByte(TE_RAILTRAIL);
    SV_WritePos(start);
    SV_WritePos(tr.endpos);
    SV_Multicast(self->s.origin, MULTICAST_PHS, false);
    if (water) {
        SV_WriteByte(svc_temp_entity);
        SV_WriteByte(TE_RAILTRAIL);
        SV_WritePos(start);
        SV_WritePos(tr.endpos);
        SV_Multicast(tr.endpos, MULTICAST_PHS, false);
    }

    if (self->client)
        PlayerNoise(self, tr.endpos, PNOISE_IMPACT);
}


/*
=================
fire_bfg
=================
*/
void bfg_explode(edict_t *self)
{
    edict_t *ent;
    float   points;
    vec3_t  v;
    float   dist;

    if (self->s.frame == 0) {
        // the BFG effect
        ent = NULL;
        while ((ent = findradius(ent, self->s.origin, self->dmg_radius)) != NULL) {
            if (!ent->takedamage)
                continue;
            if (ent == self->owner)
                continue;
            if (!CanDamage(ent, self))
                continue;
            if (!CanDamage(ent, self->owner))
                continue;

            VectorAdd(ent->mins, ent->maxs, v);
            VectorMA(ent->s.origin, 0.5f, v, v);
            VectorSubtract(self->s.origin, v, v);
            dist = VectorLength(v);
            points = self->radius_dmg * (1.0f - sqrtf(dist / self->dmg_radius));
            if (ent == self->owner)
                points = points * 0.5f;

            SV_WriteByte(svc_temp_entity);
            SV_WriteByte(TE_BFG_EXPLOSION);
            SV_WritePos(ent->s.origin);
            SV_Multicast(ent->s.origin, MULTICAST_PHS, false);
            T_Damage(ent, self, self->owner, self->velocity, ent->s.origin, vec3_origin, (int)points, 0, DAMAGE_ENERGY, MOD_BFG_EFFECT);
        }
    }

    self->nextthink = level.time + 100;
    self->s.frame++;
    if (self->s.frame == 5)
        self->think = G_FreeEdict;
}

void bfg_touch(edict_t *self, edict_t *other, cplane_t *plane, csurface_t *surf)
{
    if (other == self->owner)
        return;

    if (surf && (surf->flags & SURF_SKY)) {
        G_FreeEdict(self);
        return;
    }

    if (self->owner->client)
        PlayerNoise(self->owner, self->s.origin, PNOISE_IMPACT);

    // core explosion - prevents firing it into the wall/floor
    if (other->takedamage)
        T_Damage(other, self, self->owner, self->velocity, self->s.origin, plane->normal, 200, 0, 0, MOD_BFG_BLAST);
    T_RadiusDamage(self, self->owner, 200, other, 100, MOD_BFG_BLAST);

    SV_StartSound(self, CHAN_VOICE, SV_SoundIndex("weapons/bfg__x1b.wav"), 1, ATTN_NORM, 0);
    self->solid = SOLID_NOT;
    self->touch = NULL;
    VectorMA(self->s.origin, -1 * BASE_FRAMETIME_S, self->velocity, self->s.origin);
    VectorClear(self->velocity);
    self->s.modelindex = SV_ModelIndex("sprites/s_bfg3.sp2");
    self->s.frame = 0;
    self->s.sound = 0;
    self->s.effects &= ~EF_ANIM_ALLFAST;
    self->think = bfg_explode;
    self->nextthink = level.time + 1;
    self->enemy = other;

    SV_WriteByte(svc_temp_entity);
    SV_WriteByte(TE_BFG_BIGEXPLOSION);
    SV_WritePos(self->s.origin);
    SV_Multicast(self->s.origin, MULTICAST_PVS, false);
}


void bfg_think(edict_t *self)
{
    edict_t *ent;
    edict_t *ignore;
    vec3_t  point;
    vec3_t  dir;
    vec3_t  start;
    vec3_t  end;
    int     dmg;
    trace_t tr;

    if (deathmatch.integer)
        dmg = 5;
    else
        dmg = 10;

    ent = NULL;
    while ((ent = findradius(ent, self->s.origin, 256)) != NULL) {
        if (ent == self)
            continue;

        if (ent == self->owner)
            continue;

        if (!ent->takedamage)
            continue;

        if (!(ent->svflags & SVF_MONSTER) && (!ent->client) && (strcmp(ent->classname, "misc_explobox") != 0))
            continue;

        VectorMA(ent->absmin, 0.5f, ent->size, point);

        VectorSubtract(point, self->s.origin, dir);
        VectorNormalize(dir);

        ignore = self;
        VectorCopy(self->s.origin, start);
        VectorMA(start, 2048, dir, end);
        while (1) {
            tr = SV_Trace(start, NULL, NULL, end, ignore, CONTENTS_SOLID | CONTENTS_MONSTER | CONTENTS_DEADMONSTER);

            if (!tr.ent)
                break;

            // hurt it if we can
            if ((tr.ent->takedamage) && !(tr.ent->flags & FL_IMMUNE_LASER) && (tr.ent != self->owner))
                T_Damage(tr.ent, self, self->owner, dir, tr.endpos, vec3_origin, dmg, 1, DAMAGE_ENERGY, MOD_BFG_LASER);

            // if we hit something that's not a monster or player we're done
            if (!(tr.ent->svflags & SVF_MONSTER) && (!tr.ent->client)) {
                SV_WriteByte(svc_temp_entity);
                SV_WriteByte(TE_LASER_SPARKS);
                SV_WriteByte(4);
                SV_WritePos(tr.endpos);
                SV_WriteDir(tr.plane.normal);
                SV_WriteByte(self->s.skinnum);
                SV_Multicast(tr.endpos, MULTICAST_PVS, false);
                break;
            }

            ignore = tr.ent;
            VectorCopy(tr.endpos, start);
        }

        SV_WriteByte(svc_temp_entity);
        SV_WriteByte(TE_BFG_LASER);
        SV_WritePos(self->s.origin);
        SV_WritePos(tr.endpos);
        SV_Multicast(self->s.origin, MULTICAST_PHS, false);
    }

    self->nextthink = level.time + 100;
}


void fire_bfg(edict_t *self, vec3_t start, vec3_t dir, int damage, int speed, float damage_radius)
{
    edict_t *bfg;

    bfg = G_Spawn();
    VectorCopy(start, bfg->s.origin);
    VectorCopy(dir, bfg->movedir);
    vectoangles(dir, bfg->s.angles);
    VectorScale(dir, speed, bfg->velocity);
    bfg->movetype = MOVETYPE_FLYMISSILE;
    bfg->clipmask = MASK_SHOT;
    bfg->solid = SOLID_BBOX;
    bfg->s.effects |= EF_BFG | EF_ANIM_ALLFAST;
    VectorClear(bfg->mins);
    VectorClear(bfg->maxs);
    bfg->s.modelindex = SV_ModelIndex("sprites/s_bfg1.sp2");
    bfg->owner = self;
    bfg->touch = bfg_touch;
    bfg->nextthink = level.time + 2000;
    bfg->think = G_FreeEdict;
    bfg->radius_dmg = damage;
    bfg->dmg_radius = damage_radius;
    bfg->classname = "bfg blast";
    bfg->s.sound = SV_SoundIndex("weapons/bfg__l1a.wav");

    bfg->think = bfg_think;
    bfg->nextthink = level.time + 100;
    bfg->teammaster = bfg;
    bfg->teamchain = NULL;

    if (self->client)
        check_dodge(self, bfg->s.origin, dir, speed);

    SV_LinkEntity(bfg);
}

/*
 * Drops a spark from the flare flying thru the air.  Checks to make
 * sure we aren't in the water.
 */
void flare_sparks(edict_t *self)
{
	vec3_t dir;
	vec3_t forward, right, up;
	// Spawn some sparks.  This isn't net-friendly at all, but will 
	// be fine for single player. 
	// 
	SV_WriteByte(svc_temp_entity);
	SV_WriteByte(TE_FLARE);

    SV_WriteShort((int)(self - g_edicts));
    // if this is the first tick of flare, set count to 1 to start the sound
    SV_WriteByte(self->air_finished_time);
    SV_WritePos(self->s.origin);

	// If we are still moving, calculate the normal to the direction 
	 // we are travelling. 
	 // 
	if (VectorLength(self->velocity) > 0.0)
	{
		vectoangles(self->velocity, dir);
		AngleVectors(dir, forward, right, up);

		SV_WriteDir(up);
	}
	// If we're stopped, just write out the origin as our normal 
	// 
	else
	{
		SV_WriteDir(vec3_origin);
	}
	SV_Multicast(self->s.origin, MULTICAST_PVS, false);

    self->air_finished_time = 0;
}

/*
   void flare_think( edict_t *self )

   Purpose: The think function of a flare round.  It generates sparks
			on the flare using a temp entity, and kills itself after
			self->timestamp runs out.
   Parameters:
	 self: A pointer to the edict_t structure representing the
		   flare round.  self->timestamp is the value used to
		   measure the lifespan of the round, and is set in
		   fire_flaregun blow.

   Notes:
	 - I'm not sure how much bandwidth is eaten by spawning a temp
	   entity every BASE_FRAMETIME_S seconds.  It might very well turn out
	   that the sparks need to go bye-bye in favor of less bandwidth
	   usage.  Then again, why the hell would you use this gun on
	   a DM server????

	 - I haven't seen self->timestamp used anywhere else in the code,
	   but I never really looked that hard.  It doesn't seem to cause
	   any problems, and is aptly named, so I used it.
 */
void flare_think(edict_t *self)
{
	// self->timestamp is 15 seconds after the flare was spawned. 
	// 
	if (level.time > self->timestamp)
	{
		G_FreeEdict(self);
		return;
	}

	// We're still active, so lets shoot some sparks. 
	// 
	flare_sparks(self);
	
	// We'll think again in .2 seconds 
	// 
	self->nextthink = level.time + 200;
}

void flare_touch(edict_t *ent, edict_t *other,
	cplane_t *plane, csurface_t *surf)
{
	// Flares don't weigh that much, so let's have them stop 
	// the instant they whack into anything. 
	// 
	VectorClear(ent->velocity);
}

void fire_flaregun(edict_t *self, vec3_t start, vec3_t aimdir,
	int damage, int speed, float timer,
	float damage_radius)
{
	edict_t *flare;
	vec3_t dir;
	vec3_t forward, right, up;

	vectoangles(aimdir, dir);
	AngleVectors(dir, forward, right, up);

	flare = G_Spawn();
	VectorCopy(start, flare->s.origin);
	VectorScale(aimdir, speed, flare->velocity);
	VectorSet(flare->avelocity, 300, 300, 300);
	flare->movetype = MOVETYPE_BOUNCE;
	flare->clipmask = MASK_SHOT;
	flare->solid = SOLID_BBOX;

	const float size = 4;
	VectorSet(flare->mins, -size, -size, -size);
	VectorSet(flare->maxs, size, size, size);

	flare->s.modelindex = SV_ModelIndex("models/objects/flare/tris.md2");
	flare->owner = self;
	flare->touch = flare_touch;
	flare->nextthink = level.time + 200;
	flare->think = flare_think;
	flare->radius_dmg = damage;
	flare->dmg_radius = damage_radius;
	flare->classname = "flare";
	flare->timestamp = level.time + G_SecToMs(15);
    flare->air_finished_time = 1;
	SV_LinkEntity(flare);
}