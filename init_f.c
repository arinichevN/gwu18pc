#include <string.h>

#include "main.h"

int readSettings ( int *sock_port, const char *data_path ) {
    TSVresult tsv = TSVRESULT_INITIALIZER;
    TSVresult* r = &tsv;
    if ( !TSVinit ( r, data_path ) ) {
        TSVclear ( r );
        return 0;
    }
    char *str = TSVgetvalues ( r, 0, "port" );
    if ( str == NULL ) {
        TSVclear ( r );
        return 0;
    }
    *sock_port = atoi ( str );
    TSVclear ( r );
    return 1;
}

static int parseAddress ( uint8_t *address, char *address_str ) {
    int n = sscanf ( address_str, "%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx", &address[0], &address[1], &address[2], &address[3], &address[4], &address[5], &address[6], &address[7] );
    if ( n != 8 ) {
        return 0;
    }
    return 1;
}

int initChannel ( ChannelList *list,  LCorrectionList *lcl, const char *data_path ) {
    TSVresult tsv = TSVRESULT_INITIALIZER;
    TSVresult* r = &tsv;
    if ( !TSVinit ( r, data_path ) ) {
        TSVclear ( r );
        return 0;
    }
    int n = TSVntuples ( r );
    if ( n <= 0 ) {
        TSVclear ( r );
        return 1;
    }
    RESIZE_M_LIST ( list, n );

    if ( LML != n ) {
        putsde ( "failure while resizing list\n" );
        TSVclear ( r );
        return 0;
    }
    NULL_LIST ( list );
    for ( int i = 0; i < LML; i++ ) {
        LIi.id = LIi.result.id =TSVgetis ( r, i, "id" );
        LIi.device.pin = TSVgetis ( r, i, "dev_pin" );
        LIi.device.resolution = TSVgetis ( r, i, "dev_resolution" );
        int lcorrection_id = TSVgetis ( r, i, "lcorrection_id" );
        LIST_GETBYID ( LIi.lcorrection, lcl, lcorrection_id );
        if ( !parseAddress ( LIi.device.address, TSVgetvalues ( r, i, "dev_address" ) ) ) {
            printde ( "bad address where channel_id=%d\n", LIi.id );
            break;
        }
        if ( TSVnullreturned ( r ) ) {
            break;
        }
        if ( !initMutex ( &LIi.mutex ) ) {
            break;
        }
        LL++;
    }
    TSVclear ( r );
    if ( LL != LML ) {
        putsde ( "failure while reading rows\n" );
        return 0;
    }
    return 1;
}
int prepFilterList ( FilterList *list, ChannelList *cl ) {
    RESIZE_M_LIST ( list, cl->length );
    if ( LML != cl->length ) {
        putsde ( "failure while resizing filter list\n" );
        return 0;
    }
    NULL_LIST ( list );
    for ( int i = 0; i < LML; i++ ) {
        LIi.id = cl->item[i].id;
        LL++;
    }
    if ( LL != LML ) {
        putsde ( "failure while making filter list\n" );
        return 0;
    }
    return 1;
}
static int checkThreadChannel ( TSVresult* r ) {
    int n = TSVntuples ( r );
    int valid = 1;
    //unique thread_id and channel_id
    for ( int k = 0; k < n; k++ ) {
        int thread_id_k = TSVgetis ( r, k, "thread_id" );
        int channel_id_k = TSVgetis ( r, k, "channel_id" );
        if ( TSVnullreturned ( r ) ) {
            putsde ( "check thread_channel configuration file: bad format\n" );
            return 0;
        }
        for ( int g = k + 1; g < n; g++ ) {
            int thread_id_g = TSVgetis ( r, g, "thread_id" );
            int channel_id_g = TSVgetis ( r, g, "channel_id" );
            if ( TSVnullreturned ( r ) ) {
                putsde ( "check thread_channel configuration file: bad format\n" );
                return 0;
            }
            if ( thread_id_k == thread_id_g && channel_id_k == channel_id_g ) {
                printde ( "check thread_channel configuration file: thread_id and channel_id shall be unique (row %d and row %d)\n", k, g );
                valid = 0;
            }
        }

    }
    //unique channel_id
    for ( int k = 0; k < n; k++ ) {
        int channel_id_k = TSVgetis ( r, k, "channel_id" );
        if ( TSVnullreturned ( r ) ) {
            fprintf ( stderr, "%s(): check thread_channel configuration file: bad format\n", F );
            return 0;
        }
        for ( int g = k + 1; g < n; g++ ) {
            int channel_id_g = TSVgetis ( r, g, "channel_id" );
            if ( TSVnullreturned ( r ) ) {
                fprintf ( stderr, "%s(): check thread_channel configuration file: bad format\n", F );
                return 0;
            }
            if ( channel_id_k == channel_id_g ) {
                fprintf ( stderr, "%s(): check thread_channel configuration file: channel_id shall be unique (row %d and row %d)\n", F, k, g );
                valid = 0;

                break;
            }
        }

    }
    return valid;
}

static int countThreadItem ( int thread_id_in, TSVresult* r ) {
    int c = 0;
    int n = TSVntuples ( r );
    for ( int k = 0; k < n; k++ ) {
        int thread_id = TSVgetis ( r, k, "thread_id" );
        if ( TSVnullreturned ( r ) ) {
            return 0;
        }
        if ( thread_id == thread_id_in ) {
            c++;
        }
    }
    return c;
}

static int countThreadUniquePin ( Thread *thread ) {
#define TDL thread->channel_plist
#define TDLL TDL.length
    int c = 0;
    for ( size_t i = 0; i < TDLL; i++ ) {
        int f = 0;
        for ( size_t j = i + 1; j < TDLL; j++ ) {
            if ( TDL.item[i]->device.pin == TDL.item[j]->device.pin ) {
                f = 1;
                break;
            }
        }
        if ( !f ) {
            c++;
        }
    }
    return c;
#undef TDL
#undef TDLL
}

static int buildThreadUniquePin ( Thread *thread ) {
#define TCPL thread->channel_plist
#define TCPLL TCPL.length
#define TUPL thread->unique_pin_list
#define TUPLL TUPL.length
#define TUPLML TUPL.max_length
    int n = countThreadUniquePin ( thread );
    if ( n <= 0 ) {
        printde ( "no unique pin found where thread_id=%d\n", thread->id );
        return 0;
    }
    RESIZE_M_LIST ( &TUPL, n );

    if ( TUPLML != n ) {
        printde ( "failure while resizing thread unique pin list where thread_id=%d\n", thread->id );
        return 0;
    }
    NULL_LIST ( &TUPL );
    for ( size_t i = 0; i < TCPLL; i++ ) {
        int f = 0;
        for ( size_t j = i + 1; j < TCPLL; j++ ) {
            if ( TCPL.item[i]->device.pin == TCPL.item[j]->device.pin ) {
                f = 1;
                break;
            }
        }
        if ( !f ) {
            TUPL.item[TUPLL] = TCPL.item[i]->device.pin;
            TUPLL++;
        }
    }
    if ( TUPLML != TUPLL ) {
        printde ( "failure while assigning thread unique pin list where thread_id=%d\n", thread->id );
        return 0;
    }
    return 1;
#undef TCPL
#undef TCPLL
#undef TUPL
#undef TUPLL
#undef TUPLML
}

int initThread ( ThreadList *list, ChannelList *cl, FilterList *fl, const char *thread_path, const char *thread_channel_path ) {
    TSVresult tsv = TSVRESULT_INITIALIZER;
    TSVresult* r = &tsv;
    if ( !TSVinit ( r, thread_path ) ) {
        TSVclear ( r );
        return 0;
    }
    int n = TSVntuples ( r );
    if ( n <= 0 ) {
        TSVclear ( r );
        putsde ( "no data rows in file\n" );
        return 0;
    }
    RESIZE_M_LIST ( list, n );

    if ( LML != n ) {
        putsde ( "failure while resizing list\n" );
        TSVclear ( r );
        return 0;
    }
    NULL_LIST ( list );
    for ( int i = 0; i < LML; i++ ) {
        LIi.id = TSVgetis ( r, i, "id" );
        LIi.cycle_duration.tv_sec = TSVgetis ( r, i, "cd_sec" );
        LIi.cycle_duration.tv_nsec = TSVgetis ( r, i, "cd_nsec" );
        RESET_LIST ( &LIi.channel_plist );
        if ( TSVnullreturned ( r ) ) {
            break;
        }
        LL++;
    }
    TSVclear ( r );
    if ( LL != LML ) {
        putsde ( "failure while reading rows\n" );
        return 0;
    }
    if ( !TSVinit ( r, thread_channel_path ) ) {
        TSVclear ( r );
        return 0;
    }
    n = TSVntuples ( r );
    if ( n <= 0 ) {
        putsde ( "no data rows in thread channel file\n" );
        TSVclear ( r );
        return 0;
    }
    if ( !checkThreadChannel ( r ) ) {
        TSVclear ( r );
        return 0;
    }

    FORLi {
        int thread_channel_count = countThreadItem ( LIi.id, r );
        //allocating memory for thread channel pointers
        RESET_LIST ( &LIi.channel_plist )
        RESET_LIST ( &LIi.filter_plist )
        if ( thread_channel_count <= 0 ) {
            continue;
        }
        RESIZE_M_LIST ( &LIi.channel_plist, thread_channel_count )
        RESIZE_M_LIST ( &LIi.filter_plist, thread_channel_count )
        if ( LIi.channel_plist.max_length != thread_channel_count ) {
            putsde ( "failure while resizing channel_plist list\n" );
            TSVclear ( r );
            return 0;
        }
        if ( LIi.filter_plist.max_length != thread_channel_count ) {
            putsde ( "failure while resizing filter_plist list\n" );
            TSVclear ( r );
            return 0;
        }
        //assigning channels to this thread
        for ( int k = 0; k < n; k++ ) {
            int thread_id = TSVgetis ( r, k, "thread_id" );
            int channel_id = TSVgetis ( r, k, "channel_id" );
            if ( TSVnullreturned ( r ) ) {
                break;
            }
            if ( thread_id == LIi.id ) {
                Channel *d;
                LIST_GETBYID ( d, cl, channel_id );
                if ( d!=NULL ) {
                    LIi.channel_plist.item[LIi.channel_plist.length] = d;
                    LIi.channel_plist.length++;
                }
                Filter *ft;
                LIST_GETBYID ( ft, fl, channel_id );
                if ( ft!=NULL ) {
                    LIi.filter_plist.item[LIi.filter_plist.length] = ft;
                    LIi.filter_plist.length++;
                }
            }
        }
        if ( LIi.channel_plist.max_length != LIi.channel_plist.length ) {
            putsde ( "failure while assigning channels to threads: some not found\n" );
            TSVclear ( r );
            return 0;
        }
        if ( LIi.filter_plist.max_length != LIi.filter_plist.length ) {
            putsde ( "failure while assigning filters to threads: some not found\n" );
            TSVclear ( r );
            return 0;
        }
        if ( !buildThreadUniquePin ( &LIi ) ) {
            TSVclear ( r );
            return 0;
        }
    }
    TSVclear ( r );

    //starting threads
    FORLi {
        if ( !createMThread ( &LIi.thread, &threadFunction, &LIi ) ) {
            return 0;
        }
    }
    return 1;
}
