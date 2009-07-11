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

#include "base.h"
#include "stpm.h"

/* The Port Information State Machine : 17.27 */

#define STATES { \
	CHOOSE(DISABLED),		\
	CHOOSE(AGED),			\
	CHOOSE(UPDATE),			\
	CHOOSE(CURRENT),		\
	CHOOSE(RECEIVE),		\
	CHOOSE(SUPERIOR_DESIGNATED),	\
	CHOOSE(REPEATED_DESIGNATED),	\
	CHOOSE(INFERIOR_DESIGNATED),	\
	CHOOSE(NOT_DESIGNATED),		\
	CHOOSE(OTHER),			\
}

#define GET_STATE_NAME STP_info_get_state_name
#include "choose.h"

#if 0 /* for debug */
void _stp_dump(char* title, unsigned char* buff, int len)
{
	register int iii;

	printf ("\n%s:", title);
	for (iii = 0; iii < len; iii++) {
		if (!(iii % 24)) {
			Print ("\n%6d:", iii);
		}
		if (!(iii % 8)) {
			Print (" ");
		}
		Print ("%02lx", (unsigned long)buff[iii]);
	}
	Print ("\n");
}
#endif

/*! \function static RCVD_INFO_T rcvInfo(STATE_MACH_T *this)
 *  \brief Implements section 17.21.8
 */
static RCVD_INFO_T rcvInfo(STATE_MACH_T *this)
{
	int bridcmp;
	register PORT_T *port = this->owner.port;

#ifdef STP_DBG
	if (this->debug) {
		stp_trace("port->msgBpduType = %d", port->msgBpduType);
	}
#endif
	
	port->msgPortRole = (port->msgFlags & PORT_ROLE_MASK) >> PORT_ROLE_OFFS;

#ifdef STP_DBG
	if (this->debug) {
		stp_trace("Updating port->msgPortRole = %d", port->msgPortRole);
	}
#endif		
	/* See 17.21.8 Note: A configuration BPDU explicitly conveys a Designated Port Role. */
	if (BPDU_CONFIG_TYPE == port->msgBpduType) {
		port->msgPortRole = RSTP_PORT_ROLE_DESGN;
	}
	/*
	 * Returns SuperiorDesignatedInfo if:
	 *   a) The received message conveys a Designated Port Role, and
	 *      1) The message priority is superior (17.6) to the Port's port priority vector, or
	 *      2) The message priority vector is the same as the Port's port priority vector, and any of the
	 *         received timer parameter values (msgTimes-17.19.15) differ from those already held for the
	 *         Port (portTimes-17.19.22).
	 */
	if (RSTP_PORT_ROLE_DESGN == port->msgPortRole) {
		bridcmp = STP_VECT_compare_vector (&port->msgPriority, &port->portPriority);
		if (bridcmp < 0 ||
		    (!STP_VECT_compare_bridge_id (&port->msgPriority.design_bridge,
						  &port->portPriority.design_bridge) &&
		    port->msgPriority.design_port == port->portPriority.design_port &&
		    STP_compare_times (&port->msgTimes, &port->portTimes))) {
#ifdef STP_DBG
			if (this->debug) {
				stp_trace ("SuperiorDesignatedInfo:bridcmp=%d", (int)bridcmp);
			}
#endif
			return SuperiorDesignatedInfo;
		}
	}

	/*
	 * Returns RepeatedDesignatedInfo if:
	 *   b) The received message conveys Designated Port Role, and a message priority vector and timer
	 *      parameters that are the same as the Port's port priority vector or timer values.
	 */
	if (RSTP_PORT_ROLE_DESGN == port->msgPortRole &&
	    (!STP_VECT_compare_vector (&port->msgPriority,
				       &port->portPriority) &&
	     !STP_compare_times (&port->msgTimes, &port->portTimes))) {
#ifdef STP_DBG
		if (this->debug) {
			stp_trace ("%s", "RepeatedDesignatedInfo");
		}
#endif
		return RepeatedDesignatedInfo;
	}

	/*
	 * Returns InferiorDesignatedInfo if:
	 *   c) The received message conveys a Designated Port Role, and a message priority vector that is worse
	 *      than the Port's port priority vector.
	 */
	if (RSTP_PORT_ROLE_DESGN == port->msgPortRole &&
	    STP_VECT_compare_vector(&port->msgPriority,
				    &port->portPriority) > 0) {
#ifdef STP_DBG
		if (this->debug) {
			stp_trace ("%s", "InferiorDesignatedInfo");
		}
#endif
		return InferiorDesignatedInfo;
	}
 
	/*
	 * Returns InferiorRootAlternateInfo if:
	 *   d) The received message conveys a Root Port, Alternate Port, or Backup Port Role and a message
	 *      priority that is the same as or worse than the port priority vector.
	 */
	if ((RSTP_PORT_ROLE_ROOT == port->msgPortRole || 
	     RSTP_PORT_ROLE_ALTBACK == port->msgPortRole) &&
	    STP_VECT_compare_vector (&port->msgPriority,
				     &port->portPriority) >= 0) {
#ifdef STP_DBG
		if (this->debug) {
			stp_trace ("%s", "InferiorRootAlternateInfo");
		}
#endif
		return InferiorRootAlternateInfo;
	}

#ifdef STP_DBG
	if (this->debug) {
		stp_trace ("%s", "OtherInfo");
	}
#endif
	return OtherInfo;
}

/*! \function static Bool betterorsameinfo(STATE_MACH_T *this)
 *  \brief Implements 17.21.1
 */
static Bool betterorsameinfo(STATE_MACH_T *this)
{
	register PORT_T *port = this->owner.port;

	if (port->newInfo == Received && port->infoIs == Received &&
	    STP_VECT_compare_vector (&port->msgPriority, &port->portPriority) <= 0) {
		return True;
	}
	
	if (port->newInfo == Mine && port->infoIs == Mine &&
	    STP_VECT_compare_vector (&port->designatedPriority, &port->portPriority) <= 0) {
		return True;
	}
	return False;
}

/*! \function static void recordAgreement(STATE_MACH_T *this)
 *  \brief Implements 17.12.9
 *  If rstpVersion is TRUE, operPointToPointMAC (6.4.3) is TRUE,
 *  and the received Configuration Message has the Agreement flag set,
 *  the agreed flag is set and the proposing flag is cleared.
 *  Otherwise, the agreed flag is cleared.
 */
static void recordAgreement(STATE_MACH_T *this)
{
	register PORT_T *port = this->owner.port;

	if (port->owner->rstpVersion && 
	    port->operPointToPointMac &&
	    AGREEMENT_BIT & port->msgFlags) {
		port->agreed = True;
		port->proposing = False;
	} else {
		port->agreed = False;
	}	
}

/*! \function static void recordDisputed(STATE_MACH_T *this)
 *  \brief Implements 17.12.10
 *  If an RST BPDU with the learning flag set has been received:
 *    a) The agreed flag is set; and
 *    b) The proposing flag is cleared.
 */
static void recordDispute(STATE_MACH_T *this)
{
	register PORT_T *port = this->owner.port;
	
	if (LEARN_BIT & port->msgFlags) {
		port->agreed = True;
		port->proposing = False;
	}
}

/*! \function static void recordProposal(STATE_MACH_T *this)
 *  \brief Implements 17.21.11
 *  If the received Configuration Message conveys a Designated Port Role,
 *  and has the Proposal flag is set, the proposed flag is set.
 *  Otherwise, the proposed flag is not changed.
 */
static void recordProposal(STATE_MACH_T *this)
{
	register PORT_T *port = this->owner.port;

	if (RSTP_PORT_ROLE_DESGN == port->msgPortRole &&
	    (PROPOSAL_BIT & port->msgFlags)) {
		port->proposed = True;
	}
}

/*! \function static Bool recordPriority(STATE_MACH_T *this)
 *  \brief Implements 17.21.12
 *  Sets the components of the portPriorityrity variable to the values
 *  of the corresponding msgPriority components.
 */
static void recordPriority(STATE_MACH_T *this)
{
	register PORT_T *port = this->owner.port;

	STP_VECT_copy(&port->portPriority, &port->msgPriority);
}

/*! \function static Bool recordTimes(STATE_MACH_T *this)
 *  \brief Implements 17.21.13
 *  Sets portTimes' Message Age, MaxAge, and Forward Delay to the
 *  received values held in the messageTimes parameter. and portTimes'
 *  Hello time to messageTimes' HelloTime if that is greater than the
 *  minimum specified in the Compatibility Range column of Table 17-1,
 *  and to that minimum otherwise.
 */
static void recordTimes(STATE_MACH_T *this)
{
	register PORT_T *port = this->owner.port;

	STP_copy_times(&port->portTimes, &port->msgTimes);
}

/*! \function static static void setTcFlags(STATE_MACH_T *this)
 *  \brief Implements 17.21.17
 */
static void setTcFlags(STATE_MACH_T *this)
{
	register PORT_T *port = this->owner.port;

	if (BPDU_TOPO_CHANGE_TYPE == port->msgBpduType) {
#ifdef STP_DBG
		if (this->debug) {
			stp_trace ("port %s rx rcvdTcn", port->port_name);
		}
#endif
		port->rcvdTcn = True;
	} else {
		if (TOPOLOGY_CHANGE_BIT & port->msgFlags) {
#ifdef STP_DBG
			if (this->debug) {
				stp_trace("(%s-%s) rx rcvdTc 0X%lx",
					  port->owner->name, port->port_name,
					  (unsigned long) port->msgFlags);
			}
#endif
			port->rcvdTc = True;
		}
		if (TOPOLOGY_CHANGE_ACK_BIT & port->msgFlags) {
#ifdef STP_DBG
			if (this->debug) {
				stp_trace("port %s rx rcvdTcAck 0X%lx",
					  port->port_name,
					  (unsigned long) port->msgFlags);
			}
#endif
			port->rcvdTcAck = True;
		}
	}
}

/*! \function static void updtBPDUVersion(STATE_MACH_T *this)
 *  \brief Implements 17.21.22
 *  Sets rcvdSTP TRUE if the BPDU received is a version 0 or version 1 TCN
 *  or a Config BPDU. Sets rcvdRSTP TRUE if the received BPDU is an RST BPDU.
 */
Bool updtBPDUVersion(STATE_MACH_T *this)
{
	register PORT_T *port = this->owner.port;

	if (BPDU_TOPO_CHANGE_TYPE == port->msgBpduType) {
		port->rcvdSTP = True;
	}

	if (port->msgBpduVersion < 2) {
		port->rcvdSTP = True;
	}
  
	if (BPDU_RSTP == port->msgBpduType) {
		/* port->port->owner->ForceVersion >= NORMAL_RSTP
		   we have checked in STP_info_rx_bpdu */
		port->rcvdRSTP = True;
	}

	return True;
}

/*! \function static Bool updtRcvdInfoWhile (STATE_MACH_T *this)
 *  \brief Implements 17.21.23
 */
static Bool updtRcvdInfoWhile(STATE_MACH_T *this)
{
	register int eff_age, dm, dt;
	register int hello3;
	register PORT_T *port = this->owner.port;
  
	eff_age = ( + port->portTimes.MaxAge) / 16;
	if (eff_age < 1) {
		eff_age = 1;
	}
	eff_age += port->portTimes.MessageAge;

	if (eff_age <= port->portTimes.MaxAge) {
		hello3 = 3 *  port->portTimes.HelloTime;
		dm = port->portTimes.MaxAge - eff_age;
		if (dm > hello3) {
			dt = hello3;
		} else {
			dt = dm;
		}
		port->rcvdInfoWhile = dt;
#if 0
		stp_trace("ma=%d eff_age=%d dm=%d dt=%d p=%s",
			  (int) port->portTimes.MessageAge,
			  (int) eff_age, (int) dm, (int) dt, port->port_name);
#endif
	} else {
		port->rcvdInfoWhile = 0;
/****/
#ifdef STP_DBG
		/*if (this->debug) */
		{
			stp_trace("port %s: MaxAge=%d MessageAge=%d HelloTime=%d rcvdInfoWhile=null !",
				  port->port_name,
				  (int) port->portTimes.MaxAge,
				  (int) port->portTimes.MessageAge,
				  (int) port->portTimes.HelloTime);
		}
#endif
/****/
	}

	return True;
}


void STP_info_rx_bpdu(PORT_T *port, struct stp_bpdu_t *bpdu, size_t len)
{  
#if 0
	_stp_dump ("\nall BPDU", ((unsigned char*) bpdu) - 12, len + 12);
	_stp_dump ("ETH_HEADER", (unsigned char*) &bpdu->eth, 5);
	_stp_dump ("BPDU_HEADER", (unsigned char*) &bpdu->hdr, 4);
	printf("protocol=%02x%02x version=%02x bpdu_type=%02x\n",
		bpdu->hdr.protocol[0], bpdu->hdr.protocol[1],
		bpdu->hdr.version, bpdu->hdr.bpdu_type);

	_stp_dump ("\nBPDU_BODY", (unsigned char*) &bpdu->body, sizeof (BPDU_BODY_T) + 2);
	printf ("flags=%02x\n", bpdu->body.flags);
	_stp_dump ("root_id", bpdu->body.root_id, 8);
	_stp_dump ("root_path_cost", bpdu->body.root_path_cost, 4);
	_stp_dump ("bridge_id", bpdu->body.bridge_id, 8);
	_stp_dump ("portId", bpdu->body.portId, 2);
	_stp_dump ("message_age", bpdu->body.message_age, 2);
	_stp_dump ("max_age", bpdu->body.max_age, 2);
	_stp_dump ("hello_time", bpdu->body.hello_time, 2);
	_stp_dump ("forward_delay", bpdu->body.forward_delay, 2);
	_stp_dump ("ver_1_len", bpdu->ver_1_len, 2);
#endif

	/* check bpdu type */
	switch (bpdu->hdr.bpdu_type) {
		case BPDU_CONFIG_TYPE:
			port->rx_cfg_bpdu_cnt++;
#ifdef STP_DBG
			if (port->info->debug) {
				stp_trace("CfgBpdu on port %s", port->port_name);
			}
#endif
			if (port->admin_non_stp) {
				return;
			}
			port->rcvdBPDU = True;
			break;
		case BPDU_TOPO_CHANGE_TYPE:
			port->rx_tcn_bpdu_cnt++;
#ifdef STP_DBG
			if (port->info->debug) {
				stp_trace("TcnBpdu on port %s", port->port_name);
			}
#endif
			if (port->admin_non_stp) {
				return;
			}
			port->rcvdBPDU = True;
			port->msgBpduVersion = bpdu->hdr.version;
			port->msgBpduType = bpdu->hdr.bpdu_type;
			return;
		default:
			stp_trace ("RX undef bpdu type=%d", (int) bpdu->hdr.bpdu_type);
			return;
		case BPDU_RSTP:
			port->rx_rstp_bpdu_cnt++;
			if (port->admin_non_stp) {
				return;
			}
			if (port->owner->ForceVersion >= NORMAL_RSTP) {
				port->rcvdBPDU = True;
			} else {
				return;
			}
#if 0 /* def STP_DBG */
			if (port->info->debug) {
				stp_trace("BPDU_RSTP on port %s", port->port_name);
			}
#endif
			break;
	}

	port->msgBpduVersion = bpdu->hdr.version;
	port->msgBpduType =    bpdu->hdr.bpdu_type;
	port->msgFlags =       bpdu->body.flags;

	/* 17.18.11 */
	STP_VECT_get_vector(&bpdu->body, &port->msgPriority);
	port->msgPriority.bridge_port = port->portId;

	/* 17.18.12 */
	STP_get_times(&bpdu->body, &port->msgTimes);

	/* 17.18.25, 17.18.26 : see setTcFlags() */
}

/*! \function void STP_info_enter_state(STATE_MACH_T *this)
 *  \brief Implements
 */
void STP_info_enter_state(STATE_MACH_T *this)
{
	register PORT_T *port = this->owner.port;

	switch (this->State) {
		case BEGIN:
			port->msgBpduType = -1;
			port->msgPortRole = RSTP_PORT_ROLE_UNKN;
			port->msgFlags = 0;

			/* clear port statistics */
			port->rx_cfg_bpdu_cnt = 0;
			port->rx_rstp_bpdu_cnt = 0;
			port->rx_tcn_bpdu_cnt = 0;
      
		case DISABLED:
			port->rcvdMsg = False;
			port->proposing = port->proposed = port->agree = port->agreed = False;
			port->rcvdInfoWhile = 0;
			port->infoIs = Disabled;
			port->reselect = True;
			port->selected = False;
			break;
		case AGED:
			port->infoIs = Aged;
			port->reselect = True;
			port->selected = False;
			break;
		case UPDATE:
			port->proposing = port->proposed = False;
			port->agreed = port->agreed && betterorsameinfo(this);
			port->synced = port->synced && port->agreed;
			
			STP_VECT_copy(&port->portPriority, &port->designatedPriority);
			STP_copy_times(&port->portTimes, &port->designatedTimes);
			
			port->updtInfo = False;
			port->infoIs = Mine;
			port->newInfo = True;
#ifdef STP_DBG
			if (this->debug) {
				STP_VECT_br_id_print("updated: portPriority.design_bridge",
						     &port->portPriority.design_bridge, True);
			}
#endif
			break;
		case CURRENT:
			break;
		case RECEIVE:
			port->rcvdInfo = rcvInfo (this);
			break;
		case SUPERIOR_DESIGNATED:
			port->agreed = port->proposing = False;
			recordProposal(this);
			setTcFlags(this);
			port->agree = port->agree && betterorsameinfo(this);
			recordPriority(this);
			recordTimes(this);
			updtRcvdInfoWhile(this);
			port->infoIs = Received;
			port->reselect = True;
			port->selected = False;
			port->rcvdMsg = False;
			break;
		case REPEATED_DESIGNATED:
			recordProposal(this);
			setTcFlags(this);
			updtRcvdInfoWhile(this);
			port->rcvdMsg = False;
			break;
		case INFERIOR_DESIGNATED:
			recordDispute(this);
			port->rcvdMsg = False;
			break;
		case NOT_DESIGNATED:
			recordAgreement(this);
			setTcFlags(this);
			port->rcvdMsg = False;
			break;
		case OTHER:
			port->rcvdMsg = False;
			break;
	}
}

/*! \function Bool STP_info_check_conditions(STATE_MACH_T *this)
 *  \brief Implements
 */
Bool STP_info_check_conditions(STATE_MACH_T *this)
{
	register PORT_T *port = this->owner.port;

	if ((!port->portEnabled && port->infoIs != Disabled) || BEGIN == this->State) {
		return STP_hop_2_state(this, DISABLED);
	}

	switch (this->State) {
		case DISABLED:
			if (port->portEnabled) {
				return STP_hop_2_state(this, AGED);
			}
			if (port->rcvdMsg) {
				return STP_hop_2_state(this, DISABLED);
			}
			break;
		case AGED:
			if (port->selected && port->updtInfo) {
				return STP_hop_2_state(this, UPDATE);
			}
			break;
		case UPDATE:
			return STP_hop_2_state(this, CURRENT);
			break;
		case CURRENT:
			if (port->selected && port->updtInfo) {
				return STP_hop_2_state(this, UPDATE);
			}

			if (Received == port->infoIs &&
			    !port->rcvdInfoWhile &&
			    !port->updtInfo &&
			    !port->rcvdMsg) {
				return STP_hop_2_state(this, AGED);
			}
			if (port->rcvdMsg && !port->updtInfo) {
				return STP_hop_2_state(this, RECEIVE);
			}
			break;
		case RECEIVE:
			switch (port->rcvdInfo) {
				case SuperiorDesignatedInfo:
					return STP_hop_2_state(this, SUPERIOR_DESIGNATED);
				case RepeatedDesignatedInfo:
					return STP_hop_2_state(this, REPEATED_DESIGNATED);
				case InferiorDesignatedInfo:
					return STP_hop_2_state(this, INFERIOR_DESIGNATED);
				case InferiorRootAlternateInfo:
					return STP_hop_2_state(this, NOT_DESIGNATED);
				case OtherInfo:
					return STP_hop_2_state(this, OTHER);
				default:
					return STP_hop_2_state(this, CURRENT);
			}
			break;
		case SUPERIOR_DESIGNATED:
		case REPEATED_DESIGNATED:
		case INFERIOR_DESIGNATED:
		case NOT_DESIGNATED:
		case OTHER:
			return STP_hop_2_state(this, CURRENT);
			break;
	}

	return False;
}
