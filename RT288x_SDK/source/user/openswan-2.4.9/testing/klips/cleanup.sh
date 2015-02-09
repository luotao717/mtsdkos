#
# use this script to undo effects of sourcing a "testparams.sh" into
# your shell, when testing.
#
# $Id: //WIFI_SOC/MP/SDK_4_3_0_0/RT288x_SDK/source/user/openswan-2.4.9/testing/klips/cleanup.sh#1 $
#
unset SCRIPT
unset REFCONSOLEOUTPUT 
unset PRIVINPUT
unset PUBINPUT
unset REFPRIVOUTPUT
unset REFPUBOUTPUT
unset TCPDUMPARGS

#
# $Log: cleanup.sh,v $
# Revision 1.4  2002/04/24 07:36:13  mcr
# Moved from ./klips/test/cleanup.sh,v
#
# Revision 1.3  2002/02/20 07:26:24  rgb
# Corrected de-pluralized variable names.
#
# Revision 1.2  2001/11/23 01:08:12  mcr
# 	pullup of test bench from klips2 branch.
#
# Revision 1.1.2.1  2001/10/23 04:43:18  mcr
# 	shell/testing cleaning script.
#
# 
