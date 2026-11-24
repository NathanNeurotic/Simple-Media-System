#ifndef IOP_IRX_IMPORTS_H
#define IOP_IRX_IMPORTS_H

#include <irx.h>

#include <ps2ip.h>
#include <sifcmd.h>
#include <sysclib.h>
#include <thbase.h>

/* SMS embeds ps2ip 1.3. The pinned SDK header declares a newer version by default.
 * Keep the provider binding the embedded stack answers to. */
#undef ps2ip_IMPORTS_start
#define ps2ip_IMPORTS_start DECLARE_IMPORT_TABLE(ps2ip, 1, 3)

#endif
