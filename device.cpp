#include "device.h"

#include <stdlib.h>

#include <deque>
using std::deque;

#include <boost/lexical_cast.hpp>
using boost::lexical_cast;

#include <numeric>
using std::accumulate;

Device::Device() throw(string)
{
	_isopen			= false;
	_isinit			= false;
	_debug			= false;
	_mutex_valid	= false;
	pthread_mutex_init(&_mutex, 0);
	_mutex_valid	= true;
}

Device::~Device() throw()
{
	_isopen = false;

	if(_mutex_valid)
	{
		pthread_mutex_destroy(&_mutex);
		_mutex_valid = false;
	}
}

void Device::open() throw(string)
{
	if(_isopen)
		throw(string("Device::open: already open"));

	__open();
	_isopen = true;

	if(!_isinit)
	{
		__init();
		_isinit = true;
	}
}

void Device::close() throw()
{
	if(_isopen)
		__close();

	_isopen = false;
}

string Device::device_name() const throw()
{
	return(__device_name());
}

void Device::debug(bool onoff) throw()
{
	_debug = onoff;
}

void Device::_update(DeviceIOIterator io) throw(string)
{
	if((io->type == DeviceIO::no_type) || (io->direction == DeviceIO::no_direction))
		throw(string("Device::_update: i/o uninitialised"));

	__update(io);

	io->stamp_updated = time(0);
}

double Device::read(DeviceIOIterator io) throw(string)
{
	if((io->type == DeviceIO::no_type) || (io->direction == DeviceIO::no_direction))
		throw(string("Device::read: i/o uninitialised"));

	io->stamp_read = time(0);

	return(io->value);
}

void Device::write(DeviceIOIterator io, double value) throw(string)
{
	if((io->type == DeviceIO::no_type) || (io->direction == DeviceIO::no_direction))
		throw(string("Device::write: i/o uninitialised"));

	if(value < io->lower_boundary)
		throw(string("Device::write: value too low"));

	if(value > io->upper_boundary)
		throw(string("Device::write: value too high"));

	io->stamp_set	= time(0);
	io->value		= value;
}

void Device::resampling(DeviceIOIterator io, int value) throw(string)
{
	if((io->type == DeviceIO::no_type) || (io->direction == DeviceIO::no_direction))
		throw(string("Device::resampling: i/o uninitialised"));

	if(value < 0)
		throw(string("Device::resampling: value too low"));

	if(value > 256)
		throw(string("Device::resampling: value too high"));

	io->resampling = value;
}

void Device::update(DeviceIOIterator io) throw(string)
{
	return(_update(io));
}

void Device::update() throw(string)
{
	DeviceIOs::iterator io;

	for(io = _ios.begin(); io != _ios.end(); io++)
		_update(io);
}

void Device::lock() throw(string)
{
	if(_mutex_valid)
		pthread_mutex_lock(&_mutex);
	else
		throw(string("Device::lock:: mutex invalid"));
}

void Device::unlock() throw(string)
{
	if(_mutex_valid)
		pthread_mutex_unlock(&_mutex);
	else
		throw(string("Device::unlock:: mutex invalid"));
}

DeviceIOIterator Device::begin() throw(string)
{
	return(_ios.begin());
}

DeviceIOIterator Device::end() throw(string)
{
	return(_ios.end());
}

DeviceIOIterator Device::find(int ix) throw(string)
{
	if((ix < 0) || (ix >= (int)_ios.size()))
		throw(string("Device::find: index out of range"));

	return(begin() + ix);
}

DeviceIOIterator Device::find(string name) throw(string)
{
	DeviceIOIterator i;

	int numeric = strtoul(name.c_str(), 0, 10);
	string text	= lexical_cast<string>(numeric);

	if(name == text)
	{
		try
		{
			return(find(numeric));
		}
		catch(...)
		{
			return(end());
		}
	}

	for(i = begin(); i != end(); i++)
		if(i->name == name)
			return(i);

	return(end());
}
