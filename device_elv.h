#ifndef _device_elv_h
#define _device_elv_h

#include "device.h"

#include <string>
using std::string;

class DeviceElv : public Device
{
	private:

		int		_device_fd;
		string	_device_node;

		bool	_parse_bytes(string, int, vector<int> &)								throw();
		bool	_read_digipicco(int fd, int addr, double &temp, double &hum)			throw();
		bool	_read_tsl2550_1(int fd, int addr, bool erange, double &lux)				throw();
		bool	_read_tsl2550(int fd, int addr, double &lux)							throw();
		int		_open()																	throw(string);
		int		_close(int fd)															throw();
		string	_command(int fd, string cmd, int timeout = 200, int chunks = 1)			throw(string);

	protected:

		string		__device_name()							const	throw();
		int			__open()										throw(string);
		void		__close()										throw();
		void		__update(DeviceIOIterator io)					throw(string);

	public:

				DeviceElv(string device_node, int device_address)	throw(string);
		virtual	~DeviceElv()										throw();
};
#endif
