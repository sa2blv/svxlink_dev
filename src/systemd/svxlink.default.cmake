#############################################################################
#
# Configuration file for the SvxLink startup script /etc/init.d/svxlink
#
#############################################################################
# The log file to use
LOGFILE=/var/log/svxlink

# The PID file to use
PIDFILE=/run/svxlink.pid

# The user to run the SvxLink server as
RUNASUSER=svxlink

# Specify which configuration file to use
CFGFILE=/etc/svxlink/svxlink.conf

# Environment variables to set up. Separate variables with a space.
ENV="ASYNC_AUDIO_NOTRIGGER=1"

if [ -r /etc/svxlink/svxlink_gpio.conf ]; then
        . /etc/svxlink/svxlink_gpio.conf
fi