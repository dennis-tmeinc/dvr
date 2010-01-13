#!/bin/sh

if [ -r /var/dvr/connectid ] ; then

    connectid=`cat /var/dvr/connectid`
    connecttype=${connectid%%[0-9]*}
    if [ ${connecttype} = "MF" ] ; then
        cat master_form.hi
    fi

fi

