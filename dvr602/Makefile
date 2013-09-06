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

application: 

#cfg/cfg:
	cd cfg ; make 

#dvrsvr/dvrsvr:
	cd dvrsvr ; make 

#dvrsvr/libfbdraw.so:
	cd fb ; make  
    
#dvrtime/dvrtime:
	cd dvrtime ; make

#eaglehost/eaglehost:
#eaglehost/slaveip:
	cd eaglehost; make 

#glog/glog:
	cd glog;make

#httpd/eaglehttpd:
	cd httpd;make
    
#ioprocess/ioprocess:
	cd ioprocess;make

#nfile/nfile:
	cd nfile; make 
    
#sfx/sfx:
#sfx/mksfx:
	cd sfx;	make
    
#tdev/tdevd:
#tdev/tdevmount:
	cd tdev;make
    
#usbgenkey/usbgenkey:
#usbgenkey/usbcheck :
	cd usbgenkey;make

#volumeid/md5check
	cd volumeid; make

#tab102/tab102
	cd tab102; make

#smartupload/smartftp
	cd smartupload; make
	cd smartupload2; make

#filert/filert
	cd filert; make

#gforce_conv
	cd gforce_conv; make

#vter
	cd vter; make

firmware:
	cp cfg.h.m cfg.h ; make application 
	cd deploy/MDVR5100 ; ./mkdeploy MDVR602 1 
	cd deploy/MDVR5100 ; ./mkdeploy MDVR5102 1
	cd deploy/MDVR5100 ; ./mkdeploy MDVR5104 2 
	cd deploy/MDVR5100 ; ./mkdeploy MDVR5108 4 

firmware_special:
	cp cfg.h.m cfg.h ; make application 
	cd deploy/MDVR5100 ; ./mkdeploy MDVR602 1
	cd deploy/MDVR5100 ; ./mkdeploy MDVR602 1 4
	cd deploy/MDVR5100 ; ./mkdeploy MDVR602 1 5
	cd deploy/MDVR5100 ; ./mkdeploy MDVR5102 1
	cd deploy/MDVR5100 ; ./mkdeploy MDVR5104 2 
	cd deploy/MDVR5100 ; ./mkdeploy MDVR5108 4 
	cd deploy/MDVR5100 ; ./mkdeploy MDVR602m 1
	cd deploy/MDVR5100 ; ./mkdeploy MDVR602m 1 4
	cd deploy/MDVR5100 ; ./mkdeploy MDVR5102m 1
	cd deploy/MDVR5100 ; ./mkdeploy MDVR5104m 2 
	cd deploy/MDVR5100 ; ./mkdeploy MDVR5108m 4 

firmware_cycle:
	cp cfg.h.cycle cfg.h ; make application 
	cd deploy/MDVR5100 ; ./mkdeploy MDVR602 1  cycle
	cd deploy/MDVR5100 ; ./mkdeploy MDVR5102 1 cycle
	cd deploy/MDVR5100 ; ./mkdeploy MDVR5104 2 cycle
	cd deploy/MDVR5100 ; ./mkdeploy MDVR5108 4 cycle

clean:
	cd cfg;make clean

	cd dvrsvr;make clean

	cd fb;make clean

	cd dvrtime;make clean

	cd eaglehost;make clean

	cd glog;make clean
    
	cd httpd;make clean
    
	cd ioprocess;make clean

	cd nfile;make clean
    
	cd sfx;make clean
    
	cd tdev;make clean
    
	cd usbgenkey;make clean
	cd volumeid;make clean
	cd tab102;make clean
	cd smartupload;make clean
	cd filert;make clean
	cd gforce_conv; make clean
	cd vter; make clean    
	cd deploy/MDVR5100 ; ./mkdeploy clean 0
