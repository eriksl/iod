#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

#include "iod.h"

#include <string>
using std::string;

#include "device.h"
#include "http_server.h"
#include "syslog.h"

#if defined(DEVICE_K8055)
#include "device_k8055.h"
#endif

#if defined(DEVICE_HB627)
#include "device_hb627.h"
#endif

#if defined(DEVICE_ELV)
#include "device_elv.h"
#endif

static bool quit = false;

static void sigint(int)
{
	signal(SIGINT, SIG_DFL);
	quit = true;
}

static void _internal_throw(const string & error)
{
	string errormessage = string("error caught, message = ") + error;

	if(errno != 0)
	{
		errormessage += string(", system error = ");
		errormessage += strerror(errno);
	}

	vlog("%s\n", errormessage.c_str());
}

static Device * new_device(string unused devicetype, string unused devicenode, int unused address)
{
	Device * dev = 0;

#ifdef DEVICE_K8055
	if(devicetype == "k8055")
	{
		dev = new DeviceK8055(devicenode, address);
	}
#endif
#ifdef DEVICE_HB627
	if(devicetype == "hb627")
	{
		dev = new DeviceHb627(devicenode, address);
	}
#endif
#ifdef DEVICE_ELV
	if(devicetype == "elv")
	{
		dev = new DeviceElv(devicenode, address);
	}
#endif

	return(dev);
}

int main(int argc, char ** argv)
{
	int					opt;
	bool				foreground	= false;
	bool				quick		= false;
	string				keyscript;
	Device *			device = (Device *)0;

	try
	{
		string	devices;
		string	devicetype;
		string	devicenode = "/dev/ttyACM0";
		int		deviceaddress = 0;

#if defined(DEVICE_K8055)
		devices += " k8055";
#endif
#if defined(DEVICE_HB627)
		devices += " hb627";
#endif
#if defined(DEVICE_ELV)
		devices += " elv";
#endif
		while((opt = getopt(argc, argv, "fd:D:a:qv")) != -1)
		{
			switch(opt)
			{
				case('f'):
				{
					foreground = true;
					break;
				}

				case('v'):
				{
					foreground = true;
					debug = true;
					break;
				}

				case('q'):
				{
					quick = true;
					break;
				}

				case('d'):
				{
					devicetype = optarg;
					break;
				}

				case('D'):
				{
					devicenode = optarg;
					break;
				}

				case('a'):
				{
					deviceaddress = strtoul(optarg, 0, 0);
					break;
				}

				default:
				{
					errno = 0;
					throw(string("\nusage: iod -f -d type -q [-D device] [-a address]"
									"\ntype =") + devices + string(", device = /dev/*"
									"\n-f = foreground, -v = debug, -q = monitor software counters"));
				}
			}
		}

		if(devicetype == "help" || devicetype == "")
		{
			fprintf(stderr, "available devices:%s\n", devices.c_str());
			exit(-1);
		}

		device = new_device(devicetype, devicenode, deviceaddress);

		if(!device)
		{
			fprintf(stderr, "unknown device, available devices:%s\n", devices.c_str());
			exit(-1);
		}

		device->open();

		signal(SIGINT, sigint);

		if(!foreground)
		{
			isdaemon = true;
			daemon(0, 0);
		}
		else
			isdaemon = false;

		setresuid(65534, 65534, 65534);

#ifdef MHD_mode_multithread
		HttpServer * http_server = new HttpServer(device, 4242 + deviceaddress, true);
#else
#ifdef MHD_mode_singlethread
		HttpServer * http_server = new HttpServer(device, 4242 + deviceaddress, false);
#else
#error "Either MHD_mode_singlethread or MHD_mode_multithread should be set"
#endif
#endif
		struct timeval	tlast, tnow;
		uint64_t		last, now;

		gettimeofday(&tlast, 0);
		last = (tlast.tv_sec * 1000) + (tlast.tv_usec / 1000);

		while(!quit)
		{
			try
			{
				if(quick)
				{
					gettimeofday(&tnow, 0);
					now = (tnow.tv_sec * 1000) + (tnow.tv_usec / 1000);

					if((last + 1000) < now)
					{
						last = now;
						//device->lock();
						//device->activity();
						//device->unlock();
					}

					http_server->poll(0);
					//device->lock();
					//device->update_inputs();
					//device->unlock();
				}
				else
				{
					//device->lock();
					//device->activity();
					//device->unlock();
					http_server->poll(1000000);
				}

			}
			catch(string e)
			{
				int retry;

				for(retry = 0; !quit; retry++)
				{
					if(retry != 0)
						sleep(1);

					vlog("device exception: %s, retry %d\n", e.c_str(), retry);

					try
					{
						device->close();
						device->open();
						device->unlock();
						vlog("device OK, resuming\n");
						break;
					}
					catch(string e2)
					{
						continue;
					}
				}

				if(quit)
					break;

				continue;
			}
		}

		delete(http_server);
		http_server = 0;
		delete(device);
		device = 0;
	}
	catch(const string & error)
	{
		_internal_throw(error);
		exit(-1);
	}
	catch(const char * error)
	{
		_internal_throw(string(error));
		exit(-1);
	}
	catch(...)
	{
		_internal_throw(string("caught unknown error"));
		exit(-1);
	}

	if(quit)
		vlog("interrupt\n");

	exit(0);
}
