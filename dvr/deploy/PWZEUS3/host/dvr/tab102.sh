#!/bin/sh

PATH=/bin:/dav:/dav/dvr
export PATH

# do this only if tab102 was detected (on 510x)
if [ -f /var/dvr/tab102 ];
then
    
    # download tab102 data
   # /dav/dvr/tab102
    echo 1 > /var/dvr/tab102check
    # wait until the tab102b downloading done
    while [-f /var/dvr/tab102check -a -f /var/dvr/tab102.pid]
    do 
      sleep 1
    done

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

elif [ -f /var/dvr/backupdisk -a -f /var/dvr/dvrsvr.pid ]; then

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

elif [ -f /var/dvr/multidisk -a -f /var/dvr/dvrsvr.pid ]; then
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
fi
sleep 1


