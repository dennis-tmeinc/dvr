#!/bin/sh

if [ -r /var/dvr/connectid ] ; then

    connectid=`cat /var/dvr/connectid`
    connecttype=${connectid%%[0-9]*}
    if [ ${connecttype} = "MF" -o ${connecttype} = "IN" ] ; then
        if [ -h /var/dvr/dvrlogfile ] ; then
            cat /var/dvr/dvrlogfile
        fi
    fi

fi

