#!/bin/sh

toupper() {
	if [ -n "$1" ]; then
		echo "$1" | awk '{print toupper($FS)}'
	fi
}

tolower() {
	if [ -n "$1" ]; then
		echo "$1" | awk '{print tolower($FS)}'
	fi
}

mac_suffix() {
	local mac
	local path="$1"
	local offset="$2"

	if [ -z "$path" ]; then
		return
	fi

	mac=$(hexdump -v -n 6 -s $offset -e '6/1 "%02X"' $path 2>/dev/null)
	echo ${mac:9:3}
}
