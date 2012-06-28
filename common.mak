WARNINGS		= -Wall -Wextra -Wshadow -Wundef -Wformat=2 -Winit-self -Wunused -Werror -Wpointer-arith -Wcast-qual -Wmultichar -fno-rtti

ifneq ($(DEBUG), on)
CPPFLAGS		+= -O2 $(WARNINGS)
LDFLAGS			+= -s
else
CPPFLAGS		+= -O0 -g $(WARNINGS)
LDFLAGS			+= -g
endif

ifeq ($(TARGET), x86_64)
	CPPFLAGS	+= -DTARGET=x86_64
endif

ifeq ($(TARGET), i386)
	CPPFLAGS	+= -m32 -DTARGET=i386
	LDFLAGS		+= -m32
endif

ifeq ($(TARGET), mipsel)
	EXECPREFIX	= /home/erik/src/openpli/build-dm8000/tmp/cross/mipsel-linux/bin/
	CPPFLAGS	+= -I/home/erik/src/openpli/build-dm8000/tmp/cross/mipsel-linux/include
	CPPFLAGS	+= -I/home/erik/src/libmicrohttpd/mipsel/usr/include
	CPPFLAGS	+= -DTARGET=mipsel
	LDFLAGS		+= -L/home/erik/src/libmicrohttpd/mipsel/usr/lib
endif

ifeq ($(TARGET), ppc)
	EXECPREFIX	= /home/erik/src/tuxbox/root/cdk/bin/powerpc-tuxbox-linux-gnu-
	CPPFLAGS	+= -I/home/erik/src/tuxbox/root/cdkroot/include
	CPPFLAGS	+= -DTARGET=ppc
endif

CC	= $(EXECPREFIX)gcc
CPP	= $(EXECPREFIX)g++

.PHONY:		all depend clean install rpm dpkg

DATE			=	`date '+%Y%m%d%H%M'`
VERSION			=	daily-$(DATE)

RPM				=	`pwd`/rpm
RPMTARBALL		=	$(RPM)/$(PROGRAM).tar
RPMPKGDIR		=	$(RPM)/$(TARGET)
RPMDEBUGDIR		=	$(RPMPKGDIR)/debug
RPMBUILDDIR		=	$(RPM)/build
RPMTMP			=	$(RPM)/tmp
RPMSPECIN		=	$(RPM)/$(PROGRAM).spec
RPMSPECOUTDIR	=	$(RPMBUILDDIR)/rpm
RPMSPECOUT		=	$(RPMSPECOUTDIR)/$(PROGRAM).spec

DPKG			=	`pwd`/dpkg
DPKGTARBALL 	=	$(DPKG)/$(PROGRAM).tar
DPKGDEBIANDIR	=	$(DPKG)/DEBIAN
DPKGCONFIGURE	=	$(DPKG)/configure.dpkg
DPKGPKGDIR		=	$(DPKG)/$(TARGET)
DPKGBUILDDIR	=	$(DPKG)/build
DPKGDESTDIR		=	$(DPKGBUILDDIR)/dpkg/root
DPKGDEBIANDESTD	=	$(DPKGDESTDIR)/DEBIAN
DPKGCHANGELOG	=	$(DPKGDEBIANDESTD)/changelog
DPKGCONTROL		=	$(DPKGDEBIANDESTD)/control

all:		depend $(PROGRAM)

ifeq ($(DEPS), "")
else
-include .deps
endif

depend:		.deps

.deps:		$(DEPS)
			@cat $^ /dev/null > $@

.%.d:		%.cpp
			@$(CPP) $(CPPFLAGS) -M $^ -o $@

.%.d:		%.c
			@$(CPP) $(CPPFLAGS) -M $^ -o $@

%.o:		%.cpp
			@echo "CPP $< -> $@"
			@$(CPP) $(CPPFLAGS) -c $< -o $@

$(PROGRAM):	$(OBJS)
			@echo "LD $@"
			@$(CPP) $(LDFLAGS) $^ $(LDLIBS) -o $@ 

install:	$(PROGRAM)
			@echo "INSTALL $(PROGRAM) -> $(DESTDIR)/usr/bin"
			@mkdir -p $(DESTDIR)/usr/bin
			@cp $(PROGRAM) $(DESTDIR)/usr/bin
			@-chown root:root $(DESTDIR)/usr/bin/$(PROGRAM)
			@-chmod 755 $(DESTDIR)/usr/bin/$(PROGRAM)

clean:
			@echo "CLEAN"
			@-git clean -f -d -q 2> /dev/null || true
			rm -f $(PROGRAM) $(OBJS) 2> /dev/null || true

rpm:
#
			@echo "PREPARE $(VERSION)"
			@-rm -rf $(RPMPKGDIR) $(RPMBUILDDIR) $(RPMTMP) 2> /dev/null || true
			@mkdir -p $(RPMPKGDIR) $(RPMBUILDDIR) $(RPMTMP) $(RPMSPECOUTDIR)
			@sed $(RPMSPECIN) -e "s/%define dateversion.*/%define dateversion $(DATE)/" > $(RPMSPECOUT)
#
			@echo "TAR $(RPMTARBALL)"
			@-rm -f $(RPMTARBALL) 2> /dev/null
#			tar cf $(RPMTARBALL) -C .. --exclude rpm --exclude .git $(PROGRAM)
			tar cf $(RPMTARBALL) -C .. --exclude rpm $(PROGRAM)
#
			@echo "CREATE RPM $(RPMSPECOUT)"
			@rpmbuild -bb $(RPMSPECOUT)
#
			@-rm -rf $(RPMDEBUGDIR) 2> /dev/null || true
			@-mkdir -p $(RPMDEBUGDIR) 2> /dev/null || true
			@-mv rpm/$(TARGET)/$(PROGRAM)-debuginfo-*.rpm rpm/$(TARGET)/debug 2> /dev/null || true
			@-rm -rf $(RPMBUILDDIR) $(RPMTMP)

dpkg:
#
			@echo "PREPARE $(VERSION)"
			@-rm -f $(DPKGPKGDIR) $(DPKGBUILDDIR) $(DPKGDESTDIR) 2> /dev/null || true
			@mkdir -p $(DPKGPKGDIR) $(DPKGBUILDDIR)
#
			@echo "TAR $(DPKGTARBALL)"
			@-rm -f $(DPKGTARBALL) 2> /dev/null
			@git archive -o $(DPKGTARBALL) HEAD
			@tar xf $(DPKGTARBALL) -C $(DPKGBUILDDIR)
#
			@echo "DUP $(DPKGDEBIANDIR)"
			@mkdir -p $(DPKGDEBIANDESTD)
			@cp $(DPKGDEBIANDIR)/* $(DPKGDEBIANDESTD)
#
			@echo "CHANGELOG $(DPKGCHANGELOG)"
			@-rm -f $(DPKGCHANGELOG) 2> /dev/null
			@echo "$(PROGRAM) ($(VERSION)) stable; urgency=low" > $(DPKGCHANGELOG)
			@echo >> $(DPKGCHANGELOG)
			@git log | head -100 >> $(DPKGCHANGELOG)
			@sed --in-place $(DPKGCONTROL) -e "s/^Version:.*/Version: `date "+%Y%m%d"`/"
			@sed --in-place $(DPKGCONTROL) -e "s/^Architecture:.*/Architecture: $(TARGET)/"
#
			@echo "CONFIGURE $(DPKGCONFIGURE)"
			@(cd $(DPKGBUILDDIR); $(DPKGCONFIGURE))
#
			@echo "BUILD $(DPKGBUILDDIR)"
			@$(MAKE) -C $(DPKGBUILDDIR) all
#
			@echo "CREATE DEB"
			@fakeroot /bin/sh -c "\
				$(MAKE) -C $(DPKGBUILDDIR) DESTDIR=$(DPKGDESTDIR) install; \
				dpkg --build $(DPKGDESTDIR) $(DPKGPKGDIR)"

rpminstall:
			@echo "INSTALL"
			@sudo rpm -Uvh --force rpm/$(TARGET)/*.rpm
