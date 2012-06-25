#include "device_elv.h"
#include "syslog.h"

#include <string.h>
#include <stdio.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <math.h>

int DeviceElv::_open() throw()
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

    cfsetispeed(&tio, B115200);
    cfsetospeed(&tio, B115200);

    if(tcsetattr(fd, TCSANOW, &tio) == 1)
		return(-1);

	return(fd);
}

void DeviceElv::_close(int fd) throw()
{
	::close(fd);
}

string DeviceElv::_command(string cmd, int timeout) throw()
{
	static char		buffer[256];
	struct pollfd	pfd;
	ssize_t			len;
	int				attempt, attempt1;
	int				pr;
	string			rv;

	for(attempt = 25; attempt > 0; attempt--)
	{
		if(_device_fd < 0)
		{
			if(_debug)
				vlog("open \"%s\"\n", _device_node.c_str());
			_device_fd = _open();
		}

		if(_device_fd < 0)
		{
			if(_debug)
				vlog("cannot open \"%s\"\n", _device_node.c_str());
			goto retry;
		}

		for(attempt1 = 25; attempt1 > 0; attempt1--)
		{
			pfd.fd		= _device_fd;
			pfd.events	= POLLIN | POLLERR;

			pr = poll(&pfd, 1, 0);

			if(pr < 0)
			{
				vlog("poll error\n");
				goto retry;
			}

			if(pr == 0)
			{
				//vlog("flush poll timeout\n");
				break;
			}

			if(pfd.revents & POLLERR)
			{
				vlog("i/o error\n");
				goto retry;
			}

			if(pfd.revents & POLLIN)
			{
				len = ::read(_device_fd, buffer, sizeof(buffer));

				if(_debug)
					vlog("clearing backlog, try %d, cleared %d bytes\n", attempt1, len);
			}
		}

		vlog("write: %s\n", cmd.c_str());

		if(::write(_device_fd, cmd.c_str(), cmd.length()) != (ssize_t)cmd.length())
		{
			if(_debug)
				vlog("write error\n");

			goto retry;
		}

		for(attempt1 = 25; attempt1 > 0; attempt1--)
		{
			pfd.fd		= _device_fd;
			pfd.events	= POLLIN | POLLERR;

			//vlog("read poll, timeout = %d\n", timeout);

			pr = poll(&pfd, 1, timeout);

			if(pr < 0)
			{
				vlog("read poll error\n");
				goto retry;
			}

			if(pr == 0)
				break;

			if(pfd.revents & POLLERR)
			{
				vlog("read i/o error (1)\n");
				goto retry;
			}

			if(pfd.revents & POLLIN)
			{
				len = ::read(_device_fd, buffer, sizeof(buffer) - 1);
				buffer[len] = 0;

				//if(_debug)
					//vlog("received %d bytes: %s\n", len, buffer);

				rv += buffer;
			}
		}

		if(attempt1 > 0)
			break;

		vlog("read error\n");

retry:
		vlog("retry %d\n", attempt);
		_close(_device_fd);
		_device_fd = -1;
		sleep(1);
	}

	if(attempt <= 0)
		vlog("DeviceElv::_command:i/o error, giving up");

	return(rv);
}

bool DeviceElv::_parse_digipicco(string str, int &value1, int &value2) throw()
{
	regex e("([0-9A-F]{2}) ([0-9A-F]{2}) ([0-9A-F]{2}) ([0-9A-F]{2})...");
	smatch s;
	string sval1, sval2;

	//printf("match \"%s\"\n", str.c_str());

	if(!regex_match(str, s, e))
	{
		//printf("no match\n");
		return(false);
	}

	if(s.size() != 5)
	{
		//printf("size wrong\n");
		return(false);
	}

	sval1 = string(s[1]) + string(s[2]);
	sval2 = string(s[3]) + string(s[4]);

	//printf("sval1 = \"%s\", sval2 = \"%s\"\n", sval1.c_str(), sval2.c_str());

	value1 = strtoul(sval1.c_str(), 0, 16);
	value2 = strtoul(sval2.c_str(), 0, 16);

	//printf("val1 = %d, val2 = %d\n", value1, value2);
	//printf("val1 = %x, val2 = %x\n", value1, value2);

	return(true);
}

bool DeviceElv::_detect_digipicco() throw()
{
	string rv;

	vlog("sf0\n");
	rv = _command("sf0");
	vlog("result: \"%s\"\n\n", rv.c_str());

	vlog("r04p\n");
	rv = _command("r04p");
	vlog("result: \"%s\"\n\n", rv.c_str());

	if(rv.find("Err:TWI READ") == string::npos)
		return(false);

	vlog("r04p\n");
	rv = _command("r04p");
	vlog("result: \"%s\"\n\n", rv.c_str());

	int value1, value2;

	if(!_parse_digipicco(rv, value1, value2))
		return(false);

	if((value1 == 65536) && (value2 == 65536))
		return(false);

	return(true);
}

DeviceElv::DeviceElv(string device_node, int) throw(string) : Device()
{
	if(device_node == "")
		device_node = "/dev/ttyUSB1";

	_device_node		= device_node;
	_device_fd			= -1;
}

DeviceElv::~DeviceElv() throw()
{
	__close();
}

string DeviceElv::__device_name() const throw()
{
	return(string("ELV USB-I2C"));
}

void DeviceElv::__open() throw(string)
{
    if(_device_fd >= 0)
        throw(string("DeviceElv::open: device already open"));

	_device_fd = _open();
}

void DeviceElv::__close() throw()
{
	if(_device_fd >= 0)
	{
		_close(_device_fd);
		_device_fd = -1;
	}
}

void DeviceElv::__init() throw(string)
{
	string rv;
	vlog("perform init\n");

	vlog("reset\n");
	rv = _command("z4b", 2000);
	vlog("result: \"%s\"\n", rv.c_str());

	if(rv.find("ELV USB-I2C-Interface v1.6 (Cal:44)") == string::npos)
		throw(string("interface ELV not found"));

	if(_detect_digipicco())
	{
		vlog("digipicco detected\n");

		DeviceIO io;

		io.name					= "digipicco temperature";
		io.id					= (int)_ios.size();
		io.type					= DeviceIO::analog;
		io.direction			= DeviceIO::input;
		io.lower_boundary		= -40.0;
		io.upper_boundary		= 125.0;

		_ios.push_back(io);

		io.name					= "digipicco humidity";
		io.id					= (int)_ios.size();
		io.type					= DeviceIO::analog;
		io.direction			= DeviceIO::input;
		io.lower_boundary		= 0.0;
		io.upper_boundary		= 100.0;

		_ios.push_back(io);
	}
	else
		vlog("digipicco not detected\n");
}

void DeviceElv::__update(DeviceIOIterator io) throw(string)
{
	string	rv;
	int		attempt;
	int		value1, value2;
	double	temperature, humidity;

	vlog("update %s\n", io->name.c_str());

	if((io->name != "digipicco humidity") &&  (io->name != "digipicco temperature"))
		return;

	for(attempt = 25; attempt > 0; attempt--)
	{
		vlog("sf0\n");
		rv = _command("sf0");
		vlog("result: \"%s\"\n\n", rv.c_str());

		vlog("r04p\n");
		rv = _command("r04p");
		vlog("result: \"%s\"\n\n", rv.c_str());

		if(rv.find("Err:TWI READ") == string::npos)
		{
			vlog("Err:TWI READ not found\n");
			continue;
		}

		vlog("r04p\n");
		rv = _command("r04p");
		vlog("result: \"%s\"\n\n", rv.c_str());

		if(!_parse_digipicco(rv, value1, value2))
		{
			vlog("output does not match\n");
			continue;
		}

		if((value1 == 0xffff) && (value2 == 0xffff))
		{
			vlog("answer out of range\n");
			continue;
		}

		temperature = (((double)value2 / 32767) * 165) - 40;
		humidity = (double)value1 / 327.67;

		break;
	}

	if(attempt == 0)
	{
		vlog("attempt = 0\n");
		return;
	}

	if(io->name == "digipicco temperature")
	{
		io->value = temperature;
		io->stamp_updated = time(0);
	}

	if(io->name == "digipicco humidity")
	{
		io->value = humidity;
		io->stamp_updated = time(0);
	}
}
