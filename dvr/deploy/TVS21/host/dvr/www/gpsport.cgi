#!/bin/sh
for ss in /dev/ttyS? /dev/ttyUSB? ; do
  ssn=""
  if [ $ss = "/dev/ttyS0" ] ; then
        ssn="COM1"
  elif [ $ss = "/dev/ttyS1" ] ; then
        ssn="COM2"
  elif [ $ss = "/dev/ttyS2" ] ; then
        ssn="COM3"
  elif [ $ss = "/dev/ttyS3" ] ; then
        ssn="COM4"
  elif [ $ss = "/dev/ttyS4" ] ; then
        ssn="COM5"
  elif [ $ss = "/dev/ttyS5" ] ; then
        ssn="COM6"
  elif [ ${ss%?} = "/dev/ttyUSB" ] ; then
        ssn=${ss#/dev/tty}
  else
        continue
  fi
  if [ -c ${ss} ] ; then
    echo "<option value=\"${ss}\"> ${ssn} </option>"
  fi
done
