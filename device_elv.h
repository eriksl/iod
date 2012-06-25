#ifndef _device_elv_h
#define _device_elv_h

#include <stdint.h>
#include <usb.h>

#include <string>
using std::string;

#include <bitset>
using std::bitset;

#include <boost/regex.hpp>
using boost::regex;
using boost::smatch;

#include "device.h"

class DeviceElv : public Device
{
	private:

		int		_device_fd;
		string	_device_node;

		int		_open()												throw();
		void	_close(int fd)										throw();
		string	_command(string cmd, int timeout = 200)				throw();
		bool	_detect_digipicco()									throw();

		bool	_parse_digipicco(string, int&, int&)				throw();

	protected:

		string		__device_name()							const	throw();
		void		__open()										throw(string);
		void		__close()										throw();
		void		__init()										throw(string);
		void		__update(DeviceIOIterator io)					throw(string);

	public:

				DeviceElv(string device_node, int device_address)	throw(string);
		virtual	~DeviceElv()										throw();
};
#endif
