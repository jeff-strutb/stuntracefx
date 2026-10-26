/*
 * routines.h: the routine tables tools/routines.py generates (gen/
 * routines.c), in nativeshvc's types: the same fields in the same order.
 */
#ifndef SRFX_ROUTINES_H
#define SRFX_ROUTINES_H

#include "nshvc/nshvc.h"

typedef nshvc_routine PortRoutine;
typedef nshvc_spc_use PortSpcUse;
typedef nshvc_spc_routine PortSpcRoutine;

extern const PortRoutine port_routines[];
extern const int port_routine_count;
extern const PortSpcRoutine port_spc_routines[];
extern const int port_spc_routine_count;

#endif
