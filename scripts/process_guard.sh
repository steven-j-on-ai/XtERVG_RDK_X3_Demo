#!/bin/bash

sql=`sqlite3 /usr/local/xt/db/xt.db "SELECT channel_no FROM m_channel;"`
for channel in $sql; do
	var=`ps -ef | grep -v grep | grep "xt/bin/xftp $channel"`
	if [[ -z "$var" ]]; then
		/usr/local/xt/bin/xftp "$channel" 1920 1080 > /usr/local/xt/logs/xftp_"$channel"_"$(date +%Y%m%d%H%M%S).log" 2>&1 &
	fi
done


var=`ps -ef | grep -v grep | grep "xt/bin/monitor"`
if [[ -z "$var" ]]; then
	/usr/local/xt/bin/monitor > /usr/local/xt/logs/monitor_"$(date +%Y%m%d%H%M%S).log" 2>&1 &
fi
