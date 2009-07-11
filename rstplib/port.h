/************************************************************************ 
 * RSTP library - Rapid Spanning Tree (802.1D-2004) 
 * Copyright (C) 2001-2003 Optical Access 
 * Author: Alex Rozin 
 * 
 * This file is part of RSTP library. 
 * 
 * RSTP library is free software; you can redistribute it and/or modify it 
 * under the terms of the GNU Lesser General Public License as published by the 
 * Free Software Foundation; version 2.1 
 * 
 * RSTP library is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser 
 * General Public License for more details. 
 * 
 * You should have received a copy of the GNU Lesser General Public License 
 * along with RSTP library; see the file COPYING.  If not, write to the Free 
 * Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 
 * 02111-1307, USA. 
 **********************************************************************/

/* STP PORT instance : 17.19, 17.17 */

#ifndef _STP_PORT_H__
#define _STP_PORT_H__

#include "statmch.h"

#define TIMERS_NUMBER   9
typedef unsigned int PORT_TIMER_T;

typedef enum {
	Mine,
	Aged,
	Received,
	Disabled
} INFO_IS_T;

typedef enum {
	SuperiorDesignatedInfo,
	RepeatedDesignatedInfo,
	InferiorDesignatedInfo,
	InferiorRootAlternateInfo,
	OtherInfo
} RCVD_INFO_T;

typedef enum {
	DisabledPort = 0,
	AlternatePort,
	BackupPort,
	RootPort,
	DesignatedPort,
	NonStpPort
} PORT_ROLE_T;

typedef struct port_t {
	struct port_t	*next;

	/* per Port state machines */

	STATE_MACH_T	*receive;		/* 17.23 */
	STATE_MACH_T	*migrate;		/* 17.24 */
	STATE_MACH_T	*brdgdet;		/* 17.25 */
	STATE_MACH_T	*transmit;		/* 17.26 */
	STATE_MACH_T	*info;			/* 17.27 */
	STATE_MACH_T	*roletrns;		/* 17.29 */
	STATE_MACH_T	*sttrans;		/* 17.30 */
	STATE_MACH_T	*topoch;		/* 17.31 */
	STATE_MACH_T	*p2p;			/* 6.4.3, 6.5.1 */
	STATE_MACH_T	*pcost;			/*  */

	STATE_MACH_T	*machines;		/* list of machines */

	struct stpm_t	*owner;			/* Bridge, that this port belongs to */

	/* per port Timers */
	PORT_TIMER_T	edgeDelayWhile;		/* 17.17.1 */
	PORT_TIMER_T	fdWhile;		/* 17.17.2 */
	PORT_TIMER_T	helloWhen;		/* 17.17.3 */
	PORT_TIMER_T	mdelayWhile;		/* 17.17.4 */
	PORT_TIMER_T	rbWhile;		/* 17.17.5 */
	PORT_TIMER_T	rcvdInfoWhile;		/* 17.17.6 */
	PORT_TIMER_T	rrWhile;		/* 17.17.7 */
	PORT_TIMER_T	tcWhile;		/* 17.17.8 */
	PORT_TIMER_T	txCount;		/* 17.19.44 */

	PORT_TIMER_T	*timers[TIMERS_NUMBER];	/* list of timers */

	unsigned int	ageingTime;		/* 17.19.1 */
	Bool		agree;			/* 17.19.2 */
	Bool		agreed;			/* 17.19.3 */
	PRIO_VECTOR_T	designatedPriority;	/* 17.19.4 */
	TIMEVALUES_T	designatedTimes;	/* 17.19.5 */
	Bool		disputed;		/* 17.19.6 */
	Bool		fbdFlush;		/* 17.19.7 */
	Bool		forward;		/* 17.19.8 */
	Bool		forwarding;		/* 17.19.9 */
	INFO_IS_T	infoIs;			/* 17.19.10 */
	Bool		learn;			/* 17.19.11 */
	Bool		learning;		/* 17.19.12 */
	Bool		mcheck;			/* 17.19.13 */
	PRIO_VECTOR_T	msgPriority;		/* 17.19.14 */
	TIMEVALUES_T	msgTimes;		/* 17.19.15 */
	Bool		newInfo;		/* 17.19.16 */
	Bool		operEdge;		/* 17.19.17 */
	Bool		portEnabled;		/* 17.19.18 */
	PORT_ID		portId;			/* 17.19.19 */
	unsigned long	PortPathCost;		/* 17.19.20 */
	PRIO_VECTOR_T	portPriority;		/* 17.19.21 */
	TIMEVALUES_T	portTimes;		/* 17.19.22 */
	Bool		proposed;		/* 17.19.23 */
	Bool		proposing;		/* 17.19.24 */
	Bool		rcvdBPDU;		/* 17.19.25 */
	RCVD_INFO_T	rcvdInfo;		/* 17.19.26 */
	Bool		rcvdMsg;		/* 17.19.27 */
	Bool		rcvdRSTP;		/* 17.19.28 */
	Bool		rcvdSTP;		/* 17.19.29 */
	Bool		rcvdTc;			/* 17.19.30 */
	Bool		rcvdTcAck;		/* 17.19.31 */
	Bool		rcvdTcn;		/* 17.19.32 */
	Bool		reRoot;			/* 17.19.33 */
	Bool		reselect;		/* 17.19.34 */
	PORT_ROLE_T	role;			/* 17.19.35 */
	Bool		selected;		/* 17.19.36 */
	PORT_ROLE_T	selectedRole;		/* 17.19.37 */
	Bool		sendRSTP;		/* 17.19.38 */
	Bool		sync;			/* 17.19.39 */
	Bool		synced;			/* 17.19.40 */
	Bool		tc;			/* 17.19.41 */
	Bool		tcAck;			/* 17.19.42 */
	Bool		tick;			/* 17.19.43 */
	Bool		tcProp;			/* 17.19.44 */
	Bool		updtInfo;		/* 17.19.45 */

	/* message information */
	unsigned char	msgBpduVersion;
	unsigned char	msgBpduType;
	unsigned char	msgPortRole;
	unsigned char	msgFlags;

	unsigned long	adminPCost; /* may be ADMIN_PORT_PATH_COST_AUTO */
	unsigned long	operPCost;
	unsigned long	operSpeed;
	unsigned long	usedSpeed;
	int		LinkDelay; /* TBD: LinkDelay may be managed ? */
	Bool		AdminEdgePort;
	Bool		AutoEdgePort;
	Bool		adminEnable; /* 'has LINK' */
	Bool		wasInitBpdu;
	Bool		admin_non_stp;

	Bool		p2p_recompute;
	Bool		operPointToPointMac;
	ADMIN_P2P_T	adminPointToPointMac;

	/* statistics */
	unsigned long	rx_cfg_bpdu_cnt;
	unsigned long	rx_rstp_bpdu_cnt;
	unsigned long	rx_tcn_bpdu_cnt;

	unsigned long	uptime; /* 14.8.2.1.3.a */

	int		port_index;
	char		*port_name;

#ifdef STP_DBG
	unsigned int	skip_rx;
	unsigned int	skip_tx;
#endif
} PORT_T;

PORT_T *STP_port_create(struct stpm_t *stpm, int port_index);

void STP_port_delete(PORT_T *this);

int STP_port_rx_bpdu (PORT_T *this, BPDU_T *bpdu, size_t len);

void STP_port_init (PORT_T *this, struct stpm_t *stpm, Bool check_link);

#ifdef STP_DBG
int STP_port_trace_state_machine (PORT_T *this, char *mach_name, int enadis, int vlan_id);

void STP_port_trace_flags (char *title, PORT_T *this);
#endif

#endif /*  _STP_PORT_H__ */

