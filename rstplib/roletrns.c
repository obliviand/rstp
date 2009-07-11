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

/* Port Role Transitions state machine : 17.29 */
 
#include "base.h"
#include "stpm.h"
#include "migrate.h"

#define STATES { \
	CHOOSE(INIT_PORT),		\
	CHOOSE(DISABLE_PORT),		\
	CHOOSE(DISABLED_PORT),		\
	CHOOSE(ROOT_PORT),		\
	CHOOSE(ROOT_PROPOSED),		\
	CHOOSE(ROOT_FORWARD),		\
	CHOOSE(ROOT_AGREED),		\
	CHOOSE(ROOT_LEARN),		\
	CHOOSE(REROOT),			\
	CHOOSE(REROOTED),		\
	CHOOSE(DESIGNATED_PORT),	\
	CHOOSE(DESIGNATED_PROPOSE),	\
	CHOOSE(DESIGNATED_FORWARD),	\
	CHOOSE(DESIGNATED_SYNCED),	\
	CHOOSE(DESIGNATED_LEARN),	\
	CHOOSE(DESIGNATED_RETIRED),	\
	CHOOSE(DESIGNATED_DISCARD),	\
	CHOOSE(ALTERNATE_PORT),		\
	CHOOSE(ALTERNATE_PROPOSED),	\
	CHOOSE(ALTERNATE_AGREED),	\
	CHOOSE(BLOCK_PORT),		\
	CHOOSE(BACKUP_PORT),		\
}

#define GET_STATE_NAME STP_roletrns_get_state_name
#include "choose.h"

/*! \function static void setSyncTree(STATE_MACH_T *this)
 *  \brief Implements 17.21.14
 *  Sets sync TRUE for all Ports of the Bridge.
 */
static void setSyncTree(STATE_MACH_T *this)
{
	register PORT_T *port;

	for (port = this->owner.port->owner->ports; port; port = port->next) {
		port->sync = True;
	}
}

/*! \function static void setReRootTree(STATE_MACH_T *this)
 *  \brief Implements 17.21.15
 *  Sets reRoot TRUE for all Ports of the Bridge.
 */
static void setReRootTree(STATE_MACH_T *this)
{
	register PORT_T *port;

	for (port = this->owner.port->owner->ports; port; port = port->next) {
		port->reRoot = True;
	}
}

static Bool compute_allsynced(PORT_T *this)
{
	register PORT_T *port;

	for (port = this->owner->ports; port; port = port->next) {
		if (port->port_index == this->port_index) {
			continue;
		}
		if (! port->synced) {
			return False;
		}
	}
	return True;
}

static Bool compute_rerooted(PORT_T *this)
{
	register PORT_T *port;

	for (port = this->owner->ports; port; port = port->next) {
		if (port->port_index == this->port_index) {
			continue;
		}
		if (port->rrWhile) {
			return False;
		}
	}
	return True;
}

/*! \function static unsigned short compute_edgedelay(PORT_T *port, STPM_T *stpm)
 *  \brief Implements 17.20.4
 *  Returns the value of MigrateTime if operPointToPointMAC is TRUE,
 *  and the value of MaxAge otherwise.
 */
static unsigned short compute_edgedelay(PORT_T *port, STPM_T *stpm)
{
	if (port->operPointToPointMac) {
		return MigrateTime;
	}
	return stpm->rootTimes.MaxAge;
}

void STP_roletrns_enter_state(STATE_MACH_T *this)
{
	register PORT_T *port = this->owner.port;
	register STPM_T *stpm;

	stpm = port->owner;

	switch (this->State) {
		/* 17.29.1 Disabled Port states */
		case BEGIN:
		case INIT_PORT:
			port->role = DisabledPort;
			port->learn = port->forward = False;
			port->synced = False;
			port->sync = port->reRoot = False;
			port->rrWhile = stpm->rootTimes.ForwardDelay;
			port->fdWhile = stpm->rootTimes.ForwardDelay;
			port->rbWhile = 0;
#ifdef STP_DBG
			if (this->debug) {
				STP_port_trace_flags("after init", port);
			}
#endif
			break;
		case DISABLE_PORT:
			port->role = port->selectedRole;
			port->learn = port->forward = False;
			break;
		case DISABLED_PORT:
			port->fdWhile = stpm->rootTimes.MaxAge;
			port->synced = True;
			port->rrWhile = 0;
			port->sync = port->reRoot = False;
			break;

		/* 17.29.2 Root Port states */
		case ROOT_PORT:
			port->role = RootPort;
			port->rrWhile = stpm->rootTimes.ForwardDelay;
#ifdef STP_DBG
			if (this->debug) {
				STP_port_trace_flags("ROOT_PORT", port);
			}
#endif
			break;
		case ROOT_PROPOSED:
			setSyncTree (this);
			port->proposed = False;
#ifdef STP_DBG
			if (this->debug) {
				STP_port_trace_flags("ROOT_PROPOSED", port);
			}
#endif
			break;
		case ROOT_FORWARD:
			port->fdWhile = 0;
			port->forward = True;
#ifdef STP_DBG
			if (this->debug) {
				STP_port_trace_flags("ROOT_FORWARD", port);
			}
#endif
			break;
		case ROOT_AGREED:
			port->proposed = port->sync = False; /* in ROOT_AGREED */
			port->agree = True; /* In ROOT_AGREED */
			port->newInfo = True;
#ifdef STP_DBG
			if (this->debug) {
				STP_port_trace_flags("ROOT_AGREED", port);
			}
#endif
			break;
		case ROOT_LEARN:
			port->fdWhile = stpm->rootTimes.ForwardDelay;
			port->learn = True;
#ifdef STP_DBG
			if (this->debug) {
				STP_port_trace_flags("ROOT_LEARN", port);
			}
#endif
			break;
		case REROOT:
			setReRootTree (this);
#ifdef STP_DBG
			if (this->debug) {
				STP_port_trace_flags("REROOT", port);
			}
#endif
			break;
		case REROOTED:
			port->reRoot = False; /* In REROOTED */
#ifdef STP_DBG
			if (this->debug) {
				STP_port_trace_flags("REROOTED", port);
			}
#endif
			break;

		/* 17.29.3 Designated Port states */
		case DESIGNATED_PORT:
			port->role = DesignatedPort;
#ifdef STP_DBG
			if (this->debug) {
				STP_port_trace_flags("DESIGNATED_PORT", port);
			}
#endif
			break;
		case DESIGNATED_PROPOSE:
			port->proposing = True; /* in DESIGNATED_PROPOSE */
			port->edgeDelayWhile = compute_edgedelay(port, stpm);
			port->newInfo = True;
#ifdef STP_DBG
			if (this->debug) {
				STP_port_trace_flags("DESIGNATED_PROPOSE", port);
			}
#endif
			break;
		case DESIGNATED_FORWARD:
			port->forward = True;
			port->fdWhile = 0;
			port->agreed = port->sendRSTP;
#ifdef STP_DBG
			if (this->debug) {
				STP_port_trace_flags("DESIGNATED_FORWARD", port);
			}
#endif
			break;
		case DESIGNATED_SYNCED:
			port->rrWhile = 0;
			port->synced = True; /* DESIGNATED_SYNCED */
			port->sync = False; /* DESIGNATED_SYNCED */
#ifdef STP_DBG
			if (this->debug) {
				STP_port_trace_flags("DESIGNATED_SYNCED", port);
			}
#endif
			break;
		case DESIGNATED_LEARN:
			port->learn = True;
			port->fdWhile = stpm->rootTimes.ForwardDelay;
#ifdef STP_DBG
			if (this->debug) {
				STP_port_trace_flags("DESIGNATED_LEARN", port);
			}
#endif
			break;
		case DESIGNATED_RETIRED:
			port->reRoot = False; /* DESIGNATED_RETIRED */
#ifdef STP_DBG
			if (this->debug) {
				STP_port_trace_flags("DESIGNATED_RETIRED", port);
			}
#endif
			break;
		case DESIGNATED_DISCARD:
			port->learn = port->forward = port->disputed = False;
			port->fdWhile = stpm->rootTimes.ForwardDelay;
#ifdef STP_DBG
			if (this->debug) {
				STP_port_trace_flags("DESIGNATED_DISCARD", port);
			}
#endif
			break;
		
		/* 17.29.4 Alternate Port states */
		case ALTERNATE_PORT:
			port->fdWhile = stpm->rootTimes.ForwardDelay;
			port->synced = True;
			port->rrWhile = 0;
			port->sync = port->reRoot = False;
#ifdef STP_DBG
			if (this->debug) {
				STP_port_trace_flags("ALTERNATE_PORT", port);
			}
#endif
			break;
		case ALTERNATE_PROPOSED:
			setSyncTree(this);
			port->proposed = False;
#ifdef STP_DBG
			if (this->debug) {
				STP_port_trace_flags("ALTERNATE_PROPOSED", port);
			}
#endif
			break;
		case ALTERNATE_AGREED:
			port->proposed = False;
			port->agree = True;
			port->newInfo = True;
			
#ifdef STP_DBG
			if (this->debug) {
				STP_port_trace_flags("ALTERNATE_AGREED", port);
			}
#endif
			break;
		case BLOCK_PORT:
			port->role = port->selectedRole;
			port->learn = port->forward = False;
#ifdef STP_DBG
			if (this->debug) {
				STP_port_trace_flags("BLOCK_PORT", port);
			}
#endif	
			break;
		case BACKUP_PORT:
			port->rbWhile = 2 * stpm->rootTimes.HelloTime;
#ifdef STP_DBG
			if (this->debug) {
				STP_port_trace_flags("BACKUP_PORT", port);
			}
#endif
			break;
	};
}

Bool STP_roletrns_check_conditions(STATE_MACH_T *this)
{
	register PORT_T *port = this->owner.port;
	register STPM_T *stpm;
	Bool allSynced;
	Bool allReRooted;

	stpm = port->owner;

	if (BEGIN == this->State) {
		return STP_hop_2_state(this, INIT_PORT);
	}

	/* Non-initial entry states */
	if (port->role != port->selectedRole &&
	    port->selected && !port->updtInfo) {
		switch (port->selectedRole) {
			case DisabledPort:
#if 0 /* def STP_DBG */
				if (this->debug) {
					stp_trace("hop to DISABLE_PORT role=%d selectedRole=%d",
						  (int)port->role, (int)port->selectedRole);
				}
#endif
				return STP_hop_2_state(this, DISABLE_PORT);
			case AlternatePort:
			case BackupPort:
#if 0 /* def STP_DBG */
				if (this->debug) {
					stp_trace("hop to BLOCK_PORT role=%d selectedRole=%d",
						  (int)port->role, (int)port->selectedRole);
				}
#endif
				return STP_hop_2_state(this, BLOCK_PORT);
			case RootPort:
				return STP_hop_2_state(this, ROOT_PORT);
			case DesignatedPort:
				return STP_hop_2_state(this, DESIGNATED_PORT);
			default:
				return False;
		}
	}
	
	switch (this->State) {
		/* 17.29.1 Disabled Port states */
		case INIT_PORT:
			return STP_hop_2_state(this, DISABLE_PORT);
		case DISABLE_PORT:
			if (!port->learning && !port->forwarding &&
			    port->selected && !port->updtInfo) {
				return STP_hop_2_state(this, DISABLED_PORT);
			}
			break;
		case DISABLED_PORT:
			if ((port->fdWhile != stpm->rootTimes.MaxAge ||
			    port->sync ||
			    port->reRoot ||
			    !port->synced) && port->selected && !port->updtInfo) {
				return STP_hop_2_state(this, DISABLED_PORT);
			}
			break;

		/* 17.29.2 Root Port states */
		case ROOT_PROPOSED:
		case ROOT_AGREED:
		case REROOT:
		case REROOTED:
		case ROOT_LEARN:
		case ROOT_FORWARD:
			return STP_hop_2_state(this, ROOT_PORT);

		case ROOT_PORT:
			if (port->selected && !port->updtInfo) {
				if (!port->forward && !port->reRoot) {
					return STP_hop_2_state(this, REROOT);
				}
				allSynced = compute_allsynced(port);
				if ((allSynced && !port->agree) ||
				    (port->proposed && port->agree)) {
					return STP_hop_2_state(this, ROOT_AGREED);
				}
				if (port->proposed && !port->agree) {
					return STP_hop_2_state(this, ROOT_PROPOSED);
				}
				allReRooted = compute_rerooted(port);
				if ((!port->fdWhile ||
				    (allReRooted && !port->rbWhile)) && stpm->rstpVersion &&
				    port->learn && !port->forward) {
					return STP_hop_2_state(this, ROOT_FORWARD);
				}
				if ((!port->fdWhile ||
				    (allReRooted && !port->rbWhile)) && stpm->rstpVersion &&
				    !port->learn) {
					return STP_hop_2_state(this, ROOT_LEARN);
				}

				if (port->reRoot && port->forward) {
					return STP_hop_2_state(this, REROOTED);
				}
				if (port->rrWhile != stpm->rootTimes.ForwardDelay) {
					return STP_hop_2_state(this, ROOT_PORT);
				}
			}
			break;

		/* 17.29.3 Designated Port states */
		case DESIGNATED_PROPOSE:
		case DESIGNATED_SYNCED:
		case DESIGNATED_RETIRED:
		case DESIGNATED_DISCARD:
		case DESIGNATED_LEARN:
		case DESIGNATED_FORWARD:
			return STP_hop_2_state(this, DESIGNATED_PORT);

		case DESIGNATED_PORT:
			if (port->selected && !port->updtInfo) {
				if (!port->forward && !port->agreed && !port->proposing && !port->operEdge) {
					return STP_hop_2_state(this, DESIGNATED_PROPOSE);
				}

				if ((!port->learning && !port->forwarding && !port->synced) ||
				    (port->agreed && !port->synced) ||
				    (port->operEdge && !port->synced) ||
				    (port->sync && port->synced)) {
					return STP_hop_2_state(this, DESIGNATED_SYNCED);
				}

				if (!port->rrWhile && port->reRoot) {
					return STP_hop_2_state(this, DESIGNATED_RETIRED);
				}

				if ((!port->fdWhile || port->agreed || port->operEdge) &&
				    (!port->rrWhile || !port->reRoot) &&
				    !port->sync &&
				    (port->learn && !port->forward)) {
					return STP_hop_2_state(this, DESIGNATED_FORWARD);
				}
				if ((!port->fdWhile || port->agreed || port->operEdge) &&
				    (!port->rrWhile || !port->reRoot) &&
				    !port->sync && !port->learn) {
					return STP_hop_2_state(this, DESIGNATED_LEARN);
				}
				if (((port->sync && !port->synced) || 
				     (port->reRoot && port->rrWhile) ||
				     port->disputed) &&
				     !port->operEdge && (port->learn || port->forward)) {
					return STP_hop_2_state(this, DESIGNATED_DISCARD);
				}
			}
			break;
		
		/* 17.29.4 Alternate and Backup Port states */
		case ALTERNATE_PROPOSED:
		case ALTERNATE_AGREED:
		case BACKUP_PORT:
			return STP_hop_2_state (this, ALTERNATE_PORT);
		case BLOCK_PORT:
			if (!port->learning && !port->forwarding &&
			    port->selected && !port->updtInfo) {
				return STP_hop_2_state (this, ALTERNATE_PORT);
			}
			break;
		case ALTERNATE_PORT:
			if (port->selected && !port->updtInfo) {
				allSynced = compute_allsynced(port);
				if ((allSynced && !port->agree) ||
				    (port->proposed && port->agree)) {
					return STP_hop_2_state(this, ALTERNATE_AGREED);
				}
				if (port->proposed && !port->agree) {
					return STP_hop_2_state(this, ALTERNATE_PROPOSED);
				}
				if ((port->rbWhile != 2 * stpm->rootTimes.HelloTime) &&
				    (port->role == BackupPort)) {
					return STP_hop_2_state(this, BACKUP_PORT);
				}
				if ((port->fdWhile != stpm->rootTimes.ForwardDelay) ||
				    port->sync || port->reRoot || !port->synced) {
					return STP_hop_2_state(this, ALTERNATE_PORT);
				}
			}
			break;
	};

	return False;
}

