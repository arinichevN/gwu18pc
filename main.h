
#ifndef GWU18PC_H
#define GWU18PC_H

#include "lib/app.h"
#include "lib/timef.h"
#include "lib/acp/main.h"
#include "lib/acp/app.h"
#include "lib/udp.h"
#include "lib/tsv.h"
#include "lib/lcorrection.h"
#include "lib/device/ds18b20.h"
#include "lib/filter/common.h"

#define APP_NAME gwu18pc
#define APP_NAME_STR TOSTRING(APP_NAME)

#ifdef MODE_FULL
#define CONF_DIR "/etc/controller/" APP_NAME_STR "/"
#endif
#ifndef MODE_FULL
#define CONF_DIR "./config/"
#endif
#define CONF_MAIN_FILE CONF_DIR "main.tsv"
#define CONF_CHANNEL_FILE CONF_DIR "channel.tsv"
#define CONF_THREAD_FILE CONF_DIR "thread.tsv"
#define CONF_THREAD_CHANNEL_FILE CONF_DIR "thread_channel.tsv"
#define CONF_LCORRECTION_FILE CONF_DIR "lcorrection.tsv"
#define CONF_FILTER_MA_FILE CONF_DIR "filter_ma.tsv"
#define CONF_FILTER_EXP_FILE CONF_DIR "filter_exp.tsv"
#define CONF_CHANNEL_FILTER_FILE CONF_DIR "channel_filter.tsv"

#define RETRY_COUNT 3

typedef struct {
    int pin;
    uint8_t address[DS18B20_SCRATCHPAD_BYTE_NUM];
    int resolution;
}Device;

typedef struct {
    int id;
    Device device;
    FTS result;
    LCorrection *lcorrection;
    Mutex mutex;
} Channel;

DEC_LIST(Channel)
DEC_PLIST(Channel)

struct thread_st {
    int id;
    ChannelPList channel_plist;
    FilterPList filter_plist;
    I1List unique_pin_list;
    pthread_t thread;
    struct timespec cycle_duration;
};
typedef struct thread_st Thread;
DEC_LIST(Thread)

extern int readSettings();

extern void serverRun(int *state, int init_state);

extern void *threadFunction(void *arg);

extern int initApp();

extern int initData();

extern void freeData();

extern void freeApp();

extern void exit_nicely();

#endif

