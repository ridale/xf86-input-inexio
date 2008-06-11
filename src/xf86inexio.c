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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef TS_Raw
#define TS_Raw                  57
#endif

#ifndef TS_Scaled
#define TS_Scaled               58
#endif

#include <X11/extensions/XIproto.h>
#include <xf86.h>
#include <xf86_OSlib.h>
#include <xf86Xinput.h>

#include <xf86inexio.h>

/* module */
static void     InexioUnplug(pointer p);
static pointer  InexioPlug(pointer        module,
                           pointer        options,
                           int            *errmaj,
                           int            *errmin);

/* driver */
static InputInfoPtr InexioPreInit(InputDriverPtr  drv, 
                                  IDevPtr         dev, 
                                  int             flags);

static void         InexioUnInit(InputDriverPtr   drv,
                                 InputInfoPtr     pInfo,
                                 int              flags);
static void         InexioReadInput(InputInfoPtr  pInfo);
static int          InexioControl(DeviceIntPtr    device,
                                  int             what);

/* internal */
static int _inexio_init_buttons(DeviceIntPtr device);
static int _inexio_init_axes(DeviceIntPtr device);

/**
 * Driver Rec, fields are used when device is initialised/removed.
 */
_X_EXPORT InputDriverRec INEXIO = {
    1,
    "inexio",
    NULL,
    InexioPreInit,
    InexioUnInit,
    NULL,
    0
};

/**
 * Module versioning information.
 */
static XF86ModuleVersionInfo InexioVersionRec =
{
    "inexio",
    MODULEVENDORSTRING,
    MODINFOSTRING1,
    MODINFOSTRING2,
    XORG_VERSION_CURRENT,
    1, 0, 0,
    ABI_CLASS_XINPUT,
    ABI_XINPUT_VERSION,
    MOD_CLASS_XINPUT,
    {0, 0, 0, 0}
};


/**
 * Module control. Fields are used when module is loaded/unloaded.
 */
_X_EXPORT XF86ModuleData inexioModuleData =
{
    &InexioVersionRec,
    InexioPlug,
    InexioUnplug
};

/***************************************************************************
 *                            Module procs                                 *
 ***************************************************************************/

/**
 * Called when module is unloaded.
 */
static void
InexioUnplug(pointer p)
{
}

/**
 * Called when module is loaded.
 */
static pointer
InexioPlug(pointer        module,
           pointer        options,
           int            *errmaj,
           int            *errmin)
{
    xf86AddInputDriver(&INEXIO, module, 0);
    return module;
}

/***************************************************************************
 *                            Driver procs                                 *
 ***************************************************************************/

/**
 * Called each time a new device is to be initialized.
 * Init your structs, sockets, whatever else you need to do.
 */
static InputInfoPtr InexioPreInit(InputDriverPtr  drv, 
                                  IDevPtr         dev, 
                                  int             flags)
{
    InputInfoPtr        pInfo;
    InexioDevicePtr     pInexio;
    
    if (!(pInfo = xf86AllocateInput(drv, 0)))
        return NULL;

    pInexio = xcalloc(1, sizeof(InexioDeviceRec));
    if (!pInexio)
    {
        pInfo->private = NULL;
        xf86DeleteInput(pInfo, 0);
        return NULL;
    }

    pInfo->private      = pInexio;
	// TODO add the private data

    pInfo->name         = xstrdup(dev->identifier);
    pInfo->flags        = 0;
    pInfo->type_name    = XI_TOUCHSCREEN; /* see XI.h */
    pInfo->conf_idev    = dev;
    pInfo->read_input   = InexioReadInput; /* new data avl */
    pInfo->switch_mode  = NULL;            /* toggle absolute/relative mode */
    pInfo->device_control = InexioControl; /* enable/disable dev */


    /* process driver specific options */
    pInexio->device = xf86CheckStrOption(dev->commonOptions, 
                                         "Device",
                                         "/dev/ttyS0");
    xf86Msg(X_INFO, "%s: Using device %s.\n", pInfo->name, pInexio->device);

    pInexio->inexXSize = xf86CheckIntOption(dev->commonOptions, 
                                         "MaxX",
                                         INEXIO_MAX_X); 		// want the calibration x max
    pInexio->inexXOffset = xf86CheckIntOption(dev->commonOptions, 
                                         "MinX",
                                         INEXIO_MIN_X);		// want the calibration x min
    pInexio->inexYSize = xf86CheckIntOption(dev->commonOptions, 
                                         "MaxY",
                                         INEXIO_MAX_Y); 		// want the calibration y max
    pInexio->inexYOffset = xf86CheckIntOption(dev->commonOptions, 
                                         "MinY",
                                         INEXIO_MIN_Y);		// want the calibration y min

    pInexio->inexScreenNo = xf86CheckIntOption(dev->commonOptions, 
                                         "ScreenNumber",
                                         0);		// want the screen we are attached to

    pInexio->inexButtonNo = xf86CheckIntOption(dev->commonOptions, 
                                         "ButtonNumber",
                                         0);		// want the screen we are attached to

	char* s = xf86CheckStrOption(dev->commonOptions, 
                                         "ReportingMode",
                                         NULL);
	if ((s) && (xf86NameCmp(s,"raw") == 0))
		pInexio->inexReportMode = TS_Raw;
	else
		pInexio->inexReportMode = TS_Scaled;

	//	pInexio->inexRotate = ;		// want the rotate state CW or CCW

    /* process generic options */
    xf86CollectInputOptions(pInfo, NULL, NULL);
    xf86ProcessCommonOptions(pInfo, pInfo->options);


    /* Open sockets, init device files, etc. */
    SYSCALL(pInfo->fd = open(pInexio->device, O_RDWR | O_NONBLOCK));
    if (pInfo->fd == -1)
    {
        xf86Msg(X_ERROR, "%s: failed to open %s.\n", 
                pInfo->name, pInexio->device);
        pInfo->private = NULL;
        xfree(pInexio);
        xf86DeleteInput(pInfo, 0);
        return NULL;
    }
    /* do more initialization */
	pInexio->inexMaxX = INEXIO_MAX_X;
	pInexio->inexMaxY = INEXIO_MAX_Y;
    pInexio->inexOldButton = INEXIO_BUTTON_UP;
	
    /* close file again */
    close (pInfo->fd);
    pInfo->fd = -1;

    /* set the required flags */
    pInfo->flags |= XI86_OPEN_ON_INIT;
    pInfo->flags |= XI86_CONFIGURED;

    return pInfo;
}

/**
 * Called each time a device is to be removed.
 * Clean up your mess here.
 */
static void InexioUnInit(InputDriverPtr drv,
                       InputInfoPtr   pInfo,
                       int            flags)
{
    InexioDevicePtr       pInexio = pInfo->private;

    if (pInexio->device)
    {
        xfree(pInexio->device);
        pInexio->device = NULL;
    }

    xfree(pInexio);
    xf86DeleteInput(pInfo, 0);
}

/**
 * Called when data is available on the socket.
 */
static void InexioReadInput(InputInfoPtr pInfo)
{
	// TODO need to read the entire record and deal
	// with touch and position
	/*
	 * format touch down  0x81 XH XL YH YL
	 * format touch up    0x80 XH XL YH YL
	 * where where XL,XH,YL,YH 7bit
	 * X Coord = XL + XH<<7;
	 * Y Coord = YL + YH<<7;
	 * X max = Y max = 0x3FFF (16383)
	 */

    InexioDevicePtr       pInexio = pInfo->private;
	
    unsigned char data[4], button_data;
	int x = 0, y = 0;
    int is_absolute = TRUE, num_ax = 2;

    while(xf86WaitForInput(pInfo->fd, 0) > 0)
    {
		// 1st get the button press
        read(pInfo->fd, &button_data, 1);
		if ((button_data != 0x80) && (button_data != 0x81))
			continue;  // not a button event, go again
		
		// now read the data
        read(pInfo->fd, &data, 4);
		// now we need to normalize the data
		x = data[0]<<7 + data[1];
		y = data[2]<<7 + data[3];
		// do offsets and scales
		// TODO
		if (pInexio->inexReportMode == TS_Scaled)
		{
			x = xf86ScaleAxis(x, 0, 
								pInexio->inexScreenW,
								pInexio->inexXOffset,
								pInexio->inexXSize);
			y = xf86ScaleAxis(y, 0, 
								pInexio->inexScreenH,
								pInexio->inexYOffset,
								pInexio->inexYSize);
		}

		if (pInexio->inexRotate != 0)	// swap X & Y
		{
			int tmp = x; x = y; y = tmp;
		}

		xf86XInputSetScreen(pInfo, pInexio->inexScreenNo, x, y);
		// do swaps and inverts
		// TODO
		assert(0);
		// now we can send the event on
        xf86PostMotionEvent(pInfo->dev,
                            is_absolute, 	/* is_absolute		*/
                            0, 				/* first_valuator	*/
                            num_ax, 		/* number of axis	*/
                            x,y); 			/* the data			*/
		// button changed code
		if (pInexio->inexOldButton != button_data)
		{
			int is_down = (button_data == INEXIO_BUTTON_DOWN)? 1 : 0;
			pInexio->inexOldButton = button_data; // set old to new state
			xf86PostButtonEvent(pInfo->dev, is_absolute, pInexio->inexButtonNo,
				       is_down, 0, num_ax, x, y );
		}
    }
}

/**
 * Called when the device is to be enabled/disabled, etc.
 * @return Success or X error code.
 */
static int InexioControl(DeviceIntPtr    device,
                                int             what)
{
    InputInfoPtr  pInfo = device->public.devicePrivate;
    InexioDevicePtr pInexio = pInfo->private;

    switch(what)
    {
        case DEVICE_INIT:
			_inexio_init_buttons(device);
            _inexio_init_axes(device);
            break;
        /* Switch device on. Establish socket, start event delivery.  */
        case DEVICE_ON:
            xf86Msg(X_INFO, "%s: On.\n", pInfo->name);
            if (device->public.on)
                break;

            SYSCALL(pInfo->fd = open(pInexio->device, O_RDONLY | O_NONBLOCK));
            if (pInfo->fd < 0)
            {
                xf86Msg(X_ERROR, "%s: cannot open device.\n", pInfo->name);
                return BadRequest;
            }

            xf86FlushInput(pInfo->fd);
            xf86AddEnabledDevice(pInfo);
            device->public.on = TRUE;
            break;
        /**
         * Shut down device.
         */
        case DEVICE_OFF:
            xf86Msg(X_INFO, "%s: Off.\n", pInfo->name);
            if (!device->public.on)
                break;
            xf86RemoveEnabledDevice(pInfo);
            pInfo->fd = -1;
            device->public.on = FALSE;
            break;
        case DEVICE_CLOSE:
            /* free what we have to free */
            break;
    }
    return Success;
}


/***************************************************************************
 *                            internal procs                               *
 ***************************************************************************/

/**
 * Init the button map for the random device.
 * @return Success or X error code on failure.
*/
static int
_inexio_init_buttons(DeviceIntPtr device)
{
    InputInfoPtr        pInfo = device->public.devicePrivate;
    CARD8               *map;
    int                 i;
    const int           num_buttons = 1;
    int                 ret = Success;

    map = xcalloc(num_buttons, sizeof(CARD8));

    for (i = 0; i < num_buttons; i++)
        map[i] = i;

    if (!InitButtonClassDeviceStruct(device, num_buttons, map)) {
        xf86Msg(X_ERROR, "%s: Failed to register buttons.\n", pInfo->name);
        ret = BadAlloc;
    }

    xfree(map);
    return ret;
}


/**
 * Init the valuators for the random device.
 * Only absolute mode is supported.
 * @return Success or X error code on failure.
*/
static int
_inexio_init_axes(DeviceIntPtr device)
{
    InputInfoPtr        pInfo = device->public.devicePrivate;
    InexioDevicePtr pInexio = pInfo->private;
    int                 i;
    const int           num_axes = 2;


    if (!InitValuatorClassDeviceStruct(device,
                                       num_axes,
                                       xf86GetMotionEvents,
                                       pInfo->history_size,
                                       Absolute))
        return BadAlloc;


	InitValuatorAxisStruct (device, 0, pInexio->inexXOffset, pInexio->inexXSize,
								INEXIO_MAX_X);
	InitValuatorAxisStruct (device, 1, pInexio->inexYOffset, pInexio->inexYSize,
								INEXIO_MAX_Y);

    return Success;
}

