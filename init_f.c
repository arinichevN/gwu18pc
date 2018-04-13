#include <string.h>

#include "main.h"

int readSettings(int *sock_port, const char *data_path) {
    TSVresult tsv = TSVRESULT_INITIALIZER;
    TSVresult* r = &tsv;
    if (!TSVinit(r, data_path)) {
        TSVclear(r);
        return 0;
    }
    char *str = TSVgetvalues(r, 0, "port");
    if (str == NULL) {
        TSVclear(r);
        return 0;
    }
    *sock_port = atoi(str);
    TSVclear(r);
    return 1;
}

static int parseAddress(uint8_t *address, char *address_str) {
    int n = sscanf(address_str, "%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx", &address[0], &address[1], &address[2], &address[3], &address[4], &address[5], &address[6], &address[7]);
    if (n != 8) {
        return 0;
    }
    return 1;
}

int initDevice(DeviceList *list, LCorrectionList *lcl, const char *data_path) {
    TSVresult tsv = TSVRESULT_INITIALIZER;
    TSVresult* r = &tsv;
    if (!TSVinit(r, data_path)) {
        TSVclear(r);
        return 0;
    }
    int n = TSVntuples(r);
    if (n <= 0) {
        TSVclear(r);
        return 1;
    }
    RESIZE_M_LIST(list, n);
    
    if (LML != n) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): failure while resizing list\n", F);
#endif
        TSVclear(r);
        return 0;
    }
    NULL_LIST(list);
    for (int i = 0; i < LML; i++) {
        LIi.id = LIi.result.id =TSVgetis(r, i, "id");
        LIi.pin = TSVgetis(r, i, "pin");
        LIi.resolution = TSVgetis(r, i, "resolution");
        int lcorrection_id = TSVgetis(r, i, "lcorrection_id");
        LIi.lcorrection = getLCorrectionById(lcorrection_id, lcl);
        if (!parseAddress(LIi.address, TSVgetvalues(r, i, "address"))) {
#ifdef MODE_DEBUG
            fprintf(stderr, "%s(): bad address where device_id=%d\n", F, LIi.id);
#endif
            break;
        }
        if (TSVnullreturned(r)) {
            break;
        }
        if (!initMutex(&LIi.mutex)) {
            break;
        }
        LL++;
    }
    TSVclear(r);
    if (LL != LML) {
#ifdef MODE_DEBUG

        fprintf(stderr, "%s(): failure while reading rows\n", F);
#endif
        return 0;
    }
    return 1;
}

static int checkThreadDevice(TSVresult* r) {
    int n = TSVntuples(r);
    int valid = 1;
    //unique thread_id and device_id
    for (int k = 0; k < n; k++) {
        int thread_id_k = TSVgetis(r, k, "thread_id");
        int device_id_k = TSVgetis(r, k, "device_id");
        if (TSVnullreturned(r)) {
            fprintf(stderr, "%s(): check thread_device configuration file: bad format\n", F);
            return 0;
        }
        for (int g = k + 1; g < n; g++) {
            int thread_id_g = TSVgetis(r, g, "thread_id");
            int device_id_g = TSVgetis(r, g, "device_id");
            if (TSVnullreturned(r)) {
                fprintf(stderr, "%s(): check thread_device configuration file: bad format\n", F);
                return 0;
            }
            if (thread_id_k == thread_id_g && device_id_k == device_id_g) {
                fprintf(stderr, "%s(): check thread_device configuration file: thread_id and device_id shall be unique (row %d and row %d)\n", F, k, g);
                valid = 0;
            }
        }

    }
    //unique device_id
    for (int k = 0; k < n; k++) {
        int device_id_k = TSVgetis(r, k, "device_id");
        if (TSVnullreturned(r)) {
            fprintf(stderr, "%s(): check thread_device configuration file: bad format\n", F);
            return 0;
        }
        for (int g = k + 1; g < n; g++) {
            int device_id_g = TSVgetis(r, g, "device_id");
            if (TSVnullreturned(r)) {
                fprintf(stderr, "%s(): check thread_device configuration file: bad format\n", F);
                return 0;
            }
            if (device_id_k == device_id_g) {
                fprintf(stderr, "%s(): check thread_device configuration file: device_id shall be unique (row %d and row %d)\n", F, k, g);
                valid = 0;

                break;
            }
        }

    }
    return valid;
}

static int countThreadItem(int thread_id_in, TSVresult* r) {
    int c = 0;
    int n = TSVntuples(r);
    for (int k = 0; k < n; k++) {
        int thread_id = TSVgetis(r, k, "thread_id");
        if (TSVnullreturned(r)) {
            return 0;
        }
        if (thread_id == thread_id_in) {
            c++;
        }
    }
    return c;
}

static int countThreadUniquePin(Thread *thread) {
#define TDL thread->device_plist
#define TDLL TDL.length
    int c = 0;
    for (size_t i = 0; i < TDLL; i++) {
        int f = 0;
        for (size_t j = i + 1; j < TDLL; j++) {
            if (TDL.item[i]->pin == TDL.item[j]->pin) {
                f = 1;
                break;
            }
        }
        if (!f) {
            c++;
        }
    }
    return c;
#undef TDL 
#undef TDLL 
}

static int buildThreadUniquePin(Thread *thread) {
#define TDL thread->device_plist
#define TDLL TDL.length
#define TUPL thread->unique_pin_list
#define TUPLL TUPL.length
#define TUPLML TUPL.max_length
    int n = countThreadUniquePin(thread);
    if (n <= 0) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): no unique pin found where thread_id=%d\n", F, thread->id);
#endif
        return 0;
    }
    RESIZE_M_LIST(&TUPL, n);
   
    if (TUPLML != n) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): failure while resizing thread unique pin list where thread_id=%d\n", F, thread->id);
#endif
        return 0;
    }
     NULL_LIST(&TUPL);
    for (size_t i = 0; i < TDLL; i++) {
        int f = 0;
        for (size_t j = i + 1; j < TDLL; j++) {
            if (TDL.item[i]->pin == TDL.item[j]->pin) {
                f = 1;
                break;
            }
        }
        if (!f) {
            TUPL.item[TUPLL] = TDL.item[i]->pin;
            TUPLL++;
        }
    }
    if (TUPLML != TUPLL) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): failure while assigning thread unique pin list where thread_id=%d\n", F, thread->id);
#endif
        return 0;
    }
    return 1;
#undef TDL 
#undef TDLL
#undef TUPL
#undef TUPLL 
#undef TUPLML 
}

int initThread(ThreadList *list, DeviceList *dl, const char *thread_path, const char *thread_device_path) {
    TSVresult tsv = TSVRESULT_INITIALIZER;
    TSVresult* r = &tsv;
    if (!TSVinit(r, thread_path)) {
        TSVclear(r);
        return 0;
    }
    int n = TSVntuples(r);
    if (n <= 0) {
        TSVclear(r);
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): no data rows in file\n", F);
#endif
        return 0;
    }
    RESIZE_M_LIST(list, n);
    
    if (LML != n) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): failure while resizing list\n", F);
#endif
        TSVclear(r);
        return 0;
    }
    NULL_LIST(list);
    for (int i = 0; i < LML; i++) {
        LIi.id = TSVgetis(r, i, "id");
        LIi.cycle_duration.tv_sec = TSVgetis(r, i, "cd_sec");
        LIi.cycle_duration.tv_nsec = TSVgetis(r, i, "cd_nsec");
        RESET_LIST(&LIi.device_plist);
        if (TSVnullreturned(r)) {
            break;
        }
        LL++;
    }
    TSVclear(r);
    if (LL != LML) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): failure while reading rows\n", F);
#endif
        return 0;
    }
    if (!TSVinit(r, thread_device_path)) {
        TSVclear(r);
        return 0;
    }
    n = TSVntuples(r);
    if (n <= 0) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s(): no data rows in thread device file\n", F);
#endif
        TSVclear(r);
        return 0;
    }
    if (!checkThreadDevice(r)) {
        TSVclear(r);
        return 0;
    }

    FORLi{
        int thread_device_count = countThreadItem(LIi.id, r);
        //allocating memory for thread device pointers
        RESET_LIST(&LIi.device_plist)
        if (thread_device_count <= 0) {
            continue;
        }
        RESIZE_M_LIST(&LIi.device_plist, thread_device_count)
        if (LIi.device_plist.max_length != thread_device_count) {
#ifdef MODE_DEBUG
            fprintf(stderr, "%s(): failure while resizing device_plist list\n", F);
#endif
            TSVclear(r);
            return 0;
        }
        //assigning devices to this thread
        for (int k = 0; k < n; k++) {
            int thread_id = TSVgetis(r, k, "thread_id");
            int device_id = TSVgetis(r, k, "device_id");
            if (TSVnullreturned(r)) {
                break;
            }
            if (thread_id == LIi.id) {
                Device *d = getDeviceById(device_id, dl);
                if(d!=NULL){
                LIi.device_plist.item[LIi.device_plist.length] = d;
                LIi.device_plist.length++;
                }
            }
        }
        if (LIi.device_plist.max_length != LIi.device_plist.length) {
#ifdef MODE_DEBUG
            fprintf(stderr, "%s(): failure while assigning devices to threads: some not found\n", F);
#endif
            TSVclear(r);
            return 0;
        }
        if (!buildThreadUniquePin(&LIi)) {
            TSVclear(r);
            return 0;
        }
    }
    TSVclear(r);

    //starting threads
    FORLi{
        if (!createMThread(&LIi.thread, &threadFunction, &LIi)) {
            return 0;
        }
    }
    return 1;
}
