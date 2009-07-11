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

/* Topolgy Change state machine : 17.31 */
  
#include "base.h"
#include "stpm.h"
#include "stp_to.h" /* for STP_OUT_flush_lt */

#define STATES { \
	CHOOSE(INACTIVE),	\
	CHOOSE(LEARNING),	\
	CHOOSE(DETECTED),	\
	CHOOSE(ACTIVE),		\
	CHOOSE(ACKNOWLEDGED),	\
	CHOOSE(PROPAGATING),	\
	CHOOSE(NOTIFIED_TC),	\
	CHOOSE(NOTIFIED_TCN),	\
}

#define GET_STATE_NAME STP_topoch_get_state_name
#include "choose.h"

/* We can flush learned fdb by port, so set this in stpm.c and topoch.c  */
/* This doesn't seem to solve the topology change problems. Don't use it yet */
//#define STRONGLY_SPEC_802_1W

#ifndef STRONGLY_SPEC_802_1W
/* 
 * In many kinds of hardware the function
 * STP_OUT_flush_lt is a) is very hard and b) cannot
 * delete learning emtries per port. The alternate
 * method may be used: we don't care operEdge flag here,
 * but clean learning table once for TopologyChange
 * for all ports, except the received port. I am ready to discuss :(
 * See below word STRONGLY_SPEC_802_1W
 */
#else
static Bool flush(STATE_MACH_T *this, char *reason) /* 17.19.9 */
{
	register PORT_T* port = this->owner.port;
	Bool bret;

	if (port->operEdge) {
		return True;
	}
	if (this->debug) {
		stp_trace("%s (%s, %s, %s, '%s')",
			  "flush", port->port_name, port->owner->name,
			  "this port",
			  reason);
	}

	bret = STP_OUT_flush_lt(port->port_index, port->owner->vlan_id,
				LT_FLASH_ONLY_THE_PORT, reason);
	return bret;
}
#endif

/*! \function static void setTcPropTree(STATE_MACH_T *this)
 *  \brief Implements 17.21.18
 *  Sets tcprop for all Ports except the Port that called the procedure.
 */
static void setTcPropTree(STATE_MACH_T *this)
{
	register PORT_T* port = this->owner.port;
	register PORT_T* tmp;

	for (tmp = port->owner->ports; tmp; tmp = tmp->next) {
		if (tmp->port_index != port->port_index) {
			tmp->tcProp = True;
		}
	}
}

/*! \function static unsigned int newTcWhile(STATE_MACH_T *this)
 *  \brief Implements 17.21.7
 *  If the value of tcWhile is zero and sendRstp is true, this procedure sets
 *  the value of tcWhile to HelloTime plus one second and sets newInfo true.
 *  If the value of tcWhile is zero and sendRstp is false, this procedure sets
 *  the value of tcWhile to the sum of the Max Age and Forward Delay
 *  components of rootTimes and does not change the value of newInfo.
 *  Otherwise the procedure takes no action.
 */
static void newTcWhile(STATE_MACH_T *this)
{
	register PORT_T* port = this->owner.port;

	if (!port->tcWhile && port->sendRSTP) {
		port->tcWhile = port->designatedTimes.HelloTime + 1;
		port->newInfo = True;
	}
	if (!port->tcWhile && !port->sendRSTP) {
		port->tcWhile = port->owner->rootTimes.MaxAge +
				port->owner->rootTimes.ForwardDelay;
	}
}

void STP_topoch_enter_state (STATE_MACH_T *this)
{
	register PORT_T* port = this->owner.port;

	switch (this->State) {
		case BEGIN:
		case INACTIVE:
			port->fbdFlush = True;
			port->tcWhile = 0;
			port->tcAck = False;
			break;
		case LEARNING:
			port->rcvdTc = port->rcvdTcn = port->rcvdTcAck = False;
			port->tcProp = False;
			break;
		case DETECTED:
			newTcWhile(this);
#ifdef STP_DBG
			if (this->debug)
				stp_trace("DETECTED: tcWhile=%d on port %s",
					  port->tcWhile, port->port_name);
#endif
			setTcPropTree(this);
			port->newInfo = True;
			break;
		case ACTIVE:
			break;
		case ACKNOWLEDGED:
			port->tcWhile = 0;
#ifdef STP_DBG
			if (this->debug) {
				stp_trace("ACKNOWLEDGED: tcWhile=%d on port %s",
					  port->tcWhile, port->port_name);
			}
#endif
			port->rcvdTcAck = False;
			break;
		case PROPAGATING:
			newTcWhile(this);
			port->fbdFlush = True;
#ifdef STP_DBG
			if (this->debug) {
				stp_trace("PROPAGATING: tcWhile=%d on port %s",
					  port->tcWhile, port->port_name);
			}
#endif
			port->tcProp = False;
			break;
		case NOTIFIED_TC:
			port->rcvdTcn = port->rcvdTc = False;
			if (port->role == DesignatedPort) {
				port->tcAck = True;
			}
			setTcPropTree(this);
			break;
		case NOTIFIED_TCN:
			newTcWhile (this);
#ifdef STP_DBG
			if (this->debug) {
				stp_trace("NOTIFIED_TCN: tcWhile=%d on port %s",
					  port->tcWhile, port->port_name);
			}
#endif
			break;
	};
}

Bool STP_topoch_check_conditions (STATE_MACH_T *this)
{
	register PORT_T* port = this->owner.port;

	if (BEGIN == this->State) {
		return STP_hop_2_state (this, INACTIVE);
	}

	switch (this->State) {
		case INACTIVE:
			if (port->learn && !port->fbdFlush) {
				return STP_hop_2_state(this, LEARNING);
			}
			break;
		case LEARNING:
			if (((port->role == RootPort) || (port->role == DesignatedPort)) &&
			    port->forward && !port->operEdge) {
				return STP_hop_2_state(this, DETECTED);
			}
			if ((port->role != RootPort) && (port->role != DesignatedPort) &&
			    !(port->learn || port->learning) &&
			    !(port->rcvdTc || port->rcvdTcn || port->rcvdTcAck || port->tcProp)) {
				return STP_hop_2_state(this, INACTIVE);
			}
			if (port->rcvdTc || port->rcvdTcn || port->rcvdTcAck || port->tcProp) {
				return STP_hop_2_state(this, LEARNING);
			}
			break;
		case ACTIVE:
			if (((port->role != RootPort) && (port->role != DesignatedPort)) ||
			    port->operEdge) {
				return STP_hop_2_state(this, LEARNING);
			}
			if (port->rcvdTcn) {
				return STP_hop_2_state(this, NOTIFIED_TCN);
			}
			if (port->rcvdTc) {
				return STP_hop_2_state(this, NOTIFIED_TC);
			}
			if (port->tcProp && !port->operEdge) {
				return STP_hop_2_state(this, PROPAGATING);
			}
			if (port->rcvdTcAck) {
				return STP_hop_2_state(this, ACKNOWLEDGED);
			}
			break;
		case DETECTED:
		case ACKNOWLEDGED:
		case PROPAGATING:
		case NOTIFIED_TC:
		case NOTIFIED_TCN:
			return STP_hop_2_state (this, NOTIFIED_TC);
	};
	return False;
}
