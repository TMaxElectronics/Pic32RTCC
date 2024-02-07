#include <xc.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
 
#include "portmacro.h"
#include "RTCC.h"
#include "ConMan.h"
#include "FreeRTOSConfig.h"
#include "System.h"
#include "util.h"
#include "startup.h" 


#if __has_include("TTerm.h")
    #include "TTerm.h"
#endif

#define RTCC_VERSION 1

#define RTCC_PARAM_CALIBRATION      0
#define RTCC_PARAM_CALIBRATION_DEF  0
#define RTCC_PARAM_LASTCALDATE      1
#define RTCC_PARAM_LASTCALDATE_DEF  0

static int32_t RTCC_calibration = RTCC_PARAM_CALIBRATION_DEF;
static uint32_t RTCC_lastCalDate = RTCC_PARAM_LASTCALDATE_DEF;

const static char * RTCC_weekdayNames[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const static char * RTCC_monthNames[12] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};

static uint32_t RTCC_calNotRecommended = 0;
static void RTCC_unlockRegisters();
static void RTCC_lockRegisters();
static ConMan_Result_t RTCC_configCallback(ConMan_Result_t evt, ConMan_CallbackData_t * data);

#if __has_include("TTerm.h")
static uint8_t RTCC_cmd(TERMINAL_HANDLE * handle, uint8_t argCount, char ** args){
    uint8_t currArg = 0;
    uint8_t returnCode = TERM_CMD_EXIT_SUCCESS;
    uint32_t cal = 0;
    uint32_t setTime = 0;
    uint32_t setDate = 0;
    uint32_t setEpoch = 0;
    
    for(;currArg<argCount; currArg++){
        if(strcmp(args[currArg], "-?") == 0){
            ttprintf("Contains avrious utilities for getting and setting system time\r\n");
            ttprintf("usage:\r\n");
            ttprintf("\ttime [options {arguments}]  \t prints out current system time and date\r\n");
            ttprintf("\t\t-cal                      \t Enables drift calibration for setting the time\r\n");
            ttprintf("\t\t-st {time string}         \t Sets the time. Required format=\"hh:mm:ss\"\r\n");
            ttprintf("\t\t-sd {date string}         \t Sets the date. Required format=\"hh:mm:ss\"\r\n");
            ttprintf("\t\t-se {epoch time}          \t Sets time and date from epoch timestamp\r\n");
            ttprintf("\t\t-e                        \t prints epoch time\r\n");
            return TERM_CMD_EXIT_SUCCESS;
        }else if(strcmp(args[currArg], "-p") == 0){
            ttprintf("System compile date & time: t=\"%s\" d=\"%s\"\r\n", __TIME__, __DATE__);
            
            struct tm info;
    
            RTCC_strptime(__TIME__, &info);
            RTCC_strpdateStr(__DATE__, &info);
            
            ttprintf("=%d\r\n", mktime(&info));
            
        }else if(strcmp(args[currArg], "-cal") == 0){
            cal = 1;
        }else if(strcmp(args[currArg], "-st") == 0){
            if(currArg + 1 >= argCount){
                ttprintf("Not enough arguments for command \"-st\"! use -? for help\r\n");
                return TERM_CMD_EXIT_ERROR;
            }
            setTime = ++currArg;
        }else if(strcmp(args[currArg], "-sd") == 0){
            if(currArg + 1 >= argCount){
                ttprintf("Not enough arguments for command \"-sd\"! use -? for help\r\n");
                return TERM_CMD_EXIT_ERROR;
            }
            setDate = ++currArg;
        }else if(strcmp(args[currArg], "-se") == 0){
            if(currArg + 1 >= argCount){
                ttprintf("Not enough arguments for command \"-se\"! use -? for help\r\n");
                return TERM_CMD_EXIT_ERROR;
            }
            setEpoch = ++currArg;
        }else if(strcmp(args[currArg], "-e") == 0){
            ttprintf("Current epoch time: %d\r\n", RTCC_getEpoch());
        }
    }
    
    struct tm newTime;
    uint32_t updateRequired = 0;
    
    if(setTime != 0){
        RTCC_strptime(args[setTime], &newTime);
        updateRequired = 1;
    }
    
    if(setDate != 0){
        RTCC_strpdateNum(args[setDate], &newTime);
        updateRequired = 1;
    }
    
    if(setEpoch != 0){
        uint32_t epoch = atoi(args[setEpoch]);
        if(epoch != 0){
            if((setDate != 0) || (setTime != 0)){
                ttprintf("both numerical and epoch time update requested, only writing epoch\r\n");
            }
            memcpy(&newTime, localtime(&epoch), sizeof (struct tm));
            updateRequired = 1;
        }else{
            ttprintf("invalid epoch value string entered\r\n");
        }
    }
    
    if(updateRequired){
        RTCC_setTm(&newTime, (setTime != 0) || (setEpoch != 0), (setDate != 0) || (setEpoch != 0), cal);
        ttprintf("updated system time\r\n");
    }   
    
    if(cal && !setTime && !setDate && !setEpoch){
        ttprintf("Tuning value = %i (actually %i)\r\n", RTCC_calibration, RTCCONbits.CAL);
        ttprintf("Time was last set at epoch %d (%d seconds ago)\r\n", RTCC_lastCalDate, RTCC_getEpoch() - RTCC_lastCalDate);
    }
    
    char buff[64];
    RTCC_getTimeString(buff, RTCC_FORMAT_HMS);
    ttprintf("Current time: %s\r\n", buff);
    RTCC_getDateString(buff, RTCC_FORMAT_MONTHDYY);
    ttprintf("Current date: %s\r\n", buff);
    
    return TERM_CMD_EXIT_SUCCESS;
}
#endif

void RTCC_init(char * defaultTimeString, char * defaultDateString){ 
    ConMan_addParameter("RTCC_CAL", sizeof(int32_t), RTCC_configCallback, (void*) RTCC_PARAM_CALIBRATION, RTCC_VERSION);
    ConMan_addParameter("RTCC_CALDATE", sizeof(uint32_t), RTCC_configCallback, (void*) RTCC_PARAM_LASTCALDATE, RTCC_VERSION);
    
    RTCC_unlockRegisters();
    
    RTCCON = 0b1000001001001000;
    RTCCONbits.CAL = RTCC_calibration;
    
    RTCC_lockRegisters();
    
    //now check if the last reset even was either a power on reset or a brown out reset. That would make the registers potentially wrong
    if(SYS_resetCause & (_RCON_BOR_MASK || _RCON_POR_MASK)){
        //yes, reset time to a default value
        struct tm newTime;
        if(RTCC_lastCalDate != 0){
            //lastCalDate has a valid date in it, just use that
            memcpy(&newTime, localtime(&RTCC_lastCalDate), sizeof (struct tm));
        }else{
            //not even lastCalDate is valid... use the defaultStringsstruct tm info;
            RTCC_strptime(defaultTimeString, &newTime);
            RTCC_strpdateStr(defaultDateString, &newTime);
        }
        RTCC_setTm(&newTime, 1, 1, 0);
        
        //make sure at least the next time update won't try a calibration, even if it is requested
        RTCC_calNotRecommended = 0;
    }
    
#if __has_include("TTerm.h")
    TERM_addCommand(RTCC_cmd, "time", "system time functions", configMINIMAL_STACK_SIZE + 500, &TERM_defaultList);
#endif
}


static ConMan_Result_t RTCC_configCallback(ConMan_Result_t evt, ConMan_CallbackData_t * data){
    ConMan_CallbackData_t * cbd = (ConMan_CallbackData_t *) data;
            
    //check which event was sent to us
    if(evt == CONFIG_ENTRY_CREATED){
        //a configuration was just created, populate default values
        if((uint32_t) cbd->userData == RTCC_PARAM_CALIBRATION){
            ConMan_writeData(cbd->callbackData, 0, (uint8_t*) &RTCC_calibration, sizeof(int32_t));
        }else if((uint32_t) cbd->userData == RTCC_PARAM_LASTCALDATE){
            ConMan_writeData(cbd->callbackData, 0, (uint8_t*) &RTCC_lastCalDate, sizeof(uint32_t));
        }
    }else if(evt == CONFIG_ENTRY_LOADED || evt == CONFIG_ENTRY_UPDATED){
        //a configuration was just updated, reload data
        if((uint32_t) cbd->userData == RTCC_PARAM_CALIBRATION){         
            ConMan_readData(cbd->callbackData, 0, (uint8_t*) (uint8_t*) &RTCC_calibration, sizeof(uint32_t));
            
            //TODO write cal value to register
        }else if((uint32_t) cbd->userData == RTCC_PARAM_LASTCALDATE){   
            ConMan_readData(cbd->callbackData, 0, (uint8_t*) (uint8_t*) &RTCC_lastCalDate, sizeof(uint32_t));
        }
    }else if(evt == CONFIG_VERIFY_VALUE){
        //unimplemented for now
        return CONFIG_ERROR;
    }
    
    return CONFIG_OK;
}

static void RTCC_unlockRegisters(){
    //make sure we aren't interrupted while writing SYSKEY
    portDISABLE_INTERRUPTS();

    //unlock osccon register
    SYSKEY = 0;
    SYSKEY = 0xAA996655;
    SYSKEY = 0x556699AA;
    
    RTCCONSET = _RTCCON_RTCWREN_MASK;
    
    SYSKEY = 0;
    
    //reenable interrupts, this is to reduce the amount of code after the switch to an absolute minimum
    portENABLE_INTERRUPTS();
}

static void RTCC_lockRegisters(){
    RTCCONCLR = _RTCCON_RTCWREN_MASK;
}


uint32_t RTCC_getHours(){
    return (RTCTIMEbits.HR10 * 10) + RTCTIMEbits.HR01;
} 

uint32_t RTCC_getMinutes(){
    return (RTCTIMEbits.MIN10 * 10) + RTCTIMEbits.MIN01;
}

uint32_t RTCC_getSeconds(){
    return (RTCTIMEbits.SEC10 * 10) + RTCTIMEbits.SEC01;
}

uint32_t RTCC_getDay(){
    return (RTCDATEbits.DAY10 * 10) + RTCDATEbits.DAY01;
}

uint32_t RTCC_getMonth(){
    return (RTCDATEbits.MONTH10 * 10) + RTCDATEbits.MONTH01;
}

uint32_t RTCC_getEpoch(){
    struct tm info;
    RTCC_getTm(&info);
    return mktime(&info);
}

char * RTCC_getMonthString(){
    uint32_t month = RTCC_getMonth();
    if(month > 12) return "ERROR";
    return RTCC_monthNames[month-1];
}

uint32_t RTCC_getYear(){
    return 2000 + (RTCDATEbits.YEAR10 * 10) + RTCDATEbits.YEAR01;
}

uint32_t RTCC_getWeekday(){
    return RTCDATEbits.WDAY01;
}

char * RTCC_getWeekdayString(){
    uint32_t day = RTCC_getWeekday();
    if(day > 6) return "ERROR";
    return RTCC_weekdayNames[day];
}

uint32_t RTCC_getTimeString(char * buffer, RTC_TIME_FORMAT_t format){
    switch(format){
        case RTCC_FORMAT_HM:
            return sprintf(buffer, "%02d:%02d", RTCC_getHours(), RTCC_getMinutes());
        case RTCC_FORMAT_HMS:
            return sprintf(buffer, "%02d-%02d-%02d", RTCC_getHours(), RTCC_getMinutes(), RTCC_getSeconds());
        case RTCC_FORMAT_12H_HM:
            return sprintf(buffer, "%02d:%02d %s", RTCC_getHours() % 12, RTCC_getMinutes(), (RTCC_getHours() / 12) ? "PM" : "AM");
        case RTCC_FORMAT_12H_HMS:
            return sprintf(buffer, "%02d:%02d:%02d %s", RTCC_getHours() % 12, RTCC_getMinutes(), RTCC_getSeconds(), (RTCC_getHours() / 12) ? "PM" : "AM");
        default:
            return 0;
    }
}

uint32_t RTCC_getDateString(char * buffer, RTC_TIME_FORMAT_t format){
    switch(format){
        case RTCC_FORMAT_DMY:
#ifdef DATE_FORMAT_EUROPEAN
            return sprintf(buffer, "%02d.%02d.%02d", RTCC_getDay(), RTCC_getMonth(), RTCC_getYear() - 2000);
#else
            return sprintf(buffer, "%02d-%02d-%02d", RTCC_getMonth(), RTCC_getDay(), RTCC_getYear() - 2000);
#endif
        case RTCC_FORMAT_DMYY:
#ifdef DATE_FORMAT_EUROPEAN
            return sprintf(buffer, "%02d.%02d.%04d", RTCC_getDay(), RTCC_getMonth(), RTCC_getYear());
#else
            return sprintf(buffer, "%02d-%02d-%04d", RTCC_getMonth(), RTCC_getDay(), RTCC_getYear());
#endif
        case RTCC_FORMAT_MONTHDYY:
#ifdef DATE_FORMAT_EUROPEAN
            return sprintf(buffer, "%02d. %s %04d, RTCC_getDay(), RTCC_getMonthString(), RTCC_getYear());
#else
            return sprintf(buffer, "%s %02d %04d", RTCC_getMonthString(), RTCC_getDay(), RTCC_getYear());
#endif
        case RTCC_FORMAT_EPOCH:
            return sprintf(buffer, "%d", RTCC_getEpoch());
        default:
            return 0;
    }
}

void RTCC_setTime(uint32_t hours, uint32_t minutes, uint32_t seconds){
    if(hours > 23 || minutes > 59 || seconds > 59) return;
    
    //pre-calculate register value to prevent clocking during writing messing with us
    union {
        struct {
            uint32_t :8;
            uint32_t SEC01:4;
            uint32_t SEC10:4;
            uint32_t MIN01:4;
            uint32_t MIN10:4;
            uint32_t HR01:4;
            uint32_t HR10:4;
        };
        struct {
            uint32_t w:32;
        };
    } time;
    
    time.HR10 = hours / 10;
    time.HR01 = hours % 10;
    
    time.MIN10 = minutes / 10;
    time.MIN01 = minutes % 10;
    
    time.SEC10 = seconds / 10;
    time.SEC01 = seconds % 10;
    
    //write value
    RTCC_unlockRegisters();
    RTCTIME = time.w;
    RTCC_lockRegisters();
}

void RTCC_setDate(uint8_t day, uint8_t month, uint8_t year){//pre-calculate register value to prevent clocking during writing messing with us
    
    if(day > 31 || month > 11 || year > 99) return;
    
    union {
        struct {
            uint32_t WDAY01:4;
            uint32_t :4;
            uint32_t DAY01:4;
            uint32_t DAY10:4;
            uint32_t MONTH01:4;
            uint32_t MONTH10:4;
            uint32_t YEAR01:4;
            uint32_t YEAR10:4;
        };
        struct {
            uint32_t w:32;
        };
    } date;
    
    date.YEAR10 = year / 10;
    date.YEAR01 = year % 10;
    
    date.MONTH10 = month / 10;
    date.MONTH01 = month % 10;
    
    date.DAY10 = day / 10;
    date.DAY01 = day % 10;
    
    date.WDAY01 = (day += month < 3 ? year-- : year - 2, 23*month/9 + day + 4 + year/4- year/100 + year/400)%7;
    
    RTCC_unlockRegisters();
    RTCDATE = date.w;
    RTCC_lockRegisters();
} 

void RTCC_getTm(struct tm * info){
    info->tm_year = RTCC_getYear() - 1900;
    info->tm_mon = RTCC_getMonth()-1;
    info->tm_mday = RTCC_getDay();
    info->tm_hour = RTCC_getHours();
    info->tm_min = RTCC_getMinutes();
    info->tm_sec = RTCC_getSeconds();
    info->tm_isdst = 0;
}

void RTCC_setTm(struct tm * info, uint32_t updateTime, uint32_t updateDate, unsigned calibrate){
    //does the user want to calibrate and a lastCalDate set?
    if(!RTCC_calNotRecommended && calibrate && RTCC_lastCalDate != 0){
        //yes, calculate the drift factor (no, not the initial d type)
        
        //how long ago do we think the last cal happened?
        int32_t tInt = RTCC_getEpoch() - RTCC_lastCalDate;
        
        //how long ago do the last cal actually happen?
        int32_t tRef = mktime(info) - RTCC_lastCalDate;
        
        if(tInt != 0 && tRef > 0){
            
            //calculate by how much our internal clock deviated... and yes do it in floting point math because i don't hate myself
            float factor = (float) tRef / (float) tInt;

            //convert back to the calibration constant for the register. Factor = clocksPerMinute / (clocksPerMinute+x) with clocksPerMinute = 32768*60 = 1966080
            int32_t calibrationValue = (int16_t) (((1/factor)-1) * 1966080.0);

            TERM_printDebug(TERM_handle, "RTCC calibration: tInt=%d tRef=%d drift=%dppm newCalFactor=%d ", tInt, tRef, (int32_t) ((factor - 1.0) * 1000000.0), calibrationValue);

            //did enough time pass since calibration? To make sure we don't mis-calibrate we'll only accept it after at least an hour has passed
            if(tRef > 3600){
                ConMan_updateParameter("RTCC_CAL", 0, &calibrationValue, sizeof(int32_t), RTCC_VERSION);
            }
        }
    }
    
    if(updateTime) RTCC_setTime(info->tm_hour, info->tm_min, info->tm_sec);
    if(updateDate) RTCC_setDate(info->tm_mday, info->tm_mon + 1, info->tm_year - 100);
    
    uint32_t caltime = mktime(info);
    //only update if both date and time are written
    if(updateTime && updateDate) ConMan_updateParameter("RTCC_CALDATE", 0, (uint8_t*) &caltime, sizeof(uint32_t), RTCC_VERSION);
    RTCC_calNotRecommended = 0;
}

//must be formatted as hh:mm:ss
void RTCC_strptime(char * newTime, struct tm * info){
    uint8_t hours = ((isAsciiNumber(newTime[0]) ? (newTime[0] - 48) : 0) * 10) + (newTime[1] - 48);
    uint8_t minutes = ((isAsciiNumber(newTime[3]) ? (newTime[3] - 48) : 0) * 10) + (newTime[4] - 48);
    uint8_t seconds = ((isAsciiNumber(newTime[6]) ? (newTime[6] - 48) : 0) * 10) + (newTime[7] - 48);
    
    info->tm_hour = hours;
    info->tm_min = minutes;
    info->tm_sec = seconds;
    info->tm_isdst = 0;
}

//must be formatted as either dd.mm.yy (not yet implemented:) or mm.dd.yy depending on locale
void RTCC_strpdateNum(char * newDate, struct tm * info){
    uint8_t day = ((isAsciiNumber(newDate[0]) ? (newDate[0] - 48) : 0) * 10) + (newDate[1] - 48);
    uint8_t month = ((isAsciiNumber(newDate[3]) ? (newDate[3] - 48) : 0) * 10) + (newDate[4] - 48);
    uint8_t year = ((isAsciiNumber(newDate[6]) ? (newDate[6] - 48) : 0) * 10) + (newDate[7] - 48);
    
    info->tm_mday = day;
    info->tm_mon = month - 1;
    info->tm_year = year + 100;
    info->tm_wday = (day += month < 3 ? year-- : year - 2, 23*month/9 + day + 4 + year/4- year/100 + year/400)%7;
}

//must be formatted as Mon dd yyyy
void RTCC_strpdateStr(char * newDate, struct tm * info){
    uint32_t day = ((isAsciiNumber(newDate[4]) ? (newDate[4] - 48) : 0) * 10) + (newDate[5] - 48);
    uint32_t year = ((isAsciiNumber(newDate[9]) ? (newDate[9] - 48) : 0) * 10) + (newDate[10] - 48);
    
    //convert month
    uint32_t month = 0;
    if(newDate[0] == 'J'){
        if(newDate[1] == 'a'){        //janurary
            month = 1;
        }else if(newDate[3] == 'n'){  //june
            month = 6;
        }else{                        //july
            month = 7;
        }
    }else if(newDate[0] == 'F'){      //feburary
        month = 2;
    }else if(newDate[0] == 'M'){
        if(newDate[2] == 'r'){        //march
            month = 3;
        }else{                        //may
            month = 5;
        }
    }else if(newDate[0] == 'A'){
        if(newDate[1] == 'p'){        //April
            month = 4;
        }else{                        //August
            month = 8;
        }
    }else if(newDate[0] == 'S'){      //September
        month = 9;
    }else if(newDate[0] == 'O'){      //October
        month = 10;
    }else if(newDate[0] == 'N'){      //November
        month = 11;
    }else{                            //December
        month = 12;
    }
    
    info->tm_mday = day;
    info->tm_mon = month - 1;
    info->tm_year = year + 100;
    info->tm_wday = (day += month < 3 ? year-- : year - 2, 23*month/9 + day + 4 + year/4- year/100 + year/400)%7;
} 

uint32_t RTCC_getLastCalDate(){
    return RTCC_lastCalDate; 
}