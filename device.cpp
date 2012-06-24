#include "device.h"

#include <deque>
using std::deque;

#include <numeric>
using std::accumulate;

Device::Device() throw(string)
{
	_isopen			= false;
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

void Device::_update(DeviceIO & io) throw(string)
{
	if((io.type == DeviceIO::no_type) || (io.direction == DeviceIO::no_direction))
		throw(string("Device::_update: i/o uninitialised"));

	__update(io);
}

double Device::read(DeviceIO & io, bool upd) throw(string)
{
	if((io.type == DeviceIO::no_type) || (io.direction == DeviceIO::no_direction))
		throw(string("Device::read: i/o uninitialised"));

	if((io.direction != DeviceIO::input) && (io.direction != DeviceIO::io))
		throw(string("Device::read: cannot read from this i/o"));

	if(upd)
		_update(io);

	return(io.value);
}

double Device::read(int id, bool upd) throw(string)
{
	if((id < 0) || (id > (int)_ios.size()))
		throw(string("Device::read: input id out of range"));

	return(read(_ios[id], upd));
}

void Device::write(DeviceIO & io, double value, bool upd) throw(string)
{
	if((io.type == DeviceIO::no_type) || (io.direction == DeviceIO::no_direction))
		throw(string("Device::write: i/o uninitialised"));

	if((io.direction != DeviceIO::output) && (io.direction != DeviceIO::io))
		throw(string("Device::write: cannot write to this i/o"));

	io.value = value;

	if(upd)
		_update(io);
}

void Device::write(int id, double value, bool upd) throw(string)
{
	if((id < 0) || (id > (int)_ios.size()))
		throw(string("Device::write: input id out of range"));

	write(_ios[id], value, upd);
}

void Device::update(DeviceIO & io) throw(string)
{
	return(_update(io));
}

void Device::update(int id) throw(string)
{
	if((id < 0) || (id > (int)_ios.size()))
		throw(string("Device::update: input id out of range"));

	update(_ios[id]);
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

DeviceIOIterator Device::begin() const throw(string)
{
	return(_ios.begin());
}

DeviceIOIterator Device::end() const throw(string)
{
	return(_ios.end());
}
