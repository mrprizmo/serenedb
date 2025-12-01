#!/bin/sh
#getent group serenedb > /dev/null || addgroup -S serenedb
#getent passwd serenedb > /dev/null || adduser -S -G serenedb -D -h /usr/share/serenedb -H -s /bin/false -g "SereneDB Application User" serenedb

install -o root -g root -m 755 -d /var/lib/serenedb
# Note that the log dir is 777 such that any user can log there.
install -o root -g root -m 777 -d /var/log/serenedb

mkdir /docker-entrypoint-initdb.d/

# Bind to all endpoints (in the container):
sed -i -e 's~^endpoint.*8529$~endpoint = tcp://0.0.0.0:8529~' /etc/serenedb/serened.conf
# Remove the uid setting in the config file, since we want to be able
# to run as an arbitrary user:
sed -i \
    -e 's!^\(file\s*=\s*\).*!\1 -!' \
    -e 's~^uid = .*$~~' \
    /etc/serenedb/serened.conf

apt-get update
apt-get upgrade -y
apt-get install -y --no-install-recommends wget unzip

wget --no-check-certificate https://github.com/rclone/rclone/releases/download/v1.70.1/rclone-v1.70.1-linux-amd64.zip
unzip rclone-v1.70.1-linux-amd64.zip
mv rclone-v1.70.1-linux-amd64/rclone /usr/sbin
rm -rf rclone-v1.70.1-linux-amd64*

apt-get autopurge -y wget unzip
apt-get autopurge -y
apt-get autoclean
apt-get clean
