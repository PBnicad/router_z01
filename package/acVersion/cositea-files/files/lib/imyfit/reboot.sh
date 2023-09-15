#!/bin/sh

echo "last reboot time:" > /root/reboot.txt
echo $(date +%F%n%T) >> /root/reboot.txt
reboot