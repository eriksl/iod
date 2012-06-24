#ifndef _device_h
#define _device_h

#include <pthread.h>

#include <string>
using std::string;

#include <bitset>
using std::bitset;

#include <vector>
using std::vector;

class DeviceIO
{
	public:

		typedef enum
		{
			no_device,
			digipicco_humidity,
			digipicco_temperature
		} io_device_t;

		typedef enum
		{
			no_type,
			analog,
			digital,
			counter
		} io_type_t;

		typedef enum
		{
			no_direction,
			input,
			output,
			io
		} io_direction_t;

		string			name;
		int				id;
		io_device_t		device;
		io_type_t		type;
		io_direction_t	direction;
		double			lower_boundary;
		double			upper_boundary;
		time_t			stamp;
		double			value;

		DeviceIO()
		{
			id				= -1;
			device			= no_device;
			type			= no_type;
			direction		= no_direction;
			lower_boundary	= 0;
			upper_boundary	= 0;
			stamp			= 0;
			value			= 0;
		}
};

typedef vector<DeviceIO> DeviceIOs;
typedef DeviceIOs::const_iterator DeviceIOIterator;

class Device
{
	private:

		bool				_isopen;
		bool				_debug;
		pthread_mutex_t		_mutex;
		bool				_mutex_valid;
		DeviceIOs			_ios;

		void				_update(DeviceIO & id)									throw(string);

		virtual void		__open()												throw(string)	= 0;
		virtual void		__init()												throw(string)	= 0;
		virtual void		__close()												throw()			= 0;
		virtual	string		__device_name()									const	throw()			= 0;
		virtual void		__update(DeviceIO & id)									throw(string)	= 0;

	public:
							Device()												throw(string);
		virtual				~Device()												throw();

		void				open()													throw(string);
		void				close()													throw();
		string				device_name()									const	throw();
		void				debug(bool onoff)										throw();
		double				read(DeviceIO & io, bool update = false)				throw(string);
		double				read(int id, bool update = false)						throw(string);
		void				write(DeviceIO & io, double value, bool upd = false)	throw(string);
		void				write(int id, double value, bool update = false)		throw(string);
		void				update(DeviceIO & io)									throw(string);
		void				update(int id)											throw(string);
		void				lock()													throw(string);
		void				unlock()												throw(string);

		DeviceIOIterator	begin()											const	throw(string);
		DeviceIOIterator	end()											const	throw(string);
		DeviceIO			find(int ix)									const 	throw(string);
		DeviceIO			find(string name)								const	throw(string);
};
#endif
