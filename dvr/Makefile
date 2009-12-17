#
# Makefile for Eagle32 DVR projects
#

applications: 

#dvrsvr/libfbdraw.so:
	cd fb ; make 
    
#dvrsvr/dvrsvr:
	cd dvrsvr ; make 

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

# cfg utility
	cd cfg; make

# tmefile
	cd tmeft; make

mdvr:
	cp cfg.h.mdvr cfg.h
	make applications
	cd deploy/MDVR5100 ; ./mkdeploy MDVR602 1 
	cd deploy/MDVR5100 ; ./mkdeploy MDVR5102 1
	cd deploy/MDVR5100 ; ./mkdeploy MDVR5104 2 
	cd deploy/MDVR5100 ; ./mkdeploy MDVR5108 4 

mdvr_special:
	make application 
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
	make application 
	cd deploy/MDVR5100 ; ./mkdeploy MDVR602 1  cycle
	cd deploy/MDVR5100 ; ./mkdeploy MDVR5102 1 cycle
	cd deploy/MDVR5100 ; ./mkdeploy MDVR5104 2 cycle
	cd deploy/MDVR5100 ; ./mkdeploy MDVR5108 4 cycle


release:
	date +%y.%m%d.%H > release

tvs:
	cp cfg.h.tvs cfg.h
	make applications 
	cd deploy/TVS21 ; ./mkdeploy TVS21 1 


pwii:
	cp cfg.h.pwii cfg.h
	make applications 
	cd deploy/PWII ; ./mkdeploy PWII 1 

clean:
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
    
	cd cfg; make clean

	cd tmeft; make clean

	cd deploy/PWII ; ./mkdeploy clean 0

	cd deploy/TVS21 ; ./mkdeploy clean 0
