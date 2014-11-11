#!/bin/sh

Warning()
{
	printf "\033[1;35m$@\033[0m\n"
}

Error()
{
	printf "\033[1;31m$@\033[0m\n"
}

PrepareIhttpdStartup()
{
	touch $dstscript >/dev/null 2>&1
	retval=$?
	if [ "$retval" != "0" ]; then
		Warning "Failed to install startup script, ihttpd will not start on next boot."
		exit 1
	else
		if [ -f $srcscript ]; then
			cat $srcscript | sed "s,%%PREFIX%%,$mgrdir," > $dstscript
			chmod +x $dstscript
		else
			Error "The script file '$srcscript' does not exists. Terminating"
			exit 2
		fi
	fi
}

InstallIhttpdStartup()
{
	ihttpdstartup="false"
	kern=`uname -s`
	case "$kern" in
		Linux)
			if [ -e /etc/redhat-release ]; then
				srcscript=$mgrscriptdir/ihttpd_startup_centos
				dstscript=/etc/rc.d/init.d/ihttpd
				PrepareIhttpdStartup
				if [ "$retval" = "0" ]; then
					chkconfig --add ihttpd
					iptables -I INPUT 1 -p tcp --dport 1500 -j ACCEPT
					service iptables save
					ihttpdstartup="true"
				fi
			else
				if [ -e /etc/debian_version ]; then
					srcscript=$mgrscriptdir/ihttpd_startup_debian
					dstscript=/etc/init.d/ihttpd
					PrepareIhttpdStartup
					if [ "$retval" = "0" ]; then
						update-rc.d ihttpd defaults > /dev/null
						ihttpdstartup="true"
					fi
				fi
			fi
			;;
		*)
			echo "Cannot install ihttpd startup script: uknown OS type"
			;;
	esac
}

mgrdir=$1
if [ -z $mgrdir ]; then
	mgrdir=/usr/local/mgr5
fi
mgrscriptdir=$mgrdir/etc/scripts
InstallIhttpdStartup
