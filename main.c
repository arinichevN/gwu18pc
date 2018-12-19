#include "main.h"

int app_state = APP_INIT;
int sock_port = -1;
int sock_fd = -1;

Peer peer_client = {.fd = &sock_fd, .addr_size = sizeof peer_client.addr};

ChannelList channel_list = LIST_INITIALIZER;
LCorrectionList lcorrection_list = LIST_INITIALIZER;
ThreadList thread_list = LIST_INITIALIZER;


#include "util.c"
#include "init_f.c"

void serverRun(int *state, int init_state) {
    SERVER_HEADER
    SERVER_APP_ACTIONS
    DEF_SERVER_I1LIST
    if (ACP_CMD_IS(ACP_CMD_GET_FTS)) {
		SERVER_PARSE_I1LIST
        FORLISTN(i1l, i) {
            Channel *item;
            LIST_GETBYID(item, &channel_list, i1l.item[i]);
            if ( item == NULL ) continue;
            if ( !catFTS ( item, &response ) ) return;
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

#define CPL item->channel_plist
#define CPLL CPL.length
#define CPLIi CPL.item[i]
#define CPLIiD CPLIi->device

#ifndef CPU_ANY
    for (size_t i = 0; i < UPLL; i++) {
        pinPUD(UPLIi, PUD_OFF);
        pinModeOut(UPLIi);
    }
    for (size_t i = 0; i < CPLL; i++) {
        if (!setResolution(&CPLIi->device)) {
            printde("failed to set resolution for device with id=%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx\n", CPLIiD.address[0], CPLIiD.address[1], CPLIiD.address[2], CPLIiD.address[3], CPLIiD.address[4], CPLIiD.address[5], CPLIiD.address[6], CPLIiD.address[7]);
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
            for (int i = 0; i < CPLL; i++) {
                channelRead(CPLIi);
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

#undef CPL 
#undef CPLL
#undef CPLIi 
#undef CPLIiD
}

int initApp() {
    if (!readSettings(&sock_port, CONF_MAIN_FILE)) {
        putsde("failed to read settings\n");
        return 0;
    }
    if (!initServer(&sock_fd, sock_port)) {
        putsde("failed to initialize socket server\n");
        return 0;
    }
    if (!gpioSetup()) {
        freeSocketFd(&sock_fd);
        putsde("failed to initialize GPIO\n");
        return 0;
    }
    return 1;
}

int initData() {
    initLCorrection(&lcorrection_list, CONF_LCORRECTION_FILE);
    if (!initChannel(&channel_list, &lcorrection_list, CONF_CHANNEL_FILE)) {
        freeChannelList(&channel_list);
        FREE_LIST(&lcorrection_list);
        return 0;
    }
    if (!checkChannel(&channel_list)) {
        freeChannelList(&channel_list);
        FREE_LIST(&lcorrection_list);
        return 0;
    }
    if (!initThread(&thread_list, &channel_list, CONF_THREAD_FILE, CONF_THREAD_CHANNEL_FILE)) {
        freeThreadList(&thread_list);
        FREE_LIST(&channel_list);
        FREE_LIST(&lcorrection_list);
        return 0;
    }

    return 1;
}

void freeData() {
    STOP_ALL_LIST_THREADS(&thread_list);
    freeThreadList(&thread_list);
    FREE_LIST(&channel_list);
    FREE_LIST(&lcorrection_list);
}

void freeApp() {
    freeData();
    freeSocketFd(&sock_fd);
    gpioFree();
}

void exit_nicely ( ) {
    freeApp();
    putsdo ( "\nexiting now...\n" );
    exit ( EXIT_SUCCESS );
}

int main(int argc, char** argv) {
    if (geteuid() != 0) {
		putsde("root user expected\n");
        return (EXIT_FAILURE);
    }
#ifndef MODE_DEBUG
    daemon(0, 0);
#endif
    conSig(&exit_nicely);
    if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
        perrorl ( "mlockall()" );
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
                if ( !initApp() ) {
                  return ( EXIT_FAILURE );
                }
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
                freeApp();
                putsde ( "unknown application state\n" );
                return ( EXIT_FAILURE );
        }
    }
    freeApp();
    return (EXIT_SUCCESS);
}

