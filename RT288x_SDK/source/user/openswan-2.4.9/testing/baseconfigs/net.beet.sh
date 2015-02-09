#
# $Id: //WIFI_SOC/MP/SDK_4_3_0_0/RT288x_SDK/source/user/openswan-2.4.9/testing/baseconfigs/net.beet.sh#1 $
#
if [ -n "$UML_public_CTL" ]
then
    net_eth0="eth0=daemon,10:00:00:de:76:ba,unix,$UML_public_CTL,$UML_public_DATA";
else
    net_eth0="eth0=mcast,10:00:00:de:76:ba,239.192.1.2,31200";
fi

net="$net_eth0"




