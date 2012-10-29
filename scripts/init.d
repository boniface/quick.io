#!/bin/bash
# Quick.IO daemon
# chkconfig: 345 20 80
# description: Quick.IO daemon
# processname: qio

DAEMON=/usr/bin/qio
CONFIG_FILE=/etc/quickio/quickio.ini
PIDFILE=/var/run/quickio.pid
DESC=Quick.IO
USER=nobody

start() {
	echo -n "Starting $DESC: "
	
	touch $PIDFILE
	chown $USER $PIDFILE
	
	sudo -u $USER nohup $DAEMON --config-file=$CONFIG_FILE > /dev/null 2>&1 &
	
	if [ -z $? ]; then
		echo "Fail"
	else
		echo $! > $PIDFILE
		echo "OK"
	fi
}

stop() {
	echo -n "Stopping $DESC: "
	kill `cat $PIDFILE`
	
	if [ -z $? ]; then
		echo "Fail"
	else
		echo "OK"
	fi
}

status() {
	pgrep -F $PIDFILE > /dev/null
	if [ $? -eq 0 ]; then 
		PID=`cat $PIDFILE`
		echo "Quick.IO is running with pid $PID"
	else
		echo "Quick.IO is not running"	
	fi
}

case "$1" in
	start)
		start 
		;;
	stop)
		stop
		;;
	status)
		status
		;;
	restart)
		stop
		start
		;;
	*)
		echo $"Usage: $0 {start|stop|restart|status}"
		exit 1
esac