
#include "main.h"


int checkChannel ( ChannelList *list ) {
    int valid = 1;
    FORLi {
        if ( !checkPin ( LIi.device.pin ) ) {
            fprintf ( stderr, "%s(): check channel configuration file: bad pin where id=%d\n", F, LIi.id );
            valid = 0;
        }
        if ( LIi.device.resolution < 9 || LIi.device.resolution > 12 ) {
            fprintf ( stderr, "%s(): check channel configuration file: bad resolution where id=%d\n", F, LIi.id );
            valid = 0;
        }
        //unique id
        FORLLj
        {
            if ( LIi.id == LIj.id ) {
                fprintf ( stderr, "%s(): check channel configuration file: id should be unique, repetition found where id=%d\n", F, LIi.id );
                valid = 0;
            }
        }

    }

    return valid;
}

void freeChannelList ( ChannelList *list ) {
    FORLi {
        freeMutex ( &LIi.mutex );
    }
    FREE_LIST ( list );
}

void freeThreadList ( ThreadList *list ) {
    FORLi {
        FREE_LIST ( &LIi.channel_plist );
        FREE_LIST ( &LIi.filter_plist );
        FREE_LIST ( &LIi.unique_pin_list );
    }
    FREE_LIST ( list );
}

int setResolution ( Device *item ) {
    int res = ds18b20_parse_resolution ( item->resolution );
    if ( res == -1 ) {
        return 0;
    }
    for ( int i = 0; i < RETRY_COUNT; i++ ) {
        if ( ds18b20_set_resolution ( item->pin, item->address, res ) ) {
            return 1;
        }
    }
    printde ( "setting resolution failed where device_address=%.2hhx%.2hhx%.2hhx%.2hhx%.2hhx%.2hhx%.2hhx%.2hhx\n", item->address[0],item->address[1],item->address[2],item->address[3],item->address[4],item->address[5],item->address[6],item->address[7] );
    return 0;
}
void convertTemperature ( UniquePinList *list ) {
    FORLi {
        ds18b20_convert_t_all ( LIi.pin );
    }
}
void waitConvertionAndSetTime ( UniquePinList *list ) {
    int tset[list->length];
    FORLi tset[i]=0;
    while ( 1 ) {
        int done=1;
        FORLi {
            if ( !onewire_read_bit ( LIi.pin ) ) {
                if ( tset[i] ) tset[i]=0;
                done=0;
            } else{
                if ( !tset[i] ) {
                    LIi.tm=getCurrentTime();
                    tset[i]=1;
                }
            }
        }
        if ( done ) break;
    }
    DELAY_US_BUSY ( 480 );
}
void channelRead ( Channel *item, Filter *filter, struct timespec tm ) {
    double v;
#ifdef CPU_ANY
    int r=1;
    v=0.0;
#else
    int r = ds18b20_read_temp ( item->device.pin, item->device.address, &v );
#endif
    if ( r || ( !r && item->result.state ) ) {
        if ( lockMutex ( &item->mutex ) ) {
            if ( r ) {
                item->result.tm = *item->tm;
                for ( int i = 0; i < filter->af_list.length; i++ ) {
                    filter->af_list.item[i].fnc ( &v, filter->af_list.item[i].ptr );
                }
                lcorrect ( &v, item->lcorrection );
                item->result.value = v;
            }
            item->result.state = r;
            unlockMutex ( &item->mutex );
        }
    }
    printdo ( "%d %.3f %.3f %d %ld %ld\n", item->id, v,  item->result.value, item->result.state, item->result.tm.tv_sec, item->result.tm.tv_nsec );
}

int catFTS ( Channel *item, ACPResponse * response ) {
    if ( lockMutex ( &item->mutex ) ) {
        int r =  acp_responseFTSCat ( item->result.id, item->result.value, item->result.tm, item->result.state, response );
        unlockMutex ( &item->mutex );
        return r;
    }
    return 0;
}

void printData ( ACPResponse * response ) {
#define CLi channel_list.item[i]
#define CLiD CLi.device
#define TLi thread_list.item[i]
#define CPLj channel_plist.item[j]
#define UPLj unique_pin_list.item[j]
    char q[LINE_SIZE];
    snprintf ( q, sizeof q, "CONF_MAIN_FILE: %s\n", CONF_MAIN_FILE );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "CONF_CHANNEL_FILE: %s\n", CONF_CHANNEL_FILE );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "CONF_THREAD_FILE: %s\n", CONF_THREAD_FILE );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "CONF_THREAD_CHANNEL_FILE: %s\n", CONF_THREAD_CHANNEL_FILE );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "CONF_LCORRECTION_FILE: %s\n", CONF_LCORRECTION_FILE );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "CONF_FILTER_MA_FILE: %s\n", CONF_FILTER_MA_FILE );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "CONF_FILTER_EXP_FILE: %s\n", CONF_FILTER_EXP_FILE );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "CONF_CHANNEL_FILTER_FILE: %s\n", CONF_CHANNEL_FILTER_FILE );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "port: %d\n", sock_port );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "app_state: %s\n", getAppState ( app_state ) );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "PID: %d\n", getpid() );
    SEND_STR ( q )

    acp_sendLCorrectionListInfo ( &lcorrection_list, response, &peer_client );

    SEND_STR ( "+----------------------------------------------------------------------------------------+\n" )
    SEND_STR ( "|                                      channel                                           |\n" )
    SEND_STR ( "+-----------+-----------+-----------+-----------+----------------------------------------+\n" )
    SEND_STR ( "|           |           |           |           |              device                    |\n" )
    SEND_STR ( "|           |           |           |           +----------------+-----------+-----------+\n" )
    SEND_STR ( "|  pointer  |     id    | lcorr_ptr |   tm_ptr  |     address    |    pin    | resolution|\n" )
    SEND_STR ( "+-----------+-----------+-----------+-----------+----------------+-----------+-----------+\n" )
    FORLISTN ( channel_list, i ) {
        snprintf ( q, sizeof q, "|%11p|%11d|%11p|%11p|%.2hhx%.2hhx%.2hhx%.2hhx%.2hhx%.2hhx%.2hhx%.2hhx|%11d|%11d|\n",
                   ( void * ) &CLi,
                   CLi.id,
                   ( void * ) CLi.lcorrection,
                   ( void * ) CLi.tm,
                   CLiD.address[0], CLiD.address[1], CLiD.address[2], CLiD.address[3], CLiD.address[4], CLiD.address[5], CLiD.address[6], CLiD.address[7],
                   CLiD.pin,
                   CLiD.resolution
                 );
        SEND_STR ( q )
    }
    SEND_STR ( "+-----------+-----------+-----------+-----------+----------------+-----------+-----------+\n" )

    SEND_STR ( "+-----------------------------------------------------------+\n" )
    SEND_STR ( "|                    channel runtime                        |\n" )
    SEND_STR ( "+-----------+-----------+-----------+-----------+-----------+\n" )
    SEND_STR ( "|     id    |   value   |value_state|  tm_sec   |  tm_nsec  |\n" )
    SEND_STR ( "+-----------+-----------+-----------+-----------+-----------+\n" )
    FORLISTN ( channel_list, i ) {
        snprintf ( q, sizeof q, "|%11d|%11.3f|%11d|%11ld|%11ld|\n",
                   CLi.id,
                   CLi.result.value,
                   CLi.result.state,
                   CLi.result.tm.tv_sec,
                   CLi.result.tm.tv_nsec
                 );
        SEND_STR ( q )
    }

    SEND_STR ( "+-----------+-----------+-----------+-----------+-----------+\n" )

    acp_sendFilterListInfo ( &filter_list, response, &peer_client );

    SEND_STR ( "+-----------------------+\n" )
    SEND_STR ( "|      thread channel   |\n" )
    SEND_STR ( "+-----------+-----------+\n" )
    SEND_STR ( "| thread_id |channel_ptr|\n" )
    SEND_STR ( "+-----------+-----------+\n" )
    FORLISTN ( thread_list, i ) {

        FORLISTN ( TLi.channel_plist, j ) {
            snprintf ( q, sizeof q, "|%11d|%11p|\n",
                       TLi.id,
                       ( void * ) TLi.CPLj
                     );
            SEND_STR ( q )
        }
    }

    SEND_STR ( "+-----------+-----------+\n" )

    SEND_STR ( "+-----------------------------------+\n" )
    SEND_STR ( "|          thread unique pin        |\n" )
    SEND_STR ( "+-----------+-----------+-----------+\n" )
    SEND_STR ( "| thread_id |    pin    |   tm_ptr  |\n" )
    SEND_STR ( "+-----------+-----------+-----------+\n" )
    FORLISTN ( thread_list, i ) {

        FORLISTN ( TLi.unique_pin_list, j ) {
            snprintf ( q, sizeof q, "|%11d|%11d|%11p|\n",
                       TLi.id,
                       TLi.UPLj.pin,
                       ( void * ) &TLi.UPLj.tm
                     );
            SEND_STR ( q )
        }
    }
    SEND_STR ( "+-----------+-----------+-----------+\n" )
#undef CLi
#undef CLiD
#undef TLi
#undef CPLj
#undef UPLj

}

void printHelp ( ACPResponse * response ) {
    char q[LINE_SIZE];
    SEND_STR ( "COMMAND LIST\n" )
    snprintf ( q, sizeof q, "%s\tput process into active mode; process will read configuration\n", ACP_CMD_APP_START );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "%s\tput process into standby mode; all running programs will be stopped\n", ACP_CMD_APP_STOP );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "%s\tfirst stop and then start process\n", ACP_CMD_APP_RESET );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "%s\tterminate process\n", ACP_CMD_APP_EXIT );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "%s\tget state of process; response: B - process is in active mode, I - process is in standby mode\n", ACP_CMD_APP_PING );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "%s\tget some variable's values; response will be packed into multiple packets\n", ACP_CMD_APP_PRINT );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "%s\tget this help; response will be packed into multiple packets\n", ACP_CMD_APP_HELP );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "%s\tget temperature in format: sensorId\\ttemperature\\ttimeSec\\ttimeNsec\\tvalid; program id expected\n", ACP_CMD_GET_FTS );
    SEND_STR_L ( q )
}
