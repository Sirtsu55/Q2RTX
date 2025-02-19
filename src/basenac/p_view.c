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
#include "m_player.h"



static  edict_t     *current_player;
static  gclient_t   *current_client;

static  vec3_t  forward, right, up;
float   xyspeed;

float   bobmove;
int     bobcycle;       // odd cycles are right foot going forward
float   bobfracsin;     // sin(bobfrac*M_PI)

/*
===============
SV_CalcRoll

===============
*/
float SV_CalcRoll(vec3_t angles, vec3_t velocity)
{
    float   sign;
    float   side;
    float   value;

    side = DotProduct(velocity, right);
    sign = side < 0 ? -1 : 1;
    side = fabsf(side);

    value = sv_rollangle.value;

    if (side < sv_rollspeed.value)
        side = side * value / sv_rollspeed.value;
    else
        side = value;

    return side * sign;

}


/*
===============
P_DamageFeedback

Handles color blends and view kicks
===============
*/
void P_DamageFeedback(edict_t *player)
{
    gclient_t   *client;
    float   side;
    float   realcount, count, kick;
    vec3_t  v;
    int     r, l;
    static  vec3_t  acolor = {1.0, 1.0, 1.0};
    static  vec3_t  bcolor = {1.0, 0.0, 0.0};

    client = player->client;

    // flash the backgrounds behind the status numbers
    client->ps.stats[STAT_FLASHES] = 0;
    if (client->damage_blood)
        client->ps.stats[STAT_FLASHES] |= 1;
    if (client->damage_armor && !(player->flags & FL_GODMODE) && (client->invincible_time <= level.time))
        client->ps.stats[STAT_FLASHES] |= 2;

    // total points of damage shot at the player this frame
    count = (client->damage_blood + client->damage_armor);
    if (count == 0)
        return;     // didn't take any damage

    // start a pain animation if still in the player model
    if (client->anim_priority < ANIM_PAIN && player->s.modelindex == 255) {
        static int      i;

        client->anim_priority = ANIM_PAIN;
        if (client->ps.pmove.pm_flags & PMF_DUCKED) {
            player->s.frame = FRAME_crpain1 - 1;
            client->anim_end = FRAME_crpain4;
        } else {
            i = (i + 1) % 3;
            switch (i) {
            case 0:
                player->s.frame = FRAME_pain101 - 1;
                client->anim_end = FRAME_pain104;
                break;
            case 1:
                player->s.frame = FRAME_pain201 - 1;
                client->anim_end = FRAME_pain204;
                break;
            case 2:
                player->s.frame = FRAME_pain301 - 1;
                client->anim_end = FRAME_pain304;
                break;
            }
        }
    }

    realcount = count;
    if (count < 10)
        count = 10; // always make a visible effect

    // play an apropriate pain sound
    if ((level.time > player->pain_debounce_time) && !(player->flags & FL_GODMODE) && (client->invincible_time <= level.time)) {
        r = 1 + (Q_rand() & 1);
        player->pain_debounce_time = level.time + 700;
        if (player->health < 25)
            l = 25;
        else if (player->health < 50)
            l = 50;
        else if (player->health < 75)
            l = 75;
        else
            l = 100;
        SV_StartSound(player, CHAN_VOICE, SV_SoundIndex(va("*pain%i_%i.wav", l, r)), 1, ATTN_NORM, 0);
    }

    // the total alpha of the blend is always proportional to count
    if (client->damage_alpha < 0)
        client->damage_alpha = 0;
    client->damage_alpha += count * 0.01f;
    if (client->damage_alpha < 0.2f)
        client->damage_alpha = 0.2f;
    if (client->damage_alpha > 0.6f)
        client->damage_alpha = 0.6f;    // don't go too saturated

    // the color of the blend will vary based on how much was absorbed
    // by different armors
    VectorClear(v);
    if (client->damage_armor)
        VectorMA(v, (float)client->damage_armor / realcount,  acolor, v);
    if (client->damage_blood)
        VectorMA(v, (float)client->damage_blood / realcount,  bcolor, v);
    VectorCopy(v, client->damage_blend);


    //
    // calculate view angle kicks
    //
    kick = abs(client->damage_knockback);
    if (kick && player->health > 0) { // kick of 0 means no view adjust at all
        kick = kick * 100 / player->health;

        if (kick < count * 0.5f)
            kick = count * 0.5f;
        if (kick > 50)
            kick = 50;

        VectorSubtract(client->damage_from, player->s.origin, v);
        VectorNormalize(v);

        side = DotProduct(v, right);
        client->v_dmg_roll = kick * side * 0.3f;

        side = -DotProduct(v, forward);
        client->v_dmg_pitch = kick * side * 0.3f;

        client->v_dmg_time = level.time + DAMAGE_TIME;
    }

    //
    // clear totals
    //
    client->damage_blood = 0;
    client->damage_armor = 0;
    client->damage_knockback = 0;
}




/*
===============
SV_CalcViewOffset

Auto pitching on slopes?

  fall from 128: 400 = 160000
  fall from 256: 580 = 336400
  fall from 384: 720 = 518400
  fall from 512: 800 = 640000
  fall from 640: 960 =

  damage = deltavelocity*deltavelocity  * 0.0001

===============
*/
void SV_CalcViewOffset(edict_t *ent)
{
    float       *angles;
    float       ratio;
    float       delta;
    vec3_t      v;


//===================================

    // base angles
    angles = ent->client->ps.kick_angles;

    // if dead, fix the angle and don't add any kick
    if (ent->deadflag) {
        VectorClear(angles);

        ent->client->ps.viewangles[ROLL] = 40;
        ent->client->ps.viewangles[PITCH] = -15;
        ent->client->ps.viewangles[YAW] = ent->client->killer_yaw;
    } else {
        // add angles based on weapon kick

        VectorCopy(ent->client->kick_angles, angles);

        // add angles based on damage kick

        ratio = (float) (ent->client->v_dmg_time - level.time) / DAMAGE_TIME;
        if (ratio < 0) {
            ratio = 0;
            ent->client->v_dmg_pitch = 0;
            ent->client->v_dmg_roll = 0;
        }
        angles[PITCH] += ratio * ent->client->v_dmg_pitch;
        angles[ROLL] += ratio * ent->client->v_dmg_roll;

        // add pitch based on fall kick

        ratio = (float) (ent->client->fall_time - level.time) / FALL_TIME;
        if (ratio < 0)
            ratio = 0;
        angles[PITCH] += ratio * ent->client->fall_value;

        // add angles based on velocity

        delta = DotProduct(ent->velocity, forward);
        angles[PITCH] += delta * (run_pitch.value / BASE_FRAMEDIV);

        delta = DotProduct(ent->velocity, right);
        angles[ROLL] += delta * (run_roll.value / BASE_FRAMEDIV);

        // add angles based on bob

        delta = bobfracsin * bob_pitch.value * xyspeed;
        if (ent->client->ps.pmove.pm_flags & PMF_DUCKED)
            delta *= 6;     // crouching
        angles[PITCH] += delta;
        delta = bobfracsin * bob_roll.value * xyspeed;
        if (ent->client->ps.pmove.pm_flags & PMF_DUCKED)
            delta *= 6;     // crouching
        if (bobcycle & 1)
            delta = -delta;
        angles[ROLL] += delta;
    }

//===================================

    // base origin

    VectorClear(v);

    // add view height

    v[2] += ent->viewheight;

    // absolutely bound offsets
    // so the view can never be outside the player box

    if (v[0] < -14)
        v[0] = -14;
    else if (v[0] > 14)
        v[0] = 14;
    if (v[1] < -14)
        v[1] = -14;
    else if (v[1] > 14)
        v[1] = 14;
    if (v[2] < -22)
        v[2] = -22;
    else if (v[2] > 30)
        v[2] = 30;

    VectorCopy(v, ent->client->ps.viewoffset);
}

/*
=============
SV_AddBlend
=============
*/
void SV_AddBlend(float r, float g, float b, float a, float *v_blend)
{
    float   a2, a3;

    if (a <= 0)
        return;
    a2 = v_blend[3] + (1 - v_blend[3]) * a; // new total alpha
    a3 = v_blend[3] / a2;   // fraction of color from old

    v_blend[0] = v_blend[0] * a3 + r * (1 - a3);
    v_blend[1] = v_blend[1] * a3 + g * (1 - a3);
    v_blend[2] = v_blend[2] * a3 + b * (1 - a3);
    v_blend[3] = a2;
}


/*
=============
SV_CalcBlend
=============
*/
void SV_CalcBlend(edict_t *ent)
{
    int     contents;
    vec3_t  vieworg;
    int     remaining;

    ent->client->ps.blend[0] = ent->client->ps.blend[1] =
                                   ent->client->ps.blend[2] = ent->client->ps.blend[3] = 0;

    // add for contents
    VectorAdd(ent->s.origin, ent->client->ps.viewoffset, vieworg);
    contents = SV_PointContents(vieworg);

	if (contents & (CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA))
        ent->client->ps.rdflags |= RDF_UNDERWATER;
    else
        ent->client->ps.rdflags &= ~RDF_UNDERWATER;

    if (contents & (CONTENTS_SOLID | CONTENTS_LAVA))
        SV_AddBlend(1.0f, 0.3f, 0.0f, 0.6f, ent->client->ps.blend);
    else if (contents & CONTENTS_SLIME)
        SV_AddBlend(0.0f, 0.1f, 0.05f, 0.6f, ent->client->ps.blend);
    else if (contents & CONTENTS_WATER)
        SV_AddBlend(0.5f, 0.3f, 0.2f, 0.4f, ent->client->ps.blend);

    // add for powerups
    if (ent->client->quad_time > level.time) {
        remaining = ent->client->quad_time - level.time;
        if (remaining == 3000)    // beginning to fade
            SV_StartSound(ent, CHAN_ITEM, SV_SoundIndex(ASSET_SOUND_QUAD_FADE), 1, ATTN_NORM, 0);
        if (remaining > 3000 || (remaining % 500) == 0)
            SV_AddBlend(0, 0, 1, 0.08f, ent->client->ps.blend);
    } else if (ent->client->invincible_time > level.time) {
        remaining = ent->client->invincible_time - level.time;
        if (remaining == 3000)    // beginning to fade
            SV_StartSound(ent, CHAN_ITEM, SV_SoundIndex(ASSET_SOUND_PENT_FADE), 1, ATTN_NORM, 0);
        if (remaining > 3000 || (remaining % 500) == 0)
            SV_AddBlend(1, 1, 0, 0.08f, ent->client->ps.blend);
    } else if (ent->client->enviro_time > level.time) {
        remaining = ent->client->enviro_time - level.time;
        if (remaining == 3000)    // beginning to fade
            SV_StartSound(ent, CHAN_ITEM, SV_SoundIndex(ASSET_SOUND_BIOSUIT_FADE), 1, ATTN_NORM, 0);
        if (remaining > 3000 || (remaining % 500) == 0)
            SV_AddBlend(0, 1, 0, 0.08f, ent->client->ps.blend);
    } else if (ent->client->ring_time > level.time) {
        remaining = ent->client->ring_time - level.time;
        if (remaining == 3000)    // beginning to fade
            SV_StartSound(ent, CHAN_ITEM, SV_SoundIndex(ASSET_SOUND_RING_FADE), 1, ATTN_NORM, 0);
        if (remaining > 3000 || (remaining % 500) == 0)
            SV_AddBlend(0.4, 0.3, 0.3, 0.05f, ent->client->ps.blend);
    }

    // add for damage
    if (ent->client->damage_alpha > 0)
        SV_AddBlend(ent->client->damage_blend[0], ent->client->damage_blend[1]
                    , ent->client->damage_blend[2], ent->client->damage_alpha, ent->client->ps.blend);

    if (ent->client->bonus_alpha > 0)
        SV_AddBlend(0.85f, 0.7f, 0.3f, ent->client->bonus_alpha, ent->client->ps.blend);

    // drop the damage value
    ent->client->damage_alpha -= 0.06f;
    if (ent->client->damage_alpha < 0)
        ent->client->damage_alpha = 0;

    // drop the bonus value
    ent->client->bonus_alpha -= 0.1f;
    if (ent->client->bonus_alpha < 0)
        ent->client->bonus_alpha = 0;
}


/*
=================
P_FallingDamage
=================
*/
void P_FallingDamage(edict_t *ent)
{
    float   delta;
    int     damage;
    vec3_t  dir;

    if (ent->s.modelindex != 255)
        return;     // not in the player model

    if (ent->movetype == MOVETYPE_NOCLIP)
        return;

    if ((ent->client->oldvelocity[2] < 0) && (ent->velocity[2] > ent->client->oldvelocity[2]) && (!ent->groundentity)) {
        delta = ent->client->oldvelocity[2];
    } else {
        if (!ent->groundentity)
            return;
        delta = ent->velocity[2] - ent->client->oldvelocity[2];
    }
    delta = delta * delta * 0.0001f;

    // never take falling damage if completely underwater
    if (ent->waterlevel == 3)
        return;
    if (ent->waterlevel == 2)
        delta *= 0.25f;
    if (ent->waterlevel == 1)
        delta *= 0.5f;

    if (delta < 1)
        return;

    if (delta < 15) {
        ent->s.event = EV_FOOTSTEP;
        return;
    }

    ent->client->fall_value = delta * 0.5f;
    if (ent->client->fall_value > 40)
        ent->client->fall_value = 40;
    ent->client->fall_time = level.time + FALL_TIME;

    if (delta > 30) {
        if (ent->health > 0) {
            if (delta >= 55)
                ent->s.event = EV_FALLFAR;
            else
                ent->s.event = EV_FALL;
        }
        ent->pain_debounce_time = level.time;   // no normal pain sound
        damage = (delta - 30) / 2;
        if (damage < 1)
            damage = 1;
        VectorSet(dir, 0, 0, 1);

        if (!deathmatch.integer || !(dmflags.integer & DF_NO_FALLING))
            T_Damage(ent, game.world, game.world, dir, ent->s.origin, vec3_origin, damage, 0, 0, MOD_FALLING);
    } else {
        ent->s.event = EV_FALLSHORT;
        return;
    }
}



/*
=============
P_WorldEffects
=============
*/
void P_WorldEffects(void)
{
    bool        envirosuit;
    int         waterlevel, old_waterlevel;

    if (current_player->movetype == MOVETYPE_NOCLIP) {
        current_player->air_finished_time = level.time + 12000; // don't need air
        return;
    }

    waterlevel = current_player->waterlevel;
    old_waterlevel = current_client->old_waterlevel;
    current_client->old_waterlevel = waterlevel;

    envirosuit = current_client->enviro_time > level.time;

    //
    // if just entered a water volume, play a sound
    //
    if (!old_waterlevel && waterlevel) {
        PlayerNoise(current_player, current_player->s.origin, PNOISE_SELF);
        if (current_player->watertype & CONTENTS_LAVA)
            SV_StartSound(current_player, CHAN_BODY, SV_SoundIndex(ASSET_SOUND_LAVA_ENTER), 1, ATTN_NORM, 0);
        else if (current_player->watertype & CONTENTS_SLIME)
            SV_StartSound(current_player, CHAN_BODY, SV_SoundIndex(ASSET_SOUND_WATER_ENTER), 1, ATTN_NORM, 0);
        else if (current_player->watertype & CONTENTS_WATER)
            SV_StartSound(current_player, CHAN_BODY, SV_SoundIndex(ASSET_SOUND_WATER_ENTER), 1, ATTN_NORM, 0);
        current_player->flags |= FL_INWATER;

        // clear damage_debounce, so the pain sound will play immediately
        current_player->damage_debounce_time = level.time - 1000;
    }

    //
    // if just completely exited a water volume, play a sound
    //
    if (old_waterlevel && ! waterlevel) {
        PlayerNoise(current_player, current_player->s.origin, PNOISE_SELF);
        SV_StartSound(current_player, CHAN_BODY, SV_SoundIndex(ASSET_SOUND_WATER_EXIT), 1, ATTN_NORM, 0);
        current_player->flags &= ~FL_INWATER;
    }

    //
    // check for head just going under water
    //
    if (old_waterlevel != 3 && waterlevel == 3) {
        SV_StartSound(current_player, CHAN_BODY, SV_SoundIndex(ASSET_SOUND_WATER_UNDER), 1, ATTN_NORM, 0);
    }

    //
    // check for head just coming out of water
    //
    if (old_waterlevel == 3 && waterlevel != 3) {
        if (current_player->air_finished_time < level.time) {
            // gasp for air
            SV_StartSound(current_player, CHAN_VOICE, SV_SoundIndex("player/gasp1.wav"), 1, ATTN_NORM, 0);
            PlayerNoise(current_player, current_player->s.origin, PNOISE_SELF);
        } else  if (current_player->air_finished_time < level.time + 11000) {
            // just break surface
            SV_StartSound(current_player, CHAN_VOICE, SV_SoundIndex("player/gasp2.wav"), 1, ATTN_NORM, 0);
        }
    }

    //
    // check for drowning
    //
    if (waterlevel == 3) {
        // breather or envirosuit give air
        if (envirosuit) {
            current_player->air_finished_time = level.time + 10000;
        }

        // if out of air, start drowning
        if (current_player->air_finished_time < level.time) {
            // drown!
            if (current_player->client->next_drown_time < level.time
                && current_player->health > 0) {
                current_player->client->next_drown_time = level.time + 1000;

                // take more damage the longer underwater
                current_player->dmg += 2;
                if (current_player->dmg > 15)
                    current_player->dmg = 15;

                // play a gurp sound instead of a normal pain sound
                if (current_player->health <= current_player->dmg)
                    SV_StartSound(current_player, CHAN_VOICE, SV_SoundIndex("player/drown1.wav"), 1, ATTN_NORM, 0);
                else if (Q_rand() & 1)
                    SV_StartSound(current_player, CHAN_VOICE, SV_SoundIndex("*gurp1.wav"), 1, ATTN_NORM, 0);
                else
                    SV_StartSound(current_player, CHAN_VOICE, SV_SoundIndex("*gurp2.wav"), 1, ATTN_NORM, 0);

                current_player->pain_debounce_time = level.time;

                T_Damage(current_player, game.world, game.world, vec3_origin, current_player->s.origin, vec3_origin, current_player->dmg, 0, DAMAGE_NO_ARMOR, MOD_WATER);
            }
        }
    } else {
        current_player->air_finished_time = level.time + 12000;
        current_player->dmg = 2;
    }

    //
    // check for sizzle damage
    //
    if (waterlevel && (current_player->watertype & (CONTENTS_LAVA | CONTENTS_SLIME))) {
        if (current_player->watertype & CONTENTS_LAVA) {
            if (current_player->health > 0
                && current_player->pain_debounce_time <= level.time
                && current_client->invincible_time < level.time) {
                if (Q_rand() & 1)
                    SV_StartSound(current_player, CHAN_VOICE, SV_SoundIndex("player/burn1.wav"), 1, ATTN_NORM, 0);
                else
                    SV_StartSound(current_player, CHAN_VOICE, SV_SoundIndex("player/burn2.wav"), 1, ATTN_NORM, 0);
                current_player->pain_debounce_time = level.time + 1000;
            }

            if (envirosuit) // take 1/3 damage with envirosuit
                T_Damage(current_player, game.world, game.world, vec3_origin, current_player->s.origin, vec3_origin, 1 * waterlevel, 0, 0, MOD_LAVA);
            else
                T_Damage(current_player, game.world, game.world, vec3_origin, current_player->s.origin, vec3_origin, 3 * waterlevel, 0, 0, MOD_LAVA);
        }

        if (current_player->watertype & CONTENTS_SLIME) {
            if (!envirosuit) {
                // no damage from slime with envirosuit
                T_Damage(current_player, game.world, game.world, vec3_origin, current_player->s.origin, vec3_origin, 1 * waterlevel, 0, 0, MOD_SLIME);
            }
        }
    }
}


/*
===============
G_SetClientEffects
===============
*/
void G_SetClientEffects(edict_t *ent)
{
    int     remaining;

    ent->s.effects = 0;
    ent->s.renderfx = 0;

    if (ent->health <= 0 || level.intermission_time)
        return;

    if (ent->client->quad_time > level.time) {
        remaining = ent->client->quad_time - level.time;
        if (remaining > 3000 || (remaining % 500) == 0)
            ent->s.effects |= EF_QUAD;
    }

    if (ent->client->invincible_time > level.time) {
        remaining = ent->client->invincible_time - level.time;
        if (remaining > 3000 || (remaining % 500) == 0)
            ent->s.effects |= EF_PENT;
    }

    // show cheaters!!!
    if (ent->flags & FL_GODMODE) {
        ent->s.effects |= EF_COLOR_SHELL;
        ent->s.renderfx |= (RF_SHELL_RED | RF_SHELL_GREEN | RF_SHELL_BLUE);
    }
}


/*
===============
G_SetClientEvent
===============
*/
void G_SetClientEvent(edict_t *ent)
{
    if (ent->s.event)
        return;

    if (ent->groundentity) {
        if ((int)(current_client->bobtime + bobmove) != (int) current_client->bobtime)
            ent->s.event = EV_FOOTSTEP;
    }
}

/*
===============
G_SetClientSound
===============
*/
void G_SetClientSound(edict_t *ent)
{
    char    *weap;

    if (ent->client->pers.game_helpchanged != game.helpchanged) {
        ent->client->pers.game_helpchanged = game.helpchanged;
        ent->client->pers.helpchanged = 1;
    }

    // help beep (no more than ONE time - that's annoying enough)
    if (ent->client->pers.helpchanged && ent->client->pers.helpchanged <= 1 && !(level.time % 10000)) {
        ent->client->pers.helpchanged++;
        SV_StartSound(ent, CHAN_VOICE, SV_SoundIndex("misc/pc_up.wav"), 1, ATTN_STATIC, 0);
    }


    if (ent->client->pers.weapon)
        weap = ent->client->pers.weapon->classname;
    else
        weap = "";

    if (ent->waterlevel && (ent->watertype & (CONTENTS_LAVA | CONTENTS_SLIME)))
        ent->s.sound = snd_fry;
    else if (strcmp(weap, "weapon_railgun") == 0)
        ent->s.sound = SV_SoundIndex("weapons/rg_hum.wav");
    else if (strcmp(weap, "weapon_bfg") == 0)
        ent->s.sound = SV_SoundIndex("weapons/bfg_hum.wav");
    else if (ent->client->weapon_sound)
        ent->s.sound = ent->client->weapon_sound;
    else
        ent->s.sound = 0;

    // set reverb
    edict_t *closest_reverb = NULL;
    float closest_reverb_dist;

    for (edict_t *rev = level.reverb_entities; rev; rev = rev->next_reverb) {
        vec3_t s;
        VectorSubtract(ent->s.origin, rev->s.origin, s);
        float d = VectorLengthSquared(s);

        if (d < rev->radius && (!closest_reverb || d < closest_reverb_dist)) {
            closest_reverb = rev;
            closest_reverb_dist = d;
        }
    }

    if (closest_reverb) {
        ent->client->ps.reverb = closest_reverb->sounds;
    }
}

/*
===============
G_SetClientFrame
===============
*/
void G_SetClientFrame(edict_t *ent)
{
    gclient_t   *client;
    bool        duck, run;

    if (ent->s.modelindex != 255)
        return;     // not in the player model

    client = ent->client;

    if (client->ps.pmove.pm_flags & PMF_DUCKED)
        duck = true;
    else
        duck = false;
    if (xyspeed)
        run = true;
    else
        run = false;

    // check for stand/duck and stop/go transitions
    if (duck != client->anim_duck && client->anim_priority < ANIM_DEATH)
        goto newanim;
    if (run != client->anim_run && client->anim_priority == ANIM_BASIC)
        goto newanim;
    if (!ent->groundentity && client->anim_priority <= ANIM_WAVE)
        goto newanim;

    if (client->anim_priority == ANIM_REVERSE) {
        if (ent->s.frame > client->anim_end) {
            ent->s.frame--;
            return;
        }
    } else if (ent->s.frame < client->anim_end) {
        // continue an animation
        ent->s.frame++;
        return;
    }

    if (client->anim_priority == ANIM_DEATH)
        return;     // stay there
    if (client->anim_priority == ANIM_JUMP) {
        if (!ent->groundentity)
            return;     // stay there
        ent->client->anim_priority = ANIM_WAVE;
        ent->s.frame = FRAME_jump3;
        ent->client->anim_end = FRAME_jump6;
        return;
    }

newanim:
    // return to either a running or standing frame
    client->anim_priority = ANIM_BASIC;
    client->anim_duck = duck;
    client->anim_run = run;

    if (!ent->groundentity) {
        client->anim_priority = ANIM_JUMP;
        if (ent->s.frame != FRAME_jump2)
            ent->s.frame = FRAME_jump1;
        client->anim_end = FRAME_jump2;
    } else if (run) {
        // running
        if (duck) {
            ent->s.frame = FRAME_crwalk1;
            client->anim_end = FRAME_crwalk6;
        } else {
            ent->s.frame = FRAME_run1;
            client->anim_end = FRAME_run6;
        }
    } else {
        // standing
        if (duck) {
            ent->s.frame = FRAME_crstnd01;
            client->anim_end = FRAME_crstnd19;
        } else {
            ent->s.frame = FRAME_stand01;
            client->anim_end = FRAME_stand40;
        }
    }
}


/*
=================
ClientEndServerFrame

Called for each player at the end of the server frame
and right after spawning
=================
*/
void ClientEndServerFrame(edict_t *ent)
{
    float   bobtime;

    current_player = ent;
    current_client = ent->client;

    //
    // If the origin or velocity have changed since ClientThink(),
    // update the pmove values.  This will happen when the client
    // is pushed by a bmodel or kicked by an explosion.
    //
    // If it wasn't updated here, the view position would lag a frame
    // behind the body position when pushed -- "sinking into plats"
    //
    if (current_client->ps.pmove.pm_type != PM_SPECTATOR)
    {
        VectorSnapCoord(ent->s.origin, current_client->ps.pmove.origin);
        VectorSnapCoord(ent->velocity, current_client->ps.pmove.velocity);
    }

    //
    // If the end of unit layout is displayed, don't give
    // the player any normal movement attributes
    //
    if (level.intermission_time) {
        // FIXME: add view drifting here?
        current_client->ps.blend[3] = 0;
        current_client->ps.fov = 90;
        G_SetStats(ent);
        return;
    }

    AngleVectors(ent->client->v_angle, forward, right, up);

    // burn from lava, etc
    P_WorldEffects();

    //
    // set model angles from view angles so other things in
    // the world can tell which direction you are looking
    //
    if (ent->client->v_angle[PITCH] > 180)
        ent->s.angles[PITCH] = (-360 + ent->client->v_angle[PITCH]) / 3;
    else
        ent->s.angles[PITCH] = ent->client->v_angle[PITCH] / 3;
    ent->s.angles[YAW] = ent->client->v_angle[YAW];
    ent->s.angles[ROLL] = 0;
    ent->s.angles[ROLL] = SV_CalcRoll(ent->s.angles, ent->velocity) * 4;

    //
    // calculate speed and cycle to be used for
    // all cyclic walking effects
    //
    xyspeed = sqrtf(ent->velocity[0] * ent->velocity[0] + ent->velocity[1] * ent->velocity[1]);

    if (xyspeed < 5) {
        bobmove = 0;
        current_client->bobtime = 0;    // start at beginning of cycle again
    } else if (ent->groundentity) {
        // so bobbing only cycles when on ground
        if (xyspeed > 210)
            bobmove = 0.25f;
        else if (xyspeed > 100)
            bobmove = 0.125f;
        else
            bobmove = 0.0625f;
    }

    bobmove /= BASE_FRAMEDIV;

    bobtime = (current_client->bobtime += bobmove);

    if (current_client->ps.pmove.pm_flags & PMF_DUCKED)
        bobtime *= 4;

    bobcycle = (int)bobtime;
    bobfracsin = fabs(sin(bobtime * M_PI));

    // detect hitting the floor
    P_FallingDamage(ent);

    // apply all the damage taken this frame
    P_DamageFeedback(ent);

    // determine the view offsets
    SV_CalcViewOffset(ent);

    // determine the full screen color blend
    // must be after viewoffset, so eye contents can be
    // accurately determined
    // FIXME: with client prediction, the contents
    // should be determined by the client
    SV_CalcBlend(ent);

    // chase cam stuff
    if (ent->client->resp.spectator)
        G_SetSpectatorStats(ent);
    else
        G_SetStats(ent);
    G_CheckChaseStats(ent);

    G_SetClientEvent(ent);

    G_SetClientEffects(ent);

    G_SetClientSound(ent);

    G_SetClientFrame(ent);

    VectorCopy(ent->velocity, ent->client->oldvelocity);
    VectorCopy(ent->client->ps.viewangles, ent->client->oldviewangles);

    // clear weapon kicks
    if (ent->client->kick_angles[0] || ent->client->kick_angles[1] || ent->client->kick_angles[2]) {
        VectorScale(ent->client->kick_angles, 0.5f, ent->client->kick_angles);

        if (VectorLength(ent->client->kick_angles) < 0.125f) {
            VectorClear(ent->client->kick_angles);
        }
    }

    // if the scoreboard is up, update it
    if (ent->client->showscores && !(level.time % 3000)) {
        DeathmatchScoreboardMessage(ent, ent->enemy);
        SV_Unicast(ent, false);
    }
}

