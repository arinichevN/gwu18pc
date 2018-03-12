#include "main.h"

int app_state = APP_INIT;
int sock_port = -1;
int sock_fd = -1;

Peer peer_client = {.fd = &sock_fd, .addr_size = sizeof peer_client.addr};

DeviceList device_list = LIST_INITIALIZER;
LCorrectionList lcorrection_list = LIST_INITIALIZER;
ThreadList thread_list = LIST_INITIALIZER;


#include "util.c"
#include "init_f.c"

void serverRun(int *state, int init_state) {
    SERVER_HEADER
    SERVER_APP_ACTIONS
    DEF_SERVER_I1LIST
    if (ACP_CMD_IS(ACP_CMD_GET_FTS)) {
        acp_requestDataToI1List(&request, &i1l);
        if (i1l.length <= 0) {
            return;
        }
        for (int i = 0; i < i1l.length; i++) {
            Device *item = getDeviceById(i1l.item[i], &device_list);
            if (item != NULL) {
                if (lockMutex(&item->mutex)) {
                    if (!catFTS(item, &response)) {
                        return;
                    }
                    unlockMutex(&item->mutex);
                }
            }
        }
    }
    acp_responseSend(&response, &peer_client);
}

void cleanup_handler(void *arg) {
    Thread *item = arg;
    printf("cleaning up thread %d\n", item->id);
}

void *threadFunction(void *arg) {
    Thread *item = arg;
#ifdef MODE_DEBUG
    printf("thread for program with id=%d has been started\n", item->id);
#endif
#ifdef MODE_DEBUG
    pthread_cleanup_push(cleanup_handler, item);
#endif

#define UPL item->unique_pin_list
#define UPLL UPL.length
#define UPLIi UPL.item[i]

#define DPL item->device_plist
#define DPLL DPL.length
#define DPLIi DPL.item[i]

#ifndef CPU_ANY
    for (size_t i = 0; i < UPLL; i++) {
        pinPUD(UPLIi, PUD_OFF);
        pinModeOut(UPLIi);
    }
    for (size_t i = 0; i < DPLL; i++) {
        if (!setResolution(DPLIi)) {
#ifdef MODE_DEBUG
            fprintf(stderr, "%s(): failed to set resolution for device with id=%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx\n", F, DPLIi->address[0], DPLIi->address[1], DPLIi->address[2], DPLIi->address[3], DPLIi->address[4], DPLIi->address[5], DPLIi->address[6], DPLIi->address[7]);
#endif
        }
    }
#endif
    while (1) {
        struct timespec t1 = getCurrentTime();
        int old_state;
        if (threadCancelDisable(&old_state)) {
#ifndef CPU_ANY
            for (int i = 0; i < UPLL; i++) {
                ds18b20_convert_t_all(UPLIi);
            }
            for (int i = 0; i < UPLL; i++) {
                ds18b20_wait_convertion(UPLIi);
            }
            for (int i = 0; i < DPLL; i++) {
                deviceRead(DPLIi);
            }
#endif
            threadSetCancelState(old_state);
        }
        sleepRest(item->cycle_duration, t1);
    }
#ifdef MODE_DEBUG
    pthread_cleanup_pop(1);
#endif

#undef UPL
#undef UPLL 
#undef UPLIi 

#undef DPL 
#undef DPLL
#undef DPLIi 
}

void initApp() {
    if (!readSettings(&sock_port, CONF_MAIN_FILE)) {
        exit_nicely_e("initApp: failed to read configuration file\n");
    }
    if (!initServer(&sock_fd, sock_port)) {
        exit_nicely_e("initApp: failed to initialize server\n");
    }
    if (!gpioSetup()) {
        exit_nicely_e("initApp: failed to initialize GPIO\n");
    }
}

int initData() {
    initLCorrection(&lcorrection_list, CONF_LCORRECTION_FILE);
    if (!initDevice(&device_list, &lcorrection_list, CONF_DEVICE_FILE)) {
        freeDeviceList(&device_list);
        FREE_LIST(&lcorrection_list);
        return 0;
    }
    if (!checkDevice(&device_list)) {
        freeDeviceList(&device_list);
        FREE_LIST(&lcorrection_list);
        return 0;
    }
    if (!initThread(&thread_list, &device_list, CONF_THREAD_FILE, CONF_THREAD_DEVICE_FILE)) {
        freeThreadList(&thread_list);
        FREE_LIST(&device_list);
        FREE_LIST(&lcorrection_list);
        return 0;
    }

    return 1;
}

void freeData() {
    stopAllThreads(&thread_list);
    freeThreadList(&thread_list);
    FREE_LIST(&device_list);
    FREE_LIST(&lcorrection_list);
}

void freeApp() {
    freeData();
    freeSocketFd(&sock_fd);
}

void exit_nicely() {
    freeApp();
    puts("\nBye...");
    exit(EXIT_SUCCESS);
}

void exit_nicely_e(char *s) {
    fprintf(stderr, "%s", s);
    freeApp();
    exit(EXIT_FAILURE);
}

int main(int argc, char** argv) {
    if (geteuid() != 0) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s: root user expected\n", APP_NAME_STR);
#endif
        return (EXIT_FAILURE);
    }
#ifndef MODE_DEBUG
    daemon(0, 0);
#endif
    conSig(&exit_nicely);
    if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
        perror("main: memory locking failed");
    }
#ifndef MODE_DEBUG
    setPriorityMax(SCHED_FIFO);
#endif
    int data_initialized = 0;
    while (1) {
#ifdef MODE_DEBUG
        printf("%s(): %s %d\n", F, getAppState(app_state), data_initialized);
#endif
        switch (app_state) {
            case APP_INIT:
                initApp();
                app_state = APP_INIT_DATA;
                break;
            case APP_INIT_DATA:
                data_initialized = initData();
                app_state = APP_RUN;
                break;
            case APP_RUN:
                serverRun(&app_state, data_initialized);
                break;
            case APP_STOP:
                freeData();
                data_initialized = 0;
                app_state = APP_RUN;
                break;
            case APP_RESET:
                freeApp();
                data_initialized = 0;
                app_state = APP_INIT;
                break;
            case APP_EXIT:
                exit_nicely();
                break;
            default:
                exit_nicely_e("main: unknown application state");
                break;
        }
    }
    freeApp();
    return (EXIT_SUCCESS);
}

