#ifndef _device_elv_h
#define _device_elv_h

#include <stdint.h>
#include <usb.h>

//#include <algorithm>
//using std::transform;

#include <string>
using std::string;

#include <bitset>
using std::bitset;

#include <boost/regex.hpp>
using boost::regex;
using boost::smatch;

//#include <boost/lexical_cast.hpp>
//using boost::lexical_cast;

#include "device.h"

class DeviceElv : public Device
{
	private:

		bool	_init;
		int		_testinput;
		bool	_debug;
		int		_device_fd;
		string	_device_node;
		bool	_detected_digipicco;

		int		_open()									throw();
		void	_close(int fd)							throw();
		string	_command(string cmd, int timeout = 200) throw();
		bool	_detect_digipicco()						throw();

		bool	_parse_digipicco(string, int&, int&)	throw();

	protected:

		string	__device_name()								const;
		int		__analog_inputs()							const;
		int		__analog_input_max()						const;
		void	__open()											throw(string);
		void	__close()											throw();
		void	__update_inputs()									throw(string);
		int		__read_analog_input(int input)						throw(string);

	public:

				DeviceElv(string device_node, int device_address)	throw(string);
		virtual	~DeviceElv()										throw();

		void	debug(bool onoff);
};
#endif
