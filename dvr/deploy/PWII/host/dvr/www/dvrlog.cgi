#!/bin/sh

if [ -f /var/dvr/tvsconnectid ] ; then

    connectid=`cat /var/dvr/tvsconnectid`
    connecttype=${connectid%%[0-9]*}
    if [ ${connecttype} = "MF" -o ${connecttype} = "IS" ] ; then
        if [ -h /var/dvr/dvrlogfile ] ; then
            cat /var/dvr/dvrlogfile
        fi
    fi

fi

