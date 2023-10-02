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

// This file contains all of the asset names & locations, for easy adjustments.

// Standard gameplay
#define ASSET_SOUND_GIB	"misc/udeath.wav"

#define ASSET_SOUND_WATER_ENTER "player/watr_in.wav"
#define ASSET_SOUND_WATER_UNDER "player/watr_un.wav"
#define ASSET_SOUND_WATER_EXIT "player/watr_out.wav"
#define ASSET_SOUND_WATER_IMPACT "misc/h2ohit1.wav"

#define ASSET_SOUND_LAVA_ENTER "player/lava_in.wav"
#define ASSET_SOUND_LAVA_FRYING "player/fry.wav"

#define ASSET_SOUND_MONSTER_FRY1 "player/lava1.wav"
#define ASSET_SOUND_MONSTER_FRY2 "player/lava2.wav"
#define ASSET_SOUND_MONSTER_LAND "world/land.wav"

#define ASSET_SOUND_FLIES "infantry/inflies1.wav"

#define ASSET_SOUND_SECRET_FOUND "misc/secret.wav"

#define ASSET_SOUND_CHAT "misc/talk.wav"

#define ASSET_SOUND_TELEPORT "tele/teleexit.wav"

#define ASSET_SOUND_PLAYER_LAND "player/land1.wav"

// (sound from touching doors, triggers, etc)
#define ASSET_SOUND_GAME_MESSAGE "misc/talk1.wav"


// Movers/BModels
#define ASSET_SOUND_PLATFORM_START "plats/pt1_strt.wav"
#define ASSET_SOUND_PLATFORM_LOOP "plats/pt1_mid.wav"
#define ASSET_SOUND_PLATFORM_END "plats/pt1_end.wav"

#define ASSET_SOUND_BUTTON_PRESS "switches/butn2.wav"

#define ASSET_SOUND_PLATFORM_START "plats/pt1_strt.wav"
#define ASSET_SOUND_PLATFORM_LOOP "plats/pt1_mid.wav"
#define ASSET_SOUND_PLATFORM_END "plats/pt1_end.wav"

#define ASSET_SOUND_DOOR_START "doors/dr1_strt.wav"
#define ASSET_SOUND_DOOR_LOOP "doors/dr1_mid.wav"
#define ASSET_SOUND_DOOR_END "doors/dr1_end.wav"

#define ASSET_SOUND_WATER_START "world/mov_watr.wav"
#define ASSET_SOUND_WATER_STOP "world/stp_watr.wav"

#define ASSET_SOUND_KEY_TRY "misc/keytry.wav"
#define ASSET_SOUND_KEY_USE "misc/keyuse.wav"

#define ASSET_SOUND_PUSH_WIND "misc/windfly.wav"

#define ASSET_SOUND_HURT_BURN "world/electro.wav"

// Items & Weapons
#define ASSET_SOUND_ITEM_RESPAWN "items/respawn1.wav"

#define ASSET_SOUND_ARMOR_PICKUP "misc/ar1_pkup.wav"
#define ASSET_MODEL_ARMOR "models/items/armor/armor.md3"
#define ASSET_SOUND_ARMOR_SHARD_PICKUP "misc/ar2_pkup.wav"
#define ASSET_MODEL_ARMOR_SHARD "models/items/armor/armor_shard.iqm"

#define ASSET_MODEL_AXE_WORLD "models/weapons/g_axe/g_axe.iqm"
#define ASSET_MODEL_AXE_VIEW "models/weapons/v_axe/v_axe.iqm"
#define ASSET_SOUND_AXE_HIT "makron/brain1.wav"

#define ASSET_MODEL_SHOTGUN_WORLD "models/weapons/g_shotg/g_shotg.iqm"
#define ASSET_MODEL_SHOTGUN_VIEW "models/weapons/v_shotg/v_shotg.iqm"
#define ASSET_SOUND_SHOTGUN_FIRE "weapons/shotg/shotg_fire.wav"

#define ASSET_MODEL_SSHOTGUN_WORLD "models/weapons/g_sshotg/g_sshotg.iqm"
#define ASSET_MODEL_SSHOTGUN_VIEW "models/weapons/v_sshotg/v_sshotg.iqm"
#define ASSET_SOUND_SSHOTGUN_FIRE "weapons/shotg/shotg_fire.wav"

#define ASSET_MODEL_BLASTER_WORLD "models/weapons/g_blaster/g_blaster.iqm"
#define ASSET_MODEL_BLASTER_VIEW "models/weapons/v_blaster/v_blaster.iqm"
#define ASSET_SOUND_BLASTER_FIRE "weapons/shotg/shotg_fire.wav"

#define ASSET_MODEL_PERFORATOR_WORLD "models/weapons/g_perf/g_perf.iqm"
#define ASSET_MODEL_PERFORATOR_VIEW "models/weapons/v_perf/v_perf.iqm"
#define ASSET_SOUND_PERFORATOR_SPIN "weapons/perf/perf_spin.wav"
#define ASSET_SOUND_PERFORATOR_FIRE "weapons/perf/perf_fire.wav"
#define ASSET_MODEL_NAIL "models/objects/nail/nail.iqm"

#define ASSET_MODEL_NAILGUN_WORLD "models/weapons/g_nailg/g_nailg.iqm"
#define ASSET_MODEL_NAILGUN_VIEW "models/weapons/v_nailg/v_nailg.iqm"
#define ASSET_SOUND_NAILGUN_FIRE "weapons/perf/perf_fire.wav"

#define ASSET_MODEL_ROCKET_WORLD "models/weapons/g_rocket/g_rocket.iqm"
#define ASSET_MODEL_ROCKET_VIEW "models/weapons/v_rocket/v_rocket.iqm"

#define ASSET_MODEL_LAUNCH_WORLD "models/weapons/g_launch/g_launch.iqm"
#define ASSET_MODEL_LAUNCH_VIEW "models/weapons/v_launch/v_launch.iqm"

#define ASSET_MODEL_THUNDER_WORLD "models/weapons/g_bolt/g_bolt.iqm"
#define ASSET_MODEL_THUNDER_VIEW "models/weapons/v_bolt/v_bolt.iqm"
#define ASSET_SOUND_THUNDERBOLT_END_FIRE "weapons/GRENLX1A.WAV"
#define ASSET_SOUND_THUNDERBOLT_FIRING "tele/teletunnel.wav"

#define ASSET_MODEL_SHELLS "models/items/ammo/shells/shellbox_small.iqm"
#define ASSET_MODEL_NAILS "models/items/ammo/nails/nailbox.iqm"
#define ASSET_MODEL_CELLS "models/items/ammo/cells/cellbox.iqm"
#define ASSET_MODEL_ROCKETS "models/items/ammo/rockets/rocketbox.iqm"
#define ASSET_MODEL_GRENADES "models/items/ammo/grenades/grenadebox.iqm"

#define ASSET_MODEL_SILVER_KEY "models/items/keys/gold_key/gold_key.md3"
#define ASSET_MODEL_GOLD_KEY "models/items/keys/gold_key/gold_key.md3"

#define ASSET_MODEL_QUAD_DAMAGE "models/items/powerups/quad/quad.md3"
#define ASSET_SOUND_QUAD_ACTIVATE "items/damage.wav"
#define ASSET_SOUND_QUAD_FADE "items/damage2.wav"

#define ASSET_MODEL_PENT "models/items/powerups/pent/pent.md3"
#define ASSET_SOUND_PENT_ACTIVATE "items/protect.wav"
#define ASSET_SOUND_PENT_FADE "items/protect2.wav"
#define ASSET_SOUND_PENT_HIT "items/protect4.wav"

#define ASSET_MODEL_BIOSUIT "models/items/powerups/suit/suit.md3"
#define ASSET_SOUND_BIOSUIT_ACTIVATE "items/biosuit.wav"
#define ASSET_SOUND_BIOSUIT_FADE "items/airout.wav"

#define ASSET_MODEL_RING "models/items/powerups/ring/ring.md3"

#define ASSET_MODEL_HEALTH_SMALL "models/items/health/small.iqm"
#define ASSET_SOUND_HEALTH_SMALL_PICKUP "items/s_health.wav"

#define ASSET_MODEL_HEALTH_ROTTEN "models/items/health/medium.iqm"
#define ASSET_SOUND_HEALTH_ROTTEN_PICKUP "items/n_health.wav"

#define ASSET_MODEL_HEALTH "models/items/health/large.iqm"
#define ASSET_SOUND_HEALTH_PICKUP "items/l_health.wav"

#define ASSET_MODEL_HEALTH_MEGA "models/items/health/mega.iqm"
#define ASSET_SOUND_HEALTH_MEGA_PICKUP "items/m_health.wav"

#define ASSET_SOUND_GENERIC_PICKUP "items/pkup.wav"
#define ASSET_SOUND_WEAPON_PICKUP "misc/w_pkup.wav"
#define ASSET_SOUND_AMMO_PICKUP "misc/am_pkup.wav"

#define ASSET_SOUND_OUT_OF_AMMO "weapons/noammo.wav"

#define ASSET_SOUND_ROCKET_FIRE "weapons/rocklf1a.wav"

#define ASSET_MODEL_LASER "models/objects/laser/tris.md2"
#define ASSET_SOUND_LASER_FLY "misc/lasfly.wav"

#define ASSET_MODEL_GRENADE "models/objects/grenade/grenade.iqm"
#define ASSET_SOUND_GRENADE_BOUNCE "weapons/gren/gren_bounce.wav"
#define ASSET_SOUND_GRENADE_FIRE "weapons/gren/gren_fire.wav"
#define ASSET_SOUND_GRENADE_EXPLODE "weapons/grenlx1a.wav"

#define ASSET_MODEL_ROCKET "models/objects/rocket/rocket.iqm"
#define ASSET_SOUND_ROCKET_FLY "weapons/rockfly.wav"
#define ASSET_SOUND_ROCKET_EXPLODE "weapons/rocklx1a.wav"

#define ASSET_SOUND_UNDERWATER_EXPLODE "weapons/xpld_wat.wav"

// Gibs/Misc
#define ASSET_MODEL_GIB_HEAD "models/objects/gibs/head2/tris.md2"
#define ASSET_MODEL_GIB_SKULL "models/objects/gibs/skull/tris.md2"
#define ASSET_MODEL_GIB_MEAT "models/objects/gibs/sm_meat/tris.md2"
#define ASSET_MODEL_GIB_BONE "models/objects/gibs/bone/tris.md2"
#define ASSET_MODEL_GIB_CHEST "models/objects/gibs/chest/tris.md2"

#define ASSET_MODEL_DEBRIS1 "models/objects/debris1/tris.md2"
#define ASSET_MODEL_DEBRIS2 "models/objects/debris2/tris.md2"
#define ASSET_MODEL_DEBRIS3 "models/objects/debris3/tris.md2"

#define ASSET_MODEL_EXPLOBOX "models/objects/explobox/explobox.iqm"
#define ASSET_MODEL_EXPLOBOX_SMALL "models/objects/explobox/explobox2.iqm"


// Knight
#define ASSET_MODEL_KNIGHT "models/monsters/knight/knight.iqm"
#define ASSET_SOUND_KNIGHT_HURT "nac_knight/khurt.wav"
#define ASSET_SOUND_KNIGHT_DEATH "nac_knight/kdeath.wav"
#define ASSET_SOUND_KNIGHT_IDLE "nac_knight/idle.wav"
#define ASSET_SOUND_KNIGHT_SWORD "nac_knight/sword1.wav"
#define ASSET_SOUND_KNIGHT_SIGHT "nac_knight/ksight.wav"

#define ASSET_MODEL_KNIGHT_GIB_BASE(x) \
    "models/monsters/knight/knight_gib_" #x ".iqm"
#define ASSET_MODEL_KNIGHT_GIB_ARML ASSET_MODEL_KNIGHT_GIB_BASE(arml)
#define ASSET_MODEL_KNIGHT_GIB_ARMR ASSET_MODEL_KNIGHT_GIB_BASE(armr)
#define ASSET_MODEL_KNIGHT_GIB_ARMOR ASSET_MODEL_KNIGHT_GIB_BASE(armor)
#define ASSET_MODEL_KNIGHT_GIB_CHEST ASSET_MODEL_KNIGHT_GIB_BASE(chest)
#define ASSET_MODEL_KNIGHT_GIB_FOOTL ASSET_MODEL_KNIGHT_GIB_BASE(footl)
#define ASSET_MODEL_KNIGHT_GIB_FOOTR ASSET_MODEL_KNIGHT_GIB_BASE(footr)
#define ASSET_MODEL_KNIGHT_GIB_HANDL ASSET_MODEL_KNIGHT_GIB_BASE(handl)
#define ASSET_MODEL_KNIGHT_GIB_HEAD ASSET_MODEL_KNIGHT_GIB_BASE(head)
#define ASSET_MODEL_KNIGHT_GIB_LEGL ASSET_MODEL_KNIGHT_GIB_BASE(legl)
#define ASSET_MODEL_KNIGHT_GIB_LEGR ASSET_MODEL_KNIGHT_GIB_BASE(legr)
#define ASSET_MODEL_KNIGHT_GIB_SHOULDER ASSET_MODEL_KNIGHT_GIB_BASE(shoulder)
#define ASSET_MODEL_KNIGHT_GIB_THIGH ASSET_MODEL_KNIGHT_GIB_BASE(thigh)
#define ASSET_MODEL_KNIGHT_GIB_WAIST ASSET_MODEL_KNIGHT_GIB_BASE(waist)

// Grunt
#define ASSET_MODEL_GRUNT "models/monsters/grunt/grunt.iqm"
#define ASSET_SOUND_GRUNT_IDLE "nac_grunt/idle.wav"
#define ASSET_SOUND_GRUNT_PAIN1 "nac_grunt/pain1.wav"
#define ASSET_SOUND_GRUNT_PAIN2 "nac_grunt/pain2.wav"
#define ASSET_SOUND_GRUNT_SIGHT "nac_grunt/sight.wav"
#define ASSET_SOUND_GRUNT_DEATH "nac_grunt/death.wav"


// Enforcer
#define ASSET_MODEL_ENFORCER "models/monsters/enforcer/enforcer.iqm"
#define ASSET_SOUND_ENFORCER_DEATH "nac_enforcer/death1.wav"
#define ASSET_SOUND_ENFORCER_IDLE "nac_enforcer/idle.wav"
#define ASSET_SOUND_ENFORCER_PAIN1 "nac_enforcer/pain1.wav"
#define ASSET_SOUND_ENFORCER_PAIN2 "nac_enforcer/pain2.wav"
#define ASSET_SOUND_ENFORCER_SIGHT1 "nac_enforcer/sight1.wav"
#define ASSET_SOUND_ENFORCER_SIGHT2 "nac_enforcer/sight2.wav"
#define ASSET_SOUND_ENFORCER_SIGHT3 "nac_enforcer/sight3.wav"
#define ASSET_SOUND_ENFORCER_SIGHT4 "nac_enforcer/sight4.wav"


// Fiend
#define ASSET_MODEL_FIEND "models/monsters/fiend/fiend.iqm"
#define ASSET_SOUND_FIEND_PAIN "nac_fiend/dpain1.wav"
#define ASSET_SOUND_FIEND_DIE "nac_fiend/ddeath.wav"
#define ASSET_SOUND_FIEND_IDLE "nac_fiend/idle1.wav"
#define ASSET_SOUND_FIEND_HIT "nac_fiend/dhit2.wav"
#define ASSET_SOUND_FIEND_JUMP "nac_fiend/djump.wav"
#define ASSET_SOUND_FIEND_LAND "nac_fiend/dland2.wav"
#define ASSET_SOUND_FIEND_SIGHT "nac_fiend/dsight.wav"


// Vore
#define ASSET_MODEL_VORE "models/monsters/vore/vore.iqm"
#define ASSET_MODEL_VORE_BALL "models/monsters/vore/vore_ball.iqm"
#define ASSET_SOUND_VORE_BALL_CHASE "world/amb10.wav"
#define ASSET_SOUND_VORE_HURT "nac_vore/pain.wav"
#define ASSET_SOUND_VORE_DEATH "nac_vore/death.wav"
#define ASSET_SOUND_VORE_IDLE "nac_vore/idle.wav"
#define ASSET_SOUND_VORE_ATTACK "nac_vore/attack.wav"
#define ASSET_SOUND_VORE_ATTACK2 "nac_vore/attack2.wav"
#define ASSET_SOUND_VORE_SIGHT "nac_vore/sight.wav"

// Scrag
#define ASSET_MODEL_SCRAG "models/monsters/scrag/scrag.iqm"
#define ASSET_MODEL_SCRAG_BALL "models/monsters/scrag/scrag_slime.md3"
#define ASSET_SOUND_SCRAG_HURT "nac_scrag/pain.wav"
#define ASSET_SOUND_SCRAG_DEATH "nac_scrag/death.wav"
#define ASSET_SOUND_SCRAG_IDLE "nac_scrag/idle.wav"
#define ASSET_SOUND_SCRAG_FIRE "nac_scrag/fire.wav"
#define ASSET_SOUND_SCRAG_SIGHT "nac_scrag/sight.wav"
#define ASSET_SOUND_SCRAG_SPLAT "nac_scrag/splat.wav"

#define ASSET_MODEL_SCRAG_GIB_BASE(x) \
    "models/monsters/scrag/scrag_gib_" #x ".iqm"
#define ASSET_MODEL_SCRAG_GIB_ARML ASSET_MODEL_SCRAG_GIB_BASE(arml)
#define ASSET_MODEL_SCRAG_GIB_ARMR ASSET_MODEL_SCRAG_GIB_BASE(armr)
#define ASSET_MODEL_SCRAG_GIB_CHEST ASSET_MODEL_SCRAG_GIB_BASE(chest)
#define ASSET_MODEL_SCRAG_GIB_HEAD ASSET_MODEL_SCRAG_GIB_BASE(head)
#define ASSET_MODEL_SCRAG_GIB_NUB ASSET_MODEL_SCRAG_GIB_BASE(nub)
#define ASSET_MODEL_SCRAG_GIB_SHOULDER ASSET_MODEL_SCRAG_GIB_BASE(shoulder)
#define ASSET_MODEL_SCRAG_GIB_TAIL ASSET_MODEL_SCRAG_GIB_BASE(tail)
#define ASSET_MODEL_SCRAG_GIB_TAIL_BASE ASSET_MODEL_SCRAG_GIB_BASE(tail_base)