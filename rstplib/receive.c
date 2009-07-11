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

/* Port Receive state machine : 17.23 */
  
#include "base.h"
#include "stpm.h"
#include "stp_to.h" /* for STP_OUT_get_port_mac & STP_OUT_tx_bpdu */
#include "portinfo.h"
#include "migrate.h"

#define STATES {		\
	CHOOSE(DISCARD),	\
	CHOOSE(RECEIVE),	\
}

#define GET_STATE_NAME STP_receive_get_state_name
#include "choose.h"

void STP_receive_enter_state(STATE_MACH_T *this)
{
	register PORT_T *port = this->owner.port;

	switch (this->State) {
		case BEGIN:
		case DISCARD:
			port->rcvdBPDU = port->rcvdRSTP = port->rcvdSTP = False;
			port->rcvdMsg = False;
			port->edgeDelayWhile = MigrateTime;
			break;
		case RECEIVE:
			updtBPDUVersion(this);
			port->operEdge = port->rcvdBPDU = False;
			port->rcvdMsg = True;
			port->edgeDelayWhile = MigrateTime;
			break;
	};
}

Bool STP_receive_check_conditions(STATE_MACH_T *this)
{
	register PORT_T *port = this->owner.port;

	if (BEGIN == this->State ||
	    ((port->rcvdBPDU || (port->edgeDelayWhile != MigrateTime)) && !port->portEnabled)) {
		return STP_hop_2_state(this, DISCARD);
	}
	
	switch (this->State) {
		case DISCARD:
			if (port->rcvdBPDU && port->portEnabled) {
				return STP_hop_2_state(this, RECEIVE);
			}
			break;
		case RECEIVE:
			if (port->rcvdBPDU && port->portEnabled && !port->rcvdMsg) {
				return STP_hop_2_state(this, RECEIVE);
			}
			break;
	};
	return False;
}

