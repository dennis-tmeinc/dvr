#!/bin/sh

if [ -f /var/dvr/tvsconnectid ] ; then

    connectid=`cat /var/dvr/tvsconnectid`
    connecttype=${connectid%%[0-9]*}
    if [ ${connecttype} = "MF" -o ${connecttype} = "IS" -o ${connecttype} = "PL" ] ; then
        if [ -h /var/dvr/tvslogfile ] ; then
            cat /var/dvr/tvslogfile
        fi
    fi

fi

