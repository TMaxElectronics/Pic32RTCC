#ifndef RTCC_INC
#define RTCC_INC

#include <xc.h>
#include <stdint.h>

void RTCC_init(char * defaultTimeString, char * defaultDateString);
uint32_t RTCC_getHours();
uint32_t RTCC_getMinutes();
uint32_t RTCC_getSeconds();
uint32_t RTCC_getDay();
uint32_t RTCC_getMonth();
void RTCC_getDateAndTime(struct tm * info);
uint32_t RTCC_getEpoch();
char * RTCC_getMonthString();
uint32_t RTCC_getYear();
uint32_t RTCC_getWeekday();
char * RTCC_getWeekdayString();
typedef enum {RTCC_FORMAT_DMY, RTCC_FORMAT_DMYY, RTCC_FORMAT_MONTHDYY, RTCC_FORMAT_EPOCH} RTC_DATE_FORMAT_t; 
typedef enum {RTCC_FORMAT_HM, RTCC_FORMAT_HMS, RTCC_FORMAT_12H_HM, RTCC_FORMAT_12H_HMS, RTCC_FORMAT_UNIX} RTC_TIME_FORMAT_t; 
uint32_t RTCC_getTimeString(char * buffer, RTC_TIME_FORMAT_t format);
uint32_t RTCC_getDateString(char * buffer, RTC_TIME_FORMAT_t format);
void RTCC_setTime(uint32_t hours, uint32_t minutes, uint32_t seconds, unsigned calibrate);
void RTCC_setDate(uint8_t day, uint8_t month, uint8_t year);
void RTCC_setTimeString(char* newTime, unsigned calibrate);
void RTCC_setDateStringNum(char* newDate);
void RTCC_setDateStringChar(char* newDate);
void RTCC_setDateTimeString(char* str, unsigned calibrate);

#endif