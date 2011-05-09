#!/bin/sh

PATH=/bin:/dav:/dav/dvr
export PATH

# do this only if tab102 was detected (on 510x)
if [ -f /var/dvr/tab102 ]
then
    # shut down wifi to avoid usb/uart problems
    ifconfig rausb0 down
    /bin/rmmod rt73

    # insert module in case it was removed by file retrieval stick
    #/bin/rmmod mos7840
    #/bin/rmmod usbserial

    #insmod /dav/usbserial.ko
    #/bin/insmod /dav/mos7840.ko

    # unmount every disk already mounted
    for f in /var/dvr/xdisk/* 
      do
      umount `cat $f`
      rm -f $f ;
    done
    
    # download tab102 data
    /dav/dvr/tab102

    # we don't need usb/uart anymore    
    #/bin/rmmod mos7840
    #/bin/rmmod usbserial

    # mount disks again
    /dav/dvr/tdevmount /dav/dvr/tdevhotplug

    if [ -f /var/dvr/dvrcurdisk ]; then
      # copy tab102 files
     # smartdir=`cat /var/dvr/dvrcurdisk`/smartlog
     # mkdir -p $smartdir && mv -f /var/dvr/*.log /var/dvr/*.peak $smartdir

      # Hybrid disk? (will never happen, Hybrid only on 602)
      if [ -f /var/dvr/backupdisk -a -f /var/dvr/dvrsvr.pid ]; then
        # ask dvrsvr to copy files for Hybrid disk
        echo 1 > /var/dvr/hybridcopy
        # give 5 sec for dvrsvr to create the copyack file
        sleep 5 
        # wait until the job is done
        while [ -f /var/dvr/hybridcopy -a -f /var/dvr/copyack ]
        do
          sleep 1
        done

      # Multiple disk?
      elif [ -f /var/dvr/multidisk -a -f /var/dvr/dvrsvr.pid ]; then
        # ask dvrsvr to copy log files
        echo 1 > /var/dvr/multicopy
        # give 5 sec for dvrsvr to create the copyack file
        sleep 5 
        # wait until the job is done
        while [ -f /var/dvr/multicopy -a -f /var/dvr/copyack ]
        do
          sleep 1
        done
      fi
    fi

    # insert rt73 for smartftp
    echo 1 > /var/dvr/usewifi # useless on 510x
    /bin/insmod /dav/rt73.ko
elif [ -f /var/dvr/backupdisk -a -f /var/dvr/dvrsvr.pid ]; then
    #/bin/rmmod mos7840
    #/bin/rmmod usbserial

    # shut down wifi to avoid usb/uart problems
    ifconfig rausb0 down
    /bin/rmmod rt73

    # copy files for Hybrid disk (Flash --> Sata)
    if [ -f /var/dvr/dvrcurdisk ]; then
        # ask dvrsvr to copy files for Hybrid disk
        echo 1 > /var/dvr/hybridcopy
        # give 5 sec for dvrsvr to create the copyack file
        sleep 5 
        # wait until the job is done
        while [ -f /var/dvr/hybridcopy -a -f /var/dvr/copyack ]
        do
          sleep 1
        done
    fi

    # insert rt73 for smartftp
    echo 1 > /var/dvr/usewifi # use wifi even on 602
    /bin/insmod /dav/rt73.ko
elif [ -f /var/dvr/multidisk -a -f /var/dvr/dvrsvr.pid ]; then
   # /bin/rmmod mos7840
   # /bin/rmmod usbserial

    # it can work, but just in case
    ifconfig rausb0 down
    /bin/rmmod rt73

    # ask dvrsvr to copy log files
    if [ -f /var/dvr/dvrcurdisk ]; then
        echo 1 > /var/dvr/multicopy
        # give 5 sec for dvrsvr to create the copyack file
        sleep 5 
        # wait until the job is done
        while [ -f /var/dvr/multicopy -a -f /var/dvr/copyack ]
	do
          sleep 1
        done
    fi

    # insert rt73 for smartftp
    echo 1 > /var/dvr/usewifi # useless on 510x
    /bin/insmod /dav/rt73.ko
else
    #/bin/rmmod mos7840
    #/bin/rmmod usbserial
    # it can work, but just in case
    ifconfig rausb0 down
    /bin/rmmod rt73
    # insert rt73 for smartftp
    echo 1 > /var/dvr/usewifi # useless on 510x
    /bin/insmod /dav/rt73.ko
fi
sleep 1


