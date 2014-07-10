#!/bin/sh

v_bootargs=`/davinci/dvr/fw_printenv bootargs`
#v_bootargs="bootargs=console=ttyS0,115200n8 initrd=2g,1 rw root=/dev/ram mem=64M"
old_bootargs=${v_bootargs#bootargs=}
old_console=${old_bootargs%%initrd*M}
old_initrd=${old_bootargs#console=*n8}

if [ $old_console = console=ttyS0,115200n8 ]; then
  new_bootargs="console=ttyS2,115200n8 "$old_initrd 
  echo $new_bootargs
  /davinci/dvr/fw_setenv bootargs $new_bootargs
  /bin/reboot
fi