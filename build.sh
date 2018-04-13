#!/bin/bash

APP=gwu18pc
APP_DBG=`printf "%s_dbg" "$APP"`
INST_DIR=/usr/sbin
CONF_DIR=/etc/controller
CONF_DIR_APP=$CONF_DIR/$APP

SOCK=udp
#DEBUG_PARAM="-Wall -pedantic"
DEBUG_PARAM="-Wall -pedantic -g"
MODE_DEBUG=-DMODE_DEBUG
MODE_FULL=-DMODE_FULL

source lib/gpio/cpu.sh
source lib/gpio/pinout.sh

NONE=-DNONEANDNOTHING


function move_bin {
	([ -d $INST_DIR ] || mkdir $INST_DIR) && \
	cp $APP $INST_DIR/$APP && \
	chmod a+x $INST_DIR/$APP && \
	chmod og-w $INST_DIR/$APP && \
	echo "Your $APP executable file: $INST_DIR/$APP";
}

function move_bin_dbg {
	([ -d $INST_DIR ] || mkdir $INST_DIR) && \
	cp $APP_DBG $INST_DIR/$APP_DBG && \
	chmod a+x $INST_DIR/$APP_DBG && \
	chmod og-w $INST_DIR/$APP_DBG && \
	echo "Your $APP executable file for debugging: $INST_DIR/$APP_DBG";
}

function move_conf {
	([ -d $CONF_DIR ] || mkdir $CONF_DIR) && \
	([ -d $CONF_DIR_APP ] || mkdir $CONF_DIR_APP) && \
	cp ./config/main.tsv $CONF_DIR_APP && \
	cp ./config/device.tsv $CONF_DIR_APP && \
	cp ./config/thread.tsv $CONF_DIR_APP && \
	cp ./config/thread_device.tsv $CONF_DIR_APP && \
	cp ./config/lcorrection.tsv $CONF_DIR_APP && \
	chmod -R a+w $CONF_DIR_APP
	echo "Your $APP configuration files are here: $CONF_DIR_APP";
}

#your application will run on OS startup
function conf_autostart {
	cp -v init.sh /etc/init.d/$APP && \
	chmod 755 /etc/init.d/$APP && \
	update-rc.d -f $APP remove && \
	update-rc.d $APP defaults 30 && \
	echo "Autostart configured";
}
function build_lib {
	gcc $1 -c app.c -D_REENTRANT $DEBUG_PARAM -pthread && \
	gcc $1 -c crc.c -D_REENTRANT $DEBUG_PARAM -pthread && \
	gcc $1 $CPU $PINOUT -c gpio.c -D_REENTRANT $DEBUG_PARAM -pthread && \
	gcc $1 -c timef.c -D_REENTRANT  $DEBUG_PARAM -pthread && \
	gcc $1 -c $SOCK.c -D_REENTRANT $DEBUG_PARAM -pthread && \
	gcc $1 -c util.c -D_REENTRANT $DEBUG_PARAM -pthread && \
	gcc $1 -c tsv.c -D_REENTRANT $DEBUG_PARAM -pthread && \
	gcc $1 -c lcorrection.c -D_REENTRANT $DEBUG_PARAM -pthread && \
	gcc $1 -c 1w.c -D_REENTRANT $DEBUG_PARAM -pthread && \
	cd device && \
	gcc $1 -c ds18b20.c -D_REENTRANT $DEBUG_PARAM -pthread && \
	cd ../ && \
	cd acp && \
	gcc $1 $CPU -c main.c -D_REENTRANT $DEBUG_PARAM -pthread && \
	cd ../ && \
	echo "library: making archive..." && \
	rm -f libpac.a
	ar -crv libpac.a app.o crc.o gpio.o timef.o $SOCK.o util.o tsv.o lcorrection.o 1w.o device/ds18b20.o acp/main.o && echo "library: done" && echo "hardware: $CPU $PINOUT"
	rm -f *.o acp/*.o
}
function build {
	cd lib && \
	build_lib $1 && \
	cd ../ 
	gcc -D_REENTRANT $1 $3 $CPU main.c -o $2 $DEBUG_PARAM -pthread -L./lib -lpac && echo "Application successfully compiled. Launch command: sudo ./"$2
}

function full {
	DEBUG_PARAM=$NONE
	build $NONE $APP $MODE_FULL && \
	build $MODE_DEBUG $APP_DBG $MODE_FULL && \
	move_bin && move_bin_dbg && move_conf && conf_autostart
}
function full_nc {
	DEBUG_PARAM=$NONE
	build $NONE $APP $MODE_FULL && \
	build $MODE_DEBUG $APP_DBG $MODE_FULL  && \
	move_bin && move_bin_dbg
}
function full_na {
	DEBUG_PARAM=$NONE
	build $NONE $APP $MODE_FULL && \
	build $MODE_DEBUG $APP_DBG $MODE_FULL && \
	move_bin && move_bin_dbg && move_conf
}
function part_debug {
	build $MODE_DEBUG $APP_DBG $NONE
}
function uninstall_nc {
	pkill $APP --signal 9
	pkill $APP_DBG --signal 9
	rm -f $INST_DIR/$APP
	rm -f $INST_DIR/$APP_DBG
}
function uninstall {
	uninstall_nc
	update-rc.d -f $APP remove
	rm -rf $CONF_DIR_APP
}

f=$1
${f}
