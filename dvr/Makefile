#
# Makefile for Eagle32 DVR projects
#

SUBDIRS = fb\
       dvrsvr\
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
       tmeft\
       usbid\
       usbnotify

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


rel:
	date +%y.%m%d.%H > release

tvs:
	cp cfg.h.tvs cfg.h
	$(MAKE) applications 
	cd deploy/TVS21 ; ./mkdeploy TVS21 1 

pwii:
	cp cfg.h.pwii cfg.h
	$(MAKE) applications 
	cd deploy/PWII ; ./mkdeploy PWII 1 

pw34:
	cp cfg.h.pw34 cfg.h
	$(MAKE) applications
	cd deploy/PW34 ; ./mkdeploy PW34 1 
    
clean:
	for dir in $(SUBDIRS); do $(MAKE) -C $$dir clean ; done
	$(MAKE) -C test clean
	cd deploy/TVS21 ; ./mkdeploy clean 0
	cd deploy/PWII ; ./mkdeploy clean 0
	find -name \*~ -delete
