#ifndef _device_k8055_h
#define _device_k8055_h

#include <stdint.h>
#include <usb.h>

#include <string>
using std::string;

#include <bitset>
using std::bitset;

#include "device.h"

class DeviceK8055 : public Device
{
	private:

		typedef bitset<8>	bitset8;

		typedef enum
		{
			PACKET_LENGTH			= 8,
			USB_OUT_EP				= 0x01,
			USB_INP_EP				= 0x81,
		} enum_misc;

		typedef enum
		{
			DIGITAL_INP_OFFSET		= 0,
			DIGITAL_OUT_OFFSET		= 1,
			ANALOG_1_OFFSET			= 2,
			ANALOG_2_OFFSET			= 3,
			COUNTER_1_OFFSET		= 4,
			COUNTER_2_OFFSET		= 6,
		} enum_offset;

		typedef enum
		{
			CMD_RESET				= 0x00,
			CMD_SET_DEBOUNCE_1		= 0x01,
			CMD_SET_DEBOUNCE_2		= 0x01,
			CMD_RESET_COUNTER_1		= 0x03,
			CMD_RESET_COUNTER_2		= 0x04,
			CMD_SET_ANALOG_DIGITAL	= 0x05,
		} enum_command;

		bitset8	_digital_in;
		bitset8	_digital_out;
		int		_digital_hw_counter[2];
		int		_analog_in[16];
		int		_analog_out[2];

		int					_usb_timeout;
		int					_address;
		usb_dev_handle *	_libusb_handle;
		bool				_debug;

		usb_dev_handle *	_open()						throw();
		void				_close(usb_dev_handle *)	throw();

	protected:

		string	__device_name()										const;

		int		__digital_inputs()									const;
		int		__digital_outputs()									const;
		int		__digital_hw_counters()								const;
		int		__analog_inputs()									const;
		int		__analog_outputs()									const;

		int		__analog_input_max()								const;
		int		__analog_output_max()								const;

		void	__open()											throw(string);
		void	__close();
		int		__read_digital_input(int input)						throw(string);
		void	__write_digital_output(int output, bool onoff)		throw(string);
		int		__read_digital_hw_counter(int counter)				throw(string);
		void	__reset_digital_hw_counter(int counter)				throw(string);
		int		__read_analog_input(int input)						throw(string);
		void	__write_analog_output(int output, int value)		throw(string);
		void	__update_inputs()									throw(string);
		void	__flush_outputs()									throw(string);
		void	__activity()										throw(string);

	private:

		void	_read_data(size_t length, uint8_t * packet)			throw(string);
		void	_write_data(size_t length, uint8_t * packet)		throw(string);
		void	_write_data(enum_command cmd)						throw(string);
		void	_reset_hw_counter(int counter)						throw(string);
		void	_update_inputs()									throw(string);
		void	_flush_outputs()									throw(string);
		void	_select_analog_input(int bank)						throw(string);

	public:

				DeviceK8055(string device_node, int device_address)	throw(string);
		virtual	~DeviceK8055();

		void	debug(bool onoff);
};
#endif
