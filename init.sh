#!/bin/sh
# kFreeBSD do not accept scripts as interpreters, using #!/bin/sh and sourcing.
if [ true != "$INIT_D_SCRIPT_SOURCED" ] ; then
    set "$0" "$@"; INIT_D_SCRIPT_SOURCED=true . /lib/init/init-d-script
fi
### BEGIN INIT INFO
# Provides:          gwu18pc
# Required-Start:    $remote_fs $syslog
# Required-Stop:     $remote_fs $syslog
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: UDP gateway for DS18B20 (pc)
### END INIT INFO

# Author: Arinichev Nikolay <arinichev_n@mail.ru>

DESC="UDP gateway for DS18B20 (pc)"
DAEMON=/usr/sbin/gwu18pc