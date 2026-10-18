/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# Copyright 2001-2009, ps2dev - http://www.ps2dev.org
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
# Defines all IRX imports.
*/

#ifndef IOP_IRX_IMPORTS_H
#define IOP_IRX_IMPORTS_H

#include <irx.h>

#include <intrman.h>
#include <ioman.h>
#include <ps2ip.h>
#include <sifman.h>
#include <stdio.h>
#include <sysclib.h>
#include <thbase.h>
#include <thsemap.h>

/* SMS embeds ps2ip 1.3. The pinned SDK header declares 2.6 by default.
 * Keep the exact older provider/ordinal binding used by the working IRX.
 */
#undef ps2ip_IMPORTS_start
#define ps2ip_IMPORTS_start DECLARE_IMPORT_TABLE(ps2ip, 1, 3)
#undef I_inet_addr
#define I_inet_addr DECLARE_IMPORT(24, ipaddr_addr)

#endif
