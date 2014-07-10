#!/bin/sh

if [ -r /var/dvr/connectid ] ; then

    connectid=`cat /var/dvr/connectid`
    connecttype=${connectid%%[0-9]*}
    if [ ${connecttype} = "MF" -o ${connecttype} = "PL" ] ; then
        if [ -h /var/dvr/tvslogfile ] ; then
            cat /var/dvr/tvslogfile
        fi
    fi

fi

