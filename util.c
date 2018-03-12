
#include "main.h"

FUN_LIST_GET_BY_ID(Device)
FUN_LIST_GET_BY_ID(Thread)

int checkDevice(DeviceList *list) {
    int valid = 1;
    FORLi{
        if (!checkPin(LIi.pin)) {
            fprintf(stderr, "%s(): check device configuration file: bad pin where id=%d\n", F, LIi.id);
            valid = 0;
        }
        if (LIi.resolution < 9 || LIi.resolution > 12) {
            fprintf(stderr, "%s(): check device configuration file: bad resolution where id=%d\n", F, LIi.id);
            valid = 0;
        }
        //unique id
        FORLLj
        {
            if (LIi.id == LIj.id) {
                fprintf(stderr, "%s(): check device table: id should be unique, repetition found where id=%d\n", F, LIi.id);
                valid = 0;
            }
        }

    }

    return valid;
}

void freeDeviceList(DeviceList *list) {
    FORLi{
        freeMutex(&LIi.mutex);
    }
    FREE_LIST(list);
}

void freeThreadList(ThreadList *list) {
    FORLi{
        FREE_LIST(&LIi.device_plist);
        FREE_LIST(&LIi.unique_pin_list);
    }
    FREE_LIST(list);
}

void stopAllThreads(ThreadList * list) {
    FORLi{
#ifdef MODE_DEBUG
        printf("signaling thread %d to cancel...\n", LIi.id);
#endif
        if (pthread_cancel(LIi.thread) != 0) {
#ifdef MODE_DEBUG
            perror("pthread_cancel()");
#endif
        }
    }

    FORLi{
        void * result;
#ifdef MODE_DEBUG
        printf("joining thread %d...\n", LIi.id);
#endif
        if (pthread_join(LIi.thread, &result) != 0) {
#ifdef MODE_DEBUG
            perror("pthread_join()");
#endif
        }
        if (result != PTHREAD_CANCELED) {
#ifdef MODE_DEBUG
            printf("thread %d not canceled\n", LIi.id);
#endif
        }
    }
}

int setResolution(Device *item) {
    int res = ds18b20_parse_resolution(item->resolution);
    if (res == -1) {
        return 0;
    }
    for (int i = 0; i < RETRY_COUNT; i++) {
        if (ds18b20_set_resolution(item->pin, item->address, res)) {
            return 1;
        }
    }
#ifdef MODE_DEBUG
    fprintf(stderr, "%s(): setting resolution failed where device_id=%d\n", F, item->id);
#endif
    return 0;
}

void deviceRead(Device *item) {
    float v;
    int r = ds18b20_read_temp(item->pin, item->address, &v);
    if (r || (!r && item->value_state)) {
        if (lockMutex(&item->mutex)) {
            if (r) {
                item->tm = getCurrentTime();
                item->value = v;
                lcorrect(&item->value, item->lcorrection);
            }
            item->value_state = r;
            unlockMutex(&item->mutex);
        }
    }
}

int catFTS(Device *item, ACPResponse * response) {
    return acp_responseFTSCat(item->id, item->value, item->tm, item->value_state, response);
}

void printData(ACPResponse * response) {
#define DLi device_list.item[i]
#define TLi thread_list.item[i]
#define DPLj device_plist.item[j]
#define UPLj unique_pin_list.item[j]
    char q[LINE_SIZE];
    snprintf(q, sizeof q, "CONF_MAIN_FILE: %s\n", CONF_MAIN_FILE);
    SEND_STR(q)
    snprintf(q, sizeof q, "CONF_DEVICE_FILE: %s\n", CONF_DEVICE_FILE);
    SEND_STR(q)
    snprintf(q, sizeof q, "CONF_THREAD_FILE: %s\n", CONF_THREAD_FILE);
    SEND_STR(q)
    snprintf(q, sizeof q, "CONF_THREAD_DEVICE_FILE: %s\n", CONF_THREAD_DEVICE_FILE);
    SEND_STR(q)
    snprintf(q, sizeof q, "CONF_LCORRECTION_FILE: %s\n", CONF_LCORRECTION_FILE);
    SEND_STR(q)
    snprintf(q, sizeof q, "port: %d\n", sock_port);
    SEND_STR(q)
    snprintf(q, sizeof q, "app_state: %s\n", getAppState(app_state));
    SEND_STR(q)
    snprintf(q, sizeof q, "PID: %d\n", getpid());
    SEND_STR(q)

    acp_sendLCorrectionListInfo(&lcorrection_list, response, &peer_client);

    SEND_STR("+----------------------------------------------------------------------------+\n")
    SEND_STR("|                                 device                                     |\n")
    SEND_STR("+-----------+-----------+----------------+-----------+-----------+-----------+\n")
    SEND_STR("|  pointer  |     id    |     address    |    pin    | resolution| lcorr_ptr |\n")
    SEND_STR("+-----------+-----------+----------------+-----------+-----------+-----------+\n")
    FORLISTN(device_list, i) {
        snprintf(q, sizeof q, "|%11p|%11d|%.2hhx%.2hhx%.2hhx%.2hhx%.2hhx%.2hhx%.2hhx%.2hhx|%11d|%11d|%11p|\n",
                (void *) &DLi,
                DLi.id,
                DLi.address[0], DLi.address[1], DLi.address[2], DLi.address[3], DLi.address[4], DLi.address[5], DLi.address[6], DLi.address[7],
                DLi.pin,
                DLi.resolution,
                (void *) DLi.lcorrection
                );
        SEND_STR(q)
    }

    SEND_STR("+-----------+-----------+----------------+-----------+-----------+-----------+\n")

    SEND_STR("+-----------------------------------------------------------+\n")
    SEND_STR("|                    device runtime                         |\n")
    SEND_STR("+-----------+-----------+-----------+-----------+-----------+\n")
    SEND_STR("|     id    |   value   |value_state|  tm_sec   |  tm_nsec  |\n")
    SEND_STR("+-----------+-----------+-----------+-----------+-----------+\n")
    FORLISTN(device_list, i) {
        snprintf(q, sizeof q, "|%11d|%11f|%11d|%11ld|%11ld|\n",
                DLi.id,
                DLi.value,
                DLi.value_state,
                DLi.tm.tv_sec,
                DLi.tm.tv_nsec
                );
        SEND_STR(q)
    }

    SEND_STR("+-----------+-----------+-----------+-----------+-----------+\n")

    SEND_STR("+-----------------------+\n")
    SEND_STR("|      thread device    |\n")
    SEND_STR("+-----------+-----------+\n")
    SEND_STR("| thread_id | device_ptr|\n")
    SEND_STR("+-----------+-----------+\n")
    FORLISTN(thread_list, i) {

        FORLISTN(TLi.device_plist, j) {
            snprintf(q, sizeof q, "|%11d|%11p|\n",
                    TLi.id,
                    (void *) TLi.DPLj
                    );
            SEND_STR(q)
        }
    }

    SEND_STR("+-----------+-----------+\n")

    SEND_STR("+-----------------------+\n")
    SEND_STR("|   thread unique pin   |\n")
    SEND_STR("+-----------+-----------+\n")
    SEND_STR("| thread_id |    pin    |\n")
    SEND_STR("+-----------+-----------+\n")
    FORLISTN(thread_list, i) {

        FORLISTN(TLi.unique_pin_list, j) {
            snprintf(q, sizeof q, "|%11d|%11d|\n",
                    TLi.id,
                    TLi.UPLj
                    );
            SEND_STR(q)
        }
    }
    SEND_STR_L("+-----------+-----------+\n")
#undef DLi 
#undef TLi
#undef DPLj
#undef UPLj

}

void printHelp(ACPResponse * response) {
    char q[LINE_SIZE];
    SEND_STR("COMMAND LIST\n")
    snprintf(q, sizeof q, "%s\tput process into active mode; process will read configuration\n", ACP_CMD_APP_START);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tput process into standby mode; all running programs will be stopped\n", ACP_CMD_APP_STOP);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tfirst stop and then start process\n", ACP_CMD_APP_RESET);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tterminate process\n", ACP_CMD_APP_EXIT);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget state of process; response: B - process is in active mode, I - process is in standby mode\n", ACP_CMD_APP_PING);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget some variable's values; response will be packed into multiple packets\n", ACP_CMD_APP_PRINT);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget this help; response will be packed into multiple packets\n", ACP_CMD_APP_HELP);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget temperature in format: sensorId\\ttemperature\\ttimeSec\\ttimeNsec\\tvalid; program id expected\n", ACP_CMD_GET_FTS);
    SEND_STR_L(q)
}