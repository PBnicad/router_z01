cronstatus=$(crontab -l |grep '/root/time.sh')
if [ -z "$cronstatus" ]; then
        echo '*/10 * * * * /lib/imyfit/timing.sh' >> /etc/crontabs/root
        echo '00 00 * * * /lib/imyfit/reboot.sh' >> /etc/crontabs/root
fi