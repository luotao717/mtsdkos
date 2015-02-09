#
# $Id: //WIFI_SOC/MP/SDK_4_3_0_0/RT288x_SDK/source/user/openswan-2.6.22/testing/baseconfigs/net.east.sh#1 $
#
if [ -n "$UML_east_CTL" ]
then
    net_eth0="eth0=daemon,12:00:00:dc:bc:ff,unix,$UML_east_CTL";
elif [ -n "$UML_private_CTL" ]
then
    net_eth0="eth0=daemon,12:00:00:dc:bc:ff,unix,$UML_private_CTL";
else
    net_eth0="eth0=mcast,12:00:00:dc:bc:ff,239.192.0.1,21200"
fi

if [ -n "$UML_public_CTL" ]
then
    net_eth1="eth1=daemon,12:00:00:64:64:23,unix,$UML_public_CTL";
else
    net_eth1="eth1=mcast,12:00:00:64:64:23,239.192.1.2,31200";
fi

if [ -n "$UML_admin_CTL" ]
then
    net_eth2="eth2=daemon,12:00:00:32:64:23,unix,$UML_admin_CTL";
else
    net_eth2="eth2=mcast,12:00:00:32:64:23,239.192.3.2,31210";
fi

net="$net_eth0 $net_eth1 $net_eth2"




