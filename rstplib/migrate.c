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

/* Port Protocol Migration state machine : 17.24 */
 
#include "base.h"
#include "stpm.h"
#include "migrate.h"

#define STATES {		\
	CHOOSE(CHECKING_RSTP),	\
	CHOOSE(SELECTING_STP),	\
	CHOOSE(SENSING),		\
}

#define GET_STATE_NAME STP_migrate_get_state_name
#include "choose.h"

void STP_migrate_enter_state(STATE_MACH_T *this)
{
	register PORT_T *port = this->owner.port;
	register STPM_T *stpm = port->owner;

	switch (this->State) {
		case BEGIN:
		case CHECKING_RSTP:
			port->mcheck = False;
			port->sendRSTP = stpm->rstpVersion;
			port->mdelayWhile = MigrateTime;
			break;
		case SELECTING_STP:
			port->sendRSTP = False;
			port->mdelayWhile = MigrateTime;
			break;
		case SENSING:
			port->rcvdRSTP = port->rcvdSTP = False;
			break;
	}
}

Bool STP_migrate_check_conditions(STATE_MACH_T *this)
{
	register PORT_T *port = this->owner.port;
	register STPM_T *stpm = port->owner;

	if (BEGIN == this->State)
		return STP_hop_2_state (this, CHECKING_RSTP);

	switch (this->State) {
		case CHECKING_RSTP:
			if (port->mdelayWhile == 0) {
				return STP_hop_2_state(this, SENSING);
			}
			if (port->mdelayWhile != MigrateTime && !port->portEnabled) {
				return STP_hop_2_state(this, CHECKING_RSTP);
			}
			break;
		case SELECTING_STP:
			if (port->mdelayWhile == 0 ||
			    !port->portEnabled || port->mcheck) {
				return STP_hop_2_state(this, SENSING);
			}
			break;
		case SENSING:
			if (!port->portEnabled || port->mcheck ||
			    (stpm->rstpVersion && !port->sendRSTP && port->rcvdRSTP)) {
				return STP_hop_2_state(this, CHECKING_RSTP);
			}
			if (port->sendRSTP && port->rcvdSTP) {
				return STP_hop_2_state(this, SELECTING_STP);
			}
	}
	return False;
}
