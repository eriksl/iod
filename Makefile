TARGET	= x86_64
DEBUG	= off
PROGRAM	= iod
OBJS	= iod.o device.o http_server.o http_page.o syslog.o
DEPS	= .iod.d .device.d .http_server.d .http_page.d .syslog.d

ifeq ($(TARGET), x86_64)
	ENABLE_DEVICE_K8055		= 0
	ENABLE_DEVICE_HB627		= 0
	ENABLE_DEVICE_ELV		= 1
	CPPFLAGS				+= -DMHD_mode_multithread
	LDLIBS					+= -Wl,-Bstatic -lmicrohttpd -Wl,-Bdynamic -lpthread
endif

ifeq ($(TARGET), i386)
	ENABLE_DEVICE_K8055		= 0
	ENABLE_DEVICE_HB627		= 0
	ENABLE_DEVICE_ELV		= 1
	CPPFLAGS				+= -DMHD_mode_multithread
	LDLIBS					+= -Wl,-Bstatic -lmicrohttpd -Wl,-Bdynamic -lpthread
endif

ifeq ($(TARGET), mipsel)
	ENABLE_DEVICE_K8055		= 0
	ENABLE_DEVICE_HB627		= 0
	ENABLE_DEVICE_ELV		= 1
	CPPFLAGS				+= -DMHD_mode_multithread
	LDLIBS					+= -Wl,-Bstatic -lmicrohttpd -Wl,-Bdynamic -lpthread
endif

ifeq ($(TARGET), ppc)
	ENABLE_DEVICE_K8055		= 0
	ENABLE_DEVICE_HB627		= 0
	ENABLE_DEVICE_ELV		= 0
	CPPFLAGS				+= -DMHD_mode_singlethread
	LDLIBS					= -Wl,-Bstatic -lmicrohttpd -Wl,-Bdynamic -lpthread
endif

ifeq ($(ENABLE_DEVICE_K8055),1)
	OBJS		+= device_k8055.o
	DEPS		+= .device_k8055.d
	CPPFLAGS	+= -DDEVICE_K8055
	LDFLAGS		+= -lusb
endif

ifeq ($(ENABLE_DEVICE_HB627),1)
	OBJS		+= device_hb627.o
	DEPS		+= .device_hb627.d
	CPPFLAGS	+= -DDEVICE_HB627
endif

ifeq ($(ENABLE_DEVICE_ELV),1)
	OBJS		+= device_elv.o
	DEPS		+= .device_elv.d
	CPPFLAGS	+= -DDEVICE_ELV
	LDFLAGS		+= -lboost_regex
endif

include common.mak
