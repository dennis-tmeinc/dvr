#!/bin/sh

if [ -f /var/dvr/tvsconnectid ] ; then

    connectid=`cat /var/dvr/tvsconnectid`
    connecttype=${connectid%%[0-9]*}
    if [ ${connecttype} = "MF" ] ; then
        cat tvsidform.hi
    fi

fi

