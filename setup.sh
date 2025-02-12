#!/bin/bash

# 目前只支持 Ubuntu 20/22
user=""
lsb=`lsb_release -a | grep Release | awk '{print $2}' | awk -F "." '{print $1}'`
if [ "$lsb" = "20" ]; then
	user="root"
elif [ "$lsb" = "22" ]; then
	# 如果 sunrise 用户不存在, 则创建或更改为已存在的用户
	user="sunrise"
else
	# 不支持的Ubuntu版本
	echo "Unsupported system version($lsb) exit ."
	exit
fi

# 配置DNS, 如已配置可注释掉
var=`cat /etc/resolv.conf | grep "114.114.114.114"`
if [[ -z "$var" ]]; then
	echo "nameserver 114.114.114.114" >> /etc/resolv.conf
fi

# 安装所需依赖, 如已安装可注释掉
apt-get update
apt-get -y install cron libcjson-dev libcurl4-openssl-dev sqlite3 libsqlite3-dev

# 部署所需环境
mkdir -p /usr/local/xt/bin /usr/local/xt/lib /usr/local/xt/conf /usr/local/xt/include /usr/local/xt/db /usr/local/xt/logs /usr/local/xt/scripts /usr/local/xt/models /usr/local/xt/debug /usr/local/xt/screenshot
if [ ! -e /usr/local/xt/db/xt.db ]; then
	cp -f db/xt.db /usr/local/xt/db
fi

if [ ! -e /etc/ld.so.conf.d/xt.conf ]; then
	echo "/usr/local/xt/lib" > /etc/ld.so.conf.d/xt.conf
fi

if [ ! -e /usr/local/xt/db/xt.conf ]; then
	cp -f conf/xt.conf /usr/local/xt/conf
fi

cp -f libs/* /usr/local/xt/lib
cp -f scripts/process_guard.sh /usr/local/xt/scripts
# cp -f bin/$lsb/xftp bin/$lsb/monitor /usr/local/xt/bin
cp -f models/fcos_512x512_nv12.bin /usr/local/xt/models
ldconfig

# 如果 sunrise 用户不存在, 则创建或更改为已存在的用户
chown -Rf sunrise:sunrise /usr/local/xt
chmod -Rf 777 /usr/local/xt

var=`ps -ef | grep -v grep | grep "xt/bin/monitor"`
if [ -n "$var" ]; then
	killall monitor
fi

var=`ps -ef | grep -v grep | grep "xt/bin/xftp"`
if [ -n "$var" ]; then
	killall xftp
fi

/usr/local/xt/scripts/process_guard.sh

# 开启定时任务
var=`crontab -u $user -l | grep "xt/scripts/process_guard.sh"`
if [ -n "$var" ]; then
	:
else
	crontab -u $user -l > cron
	echo "* * * * * /usr/local/xt/scripts/process_guard.sh" >> cron
	echo "* * * * * sleep 10; /usr/local/xt/scripts/process_guard.sh" >> cron
	echo "* * * * * sleep 20; /usr/local/xt/scripts/process_guard.sh" >> cron
	echo "* * * * * sleep 30; /usr/local/xt/scripts/process_guard.sh" >> cron
	echo "* * * * * sleep 40; /usr/local/xt/scripts/process_guard.sh" >> cron
	echo "* * * * * sleep 50; /usr/local/xt/scripts/process_guard.sh" >> cron
	crontab -u $user cron
	unlink cron
	systemctl enable cron
fi
