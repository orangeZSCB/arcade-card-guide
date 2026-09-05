#!/bin/bash
# CH340 的节点号每次插拔可能变（1130 -> 110 ...），跑这个自动把 libnfc.conf 指回去
CONF=/opt/homebrew/Cellar/libnfc/1.8.0/etc/nfc/libnfc.conf
PORT=$(ls /dev/cu.wchusbserial* /dev/cu.usbserial* 2>/dev/null | head -1)
if [ -z "$PORT" ]; then
  echo "没找到 CH340 串口节点，PN532 可能没插好或没上电"
  ls /dev/cu.*
  exit 1
fi
cat > "$CONF" <<CONFEOF
allow_intrusive_scan=yes
device.name = "PN532_UART"
device.connstring = "pn532_uart:${PORT}:115200"
device.autopoll = yes
CONFEOF
echo "libnfc.conf -> ${PORT}:115200"
cat "$CONF"
