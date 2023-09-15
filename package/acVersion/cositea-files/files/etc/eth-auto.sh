#!/bin/sh
# This script is used to identify whether the current network port is wan or LAN, 
# and convert the network port to the mode based on the recognition. The default 
# is wan when the network port is not connected

gw=""

link=1
link_status=0

while true
do
	sta=$(swconfig dev switch0 show | grep port:0)

	if [ ${#sta} -gt 30 ]; then
		link_stuts=1
		sleep 1

		gw=$(ubus call network.interface.wan status | grep nexthop | grep -oE '([0-9]{1,3}.){3}.[0-9]{1,3}.[0-9]{1,3}')
		if [ -z ${gw} ]; then
			vlan=$(uci get network.@switch_vlan[1].ports)
			if [ "$vlan" == "0 6t" ]; then
				#echo "now is wan"
				uci set network.@switch_vlan[1].ports="1 6t"
				uci set network.@switch_vlan[0].ports="0 6t"
				uci commit network
				/etc/init.d/network restart
				logger "eth0 is lan"
				sleep 5
			fi
		else
			vlan=$(uci get network.@switch_vlan[1].ports)
                        #if [ "$vlan" == "0 6t" ]; then
                                #echo "now is wan"
                        #else
                                #echo "now is lan"
                        #fi 
		fi		
	else
		vlan=$(uci get network.@switch_vlan[1].ports)
		if [ "$vlan" == "1 6t" ]; then 
			# this is lan, change wan
			uci set network.@switch_vlan[1].ports="0 6t"
			uci set network.@switch_vlan[0].ports="1 6t"
			uci commit network
			link_stuts=0
			/etc/init.d/network restart
			logger "eth0 disconnect"
			sleep 5
		fi
		if [ "$link_stuts" == "$link" ]; then
			logger "link disconnect, clear ip"
			link_stuts=0
			/etc/init.d/network restart
			sleep 5
		fi
	fi 
	sleep 1
done
