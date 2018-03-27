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
    zdaemon\
    rconn\
    fat32vol\
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

pwz6:
	cp cfg.h.pwz6 cfg.h
	cp config.pwz6 config

tvsz6:
	cp cfg.h.tvsz6 cfg.h
	cp config.tvsz6 config

pwz8:
	cp cfg.h.pwz8 cfg.h
	cp config.pwz8 config
	
firmware:
	$(MAKE) applications
	$(MAKE) -C $(PROJDIR)/deploy/$(proj)

clean:
	for dir in $(SUBDIRS); do $(MAKE) -C $$dir clean ; done
	$(MAKE) -C $(PROJDIR)/deploy/$(proj) clean
	find -name \*~ -delete

