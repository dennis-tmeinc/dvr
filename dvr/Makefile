#
# Makefile for Eagle32 DVR projects
#

include config

CFLAGS= -O2 -s
export CFLAGS

SUBDIRS = dvrsvr\
       dvrtime\
       eaglehost\
       glog\
       httpd\
       ioprocess\
       nfile\
       sfx\
       tdev\
       usbgenkey\
       volumeid\
       cfg\
       dhcp\
       tmeft\
       usbid\
       usbnotify\
       ssl \
       eaglesvr \
       lpc21isp \
       dosfstools

.PHONY : $(SUBDIRS) applications

applications: $(SUBDIRS)

$(SUBDIRS) :
	$(MAKE) -C $@

mdvr:
	cp cfg.h.mdvr cfg.h
	$(MAKE) applications
	cd deploy/MDVR5100 ; ./mkdeploy MDVR602 1
	cd deploy/MDVR5100 ; ./mkdeploy MDVR5102 1
	cd deploy/MDVR5100 ; ./mkdeploy MDVR5104 2
	cd deploy/MDVR5100 ; ./mkdeploy MDVR5108 4

mdvr_special:
	$(MAKE) application
	cd deploy/MDVR5100 ; ./mkdeploy MDVR5108 4
	cd deploy/MDVR5100 ; ./mkdeploy MDVR5108 4 1
	cd deploy/MDVR5100 ; ./mkdeploy MDVR5108 4 2
	cd deploy/MDVR5100 ; ./mkdeploy MDVR5104 2
	cd deploy/MDVR5100 ; ./mkdeploy MDVR5104 2 1
	cd deploy/MDVR5100 ; ./mkdeploy MDVR5104 2 2
	cd deploy/MDVR5100 ; ./mkdeploy MDVR5102 1
	cd deploy/MDVR5100 ; ./mkdeploy MDVR5102 1 1
	cd deploy/MDVR5100 ; ./mkdeploy MDVR5102 1 2

firmware_cycle:
	cp -u cfg.h.cycle cfg.h
	$(MAKE) application
	cd deploy/MDVR5100 ; ./mkdeploy MDVR602 1  cycle
	cd deploy/MDVR5100 ; ./mkdeploy MDVR5102 1 cycle
	cd deploy/MDVR5100 ; ./mkdeploy MDVR5104 2 cycle
	cd deploy/MDVR5100 ; ./mkdeploy MDVR5108 4 cycle

ssh:
	$(MAKE) -C dropbear

rel:
	date +%y.%m%d.%H > release

app_tvs:
	echo PROJDIR=`pwd` > config
	echo "proj=TVS21" >> config
	echo "EAGLE=eagle32" >> config
	cp cfg.h.TVS21 cfg.h

app_tvs22:
	echo PROJDIR=`pwd` > config
	echo "proj=TVS22" >> config
	echo "EAGLE=eagle32" >> config
	cp cfg.h.TVS21 cfg.h

app_tvsz3:
	echo PROJDIR=`pwd` > config
	echo "proj=TVSZ3" >> config
	echo "EAGLE=zeus3" >> config
	cp cfg.h.TVSZ3 cfg.h

app_pwii:
	echo PROJDIR=`pwd` > config
	echo "proj=PWII" >> config
	echo "EAGLE=eagle32" >> config
	cp cfg.h.PWII cfg.h

app_pw34:
	echo PROJDIR=`pwd` > config
	echo "proj=PW34" >> config
	echo "EAGLE=eagle34" >> config
	cp cfg.h.PW34 cfg.h

app_pwlite:
	echo PROJDIR=`pwd` > config
	echo "proj=PWLITE" >> config
	echo "EAGLE=eagle34" >> config
	cp cfg.h.PW34 cfg.h

app_zeus:
	echo PROJDIR=`pwd` > config
	echo "proj=PWZEUS" >> config
	echo "EAGLE=eagle34" >> config
	cp cfg.h.zeus cfg.h

app_pwzeus3:
	echo PROJDIR=`pwd` > config
	echo "proj=PWZEUS3" >> config
	echo "EAGLE=zeus3" >> config
	cp cfg.h.zeus3 cfg.h

app_mv:
	echo PROJDIR=`pwd` > config
	echo "proj=PW34" >> config
	echo "EAGLE=mv" >> config
	cp cfg.h.MV cfg.h

firmware:
	$(MAKE) applications
	$(MAKE) -C deploy/$(proj)

pw:
	$(MAKE) app_pwii
	$(MAKE) firmware
	$(MAKE) clean
	$(MAKE) app_pw34
	$(MAKE) firmware
	$(MAKE) clean
	$(MAKE) app_pwlite
	$(MAKE) firmware
	$(MAKE) clean
	$(MAKE) app_zeus
	$(MAKE) firmware
	$(MAKE) clean
	$(MAKE) app_pwzeus3
	$(MAKE) firmware

tvs:
	$(MAKE) app_tvs
	$(MAKE) firmware
	$(MAKE) clean
	$(MAKE) app_tvs22
	$(MAKE) firmware
	$(MAKE) clean

all: tvs pw

clean:
	for dir in $(SUBDIRS); do $(MAKE) -C $$dir clean ; done
	$(MAKE) -C dropbear clean
	$(MAKE) -C test clean
	$(MAKE) -C deploy/$(proj) clean
	find -name \*~ -delete
