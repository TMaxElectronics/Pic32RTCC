#ifndef RTCC_INC
#define RTCC_INC

#include <xc.h>
#include <stdint.h>
#include <time.h>

#define RTCC_ALARM_REPEAT_INDEFINETELY 0xffffffff

typedef enum {RTCC_ARC_EVERY_HALF_SECOND = 0b0000, RTCC_ARC_EVERY_SECOND = 0b0001, RTCC_ARC_EVERY_10_SECONDS = 0b0010, RTCC_ARC_EVERY_MINUTE = 0b0011, RTCC_ARC_EVERY_10_MINUTES = 0b0100, RTCC_ARC_EVERY_HOUR = 0b0101, RTCC_ARC_ONCE_A_DAY = 0b0110, RTCC_ARC_ONCE_A_WEEK = 0b0111, RTCC_ARC_ONCE_A_MONTH = 0b1000, RTCC_ARC_ONCE_A_YEAR = 0b1001} RTCC_ALARM_REPEAT_CON_t;

void RTCC_init(char * defaultTimeString, char * defaultDateString);
uint32_t RTCC_getHours();
uint32_t RTCC_getMinutes();
uint32_t RTCC_getSeconds();
uint32_t RTCC_getDay();
uint32_t RTCC_getMonth();
uint32_t RTCC_getEpoch();
char * RTCC_getMonthString();
uint32_t RTCC_getYear();
uint32_t RTCC_getWeekday();
char * RTCC_getWeekdayString();
typedef enum {RTCC_FORMAT_DMY, RTCC_FORMAT_DMYY, RTCC_FORMAT_MONTHDYY, RTCC_FORMAT_EPOCH} RTC_DATE_FORMAT_t; 
typedef enum {RTCC_FORMAT_HM, RTCC_FORMAT_HMS, RTCC_FORMAT_12H_HM, RTCC_FORMAT_12H_HMS, RTCC_FORMAT_UNIX} RTC_TIME_FORMAT_t; 
uint32_t RTCC_getTimeString(char * buffer, RTC_TIME_FORMAT_t format);
uint32_t RTCC_getDateString(char * buffer, RTC_TIME_FORMAT_t format);
void RTCC_setTime(uint32_t hours, uint32_t minutes, uint32_t seconds);
void RTCC_setDate(uint8_t day, uint8_t month, uint8_t year);

void RTCC_strpdateStr(char * newDate, struct tm * info);
void RTCC_strpdateNum(char * newDate, struct tm * info);
void RTCC_strptime(char * newTime, struct tm * info);

void RTCC_setTm(struct tm * info, uint32_t updateTime, uint32_t updateDate, unsigned calibrate);
void RTCC_getTm(struct tm * info);

uint32_t RTCC_getLastCalDate();


void RTCC_setAlarmDate(uint8_t weekDay, uint8_t day, uint8_t month);
void RTCC_setAlarmTime(uint32_t hours, uint32_t minutes, uint32_t seconds);
void RTCC_setAlarmConfig(uint32_t alarmEnabled, uint32_t repeatCount, RTCC_ALARM_REPEAT_CON_t repeatConfig);

/*void RTCC_setTimeString(char* newTime, unsigned calibrate);
void RTCC_setDateStringNum(char* newDate);
void RTCC_setDateStringChar(char* newDate);
void RTCC_setDateTimeString(char* str, unsigned calibrate);*/

#endif