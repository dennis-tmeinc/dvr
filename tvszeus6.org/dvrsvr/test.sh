#!/bin/sh

PATH=/bin:/davinci:/davinci/dvr
export PATH

if [ -f /var/dvr/dvrcurdisk ]; then
  smartdir=`cat /var/dvr/dvrcurdisk`/smartlog
  mkdir -p $smartdir && mv -f /var/dvr/*.log /var/dvr/*.peak $smartdir
  if [ -f /var/dvr/backupdisk ]; then
        # ask dvrsvr to copy files for Hybrid disk
        echo 1 > /var/dvr/hybridcopy
        # wait until the job is done
        while [ -f /var/dvr/hybridcopy ]
        do
          sleep 1
        done
  fi
fi
