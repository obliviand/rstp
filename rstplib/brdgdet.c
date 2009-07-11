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

/* Bridge Detection state machine : 17.25 */
 
#include "base.h"
#include "stpm.h"

#define STATES { \
	CHOOSE(EDGE),         \
	CHOOSE(NOT_EDGE),     \
}

#define GET_STATE_NAME STP_brdgdet_get_state_name
#include "choose.h"

void STP_brdgdet_enter_state(STATE_MACH_T *s)
{
	register PORT_T *port = s->owner.port;

	switch (s->State) {
		case BEGIN:
			break;
		case EDGE:
			port->operEdge = True;
			break;
		case NOT_EDGE:
			port->operEdge = False;
			break;
	}
}

Bool STP_brdgdet_check_conditions(STATE_MACH_T *s)
{
	register PORT_T *port = s->owner.port;

	switch (s->State) {
		case BEGIN:
			if (port->AdminEdgePort) {
				return STP_hop_2_state(s, EDGE);
			}
			return STP_hop_2_state(s, NOT_EDGE);
		case EDGE:
			if ((!port->portEnabled && !port->AdminEdgePort) || !port->operEdge) {
				return STP_hop_2_state(s, NOT_EDGE);
			}
			break;
		case NOT_EDGE:
			if ((!port->portEnabled && port->AdminEdgePort) ||
			    ((port->edgeDelayWhile == 0) && port->AutoEdgePort &&
			      port->sendRSTP && port->proposing)) {
				return STP_hop_2_state(s, EDGE);
			}
			break;
	}
	return False;
}
