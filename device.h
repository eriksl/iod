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

		int				id;
		string			name;
		io_type_t		type;
		io_direction_t	direction;
		double			lower_boundary;
		double			upper_boundary;
		time_t			stamp_set;
		time_t			stamp_flushed;
		time_t			stamp_read;
		time_t			stamp_updated;
		double			value;
		int				resampling;

		DeviceIO()
		{
			id				= -1;
			type			= no_type;
			direction		= no_direction;
			lower_boundary	= 0;
			upper_boundary	= 0;
			stamp_set		= 0;
			stamp_read		= 0;
			stamp_updated	= 0;
			value			= 0;
			resampling		= 1;
		}
};

typedef vector<DeviceIO>	DeviceIOs;
typedef DeviceIOs::iterator DeviceIOIterator;

class Device
{
	private:

		bool				_isopen;
		bool				_isinit;
		pthread_mutex_t		_mutex;
		bool				_mutex_valid;

		void				_update(DeviceIOIterator io)							throw(string);

		virtual	string		__device_name()									const	throw()			= 0;
		virtual void		__open()												throw(string)	= 0;
		virtual void		__close()												throw()			= 0;
		virtual void		__init()												throw(string)	= 0;
		virtual void		__update(DeviceIOIterator io)							throw(string)	= 0;

	protected:

		bool				_debug;
		DeviceIOs			_ios;

	public:
							Device()												throw(string);
		virtual				~Device()												throw();

		void				open()													throw(string);
		void				close()													throw();
		string				device_name()									const	throw();
		void				debug(bool onoff)										throw();
		double				read(DeviceIOIterator io)								throw(string);
		void				write(DeviceIOIterator io, double value)				throw(string);
		void				resampling(DeviceIOIterator io, int value)				throw(string);
		void				update(DeviceIOIterator io)								throw(string);
		void				update()												throw(string);
		void				lock()													throw(string);
		void				unlock()												throw(string);

		DeviceIOIterator	begin()													throw(string);
		DeviceIOIterator	end()													throw(string);
		DeviceIOIterator	find(int ix)											throw(string);
		DeviceIOIterator	find(string name)										throw(string);
};
#endif
