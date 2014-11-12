#
# Makefile for Eagle32 DVR projects
# Changes:
# 08/14/2009 Harrison
#    1. added make for cfg
# 09/11/2009 Harrison
#    1. added tab102, smartupload
# 09/18/2009 Harrison
#    1. added filert
#
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
	cd dosfsck_check; make
firmware:
	cp cfg.h.m cfg.h ; make application 
#	cd deploy/MDVR614 ; ./mkdeploy MDVR614 1 3

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
	cd smartupload2;make clean

	cd filert;make clean
	cd dosfsck_check; make clean     

#	cd deploy/MDVR614 ; ./mkdeploy clean 0
