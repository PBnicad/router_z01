/etc/init.d/travelmate restart
echo "last travelmate restart time:" > /root/out.txt
echo $(date +%F%n%T) >> /root/out.txt