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

#define ASSET_SOUND_TELEPORT "misc/tele1.wav"

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

#define ASSET_MODEL_AXE_VIEW "models/weapons/v_axe/axe.iqm"
#define ASSET_SOUND_AXE_HIT "makron/brain1.wav"

#define ASSET_MODEL_SHOTGUN_WORLD "models/weapons/g_shotg/g_shotg.iqm"
#define ASSET_MODEL_SHOTGUN_VIEW "models/weapons/v_shotg/v_shotg.iqm"
#define ASSET_SOUND_SHOTGUN_FIRE "weapons/shotg/shotg_fire.wav"
#define ASSET_SOUND_PERFORATOR_FIRE "weapons/perf/perf_fire.wav"

#define ASSET_MODEL_PERFORATOR_WORLD "models/weapons/g_perf/g_perf.iqm"
#define ASSET_MODEL_PERFORATOR_VIEW "models/weapons/v_perf/v_perf.iqm"
#define ASSET_SOUND_PERFORATOR_SPIN "weapons/perf/perf_spin.wav"
#define ASSET_MODEL_NAIL "models/objects/nail/nail.iqm"

#define ASSET_MODEL_SHELLS "models/items/ammo/shells/shellbox_small.iqm"
#define ASSET_MODEL_NAILS "models/items/ammo/nails/nailbox.iqm"

#define ASSET_MODEL_SILVER_KEY "models/items/keys/gold_key/gold_key.md3"

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

#define ASSET_MODEL_HEALTH_ROTTEN "models/items/healing/medium/tris.md2"
#define ASSET_SOUND_HEALTH_ROTTEN_PICKUP "items/n_health.wav"

#define ASSET_MODEL_HEALTH "models/items/healing/large/tris.md2"
#define ASSET_SOUND_HEALTH_PICKUP "items/l_health.wav"

#define ASSET_MODEL_HEALTH_MEGA "models/items/mega_h/tris.md2"
#define ASSET_SOUND_HEALTH_MEGA_PICKUP "items/m_health.wav"

#define ASSET_SOUND_GENERIC_PICKUP "items/pkup.wav"
#define ASSET_SOUND_WEAPON_PICKUP "misc/w_pkup.wav"
#define ASSET_SOUND_AMMO_PICKUP "misc/am_pkup.wav"

#define ASSET_SOUND_OUT_OF_AMMO "weapons/noammo.wav"

#define ASSET_SOUND_ROCKET_FIRE "weapons/rocklf1a.wav"

#define ASSET_MODEL_LASER "models/objects/laser/tris.md2"
#define ASSET_SOUND_LASER_FLY "misc/lasfly.wav"

#define ASSET_MODEL_GRENADE "models/objects/grenade/tris.md2"
#define ASSET_SOUND_GRENADE_BOUNCE "weapons/grenlb1b.wav"
#define ASSET_SOUND_GRENADE_EXPLODE "weapons/grenlx1a.wav"

#define ASSET_MODEL_ROCKET "models/objects/rocket/tris.md2"
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


// Fiend
#define ASSET_MODEL_FIEND "models/monsters/fiend/fiend.iqm"
#define ASSET_SOUND_FIEND_PAIN "nac_fiend/dpain1.wav"
#define ASSET_SOUND_FIEND_DIE "nac_fiend/ddeath.wav"
#define ASSET_SOUND_FIEND_IDLE "nac_fiend/idle1.wav"
#define ASSET_SOUND_FIEND_HIT "nac_fiend/dhit2.wav"
#define ASSET_SOUND_FIEND_JUMP "nac_fiend/djump.wav"
#define ASSET_SOUND_FIEND_LAND "nac_fiend/dland2.wav"
#define ASSET_SOUND_FIEND_SIGHT "nac_fiend/dsight.wav"