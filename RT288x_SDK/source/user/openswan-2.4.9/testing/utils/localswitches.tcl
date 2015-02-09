#!/usr/bin/expect --

#
# $Id: //WIFI_SOC/MP/SDK_4_3_0_0/RT288x_SDK/source/user/openswan-2.4.9/testing/utils/localswitches.tcl#1 $
#

source $env(OPENSWANSRCDIR)/testing/utils/GetOpts.tcl
source $env(OPENSWANSRCDIR)/testing/utils/netjig.tcl

set netjig_debug_opt ""

set netjig_prog $env(OPENSWANSRCDIR)/testing/utils/uml_netjig/uml_netjig

set arpreply ""
set umlid(extra_hosts) ""

spawn $netjig_prog --cmdproto -t $netjig_debug_opt 
set netjig1 $spawn_id

netjigsetup $netjig1

foreach net $managednets {
    calc_net $net
}

foreach net $managednets {
    process_net $net
}

foreach net $managednets {
    if { $umlid(net$net,arp) } {
	newswitch $netjig1 "--arpreply $net"
    } {
	newswitch $netjig1 "$net"
    }
}

foreach host $argv {
    system "$host single &"
}

puts "\nExit the netjig when you are done\n"

set timeout -1
interact -reset -i $netjig1 

foreach host $argv {
    system "uml_mconsole $host halt"
}






