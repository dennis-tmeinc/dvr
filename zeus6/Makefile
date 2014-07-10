#
# Makefile for TVS projects
#

include config

SUBDIRS = cfg\
    dvrsvr\
    fb\
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
    tab102\
    lpc21isp\
    smartupload\
    smartupload2\
    filert\
    dosfsck_check

.PHONY : $(SUBDIRS) applications

applications: $(SUBDIRS)

$(SUBDIRS) :
	$(MAKE) -C $@

rel:
	date +%y.%m%d.%H > release

pwz5:
	cp cfg.h.pwz5 cfg.h
	cp config.pwz5 config

tvz6:
	cp cfg.h.tvsz6 cfg.h
	cp config.tvsz6 config
	
firmware:
	$(MAKE) applications
	$(MAKE) -C $(PROJDIR)/deploy/$(proj)

clean:
	for dir in $(SUBDIRS); do $(MAKE) -C $$dir clean ; done
	$(MAKE) -C $(PROJDIR)/deploy/$(proj) clean
	find -name \*~ -delete

