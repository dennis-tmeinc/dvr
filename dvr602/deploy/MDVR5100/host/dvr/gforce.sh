#!/bin/sh

#int_gforce=`/davinci/dvr/cfg get glog gforce_log_enable`
timezone=`cat /var/dvr/TZ`

if [ $# -eq 0 ]; then
echo "Usage: $0 dest_path"
exit 1
fi

#if [ $int_gforce = 1 ]; then
  for file in `ls /var/dvr/*.log /var/dvr/*.peak`
  do
    echo "Converting $file..."
    /davinci/dvr/gforce_conv -s ${file} -d $1 -t ${timezone}
  done
#else
#  mv -f /var/dvr/*.log /var/dvr/*.peak $1 
#fi
