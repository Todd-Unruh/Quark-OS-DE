#!/bin/bash
# install.sh — Write Quark-OS-3 ISO to a block device (USB)
set -e
ISO="$(dirname "$0")/quark_os_3.iso"
if [ ! -f "$ISO" ]; then
    echo "Building ISO first..."
    make -C "$(dirname "$0")" iso
fi
if [ -z "$1" ]; then
    echo "Usage: sudo $0 /dev/sdX"
    echo "  Writes quark_os_3.iso to the device. ALL DATA ON DEVICE WILL BE LOST."
    exit 1
fi
DEV="$1"
if [ ! -b "$DEV" ]; then
    echo "Error: $DEV is not a block device"
    exit 1
fi
echo "WARNING: This will erase $DEV"
read -p "Type YES to continue: " confirm
if [ "$confirm" != "YES" ]; then
    echo "Aborted."
    exit 1
fi
dd if="$ISO" of="$DEV" bs=4M status=progress conv=fsync
sync
echo "Quark-OS-3 installed to $DEV. Boot from this device."
