#!/bin/sh

# Edit this to match your MAc/PC/Other device that's providing the Bluetooth network access

BT_MAC_ADDR=64:A2:F9:EE:50:29

SCRIPT=$(readlink -f $0)
SCRIPTPATH=`dirname $SCRIPT`

/sbin/ifconfig bnep0 > /dev/null 2>&1
status=$?
if [ $status -ne 0 ]; then
	echo "Connecting to $BT_MAC_ADDR"
	sudo ~/phone/autoconnect/bt-pan client -r  $BT_MAC_ADDR
fi


