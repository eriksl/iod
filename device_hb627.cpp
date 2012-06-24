#include "device_hb627.h"
#include "syslog.h"

#include <string.h>
#include <stdio.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <poll.h>

DeviceHb627::DeviceHb627(string device_node, int) throw(string) : Device()
{
	if(device_node == "")
		device_node = "/dev/ttyACM0";

	_device_node	= device_node;
	_device_fd		= -1;
}

DeviceHb627::~DeviceHb627() throw()
{
	__close();
}

int DeviceHb627::_open() throw()
{
	int				fd;
    int             result;
    struct termios  tio;

    if((fd = ::open(_device_node.c_str(), O_RDWR | O_NOCTTY, 0)) < 0)
		return(-1);

    if(ioctl(fd, TIOCMGET, &result))
		return(-1);

    result &= ~(TIOCM_DTR | TIOCM_RTS | TIOCM_CTS | TIOCM_DSR);

    if(ioctl(fd, TIOCMSET, &result))
		return(-1);

    result |= (TIOCM_DTR | TIOCM_RTS | TIOCM_CTS | TIOCM_DSR);

    if(ioctl(fd, TIOCMSET, &result))
		return(-1);

    if(tcgetattr(fd, &tio) == 1)
		return(-1);

    tio.c_iflag &= ~(BRKINT | INPCK | INLCR | IGNCR | IUCLC |
                    IXON    | IXOFF | IXANY | IMAXBEL | ISTRIP | ICRNL);
    tio.c_iflag |=  (IGNBRK | IGNPAR);

    tio.c_oflag &= ~(OPOST | OLCUC | OCRNL | ONOCR | ONLRET | OFILL | ONLCR);
    tio.c_oflag |=  0;

    tio.c_cflag &=  ~(CSIZE | PARENB | PARODD   | HUPCL | CRTSCTS);
    tio.c_cflag |=  (CREAD | CS8 | CSTOPB | CLOCAL);

    tio.c_lflag &= ~(ISIG   | ICANON    | XCASE | ECHO  | ECHOE | ECHOK |
                    ECHONL | ECHOCTL    | ECHOPRT | ECHOKE | FLUSHO | TOSTOP |
                    PENDIN | IEXTEN     | NOFLSH);
    tio.c_lflag |=  0;

    cfsetispeed(&tio, B9600);
    cfsetospeed(&tio, B9600);

    if(tcsetattr(fd, TCSANOW, &tio) == 1)
		return(-1);

	return(fd);
}

void DeviceHb627::_close(int fd) throw()
{
	::close(fd);
}

void DeviceHb627::__open() throw(string)
{
    if(_device_fd != -1)
        throw(string("DeviceHb627::open: device already open"));

	__update_inputs();
}

void DeviceHb627::__close() throw()
{
	if(_device_fd > 0)
		_close(_device_fd);

	_device_fd = -1;
}

string DeviceHb627::__device_name() const throw()
{
	return(string("H-Tronic hb627"));
}

int DeviceHb627::__analog_inputs() const throw()
{
	return(8);
}

int DeviceHb627::__analog_input_max() const throw()
{
	return(4096);
}

int DeviceHb627::__read_analog_input(int input) throw(string)
{
	return(_analog_in[input]);
}

void DeviceHb627::_command(size_t cmdlen, const uint8_t * cmd, size_t rvlen, uint8_t * rv) throw()
{
	static char		readbuffer[256];
	uint8_t			checksum;
	struct pollfd	pfd;
	ssize_t			len;
	int				attempt;
	int				pr;
	int				ix;

	for(attempt = 0;; attempt++)
	{
		if(_device_fd < 0)
		{
			if(_debug)
				vlog("reopen \"%s\"\n", _device_node.c_str());
			_device_fd = _open();
		}

		if(_device_fd < 0)
		{
			if(_debug)
				vlog("cannot open \"%s\"\n", _device_node.c_str());
			goto retry;
		}

		pfd.fd		= _device_fd;
		pfd.events	= POLLIN | POLLERR;

		pr = poll(&pfd, 1, 0);

		if(pr < 0)
		{
			vlog("poll error\n");
			goto retry;
		}

		if(pr > 0)
		{
			if(pfd.revents & POLLERR)
			{
				vlog("i/o error\n");
				goto retry;
			}

			if(pfd.revents & POLLIN)
			{
				len = read(_device_fd, readbuffer, sizeof(readbuffer));

				if(_debug)
					vlog("clearing backlog, cleared %d bytes\n", len);
			}
		}

		if(write(_device_fd, cmd, cmdlen) != (ssize_t)cmdlen)
		{
			if(_debug)
				vlog("write error\n");

			goto retry;
		}

		if(rvlen && rv)
		{
			if(read(_device_fd, rv, rvlen) != (ssize_t)rvlen)
			{
				if(_debug)
					vlog("read error\n");

				goto retry;
			}

			checksum = 0;

			for(ix = 0; ix < 16; ix++)
				checksum += rv[ix];

			if(checksum != (uint8_t)rv[16])
			{
				if(_debug)
					vlog("checksums do not match, received: %d, calculated: %d\n", (uint16_t)checksum, (uint16_t)rv[16]);

				goto retry;
			}
		}

		break;

retry:
		vlog("retry %d\n", attempt);
		_close(_device_fd);
		_device_fd = -1;
		sleep(1);
	}
}

void DeviceHb627::__update_inputs() throw(string)
{
	static uint8_t	result[32];
	int				input;
	uint16_t		value;

	_command(3, (uint8_t *)"c09", 17, result);

	for(input = 0; input < 8; input++)
	{
		value =		result[(input << 1) + 0] << 8;
		value |=	result[(input << 1) + 1] << 0;

		_analog_in[input] = (uint16_t)value;
	}
}

void DeviceHb627::__activity() throw(string)
{
	_command(3, (uint8_t *)"a00", 0, 0);
}

void DeviceHb627::debug(bool onoff)
{
	_debug = onoff;
}
