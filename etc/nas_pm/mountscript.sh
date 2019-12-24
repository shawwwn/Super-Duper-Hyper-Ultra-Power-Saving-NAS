#!/bin/bash
# Wait for a disk to be shown as a block device before mounting it
# Shawwwn <shawwwn1@gmail.com>
uuid="$1"
mntpt="$2"
gpio="$3"

# errno
ALREADY_MOUNTED=1
DISK_TIMEOUT=2
MOUNT_FAILED=3

# Wait for disk's uuid to show up in `blkid`
wait_for_disk_uuid() {
	local uuid="$1"
	local timeout=$([ -z $2 ] && echo 15 || echo $2) # 15s
	local found=false

	for i in `seq 0 $timeout`; do
		if blkid -U "$uuid" >/dev/null; then
			found=true
			break
		fi

		if [ $i -lt $timeout ]; then
			sleep 1
		fi
	done

	$found
	return $?
}


#
# main()
#
if mountpoint -q "$mntpt"; then
	exit $ALREADY_MOUNTED
fi

if ! wait_for_disk_uuid "$uuid" 15; then
	exit $DISK_TIMEOUT
fi

if ! mount "UUID=$uuid" "$mntpt"; then
	exit $MOUNT_FAILED
fi

sleep 0.5
exit 0
