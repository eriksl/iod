#ifndef _device_hb627_h
#define _device_hb627_h

#include <stdint.h>
#include <usb.h>

#include <string>
using std::string;

#include <bitset>
using std::bitset;

#include "device.h"

class DeviceHb627 : public Device
{
	private:

		int					_analog_in[8];
		bool				_debug;
		int					_device_fd;
		string				_device_node;

		int					_open()									throw();
		void				_close(int fd)							throw();
		void				_command(size_t cmdlen, const uint8_t * cmd, size_t rvlen, uint8_t * rv)
																	throw();

	protected:

		string	__device_name()								const	throw();
		int		__analog_inputs()							const	throw();
		int		__analog_input_max()						const	throw();
		void	__open()											throw(string);
		void	__close()											throw();
		int		__read_analog_input(int input)						throw(string);
		void	__update_inputs()									throw(string);
		void	__activity()										throw(string);

	public:

				DeviceHb627(string device_node, int device_address)	throw(string);
		virtual	~DeviceHb627()										throw();

		void	debug(bool onoff);
};
#endif
