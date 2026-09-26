#!/bin/bash
set -e

DEVICE="/dev/cdc-wdm0"
WWAN="wwan0"
#access point name
APN="nxtgenphone"

echo "Starting QMI connection"

#stop modem manager to prevent conflicts with qmicli
echo "Stopping Modem Manager"
sudo systemctl stop ModemManager

#bring up wwan interface
echo "Bringing up wwan interface"
sudo ip link set "$WWAN" up

#start qmi data session
echo "Starting QMI data session"
sudo qmicli -d "$DEVICE" \
    --device-open-qmi \
    --wds-start-network="apn=$APN,ip-type=4" \
    --client-no-release-cid

#get current network settings 
echo "Getting current network settings"
SETTINGS=$(sudo qmicli -d "$DEVICE" \
    --device-open-qmi \
    --wds-get-current-settings)

echo "Current network settings: $SETTINGS"

#extract ipv4 information
IP=$(echo "$SETTINGS" | awk -F': ' '/IPv4 address:/ {print $2}') 
MASK=$(echo "$SETTINGS" | awk -F': ' '/IPv4 subnet mask:/ {print $2}') 
GATEWAY=$(echo "$SETTINGS" | awk -F': ' '/IPv4 gateway address:/ {print $2}')
MTU=$(echo "$SETTINGS" | awk -F': ' '/MTU:/ {print $2}') 


#convert subnet mask to get full IP address
IFS=. read -r a b c d <<< "$MASK"
CIDR=$(python3 -c "
mask='$MASK'
bits=''.join(format(int(x), '08b') for x in mask.split('.'))
print(bits.count('1'))
")

echo "IP: $IP"
echo "GATEWAY: $GATEWAY"
echo "MTU: $MTU"


#remove any existing IP addresses on the interface
sudo ip addr flush dev "$WWAN"

#configure interface
sudo ip addr add "$IP/$CIDR" dev "$WWAN"
sudo ip link set "$WWAN" mtu "$MTU"

#add cellular subnet route (but dont make default)
#sudo ip route replace "$IP/$CIDR" dev "$WWAN"

echo "QMI connection started successfully"

echo "LTE interface:"
ip addr show "$WWAN"
echo
echo "Routing:"
ip route
