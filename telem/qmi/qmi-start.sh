#!/bin/bash
set -e

DEVICE="/dev/cdc-wdm0"
WWAN="wwan0"
#access point name
APN="nxtgenphone"
#route priority: lower than wifi's, so cellular becomes the main door (wifi stays as backup)
METRIC=50
#where the session ID is saved so qmi-stop.sh can end the session cleanly
STATE="/run/qmi-session"


echo "Starting QMI connection"

#wait up to 30s for the modem to show up (it can be slow right after pi boot)
for i in $(seq 30); do [ -e "$DEVICE" ] && break; sleep 1; done
[ -e "$DEVICE" ] || { echo "Modem $DEVICE not found"; exit 1; }

#stop modem manager to prevent conflicts with qmicli
echo "Stopping Modem Manager"
sudo systemctl stop ModemManager


#witch the interface to raw-IP mode if the modem uses it
#(without this you can get an IP address but no traffic flows)
if sudo qmicli -d "$DEVICE" --device-open-qmi --wda-get-data-format | grep -qi "raw-ip"; then
    echo "Modem uses raw-IP; enabling raw-IP on $WWAN"
    sudo ip link set "$WWAN" down
    echo Y | sudo tee "/sys/class/net/$WWAN/qmi/raw_ip" > /dev/null
fi
 

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

#save the session ID (packet data handle + client ID) for qmi-stop.sh
HANDLE=$(echo "$START_OUT" | grep -oP "Packet data handle: '\K[0-9]+" || true)
CID=$(echo "$START_OUT" | grep -oP "CID: '\K[0-9]+" || true)
printf 'HANDLE=%s\nCID=%s\n' "$HANDLE" "$CID" | sudo tee "$STATE" > /dev/null
echo "Saved session: handle=$HANDLE cid=$CID"

echo "Current network settings: $SETTINGS"

#extract ipv4 information
IP=$(echo "$SETTINGS" | awk -F': ' '/IPv4 address:/ {print $2}') 
MASK=$(echo "$SETTINGS" | awk -F': ' '/IPv4 subnet mask:/ {print $2}') 
GATEWAY=$(echo "$SETTINGS" | awk -F': ' '/IPv4 gateway address:/ {print $2}')
MTU=$(echo "$SETTINGS" | awk -F': ' '/MTU:/ {print $2}') 



#convert subnet mask to prefix length (e.g. 255.255.255.252 -> 30)
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
#make cellular the main route for internet traffic
#wifi's default route stays as a backup, and ssh over the local network is unaffected
sudo ip route replace default dev "$WWAN" metric "$METRIC"
 
echo "QMI connection started successfully"
 
echo "LTE interface:"
ip addr show "$WWAN"
echo
echo "Routing:"
ip route
