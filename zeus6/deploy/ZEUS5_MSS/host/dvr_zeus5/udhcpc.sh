#!/bin/sh

# udhcpc script edited by Tim Riker <Tim@Rikers.org>

[ -z "$1" ] && echo "Error: should be called from udhcpc" && exit 1

[ -n "$broadcast" ] && BROADCAST="broadcast $broadcast"
[ -n "$subnet" ] && NETMASK="netmask $subnet"

case "$1" in
	deconfig)
		ifconfig $interface 0.0.0.0
		;;

	renew|bound)
		ifconfig $interface $ip $BROADCAST $NETMASK

		if [ -n "$router" ] ; then
			# echo "deleting routers"
			while route del default gw 0.0.0.0 dev $interface ; do
				:
			done

			# echo "setting routers: $router"
			for i in $router ; do
				route add default gw $i dev $interface
			done
		fi

		;;
esac

exit 0
