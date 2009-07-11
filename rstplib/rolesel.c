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

/* Port Role Selection state machine : 17.28 */

#include "base.h"
#include "stpm.h"

#define STATES { \
	CHOOSE(INIT_BRIDGE),	\
	CHOOSE(ROLE_SELECTION),	\
}

#define GET_STATE_NAME STP_rolesel_get_state_name
#include "choose.h"

#ifdef STP_DBG
void stp_dbg_break_point (PORT_T * port, STPM_T* stpm)
{
}
#endif

/*! \function static Bool _is_backup_port (PORT_T *port, STPM_T *this)
 *  \brief Implements 17.21.25 subfunction (j) and (k) check.
 *     j) If the port priority vector was received in a Configuration Message
 *        and is not aged (infoIs = Received), the root priority vector is not
 *        now derived from it, the designated priority vector is not higher
 *        than the port priority vector, and the designated bridge and
 *        designated port components of the port priority vector do not
 *        reflect another port on this bridge, selectedRole is set to
 *        AlternatePort and updtInfo is reset.
 *     k) If the port priority vector was received in a Configuration Message
 *        and is not aged (infoIs = Received), the root priority vector is not
 *        now derived from it, the designated priority vector is not higher
 *        than the port priority vector, and the designated bridge and
 *        designated port components of the port priority vector reflect
 *        another port on this bridge, selectedRole is set to BackupPort and
 *        updtInfo is reset.
 */
static Bool _is_backup_port(PORT_T *port, STPM_T *this)
{
	if (!STP_VECT_compare_bridge_id
	    (&port->portPriority.design_bridge, &this->BridgeIdentifier) &&
	    port->portPriority.design_port != this->rootPortId) {
#if 0 /* def STP_DBG */
		if (port->info->debug) {
			STP_VECT_br_id_print("portPriority.design_bridge",
					     &port->portPriority.desig_bridge, True);
			STP_VECT_br_id_print("            this->BridgeIdentifier",
					     &this->BridgeIdentifier, True);
		}
		stp_dbg_break_point(port, this);
#endif
		return True;
	} else {
		return False;
	}
}

static void setRoleSelected(char *reason, STPM_T *stpm, PORT_T *port,
			    PORT_ROLE_T newRole)
{
	char *new_role_name;

	port->selectedRole = newRole;

	if (newRole == port->role) {
		return;
	}

	switch (newRole) {
		case DisabledPort:
			new_role_name = "Disabled";
			break;
		case AlternatePort:
			new_role_name = "Alternate";
			break;
		case BackupPort:
			new_role_name = "Backup";
			break;
		case RootPort:
			new_role_name = "Root";
			break;
		case DesignatedPort:
			new_role_name = "Designated";
			break;
		case NonStpPort:
			new_role_name = "NonStp";
			port->role = newRole;
			break;
		default:
			stp_trace("%s-%s:port %s => Unknown (%d ?)",
				  reason, stpm->name, port->port_name, (int) newRole);
			return;
	}

#ifdef STP_DBG
	if (port->roletrns->debug) {
		stp_trace("%s(%s-%s) => %s",
			  reason, stpm->name,
			  port->port_name, new_role_name);
	}
#endif
}

/*! \function static void updtRoleDisableTree(STPM_T *this)
 *  \brief Implements 17.21.24
 *  Sets selectedRole to DisabledPort for all Ports of the Bridge.
*/
static void updtRoleDisableTree(STPM_T *this)
{
	register PORT_T *port;

	for (port = this->ports; port; port = port->next) {
		port->selectedRole = DisabledPort;
	}
}

/*! \function static void clearReselectTree(STPM_T *this)
 *  \brief Implements 17.21.2
 *  Clears reselect for all Ports of the Bridge.
 */
static void clearReselectTree(STPM_T *this)
{
	register PORT_T *port;

	for (port = this->ports; port; port = port->next) {
		port->reselect = False;
	}
}

/*! \function static void updtRootPriority(STATE_MACH_T *this)
 *  \brief Implements 17.21.25 subfunction (a), (b) & (c).
 *     a) The root path priority vector for each Port that has a port priority
 *        vector (portPriority plus portId; 17.19.19, 17.19.21), recorded from
 *        a received message and not aged out (infoIs == Received)
 *     b) The Bridge's root priority vector (rootPriority plus rootPortId;
 *        17.18.6, 17.18.5), chosen as the best of the set of priority vectors
 *        comprising the Bridge's own bridge priority vector (BridgePriority;
 *        17.18.3) and all the calculated root path priority vectors whose
 *        DesignatedBridgeID Bridge Address component is not equal to that
 *        component of the Bridge's own bridge priority vector (see 17.6)
 *     c) The Bridge's rootTimes (17.18.7) parameter, set equal to:
 *        1) BridgeTimes (17.18.4), if the chosen root priority vector is the
 *           bridge priority vector, otherwise
 *        2) portTimes (17.19.22) for the port associated with the selected
 *           root priority vector, with the Message Age component incremented
 *           by 1 second and rounded to the nearest whole second.
 */
static void updtRootPriority(STATE_MACH_T *this)
{
	PRIO_VECTOR_T rootPathPrio;   /* 17.4.2.2 */
	register PORT_T *port;
	register STPM_T *stpm;
	register unsigned int dm;

	stpm = this->owner.stpm;

	for (port = stpm->ports; port; port = port->next) {
		if (port->admin_non_stp) {
			continue;
		}

		if (Received == port->infoIs) {
			STP_VECT_copy(&rootPathPrio, &port->portPriority);
			rootPathPrio.root_path_cost += port->portId;

			if (STP_VECT_compare_vector (&rootPathPrio, &stpm->rootPriority) < 0) {
				STP_VECT_copy(&stpm->rootPriority, &rootPathPrio);
				STP_copy_times(&stpm->rootTimes, &port->portTimes);
				dm = (8 +  stpm->rootTimes.MaxAge) / 16;
				if (!dm) {
					dm = 1;
				}
				stpm->rootTimes.MessageAge += dm;
#ifdef STP_DBG
				if (port->roletrns->debug) {
					stp_trace("updtRootPriority: dm=%d rootTimes.MessageAge=%d on port %s",
						  (int)dm, (int)stpm->rootTimes.MessageAge,
						  port->port_name);
				}
#endif
			}
		}
	}
}

/*! \function static void updtRolesTree(STATE_MACH_T *this)
 *  \brief Implements 17.21.25
 *  This procedure calculates the following spanning tree priority vectors
 *  (17.5, 17.6) and timer values:
 *     d) The first four components of the designated priority vector
 *        (designatedPriority, 17.19.4) for each port.
 *     e) The designatedTimes (17.19.5) for each Port, set equal to the value
 *        of rootTimes, except for the Hello Time component, which is set equal
 *        to BridgeTimes' Hello Time.
 *     The port role for each Port is assigned, and its port priority vector and
 *     Spanning Tree timer information are updated as follows:
 *     f) If the Port is Disabled (infoIs = Disabled), selectedRole is set to
 *        DisabledPort. Otherwise:
 *     g) If the port priority vector information was aged (infoIs = Aged),
 *        updtInfo is set and selectedRole is set to DesignatedPort.
 *     h) If the port priority vector was derived from another port on the
 *        Bridge or from the Bridge itself as the Root Bridge (infoIs = Mine),
 *        selectedRole is set to DesignatedPort. Additionally, updtInfo is set
 *        if the port priority vector differs from the designated priority
 *        vector or the Port's associated timer parameters differ from those
 *        for the Root Port.
 *     i) If the port priority vector was received in a Configuration Message
 *        and is not aged (infoIs == Received), and the root priority vector
 *        is now derived from it, selectedRole is set to RootPort and updtInfo
 *        is reset.
 *     j) If the port priority vector was received in a Configuration Message
 *        and is not aged (infoIs = Received), the root priority vector is not
 *        now derived from it, the designated priority vector is not higher
 *        than the port priority vector, and the designated bridge and
 *        designated port components of the port priority vector do not
 *        reflect another port on this bridge, selectedRole is set to
 *        AlternatePort and updtInfo is reset.
 *     k) If the port priority vector was received in a Configuration Message
 *        and is not aged (infoIs = Received), the root priority vector is not
 *        now derived from it, the designated priority vector is not higher
 *        than the port priority vector, and the designated bridge and
 *        designated port components of the port priority vector reflect
 *        another port on this bridge, selectedRole is set to BackupPort and
 *        updtInfo is reset.
 *     l) If the port priority vector was received in a Configuration Message
 *        and is not aged (infoIs == Received), the root priority vector is
 *        not now derived from it, the designated priority vector is higher
 *        than the port priority vector, selectedRole is set to DesignatedPort
 *        and updtInfo is set.
 */
static void updtRolesTree(STATE_MACH_T *this)
{
	register PORT_T *port;
	register STPM_T *stpm;
	PORT_ID old_root_port; /* for tracing of root port changing */

	stpm = this->owner.stpm;
	old_root_port = stpm->rootPortId;

	/* Create an new rootPriority structure that makes this bridge root */
	STP_VECT_create(&stpm->rootPriority, &stpm->BridgeIdentifier, 0, &stpm->BridgeIdentifier, 0, 0);
	/* Copy the times from this bridge to the rootTimes structure */
	STP_copy_times(&stpm->rootTimes, &stpm->BridgeTimes);
	stpm->rootPortId = 0;

	/* (a), (b) & (c) */
	updtRootPriority(this);

	/* (d) & (e) */
	for (port = stpm->ports; port; port = port->next) {
		if (port->admin_non_stp) {
			continue;
		}
		STP_VECT_create(&port->designatedPriority,
				&stpm->rootPriority.root_bridge,
				stpm->rootPriority.root_path_cost,
				&stpm->BridgeIdentifier, port->portId, port->portId);
		STP_copy_times(&port->designatedTimes, &stpm->rootTimes);
		port->designatedTimes.HelloTime = stpm->BridgeTimes.HelloTime;

#if 0
#ifdef STP_DBG
		if (port->roletrns->debug) {
			STP_VECT_br_id_print("ch:designatedPriority.designated_bridge",
					     &port->designatedPriorityrity.designated_bridge, True);
		}
#endif
#endif
	}

	stpm->rootPortId = stpm->rootPriority.bridge_port;

#ifdef STP_DBG
	if (old_root_port != stpm->rootPortId) {
		if (!stpm->rootPortId) {
			stp_trace("\nbrige %s became root", stpm->name);
		} else {
			stp_trace("\nbrige %s new root port: %s",
				  stpm->name,
				  STP_stpm_get_port_name_by_id (stpm, stpm->rootPortId));
		}
	}
#endif

	/* (f), (g), (h), (i), (j), (k) and (l) */
	for (port = stpm->ports; port; port = port->next) {
		if (port->admin_non_stp) {
			setRoleSelected("Non", stpm, port, NonStpPort);
			port->forward = port->learn = True;
			continue;
		}

		switch (port->infoIs) {
			case Disabled:
				setRoleSelected("Dis", stpm, port, DisabledPort);
				break;
			case Aged:
				setRoleSelected("Age", stpm, port, DesignatedPort);
				port->updtInfo = True;
				break;
			case Mine:
				/*     h) If the port priority vector was derived from another port on the
				 *        Bridge or from the Bridge itself as the Root Bridge (infoIs = Mine),
				 *        selectedRole is set to DesignatedPort. Additionally, updtInfo is set
				 *        if the port priority vector differs from the designated priority
				 *        vector or the Port's associated timer parameters differ from those
				 *        for the Root Port.
				 */
				setRoleSelected("Mine", stpm, port, DesignatedPort);
				if (0 != STP_VECT_compare_vector(&port->portPriority,
				    &port->designatedPriority) ||
				    0 != STP_compare_times(&port->portTimes,
				    &stpm->rootTimes)) {
					port->updtInfo = True;
				}
				break;
			case Received:
				if (stpm->rootPortId == port->portId) {
					/*     i) If the port priority vector was received in a Configuration Message
					 *        and is not aged (infoIs == Received), and the root priority vector
					 *        is now derived from it, selectedRole is set to RootPort and updtInfo
					 *        is reset.
					 */
					setRoleSelected("Rec", stpm, port, RootPort);
				} else if (STP_VECT_compare_vector (&port->designatedPriority, &port->portPriority) < 0) {
					/*     l) If the port priority vector was received in a Configuration Message
					 *        and is not aged (infoIs == Received), the root priority vector is
					 *        not now derived from it, the designated priority vector is higher
					 *        than the port priority vector, selectedRole is set to DesignatedPort
					 *        and updtInfo is set.
					 */
					setRoleSelected("Rec", stpm, port, DesignatedPort);
					port->updtInfo = True;
					break;
				} else {
					if (_is_backup_port (port, stpm)) {
						/*     k) If the port priority vector was received in a Configuration Message
						 *        and is not aged (infoIs = Received), the root priority vector is not
						 *        now derived from it, the designated priority vector is not higher
						 *        than the port priority vector, and the designated bridge and
						 *        designated port components of the port priority vector reflect
						 *        another port on this bridge, selectedRole is set to BackupPort and
						 *        updtInfo is reset.
						 */
						setRoleSelected("rec", stpm, port, BackupPort);
					} else {
						/*     j) If the port priority vector was received in a Configuration Message
						 *        and is not aged (infoIs = Received), the root priority vector is not
						 *        now derived from it, the designated priority vector is not higher
						 *        than the port priority vector, and the designated bridge and
						 *        designated port components of the port priority vector do not
						 *        reflect another port on this bridge, selectedRole is set to
						 *        AlternatePort and updtInfo is reset.
						 */
						setRoleSelected("rec", stpm, port, AlternatePort);
					}
				}
				port->updtInfo = False;
				break;
			default:
				stp_trace("undef infoIs=%d", (int) port->infoIs);
				break;
		}
	}
}

/*! \function static Bool setSelected Tree(STPM_T *this)
 *  \brief Implements 17.21.16
 *  Sets the selected variable TRUE for all Ports of the Bridge if reselect
 *  is FALSE for all Ports. If reselect is TRUE for any Port, this procedure
 *  takes no action.
 */
static Bool setSelectedTree(STPM_T *this)
{
	register PORT_T *port;

	for (port = this->ports; port; port = port->next) {
		if (port->reselect) {
#ifdef STP_DBG
			stp_trace("setSelectedBridge: TRUE=reselect on port %s", port->port_name);
#endif
			return False;
		}
	}

	for (port = this->ports; port; port = port->next) {
		port->selected = True;
	}
	return True;
}

void STP_rolesel_enter_state(STATE_MACH_T *this)
{
	STPM_T *stpm;

	stpm = this->owner.stpm;

	switch (this->State) {
		case BEGIN:
		case INIT_BRIDGE:
			updtRoleDisableTree(stpm);
			break;
		case ROLE_SELECTION:
			clearReselectTree(stpm);
			updtRolesTree(this);
			setSelectedTree(stpm);
			break;
	}
}

Bool STP_rolesel_check_conditions(STATE_MACH_T *s)
{
	STPM_T *stpm;
	register PORT_T *port;

	if (BEGIN == s->State) {
		return STP_hop_2_state (s, INIT_BRIDGE);
	}

	switch (s->State) {
		case INIT_BRIDGE:
			return STP_hop_2_state(s, ROLE_SELECTION);
		case ROLE_SELECTION:
			stpm = s->owner.stpm;
			for (port = stpm->ports; port; port = port->next) {
				if (port->reselect) {
					/* stp_trace ("reselect on port %s", port->port_name); */
					return STP_hop_2_state(s, ROLE_SELECTION);
				}
			}
			break;
	}
	return False;
}
