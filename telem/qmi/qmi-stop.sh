#!/bin/bash

DEVICE="/dev/cdc-wdm0"
WWAN="wwan0"
STATE="/run/qmi-session"

echo "Stopping QMI connection"

#remove the cellular default route so wifi becomes the main door again
sudo ip route del default dev "$WWAN" 2>/dev/null || true

#end the data session using the ID qmi-start.sh saved
if [ -f "$STATE" ]; then
    source "$STATE"
    if [ -n "$HANDLE" ] && [ -n "$CID" ]; then
        echo "Ending session: handle=$HANDLE cid=$CID"
        sudo qmicli -d "$DEVICE" --device-open-qmi \
            --wds-stop-network="$HANDLE" --client-cid="$CID" || true
    fi
    sudo rm -f "$STATE"
else
    echo "No saved session found; skipping QMI stop"
fi

#clear the interface
sudo ip addr flush dev "$WWAN" 2>/dev/null || true
sudo ip link set "$WWAN" down 2>/dev/null || true

#give the modem back to modem manager
sudo systemctl start ModemManager || true

echo "QMI connection stopped"