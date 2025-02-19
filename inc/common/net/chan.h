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

#ifndef NET_CHAN_H
#define NET_CHAN_H

#include "common/msg.h"
#include "common/net/net.h"
#include "common/sizebuf.h"

typedef struct netchan_s {
    int         protocol;
    size_t      maxpacketlen;

    bool        fatal_error;

    netsrc_t    sock;

    int         dropped;            // between last packet and previous
    unsigned    total_dropped;      // for statistics
    unsigned    total_received;

    unsigned    last_received;      // for timeouts
    unsigned    last_sent;          // for retransmits

    netadr_t    remote_address;
    int         qport;              // qport value to write when transmitting

    sizebuf_t   message;            // writing buffer for reliable data

    size_t      reliable_length;

    bool        reliable_ack_pending;   // set to true each time reliable is received
    bool        fragment_pending;

    // sequencing variables
    int         incoming_sequence;
    int         incoming_acknowledged;
    int         outgoing_sequence;

// sequencing variables
    int         incoming_reliable_acknowledged; // single bit
    int         incoming_reliable_sequence;     // single bit, maintained local
    int         reliable_sequence;          // single bit
    int         last_reliable_sequence;     // sequence number of last send
    int         fragment_sequence;

// reliable staging and holding areas
    byte        message_buf[MAX_MSGLEN];        // leave space for header

// message is copied to this buffer when it is first transfered
    byte        reliable_buf[MAX_MSGLEN];   // unacked reliable message

    sizebuf_t   fragment_in;
    byte        fragment_in_buf[MAX_MSGLEN];

    sizebuf_t   fragment_out;
    byte        fragment_out_buf[MAX_MSGLEN];
} netchan_t;

extern cvar_t       *net_qport;
extern cvar_t       *net_maxmsglen;

void Netchan_Init(void);
void Netchan_OutOfBand(netsrc_t sock, const netadr_t *adr,
                       const char *fmt, ...) q_printf(3, 4);
size_t Netchan_TransmitNextFragment(netchan_t *netchan);
size_t Netchan_Transmit(netchan_t *netchan, size_t length, const void *data, int numpackets);
bool Netchan_Process(netchan_t *netchan);
bool Netchan_ShouldUpdate(netchan_t *netchan);
netchan_t *Netchan_Setup(netsrc_t sock, const netadr_t *adr,
                         int qport, size_t maxpacketlen, int protocol);
void Netchan_Close(netchan_t *netchan);

#define OOB_PRINT(sock, addr, data) \
    NET_SendPacket(sock, CONST_STR_LEN("\xff\xff\xff\xff" data), addr)

#endif // NET_CHAN_H
