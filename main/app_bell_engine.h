#ifndef APP_BELL_ENGINE_H
#define APP_BELL_ENGINE_H

#include <time.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>

/*
 * Pure school-bell engine: calendar logic, validation, next-event scan and
 * the NVS blob codec. Plain-C headers ONLY so it compiles in test-host
 * without any ESP/FreeRTOS stubs. No logging, no allocation, no globals.
 *
 * Weekday indexes follow the canonical JSON weekly key order:
 *   0=mon 1=tue 2=wed 3=thu 4=fri 5=sat 6=sun
 */

#define BELL_DAYS 7
#define BELL_MAX_EVENTS_PER_DAY 32
#define BELL_MAX_PATTERNS 8
#define BELL_MAX_EXCEPTIONS 32
#define BELL_MAX_RING_TOTAL_S 30
#define BELL_NEXT_SCAN_DAYS 8
#define BELL_MAX_PAYLOAD 1024
#define BELL_MAX_ALIAS_NAME 24

struct BellPattern {
  unsigned char count;   /* rings per trigger, 1..10 */
  unsigned char dur_s;   /* relay-on seconds, 1..30 */
  unsigned char gap_s;   /* pause seconds, 0..60 */
  unsigned char enabled;
};

struct BellEvent {
  unsigned short min_of_day; /* 0..1439 */
  unsigned char pattern;     /* index into BellProgram patterns */
};

struct BellDate {
  unsigned short year;
  unsigned char month; /* 1..12 */
  unsigned char day;   /* 1..31 */
};

struct BellException {
  struct BellDate from;
  struct BellDate to;
  unsigned char type; /* 0 = off (reserved, anything else rejected) */
};

struct BellProgram {
  unsigned char v;
  unsigned char enabled;
  unsigned char pattern_count;
  struct BellPattern patterns[BELL_MAX_PATTERNS];
  unsigned char event_count[BELL_DAYS];
  struct BellEvent weekly[BELL_DAYS][BELL_MAX_EVENTS_PER_DAY];
};

struct BellCalendar {
  unsigned char v;
  unsigned char exception_count;
  struct BellException exceptions[BELL_MAX_EXCEPTIONS];
};

struct BellDayEvent {
  unsigned short min_of_day;
  unsigned char pattern;
};

/*
 * Presentation names owned by the device: the bell itself and one name
 * per pattern ("tip de sunet"). Empty strings mean unnamed.
 */
struct BellAlias {
  char bell[BELL_MAX_ALIAS_NAME + 1];
  unsigned char pattern_count; /* ≤ BELL_MAX_PATTERNS */
  char patterns[BELL_MAX_PATTERNS][BELL_MAX_ALIAS_NAME + 1];
};

enum BellError {
  BELL_OK = 0,
  BELL_ERR_CAPS,        /* count/size/range-of-amounts limits */
  BELL_ERR_TIME,        /* min_of_day out of 0..1439 */
  BELL_ERR_DUP_TIME,    /* duplicate time within a weekday */
  BELL_ERR_PATTERN_REF, /* referenced pattern missing or disabled */
  BELL_ERR_DATE,        /* unreal calendar date */
  BELL_ERR_DATE_ORDER,  /* exception from > to */
  BELL_ERR_TYPE,        /* exception type other than off */
  BELL_ERR_CORRUPT,     /* blob magic/schema/crc mismatch */
  BELL_ERR_JSON,        /* malformed or unexpected json shape */
};

/*
 * Builders: the JSON parse layer (app_bell.c) feeds cJSON values in and
 * ALL policy decisions happen here. Each add_* returns BELL_OK or an error.
 */
struct BellProgramBuilder {
  struct BellProgram prog;
};

struct BellCalendarBuilder {
  struct BellCalendar cal;
};

void bell_program_builder_init(struct BellProgramBuilder *b, int v, int enabled);
int bell_builder_add_pattern(struct BellProgramBuilder *b, int count, int dur_s,
                             int gap_s, int enabled);
int bell_builder_add_event(struct BellProgramBuilder *b, int weekday,
                           int min_of_day, int pattern);
int bell_program_builder_finish(struct BellProgramBuilder *b);

void bell_calendar_builder_init(struct BellCalendarBuilder *b, int v);
int bell_builder_add_exception(struct BellCalendarBuilder *b,
                               int from_year, int from_month, int from_day,
                               int to_year, int to_month, int to_day, int type);
int bell_calendar_builder_finish(struct BellCalendarBuilder *b);

/* Full-struct validation; also used defensively on NVS-loaded data. */
int bell_validate_program(const struct BellProgram *p);
int bell_validate_calendar(const struct BellCalendar *c);
int bell_validate_alias(const struct BellAlias *a);

/* Calendar queries. day/now are local time (TZ applied by caller). */
bool bell_is_day_off(const struct BellCalendar *c, const struct tm *day);
int bell_materialize_day(const struct BellProgram *p, const struct BellCalendar *c,
                         const struct tm *day, struct BellDayEvent *out, int out_max);

/*
 * Next scheduled ring strictly after 'now', scanning today plus up to
 * BELL_NEXT_SCAN_DAYS days ahead, skipping off days. Returns false when
 * nothing is scheduled in the horizon. out_day is fully populated
 * (y/m/d/wday/hour/min, sec=0).
 */
bool bell_next_event(const struct BellProgram *p, const struct BellCalendar *c,
                     const struct tm *now, struct tm *out_day,
                     unsigned short *out_min_of_day, unsigned char *out_pattern);

/* count*dur + (count-1)*gap */
int bell_pattern_total_s(const struct BellPattern *pat);

/*
 * NVS blob codec. Layout: magic u32le | schema u8 | pad u8 | crc16 u16le
 * over the payload that follows. Decode failure (magic/schema/crc/invalid)
 * fills defaults: empty calendar (ring per weekly), empty+disabled program.
 */
size_t bell_program_blob_size(void);
size_t bell_calendar_blob_size(void);
size_t bell_alias_blob_size(void);
size_t bell_encode_program(const struct BellProgram *p, unsigned char *buf, size_t buf_len);
size_t bell_encode_calendar(const struct BellCalendar *c, unsigned char *buf, size_t buf_len);
size_t bell_encode_alias(const struct BellAlias *a, unsigned char *buf, size_t buf_len);
bool bell_decode_program(const unsigned char *buf, size_t len, struct BellProgram *out);
bool bell_decode_calendar(const unsigned char *buf, size_t len, struct BellCalendar *out);
bool bell_decode_alias(const unsigned char *buf, size_t len, struct BellAlias *out);

unsigned short bell_crc16(const unsigned char *buf, size_t len);

/*
 * Canonical JSON serializers (same shape as the accepted cmd payloads).
 * Return the written length, or 0 when the buffer is too small.
 */
size_t bell_program_to_json(const struct BellProgram *p, char *buf, size_t buf_len);
size_t bell_calendar_to_json(const struct BellCalendar *c, char *buf, size_t buf_len);
size_t bell_alias_to_json(const struct BellAlias *a, char *buf, size_t buf_len);

#endif /* APP_BELL_ENGINE_H */
