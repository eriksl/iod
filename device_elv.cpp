#include "device_elv.h"
#include "syslog.h"

#include <string.h>
#include <stdio.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <math.h>

#include <sstream>
#include <iomanip>
using namespace std;

#include <boost/regex.hpp>
using boost::regex;
using boost::smatch;

int DeviceElv::_open() throw(string)
{
	int				fd;
    int             result;
    struct termios  tio;

    if((fd = ::open(_device_node.c_str(), O_RDWR | O_NOCTTY, 0)) < 0)
		throw(string("DeviceElv::_open: cannot open device ") + _device_node);

    if(ioctl(fd, TIOCMGET, &result))
	{
		::close(fd);
		throw(string("DeviceElv::_open: error in TIOCMGET"));
	}

    result &= ~(TIOCM_DTR | TIOCM_RTS | TIOCM_CTS | TIOCM_DSR);

    if(ioctl(fd, TIOCMSET, &result))
	{
		::close(fd);
		throw(string("DeviceElv::_open: error in TIOCMSET"));
	}

    result |= (TIOCM_DTR | TIOCM_RTS | TIOCM_CTS | TIOCM_DSR);

    if(ioctl(fd, TIOCMSET, &result))
	{
		::close(fd);
		throw(string("DeviceElv::_open: error in TIOCMSET"));
	}

    if(tcgetattr(fd, &tio) == 1)
	{
		::close(fd);
		throw(string("DeviceElv::_open: error in tcgetattr"));
	}

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
	{
		::close(fd);
		throw(string("DeviceElv::_open: error in tcsetattr"));
	}

	dlog("*** reset\n");

	string	rv;

	try
	{
		rv = _command(fd, "z 4b", 2000, 2);
	}
	catch(string s)
	{
		vlog("open: exception during reset: %s\n", s.c_str());
		::close(fd);
		throw(string("interface ELV not found"));
	}

	dlog("result: \"%s\"\n", rv.c_str());

	if(rv.find("ELV USB-I2C-Interface v1.6 (Cal:44)") == string::npos)
	{
		vlog("init: id string not recognised\n");
		::close(fd);
		throw(string("interface ELV not found"));
	}

	dlog("*** init\n");
	_ios.clear();

	double digipicco_humidity, digipicco_temperature;

	if(_read_digipicco(fd, 0xf0, digipicco_temperature, digipicco_humidity))
	{
		vlog("digipicco detected at 0xf0\n");

		DeviceIO io;

		io.name					= "digipicco temperature";
		io.id					= (int)_ios.size();
		io.address				= 0xf0;
		io.type					= DeviceIO::analog;
		io.direction			= DeviceIO::input;
		io.lower_boundary		= -40.0;
		io.upper_boundary		= 125.0;
		io.precision			= 2;
		io.value				= digipicco_temperature;
		io.stamp_updated		= time(0);

		_ios.push_back(io);

		io.name					= "digipicco humidity";
		io.id					= (int)_ios.size();
		io.address				= 0xf0;
		io.type					= DeviceIO::analog;
		io.direction			= DeviceIO::input;
		io.lower_boundary		= 0.0;
		io.upper_boundary		= 100.0;
		io.precision			= 0;
		io.value				= digipicco_humidity;
		io.stamp_updated		= time(0);

		_ios.push_back(io);
	}
	else
		vlog("digipicco not detected\n");

	int tsl2550_lux;

	if(_read_tsl2550(fd, 0x72, tsl2550_lux))
	{
		vlog("tsl2550 detected at 0x72\n");

		DeviceIO io;

		io.name					= "tsl2550";
		io.id					= (int)_ios.size();
		io.address				= 0x72;
		io.type					= DeviceIO::analog;
		io.direction			= DeviceIO::input;
		io.lower_boundary		= 0;
		io.upper_boundary		= 10000;
		io.precision			= 0;
		io.value				= tsl2550_lux;
		io.stamp_updated		= time(0);

		_ios.push_back(io);

	}
	else
		vlog("tsl2550 not detected\n");

	return(fd);
}

int DeviceElv::_close(int fd) throw()
{
	::close(fd);

	return(-1);
}

static int timespec_diff(timespec start, timespec end)
{
	timespec temp;

	if((end.tv_nsec - start.tv_nsec) < 0)
	{
		temp.tv_sec		= end.tv_sec - start.tv_sec - 1;
		temp.tv_nsec	= 1000000000 + end.tv_nsec - start.tv_nsec;
	}
	else
	{
		temp.tv_sec		= end.tv_sec - start.tv_sec;
		temp.tv_nsec	= end.tv_nsec - start.tv_nsec;
	}

	return((temp.tv_sec * 1000) + (temp.tv_nsec / 1000000));
}

static bool _tsl2550_adccount(int in, int &out, bool &overflow)
{
	bool	valid	= !!(in & 0x80);
	int		chord	= (in & 0x70) >> 4;
	int		step	= (in & 0x0f);

	if(!valid)
	{
		//dlog("_tsl2550_adccount: invalid value\n");
		return(false);
	}

	if((in & 0x7f) == 0x7f)
		overflow = true;

	int	chordval	= 16.5 * ((1 << chord) - 1);
	int	stepval		= step * (1 << chord);

	out = chordval + stepval;

	//dlog("_tsl2550_adccount: valid = %d, chord = %d, step = %d, chordval = %d, stepval = %d, count = %d, overflow = %d\n",
			//valid, chord, step, chordval, stepval, out, (int)overflow);

	return(true);
}

static int _tsl2550_count2lux(int ch0, int ch1, int multiplier)
{
	double	r = (double)ch1 / ((double)ch0 - (double)ch1);
	double	e = exp(-0.181 * r * r);
	double	l = ((double)ch0 - (double)ch1) * 0.39 * e;

	return((int)(l * (double)multiplier));
}

string DeviceElv::_command(int fd, string cmd, int timeout, int chunks) throw(string)
{
	static char		buffer[256];
	struct pollfd	pfd;
	ssize_t			len;
	int				pr;
	string			rv;
	struct timespec	start, now;
	int				timeout_left;

	if(clock_gettime(CLOCK_MONOTONIC, &start))
		throw(string("DeviceElv::_command: clock_gettime error\n"));

	cmd = string(":") + cmd + string("\n");

	while(true)
	{
		if(clock_gettime(CLOCK_MONOTONIC, &now))
			throw(string("DeviceElv::_command: clock_gettime error\n"));

		timeout_left = timespec_diff(start, now);

		if((timeout > 0) && (timeout_left < 0))
			throw(string("DeviceElv::_command: fatal timeout (1)"));

		pfd.fd		= fd;
		pfd.events	= POLLIN | POLLERR;

		pr = poll(&pfd, 1, 0);

		if(pr < 0)
			throw(string("DeviceElv::_command: poll error (1)"));

		if(pr == 0)
			break;

		if(pfd.revents & POLLERR)
			throw(string("DeviceElv::_command: poll error (2)"));

		if(pfd.revents & POLLIN)
		{
			len = ::read(fd, buffer, sizeof(buffer) - 1);
			buffer[len] = 0;
			dlog("clearing backlog, cleared %d bytes: %s\n", len, buffer);
		}
	}

	if(clock_gettime(CLOCK_MONOTONIC, &now))
		throw(string("DeviceElv::_command: clock_gettime error\n"));

	timeout_left = timeout > 0 ? timeout - timespec_diff(start, now) : -1;

	pfd.fd		= fd;
	pfd.events	= POLLOUT | POLLERR;

	pr = poll(&pfd, 1, timeout_left);

	if(pr < 0)
		throw(string("DeviceElv::_command: poll error (3)"));

	if(pfd.revents & POLLERR)
		throw(string("DeviceElv::_command: poll error (4)"));

	if(!(pfd.revents & POLLOUT))
		throw(string("DeviceElv::_command: fatal timeout (2)"));

	//dlog("write: \"%s\"\n", cmd.c_str());

	if(::write(fd, cmd.c_str(), cmd.length()) != (ssize_t)cmd.length())
		throw(string("DeviceElv::_command: write error"));

	while(chunks > 0)
	{
		if(clock_gettime(CLOCK_MONOTONIC, &now))
			throw(string("DeviceElv::_command: clock_gettime error\n"));

		timeout_left = timeout > 0 ? timeout - timespec_diff(start, now) : -1;

		if((timeout > 0) && (timeout_left < 0))
			break;

		pfd.fd		= fd;
		pfd.events	= POLLIN | POLLERR;

		//dlog("read poll, timeout = %d\n", timeout_left);

		pr = poll(&pfd, 1, timeout_left);

		if(pr < 0)
			throw(string("DeviceElv::_command: poll error (5)"));

		if(pr == 0)
			break;

		if(pfd.revents & POLLERR)
			throw(string("DeviceElv::_command: poll error (6)"));

		if(pfd.revents & POLLIN)
		{
			len = ::read(fd, buffer, sizeof(buffer) - 1);
			buffer[len] = 0;
			//dlog("received %d bytes: %s\n", len, buffer);
			rv += buffer;
			chunks--;
		}
	}

	return(rv);
}

bool DeviceElv::_parse_bytes(string str, int amount, vector<int> & value) throw()
{
	string			match_string;
	smatch			s;
	vector<string>	sval;
	int				i;

	sval.resize(amount);
	value.resize(amount);

	match_string = "\\s*";
	for(i = 0; i < amount; i++)
	{
		match_string += "([0-9A-F]{2})";
		if((i + 1) < amount)
			match_string += "\\s+";
	}
	match_string += "\\s*";

	//dlog("match_bytes(%d) \"%s\" =~ \"%s\"\n", amount, str.c_str(), match_string.c_str());

	regex e(match_string);

	if(!regex_match(str, s, e))
	{
		//dlog("no match\n");
		return(false);
	}

	if(s.size() != (amount + 1))
	{
		//dlog("size wrong\n");
		return(false);
	}

	for(i = 0; i < amount; i++)
	{
		sval[i] = string(s[i + 1]);
		//dlog("sval[%d] = \"%s\"  ", i, sval[i].c_str());
	}
	//dlog("\n");

	for(i = 0; i < amount; i++)
	{
		stringstream conv;
		conv << hex << sval[i];
		conv >> value[i];
	}

	//for(i = 0; i < amount; i++)
		//vlog("value[%d] = %x  ", i, value[i]);
	//dlog("\n");

	return(true);
}

bool DeviceElv::_read_digipicco(int fd, int addr, double & temperature, double & humidity) throw()
{
	ostringstream	cmd;
	string			rv;
	int				attempt;
	int				v1, v2;
	vector<int>		v;

	try
	{
		// s <f0> p
		cmd.str("");
		cmd << "s " << hex << setfill('0') << setw(2) << addr << " p";
		dlog("> %s\n", cmd.str().c_str());
		_command(fd, cmd.str(), 200, 0);

		// r 04 p
		cmd.str("");
		cmd << "r 04 p";

		for(attempt = 3; attempt > 0; attempt--)
		{
			dlog("> %s\n", cmd.str().c_str());
			rv = _command(fd, cmd.str());
			dlog("< \"%s\"\n", rv.c_str());

			if(_parse_bytes(rv, 4, v))
				break;

			usleep(100000);
		}
	}
	catch(string s)
	{
		vlog("read_digipicco: exception: %s\n", s.c_str());
		return(false);
	}

	if(attempt == 0)
	{
		vlog("read_digipicco: error during i/o: %s\n", rv.c_str());
		return(false);
	}

	v1	= (v[0] << 8) | v[1];
	v2	= (v[2] << 8) | v[3];

	//dlog("temperature = 0x%x, humidity = 0x%x\n", v2, v1);
	//dlog("temperature = %d, humidity = %d\n", v2, v1);

	if((v1 == 0xffff) && (v2 == 0xffff))
	{
		dlog("read_digipicco: invalid values 0xffff\n");
		return(false);
	}

	temperature	= (((double)v2 / 32767) * 165) - 40;
	humidity	= (double)v1 / 327.67;

	dlog("temperature = %f, humidity = %f\n", temperature, humidity);

	return(true);
}

bool DeviceElv::_read_tsl2550(int fd, int addr, int &lux) throw()
{
	ostringstream	cmd;
	string			rv;
	vector<int>		v;
	int				ch0s, ch1s, ch0e, ch1e;
	int				cch0s, cch1s, cch0e, cch1e;
	int				ls, le;
	bool			overflows, overflowe;
	int				conversiontimeout = 1000;

	try
	{
		// <72> 03 // startup
		cmd.str("");
		cmd << "s " << hex << setfill('0') << setw(2) << addr << " 03 p";
		dlog("> %s\n", cmd.str().c_str());
		rv = _command(fd, cmd.str(), conversiontimeout, 2);
		dlog("< %s\n\n", rv.c_str());

		// read command register
		cmd.str("r 01 p");
		dlog("> %s\n", cmd.str().c_str());
		rv = _command(fd, cmd.str());
		dlog("< %s\n", rv.c_str());
		if(!_parse_bytes(rv, 1, v))
		{
			vlog("_read_tsl2550: cmd read error\n");
			return(false);
		}
		if(v[0] != 3)
		{
			vlog("_read_tsl2550: cmd read does not return 0x03\n");
			return(false);
		}
		
		// 18 43 // select standard range mode, channel 0
		cmd.str("w 18 w 43 p");
		dlog("> %s\n", cmd.str().c_str());
		rv = _command(fd, cmd.str(), conversiontimeout, 2);
		dlog("< %s\n\n", rv.c_str());
		
		// read ch0s
		cmd.str("r 01 p");
		dlog("> %s\n", cmd.str().c_str());
		rv = _command(fd, cmd.str());
		dlog("< %s\n\n", rv.c_str());
		if(!_parse_bytes(rv, 1, v))
		{
			vlog("_read_tsl2550: read channel 0/standard read error\n");
			return(false);
		}
		ch0s = v[0];

		// 18 83 // select standard range mode, channel 1
		cmd.str("w 18 w 83 p");
		dlog("> %s\n", cmd.str().c_str(), conversiontimeout, 2);
		rv = _command(fd, cmd.str());
		dlog("< %s\n\n", rv.c_str());
		
		// read ch1s
		cmd.str("r 01 p");
		dlog("> %s\n", cmd.str().c_str());
		rv = _command(fd, cmd.str());
		dlog("< %s\n\n", rv.c_str());
		if(!_parse_bytes(rv, 1, v))
		{
			vlog("_read_tsl2550: read channel 1/standard read error\n");
			return(false);
		}
		ch1s = v[0];

		// 1d 43 // select extended range mode, channel 0
		cmd.str("w 1d w 43 p");
		dlog("> %s\n", cmd.str().c_str());
		rv = _command(fd, cmd.str(), conversiontimeout, 2);
		dlog("< %s\n", rv.c_str());

		// read ch0e
		cmd.str("r 01 p");
		dlog("> %s\n", cmd.str().c_str());
		rv = _command(fd, cmd.str());
		dlog("< %s\n", rv.c_str());
		if(!_parse_bytes(rv, 1, v))
		{
			vlog("_read_tsl2550: read channel 0/extended read error\n");
			return(false);
		}
		ch0e = v[0];

		// 1d 83 // select extended range mode, channel 1
		cmd.str("w 1d w 83 p");
		dlog("> %s\n", cmd.str().c_str());
		rv = _command(fd, cmd.str(), conversiontimeout, 2);
		dlog("< %s\n\n", rv.c_str());
		
		// read ch1e
		cmd.str("r 01 p");
		dlog("> %s\n", cmd.str().c_str());
		rv = _command(fd, cmd.str());
		dlog("< %s\n", rv.c_str());
		if(!_parse_bytes(rv, 1, v))
		{
			vlog("_read_tsl2550: read channel 1/extended read error\n");
			return(false);
		}
		ch1e = v[0];
	}
	catch(string s)
	{
		vlog("_read_tsl2550: exception during detection: %s\n", s.c_str());
		return(false);
	}

	overflows = overflowe = false;

	if(!_tsl2550_adccount(ch0s, cch0s, overflows))
	{
		vlog("_read_tsl2550: ch0s invalid\n");
		return(false);
	}

	if(!_tsl2550_adccount(ch1s, cch1s, overflows))
	{
		vlog("_read_tsl2550: ch1s invalid\n");
		return(false);
	}

	if(!_tsl2550_adccount(ch0e, cch0e, overflowe))
	{
		vlog("_read_tsl2550: ch0e invalid\n");
		return(false);
	}

	if(!_tsl2550_adccount(ch1e, cch1e, overflowe))
	{
		vlog("_read_tsl2550: ch1e invalid\n");
		return(false);
	}

	dlog("ch0s = %d, ch1s = %d, ch0e = %d, ch1e = %d\n", ch0s, ch1s, ch0e, ch1e);

	dlog("cch0s = %d, cch1s = %d, cch0e = %d, cch1e = %d\n", cch0s, cch1s, cch0e, cch1e);

	ls = _tsl2550_count2lux(cch0s, cch1s, 1);
	le = _tsl2550_count2lux(cch0e, cch1e, 5);

	lux = overflows ? le : ls;

	dlog("lux = %d\n", lux);

	return(true);
}

DeviceElv::DeviceElv(string device_node, int) throw(string) : Device()
{
	if(device_node == "")
		device_node = "/dev/ttyUSB1";

	_device_node	= device_node;
	_device_fd		= -1;
}

DeviceElv::~DeviceElv() throw()
{
	__close();
}

string DeviceElv::__device_name() const throw()
{
	return(string("ELV USB-I2C"));
}

int DeviceElv::__open() throw(string)
{
    if(_device_fd < 0)
		_device_fd = _open();

	return(_device_fd);
}

void DeviceElv::__close() throw()
{
	if(_device_fd >= 0)
		_device_fd = _close(_device_fd);
}

void DeviceElv::__update(DeviceIOIterator io) throw(string)
{
	int		fd;
	string	rv;

	dlog("update %s\n", io->name.c_str());

	try
	{
		fd = __open();
	}
	catch(string e)
	{
		__close();
		throw(string("DeviceElv::__update: i/o error: ") + e);
	}

	if((io->name == "digipicco humidity") || (io->name == "digipicco temperature"))
	{
		double temperature, humidity;

		if(!_read_digipicco(fd, io->address, temperature, humidity))
		{
			__close();
			throw(string("DeviceElv::__update::digipicco: i/o error"));
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

	if(io->name == "tsl2550")
	{
		int lux;

		if(!_read_tsl2550(fd, io->address, lux))
		{
			__close();
			throw(string("DeviceElv::__update::tsl2550: i/o error"));
		}

		io->value			= lux;
		io->stamp_updated	= time(0);
	}
}
