!#/bin/bash
set -e

DEVICE  ="/dev/cdc-wdm0"
WWAN = "wwan0"
#access point name
APN="nxtgenphone"

echo "Starting QMI connection"

#stop modem manager to prevent conflicts with qmicli
echo "Stopping Modem Manager"
sudo systemctl stop ModemManager

#bring up wwan interface
echo "Bringing up wwan interface"
sudo ip link set $WWAN up

#start qmi data session
echo "Starting QMI data session"
sudo qmicli -d "$DEVICE" \
    --device-open-qmi \
    --wds-start-network="apn=$APN,ip-type=4" \
    --client-no-release-cid

#get current network settings 
echo "Getting current network settings"
sudo qmicli -d "$DEVICE" \
    --device-open-qmi \
    --wds-get-current-settings

echo "Current network settings: $SETTINGS"

#extract ipv4 information
IPV4_INFO=$(sudo qmicli -d "$DEVICE" \
    --device-open-qmi \
    --wds-get-current-settings | grep "IPv4 address" | awk '{print $4}')
    

