#************************************************************************
#* RSTP library - Rapid Spanning Tree (802.1D-2004)
#* Copyright (C) 2001-2003 Optical Access
#* Author: Alex Rozin
#*
#* This file is part of RSTP library.
#*
#* RSTP library is free software; you can redistribute it and/or modify it
#* under the terms of the GNU Lesser General Public License as published by the
#* Free Software Foundation; version 2.1
#*
#* RSTP library is distributed in the hope that it will be useful, but
#* WITHOUT ANY WARRANTY; without even the implied warranty of
#* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
#* General Public License for more details.
#*
#* You should have received a copy of the GNU Lesser General Public License
#* along with RSTP library; see the file COPYING.  If not, write to the Free
#* Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
#* 02111-1307, USA.
#**********************************************************************/

DEFS=
CC = gcc #-E

DEBUG= -ggdb #-fno-stack-protector -fno-pic -fno-pie -nopie
#CFLAGS = -g -Wall -D_REENTRANT -D__LINUX__ -DSTP_DBG=1 -DOLD_READLINE
CFLAGS = -Wall -D_REENTRANT -D__LINUX__ -DSTP_DBG=1 $(DEBUG) #-DORIG=1

INCLUDES = -I.
COMPILE = $(CC) $(DEFS) $(INCLUDES) $(CFLAGS)
LIBS= -lutil -lreadline -ltermcap -lpthread

AR = ar
#ARFLAGS = rcs
ARFLAGS = cru
RANLIB = ranlib
SOFLAGS =  -nostartfiles -shared

EXECUTABLE= bridge mngr
RSTPLIBS = rstp
RSTPLIBF = lib$(RSTPLIBS).a
RSTPLIBSO = lib$(RSTPLIBS).so
UIDLIBS  = uid
UIDLIBF  = lib$(UIDLIBS).a
CLILIBS  = cli
CLILIBF  = lib$(CLILIBS).a

MNGRCFILES = mngr.c
MNGROFILES = mngr.o
BRIDGECFILES = bridge.c stp_cli.c stp_to.c
BRIDGEOFILES = bridge.o stp_cli.o stp_to.o

RSTPCFILES = statmch.c stpm.c port.c stpmgmt.c stp_in.c rolesel.c \
vector.c times.c \
portinfo.c roletrns.c sttrans.c topoch.c migrate.c transmit.c \
p2p.c pcost.c receive.c brdgdet.c
RSTPOFILES = statmch.o stpm.o port.o stpmgmt.o stp_in.o rolesel.o \
vector.o times.o \
portinfo.o roletrns.o sttrans.o topoch.o migrate.o transmit.o \
p2p.o pcost.o receive.o brdgdet.o

UIDCFILES = uid_sock.c
UIDOFILES = uid_sock.o

CLICFILES = cli.c
CLIOFILES = cli.o

CFILES= $(MNGRCFILES) $(BRIDGECFILES) $(RSTPCFILES) $(UIDCFILES) $(CLICFILES)

OFILES= $(MNGROFILES) $(BRIDGEOFILES) $(RSTPOFILES) $(UIDOFILES) $(CLIOFILES)

.SUFFIXES: .S .c .o .s

all: depend $(EXECUTABLE)

mngr: ${MNGROFILES} $(UIDLIBF) $(CLILIBF)
	$(CC) $(CFLAGS) ${MNGROFILES} $(LIBS) -L. -l$(UIDLIBS) -l$(CLILIBS) -o $@

bridge: ${BRIDGEOFILES} $(RSTPLIBF) $(UIDLIBF) $(CLILIBF)
	$(CC) $(CFLAGS) ${BRIDGEOFILES} $(LIBS) -L. -l$(RSTPLIBS) -l$(UIDLIBS) -l$(CLILIBS) -o $@

$(RSTPLIBF): ${RSTPOFILES}
	-rm -f $@
	$(AR) $(ARFLAGS) $@ $(RSTPOFILES)
	$(RANLIB)  $@

$(RSTPLIBSO): ${RSTPOFILES}
	-rm -f $@
	$(CC) $(CFLAGS) -shared -Wl,-soname,$@.1 -o $@.1.0.0 $(RSTPOFILES)
	ln -s $@.1.0.0 $@.1
	ln -s $@.1 $@
	
$(UIDLIBF): ${UIDOFILES}
	-rm -f $@
	$(AR) $(ARFLAGS) $@ $(UIDOFILES)
	$(RANLIB) $@
	
$(CLILIBF): $(CLIOFILES)
	-rm -f $@
	$(AR) $(ARFLAGS) $@ $(CLIOFILES)
	$(RANLIB) $@

clean:
	rm -f all *.o *.a *.so* *~ $(EXECUTABLE) *.bak make.depend

depend:
	echo "# DO NOT DELETE THIS LINE -- make  depend  depends  on it." > make.depend
	makedepend -- $(DEFS) $(INCLUDES) $(CFLAGS) -- $(CFILES) -f make.depend

-include make.depend
