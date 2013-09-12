#
# Makefile for Eagle32 DVR projects
# Changes:
# 08/14/2009 Harrison
#    1. added make for cfg
# 09/11/2009 Harrison
#    1. added tab102, smartupload
# 09/18/2009 Harrison
#    1. added filert
# 09/07/2011 Harrison
#    1. added vter, gforce_conv
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
		smartupload\
		smartupload2\
		filert\
		gforce_conv\
		vter

.PHONY : $(SUBDIRS) applications

applications: $(SUBDIRS)

$(SUBDIRS) :
	$(MAKE) -C $@

rel:
	date +%y.%m%d.%H > release

firmware:
	$(MAKE) applications 
	$(MAKE) -C deploy/$(proj) 

clean:
	for dir in $(SUBDIRS); do $(MAKE) -C $$dir clean ; done
	$(MAKE) -C deploy/$(proj) clean
	find -name \*~ -delete
