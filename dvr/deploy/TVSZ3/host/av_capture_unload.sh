#!/bin/sh

killall -KILL av_server.out
killall wis-streamer

./csl_unload.sh
./drv_unload.sh

cd ../dvsdk/dm365

rmmod cmemk 
rmmod irqk 
rmmod edmak 

cd -


