//  $Id: extern.h,v 1.1 2002/10/09 17:56:58 ballen4705 Exp $
/*
 * extern.h
 *
 * Copyright (C) 2002 Bruce Allen <ballen@uwm.edu>
 * Copyright (C) 1999-2000 Michael Cornwell <cornwell@acm.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * (for example COPYING); if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _EXTERN_H_
#define _EXTERN_H_

extern unsigned char driveinfo;
extern unsigned char checksmart;
extern unsigned char smartvendorattrib;
extern unsigned char generalsmartvalues;
extern unsigned char smartselftestlog;
extern unsigned char smarterrorlog;
extern unsigned char smartdisable;
extern unsigned char smartenable; 
extern unsigned char smartstatus;
extern unsigned char smartexeoffimmediate;
extern unsigned char smartshortselftest;
extern unsigned char smartextendselftest;
extern unsigned char smartshortcapselftest;
extern unsigned char smartextendcapselftest;
extern unsigned char smartselftestabort;
extern unsigned char smartautoofflineenable;
extern unsigned char smartautoofflinedisable;
extern unsigned char smartautosaveenable;
extern unsigned char smartautosavedisable;

#endif
