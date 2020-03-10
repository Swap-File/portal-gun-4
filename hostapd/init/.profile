# ~/.profile: executed by the command interpreter for login shells.
# This file is not read by bash(1), if ~/.bash_profile or ~/.bash_login
# exists.
# see /usr/share/doc/bash/examples/startup-files for examples.
# the files are located in the bash-doc package.

# the default umask is set in /etc/profile; for setting the umask
# for ssh logins, install and configure the libpam-umask package.
#umask 022

# if running bash
if [ -n "$BASH_VERSION" ]; then
    # include .bashrc if it exists
    if [ -f "$HOME/.bashrc" ]; then
	. "$HOME/.bashrc"
    fi
fi

# set PATH so it includes user's private bin if it exists
if [ -d "$HOME/bin" ] ; then
    PATH="$HOME/bin:$PATH"
fi

# set PATH so it includes user's private bin if it exists
if [ -d "$HOME/.local/bin" ] ; then
    PATH="$HOME/.local/bin:$PATH"
fi
echo "GORDON"
sudo /home/pi/a.out
if [ "$(pidof hostapd)" ]
then
	echo "HostAPD is running already!"
else
	sudo hostapd /etc/hostapd/hostapd.conf &
fi
if [ "$(pidof portal)" ]
then
	echo "Portal is running already!"
else
	export GORDON="1"
	sudo sysctl -w net.ipv4.ip_forward=1
	sudo iptables -t nat -A PREROUTING -i bnep0 -p tcp --dport 8020 -j DNAT --to-destination 192.168.3.20:80
	sudo iptables -t nat -A PREROUTING -i bnep0 -p tcp --dport 8021 -j DNAT --to-destination 192.168.3.21:80
	sudo iptables -t nat -A POSTROUTING -j MASQUERADE
	sudo ifconfig wlan0 192.168.3.20
	cd /home/pi/portal
	sudo ./portal
fi