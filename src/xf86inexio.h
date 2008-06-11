/*
 * Copyright © 2008 Richard Lemon
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Soft-
 * ware"), to deal in the Software without restriction, including without
 * limitation the rights to use, copy, modify, merge, publish, distribute,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, provided that the above copyright
 * notice(s) and this permission notice appear in all copies of the Soft-
 * ware and that both the above copyright notice(s) and this permission
 * notice appear in supporting documentation.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABIL-
 * ITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY
 * RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS INCLUDED IN
 * THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT OR CONSE-
 * QUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFOR-
 * MANCE OF THIS SOFTWARE.
 *
 * Except as contained in this notice, the name of a copyright holder shall
 * not be used in advertising or otherwise to promote the sale, use or
 * other dealings in this Software without prior written authorization of
 * the copyright holder.
 *
 * Authors:
 *   Richard Lemon (richard@codelemon.com)
 *   Taken in large part from Peter Hutterer's (peter@cs.unisa.edu.au)
 *   Random example driver and Steven Lang's <tiger@tyger.org> summa 
 *   tablet driver.
 */

#ifndef __INEXIO_H__
#define __INEXIO_H__ 1

#define _XF86_ANSIC_H
#define XF86_LIBC_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <X11/extensions/XI.h>
#include <xf86Xinput.h>

#define INEXIO_MIN_X 0x0000
#define INEXIO_MIN_Y 0x0000
#define INEXIO_MAX_X 0x3FFF
#define INEXIO_MAX_Y 0x3FFF
#define INEXIO_BUTTON_DOWN 0x81
#define INEXIO_BUTTON_UP   0x80

/**
 * This internal struct is used by our driver.
 * For more sophisticated drivers, it would probably include a great deal
 * more...
 */
typedef struct _InexioDevice {
    char        *device;		/* device filename */
	
    int			inexOldButton;	/* previous button state */
	int 		inexButtonNo;	/* the button number we report */
	int			inexReportMode;	/* the reporting mode (raw or scaled) */

	int 		inexScreenNo;	/* the screen we are attaached to */
	int 		inexScreenW;	/* the screen width */
	int 		inexScreenH;	/* the screen height */

	int 		inexMaxX;		/* max X value */
	int 		inexMaxY;		/* max Y value */
    int			inexXSize;		/* active area X size */
    int			inexXOffset;	/* active area X offset */
    int			inexYSize;		/* active area Y size */
    int			inexYOffset;	/* active area Y offset */
	int			inexRotate;		/* rotate screen axis */
} InexioDeviceRec, *InexioDevicePtr;

#endif /* __RANDOM_H__ */
