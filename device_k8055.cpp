#include <string.h>

#include "device_k8055.h"
#include "syslog.h"

DeviceK8055::DeviceK8055(string, int device_address) throw(string) : Device()
{
	_address		= device_address;
	_libusb_handle	= 0;
	_usb_timeout	= 25;

	if(_address < 0 || _address > 3)
		throw(string("DeviceK8055::DeviceK85055: board address out of bounds"));
}

DeviceK8055::~DeviceK8055()
{
	if(_libusb_handle)
		_close(_libusb_handle);

	_libusb_handle = 0;
}

usb_dev_handle * DeviceK8055::_open() throw()
{
	struct	usb_bus	*		busses = 0;
	struct	usb_bus *		bus = 0;
	struct	usb_device *	dev = 0;
	usb_dev_handle *		handle;
	ssize_t					length = PACKET_LENGTH;
	uint8_t 				packet[PACKET_LENGTH];
	ssize_t					rv;

	::usb_init();
	::usb_find_busses();
	::usb_find_devices();
	busses = ::usb_get_busses();

	for(bus = busses; bus; bus = bus->next)
	{
		for(dev = bus->devices; dev; dev = dev->next)
		{
			if((dev->descriptor.idVendor == 0x10cf) && (dev->descriptor.idProduct == (0x5500 + _address)))
				goto found;
		}
	}

	vlog("device not found\n");
	return(0);

found:

	if(!(handle = usb_open(dev)))
	{
		if(_debug)
			vlog("usb_open returns 0\n");

		return(0);
	}

	if(_debug)
		vlog("Velleman Device Found @ Address %s Vendor 0x0%x Product ID 0x0%x\n",
				dev->filename, dev->descriptor.idVendor,
				dev->descriptor.idProduct);

	usb_detach_kernel_driver_np(handle, 0);

	if(usb_claim_interface(handle, 0) < 0)
	{
		if(_debug)
			vlog("claim interface error: %s\n", usb_strerror());

		_close(handle);
		return(0);
	}

	usb_set_altinterface(handle, 0);
	usb_set_configuration(handle, 1);

	if(_debug)
		vlog("Found interface 0\n");

	memset(packet, 0, length);
	packet[0] = (uint8_t)CMD_RESET;

	rv = ::usb_interrupt_write(handle, USB_OUT_EP, (char *)packet, length, _usb_timeout);

	if((size_t)rv != length)
	{
		if(_debug)
			vlog("write error (reset), to write: %d, written: %d\n", length, rv);

		_close(handle);
		return(0);
	}

	return(handle);
}

void DeviceK8055::_close(usb_dev_handle * handle) throw()
{
	usb_close(handle);
}

void DeviceK8055::__open() throw(string)
{
	int i;

	_digital_in.reset();
	_digital_out.reset();

	for(i = 0; i < 2; i++)
		_digital_hw_counter[i] = 0;

	for(i = 0; i < 16; i++)
		_analog_in[i] = 0;

	for(i = 0; i < 2; i++)
		_analog_out[i] = 0;

	for(i = 0; i < 2; i++)
	{
		_digital_hw_counter[i] = 0;
		_reset_hw_counter(i);
	}

	_update_inputs();
}

void DeviceK8055::__close()
{
	if(_libusb_handle)
		_close(_libusb_handle);

	_libusb_handle = 0;
}

string DeviceK8055::__device_name() const
{
	return(string("Velleman k8055"));
}

int DeviceK8055::__digital_inputs() const
{
	return(5);
}

int DeviceK8055::__digital_outputs() const
{
	return(5);
}

int DeviceK8055::__digital_hw_counters() const
{
	return(2);
}

int DeviceK8055::__analog_inputs() const
{
	return(16);
}

int DeviceK8055::__analog_outputs() const
{
	return(2);
}

int DeviceK8055::__analog_input_max() const
{
	return(255);
}

int DeviceK8055::__analog_output_max() const
{
	return(255);
}

int DeviceK8055::__read_digital_input(int input) throw(string)
{
	return(_digital_in[input]);
}

void DeviceK8055::__write_digital_output(int output, bool onoff) throw(string)
{
	_digital_out[output] = onoff;
}

int DeviceK8055::__read_digital_hw_counter(int counter) throw(string)
{
	return(_digital_hw_counter[counter]);
}

void DeviceK8055::__reset_digital_hw_counter(int counter) throw(string)
{
	_reset_hw_counter(counter);
}

int DeviceK8055::__read_analog_input(int input) throw(string)
{
	uint8_t packet[PACKET_LENGTH];
	int		bank = (input >> 1) & 0x7;

	_select_analog_input(bank);
	_read_data(sizeof(packet), packet);

	_digital_in =
	(
		((packet[DIGITAL_INP_OFFSET] >> 4) & 0x03) |	/* Input 1 and 2 */
		((packet[DIGITAL_INP_OFFSET] << 2) & 0x04) |	/* Input 3 */
		((packet[DIGITAL_INP_OFFSET] >> 3) & 0x18)		/* Input 4 and 5 */
	);

	_analog_in[(bank << 1) | 0] = packet[ANALOG_1_OFFSET];
	_analog_in[(bank << 1) | 1] = packet[ANALOG_2_OFFSET];

	return(_analog_in[input]);
}

void DeviceK8055::__activity() throw(string)
{
	int bank;
		
	bank = (_digital_out.to_ulong() >> 5) & 0x07;
	bank = (~bank) & 0x07;

	_select_analog_input(bank);
}

void DeviceK8055::__write_analog_output(int output, int value) throw(string)
{
	_analog_out[output] = value;
}

void DeviceK8055::__update_inputs() throw(string)
{
	_update_inputs();
}

void DeviceK8055::__flush_outputs() throw(string)
{
	_flush_outputs();
}

void DeviceK8055::_read_data(size_t length, uint8_t * packet) throw(string)
{
	int rv;
	int	attempt;

	if(length < PACKET_LENGTH)
		throw(string("DeviceK8055::_read_data: packet length too small"));

	for(attempt = 0;; attempt++)
	{
		if(!_libusb_handle)
		{ 
			if(_debug)
				vlog("device not open\n");

			_libusb_handle = _open();
		}

		if(!_libusb_handle)
		{ 
			if(_debug)
				vlog("cannot reopen device\n");

			goto retry;
		}

		memset(packet, 0, length);
		rv = ::usb_interrupt_read(_libusb_handle, USB_INP_EP, (char *)packet, length, _usb_timeout);

		if(((size_t)rv == length) && (packet[1] & 0x01))
			break;

		if(_debug)
			vlog("read error\n");

retry:
		vlog("attempt %d\n", attempt);

		if(_libusb_handle)
			_close(_libusb_handle);

		_libusb_handle = 0;
		sleep(1);
	}
}

void DeviceK8055::_write_data(size_t length, uint8_t * packet) throw(string)
{
	int		rv;
	int		attempt;

	if(length < PACKET_LENGTH)
		throw(string("DeviceK8055::_write_data: packet length too small"));

	for(attempt = 0;; attempt++)
	{
		if(!_libusb_handle)
		{ 
			if(_debug)
				vlog("device not open\n");

			_libusb_handle = _open();
		}

		if(!_libusb_handle)
		{ 
			if(_debug)
				vlog("cannot reopen device\n");

			goto retry;
		}

		rv = ::usb_interrupt_write(_libusb_handle, USB_OUT_EP, (char *)packet, length, _usb_timeout);

		if((size_t)rv != length)
		{
			if(_debug)
				vlog("write error, to be written: %d, wrote: %d\n", length, rv);

			goto retry;
		}

		memset(packet, 0, length);
		rv = ::usb_interrupt_read(_libusb_handle, USB_INP_EP, (char *)packet, length, _usb_timeout);

		if(((size_t)rv == length) && (packet[1] & 0x01))
			break;

		vlog("read error");

retry:
		vlog("attempt %d\n", attempt);

		if(_libusb_handle)
			_close(_libusb_handle);

		_libusb_handle = 0;
		sleep(1);
	}
}

void DeviceK8055::_write_data(enum_command cmd) throw(string)
{
	int			length = PACKET_LENGTH;
	uint8_t *	packet = (uint8_t *)alloca(length);

	memset(packet, 0, length);
	packet[0] = (uint8_t)cmd;

	_write_data(length, packet);
}

void DeviceK8055::_reset_hw_counter(int counter) throw(string)
{
	_write_data((counter == 0) ? CMD_RESET_COUNTER_1 : CMD_RESET_COUNTER_2);
	_digital_hw_counter[counter] = 0;
}

void DeviceK8055::_update_inputs() throw(string)
{
	uint8_t packet[PACKET_LENGTH];

	_read_data(sizeof(packet), packet);

	_digital_in =
	(
		((packet[DIGITAL_INP_OFFSET] >> 4) & 0x03) |	/* Input 1 and 2 */
		((packet[DIGITAL_INP_OFFSET] << 2) & 0x04) |	/* Input 3 */
		((packet[DIGITAL_INP_OFFSET] >> 3) & 0x18)		/* Input 4 and 5 */
	);

	_digital_hw_counter[0] = (packet[COUNTER_1_OFFSET + 1] << 8) | (packet[COUNTER_1_OFFSET + 0]);
	_digital_hw_counter[1] = (packet[COUNTER_2_OFFSET + 1] << 8) | (packet[COUNTER_2_OFFSET + 0]);
}

void DeviceK8055::_flush_outputs() throw(string)
{
	uint8_t packet[PACKET_LENGTH];

	packet[0] = CMD_SET_ANALOG_DIGITAL;
	packet[1] = _digital_out.to_ulong();
	packet[2] = _analog_out[0];
	packet[3] = _analog_out[1];

	_write_data(sizeof(packet), packet);
}

void DeviceK8055::_select_analog_input(int bank) throw(string)
{
	_digital_out[5] = bank & 0x01;
	_digital_out[6]	= bank & 0x02;
	_digital_out[7] = bank & 0x04;
	_flush_outputs();
}

void DeviceK8055::debug(bool onoff)
{
	_debug = onoff;
}
