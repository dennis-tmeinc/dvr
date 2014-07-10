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

firmware:
	$(MAKE) applications
	$(MAKE) -C $(PROJDIR)/deploy/$(proj)

clean:
	for dir in $(SUBDIRS); do $(MAKE) -C $$dir clean ; done
	$(MAKE) -C $(PROJDIR)/deploy/$(proj) clean
	find -name \*~ -delete

