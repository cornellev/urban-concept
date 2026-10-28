#!/bin/bash
# Cellular + Tailscale bit rate sweep. Run on the CAR Pi, from this folder.
# The receiver must already be running:  python3 stream_test.py server
#
#   ./sweep.sh <receiver-tailscale-IP>              default rates
#   ./sweep.sh <receiver-tailscale-IP> 50 100 200   your own rates (packets/s)
#
 
RX="$1"; shift
[ -z "$RX" ] && { echo "usage: $0 <receiver-tailscale-IP> [rates...]"; exit 1; }
RATES="${*:-25 50 100 200 400 800}"
SIZE=1200     # bytes per packet: fits inside Tailscale without splitting
DUR=20        # seconds per step
DIR="$(cd "$(dirname "$0")" && pwd)"
OUT="$HOME/results-$(date +%Y%m%d-%H%M%S).txt"
 
exec > >(tee "$OUT") 2>&1
 
echo "=== cellular sweep $(date) ==="
echo "--- signal ---"
sudo qmicli -d /dev/cdc-wdm0 --device-open-qmi --nas-get-signal-strength 2>&1 \
  | grep -iE "network|rssi|rsrp|rsrq|snr|sinr|ecio" || echo "(couldn't read signal)"
echo "--- route (should say dev wwan0) ---"
ip route get 8.8.8.8
echo "--- public IP (should be AT&T) ---"
curl -s --max-time 10 https://ifconfig.me; echo
echo "--- tailscale path (should NOT say via 192.168...) ---"
tailscale ping -c 3 "$RX"
 
for r in $RATES; do
  echo
  echo "=== $r pkt/s x $SIZE B = $((r * SIZE * 8 / 1000)) kbit/s offered ==="
  tx0=$(cat /sys/class/net/wwan0/statistics/tx_bytes)
  python3 "$DIR/stream_test.py" client --host "$RX" --iface '' \
      --rate "$r" --size "$SIZE" --duration "$DUR" | tail -3
  tx1=$(cat /sys/class/net/wwan0/statistics/tx_bytes)
  echo "over the air (wwan0 TX): $(( (tx1 - tx0) * 8 / DUR / 1000 )) kbit/s"
  sleep 3
done
 
echo
echo "saved to $OUT"