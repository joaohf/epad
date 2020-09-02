#include "erl_driver.h"

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <linux/uinput.h>

#define EPAD_RES_MASK 0xFF
#define EPAD_RES_OK 0
#define EPAD_RES_IO_ERROR 1
#define EPAD_RES_NOT_OPEN 2
#define EPAD_RES_ILLEGAL_ARG 3

#define EPAD_CMD_MASK  0x0000000F
#define EPAD_CMD_EVENT  0x00000001

#define EPAD_PROFILE_MASK 0x0000000F
#define EPAD_PROFILE_GAMEPAD  0x00000001
#define EPAD_PROFILE_KEYBOARD 0x00000002
#define EPAD_PROFILE_TOUCHPAD 0x00000003

struct epad_drv_state {
	int flags;
	int fd;
	ErlDrvPort port;
};

static ErlDrvData epad_drv_start(ErlDrvPort port, char *command);
static void epad_drv_stop(ErlDrvData data);
static ErlDrvSSizeT epad_drv_control(
	ErlDrvData data,
	unsigned int command,
	char *buf,
	ErlDrvSizeT buf_len,
	char **rbuf,
	ErlDrvSizeT rbuf_len);

static int epad_drv_initial_state(struct epad_drv_state *state, int profile);
static int emit(int fd, unsigned short type, unsigned short code, int val);
static ErlDrvSSizeT port_ctl_return_val(unsigned char code, unsigned int arg, char* resbuf);


static ErlDrvData epad_drv_start(ErlDrvPort port, char *command)
{
    int uinput_path_max_len = strlen(command);

    for (; *command != '\0' && *command != ' '; ++command);
    if (*command == '\0') {
        return ERL_DRV_ERROR_BADARG;
    }

    command += 1; // Skip space

    char uinput_path[uinput_path_max_len];
    int profile;	

    if (sscanf(command, "%s %d", uinput_path, &profile) != 2) {
        return ERL_DRV_ERROR_BADARG;
    }

    int fd;

try_again:
	fd = open(uinput_path, O_WRONLY | O_NONBLOCK);
	if (fd < 0) {
		switch (errno) {
		case EINTR:
			goto try_again;
			break;
		default:
			goto error_errno;
			break;
		}
	}

	struct stat st;
	if (fstat(fd, &st))
		goto error_fd_errno;

	if (!S_ISCHR(st.st_mode))
		goto error_fd;

	// Make the socket non-blocking
	int flags;
	if ((flags = fcntl(fd, F_GETFL)) == -1)
		goto error_fd_errno;

	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
		goto error_fd_errno;

	struct epad_drv_state *state = driver_alloc(sizeof(*state));
	if (!state)
		goto error_fd;

	state->fd = fd;
	state->port = port;
	state->flags = 0;

	if (epad_drv_initial_state(state, profile)) {
		goto error_state_fd_errno;
	}

	return (ErlDrvData)state;

error_fd:
    if (close(fd)) {
		switch (errno) {
			case EINTR:
			    goto error_fd;
				break;
			default:
			    break;
		}
	}
	return ERL_DRV_ERROR_GENERAL;

    int errno_stash;
error_state_fd_errno:
    errno_stash = errno;
	driver_free(state);
	errno = errno_stash;

error_fd_errno:
    errno_stash = errno;
	if (close(fd)) {
		switch (errno) {
		case EINTR:
		    errno = errno_stash;
			goto error_fd_errno;
			break;
		default:
			break;
		}
	}
	errno = errno_stash;

error_errno:
    return ERL_DRV_ERROR_ERRNO;
}

static void epad_drv_stop(ErlDrvData data)
{
    struct epad_drv_state *state = (struct epad_drv_state *)data;

    ioctl(state->fd, UI_DEV_DESTROY);

try_again:
	if (close(state->fd)) {
		switch (errno) {
			case EINTR:
			    goto try_again;
			    break;
			default:
			    break;
		}
	}

	driver_free(data);
}

static ErlDrvSSizeT epad_drv_control(
	ErlDrvData data,
	unsigned int command,
	char *buf,
	ErlDrvSizeT buf_len,
	char **rbuf,
	ErlDrvSizeT rbuf_len)
{
	struct epad_drv_state *state = (struct epad_drv_state *)data;
	char c[32];
	unsigned short type;
	unsigned short code;
	int val;

	switch (command & EPAD_CMD_MASK) {
	case EPAD_CMD_EVENT:
	{
		memcpy(&c, buf, buf_len);
		c[buf_len] = '\0';
		if (sscanf(c, "ev:%hi:%hi:%i", &type, &code, &val) != 3) {
			goto error_sscanf;
		}

        if (emit(state->fd, type, code, val)) {
            driver_failure_posix(state->port, errno);
			goto error_io;
		}
		if (emit(state->fd, EV_SYN, SYN_REPORT, 0)) {
            driver_failure_posix(state->port, errno);
			goto error_io;
		}

		return port_ctl_return_val(EPAD_RES_OK, 0, *rbuf);
	}

    default:
        break;
    }

error_io:
    return port_ctl_return_val(EPAD_RES_IO_ERROR, 0, *rbuf);

error_sscanf:
    return port_ctl_return_val(EPAD_RES_ILLEGAL_ARG, 0, *rbuf);
}

static ErlDrvSSizeT port_ctl_return_val(unsigned char code, unsigned int arg, char* resbuf)
{
    unsigned int res = (code << 24) | arg;

    memcpy(resbuf, &res, sizeof(res));
    return sizeof(res);
}

static int epad_drv_initial_state(struct epad_drv_state *state, int profile)
{
	int ret = -1;
	struct uinput_setup usetup;

	memset(&usetup, 0, sizeof(usetup));

	switch (profile & EPAD_PROFILE_MASK) {
	case EPAD_PROFILE_GAMEPAD:
	{
	    struct uinput_abs_setup uabs_x;
		struct uinput_abs_setup uabs_y;

		memset(&uabs_x, 0, sizeof(uabs_x));
		memset(&uabs_y, 0, sizeof(uabs_y));

        usetup.id.bustype = BUS_USB;
        usetup.id.vendor = 0x3;
        usetup.id.product = 0x3;
		usetup.id.version = 2;
        strcpy(usetup.name, "Virtual gamepad");

        uabs_x.code = ABS_X;
        uabs_x.absinfo.maximum = 255;
        uabs_x.absinfo.minimum = 0;
        uabs_x.absinfo.fuzz = 0;
        uabs_x.absinfo.flat = 15;

        uabs_y.code = ABS_Y;
        uabs_y.absinfo.maximum = 255;
        uabs_y.absinfo.minimum = 0;
        uabs_y.absinfo.fuzz = 0;
        uabs_y.absinfo.flat = 15;
		// Init buttons
		ioctl(state->fd, UI_SET_EVBIT, EV_KEY);
		ioctl(state->fd, UI_SET_KEYBIT, BTN_A);
        ioctl(state->fd, UI_SET_KEYBIT, BTN_B);
        ioctl(state->fd, UI_SET_KEYBIT, BTN_X);
        ioctl(state->fd, UI_SET_KEYBIT, BTN_Y);
        ioctl(state->fd, UI_SET_KEYBIT, BTN_TL);
		ioctl(state->fd, UI_SET_KEYBIT, BTN_TR);
		ioctl(state->fd, UI_SET_KEYBIT, BTN_TL2);
		ioctl(state->fd, UI_SET_KEYBIT, BTN_TR2);
        ioctl(state->fd, UI_SET_KEYBIT, BTN_START);
        ioctl(state->fd, UI_SET_KEYBIT, BTN_SELECT);
        // Init directions
        ioctl(state->fd, UI_SET_EVBIT, EV_ABS);
        ioctl(state->fd, UI_SET_ABSBIT, ABS_X);
        ioctl(state->fd, UI_SET_ABSBIT, ABS_Y);

		ioctl(state->fd, UI_DEV_SETUP, &usetup);
		ioctl(state->fd, UI_ABS_SETUP, &uabs_x);
		ioctl(state->fd, UI_ABS_SETUP, &uabs_y);
		ioctl(state->fd, UI_DEV_CREATE);

		ret = 0;

		break;
	}
	case EPAD_PROFILE_KEYBOARD:
	{
		// Init buttons
        ioctl(state->fd, UI_SET_EVBIT, EV_KEY);
		for (int i = 0; i < 255; i++) {
			ioctl(state->fd, UI_SET_KEYBIT, i);
		}

		usetup.id.bustype = BUS_USB;
        usetup.id.vendor = 0x3;
        usetup.id.product = 0x4;
		usetup.id.version = 1;
        strcpy(usetup.name, "Virtual keyboard");

		ioctl(state->fd, UI_DEV_SETUP, &usetup);
		ioctl(state->fd, UI_DEV_CREATE);

		ret = 0;

		break;
	}
	case EPAD_PROFILE_TOUCHPAD:
	{
		struct uinput_abs_setup uabs_x;
		struct uinput_abs_setup uabs_y;

		memset(&uabs_x, 0, sizeof(uabs_x));
		memset(&uabs_y, 0, sizeof(uabs_y));

		usetup.id.bustype = BUS_USB;
        usetup.id.vendor = 0x3;
        usetup.id.product = 0x5;
		usetup.id.version = 1;
        strcpy(usetup.name, "Virtual touchpad");

        uabs_x.code = ABS_X;
        uabs_x.absinfo.maximum = 255;
        uabs_x.absinfo.minimum = 0;
        uabs_x.absinfo.fuzz = 0;
        uabs_x.absinfo.flat = 15;

        uabs_y.code = ABS_Y;
        uabs_y.absinfo.maximum = 255;
        uabs_y.absinfo.minimum = 0;
        uabs_y.absinfo.fuzz = 0;
        uabs_y.absinfo.flat = 15;

        // Init buttons
        ioctl(state->fd, UI_SET_EVBIT, EV_KEY);
        ioctl(state->fd, UI_SET_KEYBIT, BTN_LEFT);
        ioctl(state->fd, UI_SET_KEYBIT, BTN_RIGHT);
        ioctl(state->fd, UI_SET_KEYBIT, BTN_MIDDLE);
        ioctl(state->fd, UI_SET_KEYBIT, BTN_A);
        ioctl(state->fd, UI_SET_KEYBIT, BTN_B);
        ioctl(state->fd, UI_SET_KEYBIT, BTN_X);
        ioctl(state->fd, UI_SET_KEYBIT, BTN_Y);
        ioctl(state->fd, UI_SET_KEYBIT, BTN_TL);
        ioctl(state->fd, UI_SET_KEYBIT, BTN_TR);
        ioctl(state->fd, UI_SET_KEYBIT, BTN_START);
        ioctl(state->fd, UI_SET_KEYBIT, BTN_SELECT);
        // Init absolute directions
        ioctl(state->fd, UI_SET_EVBIT, EV_ABS);
        ioctl(state->fd, UI_SET_ABSBIT, ABS_X);
        ioctl(state->fd, UI_SET_ABSBIT, ABS_Y);
        // Init relative directions
        ioctl(state->fd, UI_SET_EVBIT, EV_REL);
        ioctl(state->fd, UI_SET_RELBIT, REL_X);
        ioctl(state->fd, UI_SET_RELBIT, REL_Y);
        ioctl(state->fd, UI_SET_RELBIT, REL_WHEEL);

		ioctl(state->fd, UI_DEV_SETUP, &usetup);
		ioctl(state->fd, UI_ABS_SETUP, &uabs_x);
		ioctl(state->fd, UI_ABS_SETUP, &uabs_y);
		ioctl(state->fd, UI_DEV_CREATE);

		ret = 0;

		break;
	}

    default:
        break;
    }

    return ret;
}

static int emit(int fd, unsigned short type, unsigned short code, int val)
{
   struct input_event ie;

   ie.type = type;
   ie.code = code;
   ie.value = val;
   /* timestamp values below are ignored */
   ie.time.tv_sec = 0;
   ie.time.tv_usec = 0;

try_again:
   if (write(fd, &ie, sizeof(ie)) < 0) {
	   switch (errno) {
		   case EINTR:
		       goto try_again;
		       break;
		   default:
		       return -1;
			   break;
	   }
   }

   return 0;
}

ErlDrvEntry epad_driver_entry = {
	.driver_name = "epad_driver",
	.extended_marker = ERL_DRV_EXTENDED_MARKER,
	.major_version   = ERL_DRV_EXTENDED_MAJOR_VERSION,
	.minor_version   = ERL_DRV_EXTENDED_MINOR_VERSION,
	.driver_flags    = 0,
	.start       = epad_drv_start,
	.stop        = epad_drv_stop,
	.control     = epad_drv_control
};

DRIVER_INIT(epad_driver) /* must match name in driver_entry */
{
	return &epad_driver_entry;
}
