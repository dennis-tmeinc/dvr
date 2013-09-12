#!/bin/sh

PATH=/bin:/davinci:/davinci/dvr
export PATH

remove_usbdrivers()
{
  [ -f /var/dvr/udhcpc.pid ] && kill `cat /var/dvr/udhcpc.pid`
  ifconfig rausb0 down
  /bin/rmmod rt73
  /bin/rmmod mos7840
  /bin/rmmod usbserial
}

# let tab102(live) finish its job
if [ -f /var/dvr/tab102live -a -f /var/dvr/tab102.pid ]; then
  echo 1 > /var/dvr/tab102check
  # wait until the tab102 download is done
  while [ -f /var/dvr/tab102check ]
  do
    sleep 1
  done
  # copy tab102 files
  smartdir=`cat /var/dvr/dvrcurdisk`/smartlog
  mkdir -p $smartdir && /davinci/dvr/gforce.sh $smartdir
fi

# do this only if tab102 was detected (on 510x)
if [ -f /var/dvr/tab102 ]
then
    # shut down wifi/serial to avoid usb/uart problems
    remove_usbdrivers

    # insert module in case it was removed by file retrieval stick
    insmod /davinci/usbserial.ko
    /bin/insmod /davinci/mos7840.ko

    # unmount every disk already mounted
    for f in /var/dvr/xdisk/* 
      do
      umount `cat $f`
      rm -f $f ;
    done
    
    # download tab102 data
    /davinci/dvr/tab102

    # we don't need usb/uart anymore    
    /bin/rmmod mos7840
    /bin/rmmod usbserial

    # mount disks again
    /davinci/dvr/tdevmount /davinci/dvr/tdevhotplug

    if [ -f /var/dvr/dvrcurdisk ]; then
      # copy tab102 files
      smartdir=`cat /var/dvr/dvrcurdisk`/smartlog
      mkdir -p $smartdir && /davinci/dvr/gforce.sh $smartdir

      # Hybrid disk?
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
    /bin/insmod /davinci/rt73.ko
elif [ -f /var/dvr/backupdisk -a -f /var/dvr/dvrsvr.pid ]; then
    # shut down wifi/serial to avoid usb/uart problems
    remove_usbdrivers

    # copy files for Hybrid disk (Flash --> Sata)
    if [ -f /var/dvr/dvrcurdisk ]; then
        # copy gforce files (if exist)
        smartdir=`cat /var/dvr/dvrcurdisk`/smartlog
        mkdir -p $smartdir && /davinci/dvr/gforce.sh $smartdir

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
    /bin/insmod /davinci/rt73.ko
elif [ -f /var/dvr/multidisk -a -f /var/dvr/dvrsvr.pid ]; then
    # shut down wifi/serial to avoid usb/uart problems
    remove_usbdrivers

    # ask dvrsvr to copy log files
    if [ -f /var/dvr/dvrcurdisk ]; then
        # copy gforce files (if exist)
        smartdir=`cat /var/dvr/dvrcurdisk`/smartlog
        mkdir -p $smartdir && /davinci/dvr/gforce.sh $smartdir

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
    /bin/insmod /davinci/rt73.ko
else
    # shut down wifi/serial to avoid usb/uart problems
    remove_usbdrivers

    if [ -f /var/dvr/dvrcurdisk ]; then
        # copy gforce files (if exist)
        smartdir=`cat /var/dvr/dvrcurdisk`/smartlog
        mkdir -p $smartdir && /davinci/dvr/gforce.sh $smartdir
    fi

    # insert rt73 for smartftp
    echo 1 > /var/dvr/usewifi # useless on 510x
    /bin/insmod /davinci/rt73.ko
fi
sleep 1


