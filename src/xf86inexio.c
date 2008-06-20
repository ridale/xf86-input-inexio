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
 *	 Richard Lemon (richard@codelemon.com)
 *	 Taken in large part from Peter Hutterer's (peter@cs.unisa.edu.au)
 *	 Random example driver and Steven Lang's <tiger@tyger.org> summa
 *	 tablet driver.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <X11/extensions/XI.h>
#include <X11/extensions/XIproto.h>
#include <xf86.h>
#include <xf86_OSlib.h>
#include <xf86Xinput.h>

/* hardware specific defines */
#define INEXIO_MIN_X		0x0000
#define INEXIO_MIN_Y		0x0000
#define INEXIO_MAX_X		0x3FFF
#define INEXIO_MAX_Y		0x3FFF
#define INEXIO_BUTTON_DOWN	0x81
#define INEXIO_BUTTON_UP	0x80
#define INEXIO_BODY_LEN		0x05
#define INEXIO_BUFFER_LEN	0x100

/**
 * This internal struct is used by our driver.
 */
typedef struct _InexioDevice {
	char	*device;	/* device filename */
	
	Bool	button_down;	/* is the "button" currently down */
	int	button_number;	/* which button to report */
	int	last_x;
	int	last_y;

	int	screen_num;	/* Screen associated with the device */
	int	screen_width;	/* Width of the associated X screen */
	int	screen_height;	/* Height of the screen */

	int	min_x;		/* Minimum x reported by calibration */
	int	max_x;		/* Maximum x */
	int	min_y;		/* Minimum y reported by calibration */
	int	max_y;		/* Maximum y */

	unsigned char	buffer[INEXIO_BUFFER_LEN];/* buffer being read */
	int	bufferi;	/* index into buffer */
	unsigned char	body[16];/* packet just read */

	int	swap_axes;	/* swap x and y axis */
} InexioDeviceRec, *InexioDevicePtr;

/* module */
static void InexioUnplug(pointer p);
static pointer	InexioPlug(pointer	module,
			   pointer	options,
			   int		*errmaj,
			   int		*errmin);

/* driver */
static InputInfoPtr InexioPreInit(InputDriverPtr drv,
				  IDevPtr	dev,
				  int		flags);

static void InexioUnInit(InputDriverPtr	drv,
			   InputInfoPtr pInfo,
			   int		flags);

static void InexioReadInput(InputInfoPtr pInfo);

static int InexioControl(DeviceIntPtr	device,
				int	what);

static Bool InexioConvertProc(LocalDevicePtr	local,
				int		first,
				int		num,
				int		v0,
				int		v1,
				int		v2,
				int		v3,
				int		v4,
				int		v5,
				int		*x,
				int		*y);

/* internal */
static int _inexio_init_buttons(DeviceIntPtr device);
static int _inexio_init_axes(DeviceIntPtr device);
static int _inexio_read_packet(InexioDevicePtr priv);

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
	PACKAGE_VERSION_MAJOR,
	PACKAGE_VERSION_MINOR,
	PACKAGE_VERSION_PATCHLEVEL,
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
 *			  Module procs					   *
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
InexioPlug(pointer	module,
	   pointer	options,
	   int		*errmaj,
	   int		*errmin)
{
	xf86AddInputDriver(&INEXIO, module, 0);
	return module;
}

/***************************************************************************
 *			  Driver procs					   *
 ***************************************************************************/

/**
 * Called each time a new device is to be initialized.
 * Init your structs, sockets, whatever else you need to do.
 */
static InputInfoPtr InexioPreInit(InputDriverPtr	drv,
					IDevPtr		dev,
					int		flags)
{
	InputInfoPtr	pInfo;
	InexioDevicePtr	pInexio;

	if (!(pInfo = xf86AllocateInput(drv, 0)))
		return NULL;

	pInexio = xcalloc(1, sizeof(InexioDeviceRec));
	if (!pInexio)
	{
		pInfo->private = NULL;
		xf86DeleteInput(pInfo, 0);
		return NULL;
	}

	pInfo->type_name	= XI_TOUCHSCREEN;	/* see XI.h */
	pInfo->device_control	= InexioControl;	/* enable/disable dev */
	pInfo->read_input	= InexioReadInput;	/* new data avl */
	pInfo->conversion_proc	= InexioConvertProc;	/* convert coords */
	pInfo->dev		= NULL;
	pInfo->private		= pInexio;
	pInfo->private_flags	= 0;
	pInfo->flags		= XI86_POINTER_CAPABLE | XI86_SEND_DRAG_EVENTS;
	pInfo->conf_idev		= dev;

	pInfo->name		= xstrdup(dev->identifier);
	pInfo->history_size = 0;

	/* process driver specific options */
	pInexio->device = xf86CheckStrOption(dev->commonOptions,
						"Device",
						"/dev/ttyS0");
	xf86Msg(X_INFO, "%s: Using device %s.\n", pInfo->name, pInexio->device);

	pInexio->max_x = xf86CheckIntOption(dev->commonOptions,
						"MaxX",
						INEXIO_MAX_X);	/* calibration x max */
	pInexio->min_x = xf86CheckIntOption(dev->commonOptions,
						"MinX",
						INEXIO_MIN_X);	/* calibration x min */
	pInexio->max_y = xf86CheckIntOption(dev->commonOptions,
						"MaxY",
						INEXIO_MAX_Y);	/*calibration y max */
	pInexio->min_y = xf86CheckIntOption(dev->commonOptions,
						"MinY",
						INEXIO_MIN_Y);	/* calibration y min */

	pInexio->screen_num = xf86CheckIntOption(dev->commonOptions,
							"ScreenNumber",
							0);	/* screen we are attached to */

	pInexio->button_number = xf86CheckIntOption(dev->commonOptions,
							"ButtonNumber",
							1);	/* button for touch */

	/* TODO need to add rotate option */
	pInexio->swap_axes = 0;

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
	pInexio->button_down = FALSE;
	pInexio->last_x = 0;
	pInexio->last_y = 0;
	pInexio->bufferi = 0;

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
static void InexioUnInit(InputDriverPtr	drv,
			InputInfoPtr	pInfo,
			int		flags)
{
	InexioDevicePtr pInexio = pInfo->private;

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
	/*
	 * format of 5 bytes per packet
	 * format touch down  0x81 XH XL YH YL
	 * format touch up	  0x80 XH XL YH YL
	 * where where XL,XH,YL,YH 7bit
	 * X Coord = XL + XH<<7;
	 * Y Coord = YL + YH<<7;
	 * X max = Y max = 0x3FFF (16383)
	 */

	InexioDevicePtr pInexio = pInfo->private;

	unsigned char data[4], button_data = 0, is_down = FALSE;
	int x = 0, y = 0, len = -1;
	int is_absolute = TRUE, num_ax = 2;

	while(xf86WaitForInput(pInfo->fd, 0) > 0)
	{
		if (!(INEXIO_BODY_LEN - pInexio->bufferi < 0))
			SYSCALL(pInexio->bufferi = read(pInfo->fd,
							&pInexio->buffer[pInexio->bufferi],
							INEXIO_BODY_LEN - pInexio->bufferi));
		/*xf86Msg(X_INFO, "%s: read[%i] chars.\n", pInfo->name, pInexio->bufferi);*/

		if (_inexio_read_packet(pInexio) == Success)
		{
			is_down = (pInexio->body[0] & 1);
			/*xf86Msg(X_INFO, "%s: button down[%x].\n", pInfo->name, button_data&1);*/

			/* now we need to normalize the data */
			x = pInexio->body[2] + (pInexio->body[1]<<7);
			y = pInexio->body[4] + (pInexio->body[3]<<7);
			if (is_down)
			{ /* if the button is down report position */
				pInexio->last_x = x;
				pInexio->last_y = y;
			}
			else
			{ /* if the button is up keep last position */
				x = pInexio->last_x;
				y = pInexio->last_y;
			}
			/* now we can send the event on */
			xf86PostMotionEvent(pInfo->dev,
						is_absolute,	/* is_absolute	*/
						0,		/* first_valuator*/
						num_ax,		/* number of axis*/
						x,y);		/* the data	*/

			/*xf86Msg(X_INFO, "%s: posted the motion event\n", pInfo->name);*/

			/* button changed code*/
			if (pInexio->button_down != is_down)
			{

				pInexio->button_down = is_down; /* set old to new state */
				xf86PostButtonEvent(pInfo->dev, is_absolute, pInexio->button_number,
						   is_down, 0, num_ax, x, y );
				/*xf86Msg(X_INFO, "%s: posted the button event\n", pInfo->name);*/
			}

			/* The following is a USB ack string that is sent to the
			 * device by the windows driver, doesn't seem to be required
			 * ack the packet */
/*			pInexio->body[0] = 0xaa; pInexio->body[1] = 0x02;
			SYSCALL(write(pInfo->fd, pInexio->body, 2));
*/
		}
	}
}

/**
 * Called when the device is started
 * @return Success or X error code.
 */
static int DeviceOn(DeviceIntPtr dev)
{
	InputInfoPtr  pInfo = dev->public.devicePrivate;
	InexioDevicePtr pInexio = pInfo->private;
	struct termios tty;
	int err;

	if (dev->public.on)
	   return Success; /* already on */

	SYSCALL(pInfo->fd = open(pInexio->device, O_RDONLY | O_NONBLOCK));
	if (pInfo->fd < 0)
	{
		xf86Msg(X_ERROR, "%s: cannot open device.\n", pInfo->name);
		return BadRequest;
	}

	/* Set speed to 19200bps ( copied from gpm code ) */
	tcgetattr(pInfo->fd, &tty);
	tty.c_iflag = IGNBRK | IGNPAR;
	tty.c_oflag = 0;
	tty.c_lflag = 0;
	tty.c_line = 0;
	tty.c_cc[VTIME] = 0;
	tty.c_cc[VMIN] = 1;
	tty.c_cflag = B19200|CS8|CREAD|CLOCAL|HUPCL;
	err = tcsetattr(pInfo->fd, TCSAFLUSH, &tty);
	if (err < 0)
	{
		xf86Msg(X_ERROR, "%s: cannot set tty attributes for device.\n",
					pInfo->name);
		return !Success;
	}

	/* The following is a USB init string that is sent to the device by the
	 * windows driver, doesn't seem to be required */
/*	pInexio->body[0] = 0x82; pInexio->body[1] = 0x04;
	pInexio->body[2] = 0x0a; pInexio->body[3] = 0x0f;
	SYSCALL(write(pInfo->fd, pInexio->body, 4));

	pInexio->body[0] = 0x83; pInexio->body[1] = 0x06;
	pInexio->body[2] = 0x35; pInexio->body[3] = 0x2e;
	pInexio->body[4] = 0x30; pInexio->body[5] = 0x32;
	SYSCALL(write(pInfo->fd, pInexio->body, 6));
*/
	xf86FlushInput(pInfo->fd);
	xf86AddEnabledDevice(pInfo);
	dev->public.on = TRUE;
	return Success;
}

/**
 * Called when the device is to be enabled/disabled, etc.
 * @return Success or X error code.
 */
static int InexioControl(DeviceIntPtr	device,
			int		what)
{
	InputInfoPtr	pInfo = device->public.devicePrivate;
	InexioDevicePtr	pInexio = pInfo->private;

	switch(what)
	{
		case DEVICE_INIT:
			_inexio_init_buttons(device);
			_inexio_init_axes(device);
			break;
		/* Switch device on. Establish socket, start event delivery. */
		case DEVICE_ON:
			xf86Msg(X_INFO, "%s: On.\n", pInfo->name);
			return DeviceOn(device);
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

/**
 * Called to convert the device coordinates.
 * @return Success or X error code.
 */
static Bool InexioConvertProc(LocalDevicePtr	local,
				int		first,
				int		num,
				int		v0,
				int		v1,
				int		v2,
				int		v3,
				int		v4,
				int		v5,
				int		*x,
				int		*y)
{
	InexioDevicePtr pInexio = local->private;

	*x = xf86ScaleAxis(v0, 0, pInexio->screen_width, pInexio->min_x,
							pInexio->max_x);
	*y = xf86ScaleAxis(v1, 0, pInexio->screen_height, pInexio->min_y,
							pInexio->max_y);
	/*xf86Msg(X_INFO, "inexio: transform coordinates v0[%i] v1[%i] ==> x[%i] y[%i]\n",
			v0,v1,*x, *y); */
	return TRUE;
}

/***************************************************************************
 *			internal procs					   *
 ***************************************************************************/

/**
 * Init the button map for the random device.
 * @return Success or X error code on failure.
*/
static int _inexio_init_buttons(DeviceIntPtr device)
{
	InputInfoPtr	pInfo = device->public.devicePrivate;
	CARD8		*map;
	int		i;
	const int	num_buttons = 1;
	int		ret = Success;

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
static int _inexio_init_axes(DeviceIntPtr device)
{
	InputInfoPtr	pInfo = device->public.devicePrivate;
	InexioDevicePtr	pInexio = pInfo->private;
	int		i;
	const int	num_axes = 2;

	pInexio->screen_width = screenInfo.screens[pInexio->screen_num]->width;
	pInexio->screen_height = screenInfo.screens[pInexio->screen_num]->height;

	if (!InitValuatorClassDeviceStruct(device,
						num_axes,
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) < 3
						xf86GetMotionEvents,
#endif
						pInfo->history_size,
						Absolute))
		return BadAlloc;


	xf86InitValuatorAxisStruct(device, 0, pInexio->min_x, pInexio->max_x,
					INEXIO_MAX_X, 0, INEXIO_MAX_X);
	xf86InitValuatorAxisStruct(device, 1, pInexio->min_y, pInexio->max_y,
					INEXIO_MAX_Y, 0, INEXIO_MAX_Y);

	/* Allocate the motion events buffer. */
	xf86MotionHistoryAllocate(pInfo);

	return Success;
}

/**
 * Init the valuators for the random device.
 * Only absolute mode is supported.
 * @return Success or X error code on failure.
*/
static int _inexio_read_packet(InexioDevicePtr priv)
{
	/*
	 * format of 5 bytes per packet
	 * format touch down  0x81 XH XL YH YL
	 * format touch up	  0x80 XH XL YH YL
	 * where where XL,XH,YL,YH 7bit
	 * X Coord = XL + XH<<7;
	 * Y Coord = YL + YH<<7;
	 * X max = Y max = 0x3FFF (16383)
	*/

	int count = 0, buffi = priv->bufferi;
	int c;
	int retval = !Success;
	while (count < priv->bufferi)
	{
		c = priv->buffer[count];
		if (!(c & INEXIO_BUTTON_UP))
		{
			/*xf86Msg(X_INFO, "inexio: c[%x] not &0x80, count[%i], bufferi[%i]\n",
				c, count, priv->bufferi);*/
			count++;
			continue;
		}
		/*xf86Msg(X_INFO, "inexio: c[%x], count[%i], bufferi[%i]\n",
			c, count, priv->bufferi);*/

		if ((priv->bufferi - count) >= INEXIO_BODY_LEN)
		{
			memcpy(priv->body, &priv->buffer[count], INEXIO_BODY_LEN);
			memmove(priv->buffer, &priv->buffer[count + INEXIO_BODY_LEN],
					priv->bufferi - count -INEXIO_BODY_LEN);
			priv->bufferi -= count + INEXIO_BODY_LEN;
			retval = Success;
			count = 0;
			continue;
		}
		break;
	};

	if (priv->bufferi - count > 0)
		memmove(priv->buffer, &priv->buffer[count], priv->bufferi - count);
	priv->bufferi -= count;
	if (priv->bufferi < 0)
		priv->bufferi = 0; /* shouldn't be possible */
	return retval;
}

