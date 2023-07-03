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
// world.c -- world query functions

#include "server.h"

/*
===============================================================================

ENTITY AREA CHECKING

FIXME: this use of "area" is different from the bsp file use
===============================================================================
*/

typedef struct areanode_s {
    int     axis;       // -1 = leaf node
    float   dist;
    struct areanode_s   *children[2];
    list_t  trigger_edicts;
    list_t  solid_edicts;
} areanode_t;

#define    AREA_DEPTH    4
#define    AREA_NODES    32

static areanode_t   sv_areanodes[AREA_NODES];
static int          sv_numareanodes;

static const vec_t  *area_mins, *area_maxs;
static edict_t  **area_list;
static size_t   area_count, area_maxcount;
static int      area_type;

/*
===============
SV_CreateAreaNode

Builds a uniformly subdivided tree for the given world size
===============
*/
static areanode_t *SV_CreateAreaNode(int depth, const vec3_t mins, const vec3_t maxs)
{
    areanode_t  *anode;
    vec3_t      size;
    vec3_t      mins1, maxs1, mins2, maxs2;

    anode = &sv_areanodes[sv_numareanodes];
    sv_numareanodes++;

    List_Init(&anode->trigger_edicts);
    List_Init(&anode->solid_edicts);

    if (depth == AREA_DEPTH) {
        anode->axis = -1;
        anode->children[0] = anode->children[1] = NULL;
        return anode;
    }

    VectorSubtract(maxs, mins, size);
    if (size[0] > size[1])
        anode->axis = 0;
    else
        anode->axis = 1;

    anode->dist = 0.5f * (maxs[anode->axis] + mins[anode->axis]);
    VectorCopy(mins, mins1);
    VectorCopy(mins, mins2);
    VectorCopy(maxs, maxs1);
    VectorCopy(maxs, maxs2);

    maxs1[anode->axis] = mins2[anode->axis] = anode->dist;

    anode->children[0] = SV_CreateAreaNode(depth + 1, mins2, maxs2);
    anode->children[1] = SV_CreateAreaNode(depth + 1, mins1, maxs1);

    return anode;
}

/*
===============
SV_ClearWorld

===============
*/
void SV_ClearWorld(void)
{
    mmodel_t *cm;

    memset(sv_areanodes, 0, sizeof(sv_areanodes));
    sv_numareanodes = 0;

    if (sv.cm.cache) {
        cm = &sv.cm.cache->models[0];
        SV_CreateAreaNode(0, cm->mins, cm->maxs);
    }

    // make sure all entities are unlinked
    memset(sv.entities, 0, sizeof(sv.entities));
}

/*
===============
SV_LinkEdict

Links entity to PVS leafs.
===============
*/
static void SV_LinkEdict(cm_t *cm, edict_t *ent, server_entity_t *sent)
{
    mleaf_t     *leafs[MAX_TOTAL_ENT_LEAFS];
    int         clusters[MAX_TOTAL_ENT_LEAFS];
    int         num_leafs;
    int         i, j;
    int         area;
    mnode_t     *topnode;

    // set the size
    VectorSubtract(ent->maxs, ent->mins, ent->size);

    // set the abs box
    if (ent->solid == SOLID_BSP && !VectorEmpty(ent->s.angles)) {
        // expand for rotation
        float   max, v;

        max = 0;
        for (i = 0; i < 3; i++) {
            v = fabsf(ent->mins[i]);
            if (v > max)
                max = v;
            v = fabsf(ent->maxs[i]);
            if (v > max)
                max = v;
        }
        for (i = 0; i < 3; i++) {
            ent->absmin[i] = ent->s.origin[i] - max;
            ent->absmax[i] = ent->s.origin[i] + max;
        }
    } else {
        // normal
        VectorAdd(ent->s.origin, ent->mins, ent->absmin);
        VectorAdd(ent->s.origin, ent->maxs, ent->absmax);
    }

    // because movement is clipped an epsilon away from an actual edge,
    // we must fully check even when bounding boxes don't quite touch
    ent->absmin[0] -= 1;
    ent->absmin[1] -= 1;
    ent->absmin[2] -= 1;
    ent->absmax[0] += 1;
    ent->absmax[1] += 1;
    ent->absmax[2] += 1;

// link to PVS leafs
    ent->areanum = 0;
    ent->areanum2 = 0;

    //get all leafs, including solids
    num_leafs = CM_BoxLeafs(cm, ent->absmin, ent->absmax,
                            leafs, MAX_TOTAL_ENT_LEAFS, &topnode);

    // set areas
    for (i = 0; i < num_leafs; i++) {
        clusters[i] = leafs[i]->cluster;
        area = leafs[i]->area;
        if (area) {
            // doors may legally straggle two areas,
            // but nothing should evern need more than that
            if (ent->areanum && ent->areanum != area) {
                if (ent->areanum2 && ent->areanum2 != area && sv.state == ss_loading) {
                    Com_DPrintf("Object touching 3 areas at %s\n", vtos(ent->absmin));
                }
                ent->areanum2 = area;
            } else
                ent->areanum = area;
        }
    }

    // link clusters
    sent->num_clusters = 0;

    if (num_leafs >= MAX_TOTAL_ENT_LEAFS) {
        // assume we missed some leafs, and mark by headnode
        sent->num_clusters = -1;
        sent->headnode = CM_NumNode(cm, topnode);
    } else {
        sent->num_clusters = 0;
        for (i = 0; i < num_leafs; i++) {
            if (clusters[i] == -1)
                continue;        // not a visible leaf
            for (j = 0; j < i; j++)
                if (clusters[j] == clusters[i])
                    break;
            if (j == i) {
                if (sent->num_clusters == MAX_ENT_CLUSTERS) {
                    // assume we missed some leafs, and mark by headnode
                    sent->num_clusters = -1;
                    sent->headnode = CM_NumNode(cm, topnode);
                    break;
                }

                sent->clusternums[sent->num_clusters++] = clusters[i];
            }
        }
    }
}

bool SV_EntityLinked(edict_t *ent)
{
    int entnum = NUM_FOR_EDICT(ent);
    server_entity_t *sent = &sv.entities[entnum];

    return !!sent->area.prev;
}

void PF_UnlinkEdict(edict_t *ent)
{
    int entnum = NUM_FOR_EDICT(ent);
    server_entity_t *sent = &sv.entities[entnum];

    // check if we're even linked
    if (!sent->area.prev)
        return;

    List_Remove(&sent->area);
    sent->area.prev = sent->area.next = NULL;
}

void PF_LinkEdict(edict_t *ent)
{
    areanode_t *node;
    server_entity_t *sent;
    int entnum;

    // world is allowed to be (silently) ignored
    if (ent == ge->entities)
        return;

    entnum = NUM_FOR_EDICT(ent);
    sent = &sv.entities[entnum];

    if (sent->area.prev)
        PF_UnlinkEdict(ent);     // unlink from old position

    if (!ent->inuse) {
        Com_DPrintf("%s: entity %d is not in use\n", __func__, NUM_FOR_EDICT(ent));
        return;
    }

    if (!sv.cm.cache) {
        return;
    }

    ent->s.bbox = 0;

    // encode the size into the entity_state for client prediction
    switch (ent->solid) {
    case SOLID_BBOX:
        if (!(ent->svflags & SVF_DEADMONSTER) && !VectorCompare(ent->mins, ent->maxs)) {
            bool safe;
            ent->s.bbox = MSG_PackBBox(ent->mins, ent->maxs, &safe);

            if (!safe) {
                Com_WPrintf("Entity %i has unpredictable bounding box (%f %f %f -> %f %f %f)\n", ent->s.number, ent->mins[0], ent->mins[1], ent->mins[2], ent->maxs[0], ent->maxs[1], ent->maxs[2]);
            }
        }
        break;
    case SOLID_BSP:
        ent->s.bbox = BBOX_BMODEL;
        break;
    }

    SV_LinkEdict(&sv.cm, ent, sent);

    // if first time, make sure old_origin is valid
    if (!ent->linkcount) {
        VectorCopy(ent->s.origin, ent->s.old_origin);
    }
    ent->linkcount++;

    if (ent->solid == SOLID_NOT)
        return;

// find the first node that the ent's box crosses
    node = sv_areanodes;
    while (1) {
        if (node->axis == -1)
            break;
        if (ent->absmin[node->axis] > node->dist)
            node = node->children[0];
        else if (ent->absmax[node->axis] < node->dist)
            node = node->children[1];
        else
            break;        // crosses the node
    }

    // link it in
    if (ent->solid == SOLID_TRIGGER)
        List_Append(&node->trigger_edicts, &sent->area);
    else
        List_Append(&node->solid_edicts, &sent->area);
}


/*
====================
SV_AreaEdicts_r

====================
*/
static void SV_AreaEdicts_r(areanode_t *node)
{
    list_t              *start;
    server_entity_t     *check_sent;

    // touch linked edicts
    if (area_type == AREA_SOLID)
        start = &node->solid_edicts;
    else
        start = &node->trigger_edicts;

    LIST_FOR_EACH(server_entity_t, check_sent, start, area) {
        edict_t *check = EDICT_NUM(check_sent - sv.entities);
        if (check->solid == SOLID_NOT)
            continue;        // deactivated
        if (check->absmin[0] > area_maxs[0]
            || check->absmin[1] > area_maxs[1]
            || check->absmin[2] > area_maxs[2]
            || check->absmax[0] < area_mins[0]
            || check->absmax[1] < area_mins[1]
            || check->absmax[2] < area_mins[2])
            continue;        // not touching

        if (area_count == area_maxcount) {
            Com_WPrintf("SV_AreaEdicts: MAXCOUNT\n");
            return;
        }

        area_list[area_count] = check;
        area_count++;
    }

    if (node->axis == -1)
        return;        // terminal node

    // recurse down both sides
    if (area_maxs[node->axis] > node->dist)
        SV_AreaEdicts_r(node->children[0]);
    if (area_mins[node->axis] < node->dist)
        SV_AreaEdicts_r(node->children[1]);
}

/*
================
SV_AreaEdicts
================
*/
size_t SV_AreaEdicts(const vec3_t mins, const vec3_t maxs, edict_t **list,
                     size_t maxcount, int areatype)
{
    area_mins = mins;
    area_maxs = maxs;
    area_list = list;
    area_count = 0;
    area_maxcount = maxcount;
    area_type = areatype;

    SV_AreaEdicts_r(sv_areanodes);

    return area_count;
}


//===========================================================================

/*
================
SV_HullForEntity

Returns a headnode that can be used for testing or clipping an
object of mins/maxs size.
================
*/
mnode_t *SV_HullForEntity(edict_t *ent)
{
    if (ent->solid == SOLID_BSP || ent->solid == SOLID_TRIGGER) {
        int i = ent->s.modelindex - 1;

        // explicit hulls in the BSP model
        if (i >= 0 && i < sv.cm.cache->nummodels) {
            return sv.cm.cache->models[i].headnode;
        }
    }

    // create a temp hull from bounding box sizes
    return CM_HeadnodeForBox(ent->mins, ent->maxs);
}

/*
=============
SV_PointContents
=============
*/
int SV_PointContents(const vec3_t p)
{
    edict_t     *touch[MAXTOUCH * 4], *hit;
    int         i, num;
    int         contents;

    if (!sv.cm.cache) {
        Com_Errorf(ERR_DROP, "%s: no map loaded", __func__);
    }

    // get base contents from world
    contents = CM_PointContents(p, sv.cm.cache->nodes);

    // or in contents from all the other entities
    num = SV_AreaEdicts(p, p, touch, q_countof(touch), AREA_SOLID);

    for (i = 0; i < num; i++) {
        hit = touch[i];

        // might intersect, so do an exact clip
        contents |= CM_TransformedPointContents(p, SV_HullForEntity(hit),
                                                hit->s.origin, hit->s.angles);
    }

    return contents;
}

/*
====================
SV_ClipMoveToEntities

====================
*/
static void SV_ClipMoveToEntities(const vec3_t start, const vec3_t mins,
                                  const vec3_t maxs, const vec3_t end,
                                  edict_t *passedict, int contentmask, trace_t *tr)
{
    vec3_t      boxmins, boxmaxs;
    int         i, num;
    edict_t     *touchlist[MAXTOUCH * 4], *touch;
    trace_t     trace;

    // create the bounding box of the entire move
    for (i = 0; i < 3; i++) {
        if (end[i] > start[i]) {
            boxmins[i] = start[i] + mins[i] - 1;
            boxmaxs[i] = end[i] + maxs[i] + 1;
        } else {
            boxmins[i] = end[i] + mins[i] - 1;
            boxmaxs[i] = start[i] + maxs[i] + 1;
        }
    }

    num = SV_AreaEdicts(boxmins, boxmaxs, touchlist, q_countof(touchlist), AREA_SOLID);

    // be careful, it is possible to have an entity in this
    // list removed before we get to it (killtriggered)
    for (i = 0; i < num; i++) {
        touch = touchlist[i];
        if (touch->solid == SOLID_NOT)
            continue;
        if (touch == passedict)
            continue;
        if (tr->allsolid)
            return;
        if (passedict) {
            if (touch->owner == passedict)
                continue;    // don't clip against own missiles
            if (passedict->owner == touch)
                continue;    // don't clip against owner
        }

        if (!(contentmask & CONTENTS_DEADMONSTER)
            && (touch->svflags & SVF_DEADMONSTER))
            continue;

        // might intersect, so do an exact clip
        CM_TransformedBoxTrace(&trace, start, end, mins, maxs,
                               SV_HullForEntity(touch), contentmask,
                               touch->s.origin, touch->s.angles);

        CM_ClipEntity(tr, &trace, touch);
    }
}

/*
==================
SV_Trace

Moves the given mins/maxs volume through the world from start to end.
Passedict and edicts owned by passedict are explicitly not checked.
==================
*/
void SV_Trace(trace_t *tr, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end,
              edict_t *passedict, int contentmask)
{
    if (!sv.cm.cache) {
        Com_Errorf(ERR_DROP, "%s: no map loaded", __func__);
    }

    if (!mins)
        mins = vec3_origin;
    if (!maxs)
        maxs = vec3_origin;

    // clip to world
    CM_BoxTrace(tr, start, end, mins, maxs, sv.cm.cache->nodes, contentmask);

    tr->ent = ge->entities;

    if (tr->fraction != 0.f) {
        // not blocked by the world
        // clip to other solid entities
        SV_ClipMoveToEntities(start, mins, maxs, end, passedict, contentmask, tr);
    }
}

