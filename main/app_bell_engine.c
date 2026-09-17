#include "app_bell_engine.h"

#include <stdio.h>
#include <stdarg.h>

/*
 * Pure engine implementation: no ESP headers, no logging, no globals.
 * Weekday indexes: 0=mon .. 6=sun.
 */

#define BELL_BLOB_SCHEMA 1
#define BELL_PROGRAM_BLOB_MAGIC 0x314C4542UL /* "BEL1" */
#define BELL_CALENDAR_BLOB_MAGIC 0x324C4542UL /* "BEL2" */
#define BELL_BLOB_HEADER_LEN 8

/* ------------------------------------------------------------------ */
/* little-endian scalar helpers                                        */
/* ------------------------------------------------------------------ */

static void put_u32(unsigned char *buf, size_t off, unsigned long v)
{
  buf[off] = (unsigned char)(v & 0xFF);
  buf[off + 1] = (unsigned char)((v >> 8) & 0xFF);
  buf[off + 2] = (unsigned char)((v >> 16) & 0xFF);
  buf[off + 3] = (unsigned char)((v >> 24) & 0xFF);
}

static unsigned long get_u32(const unsigned char *buf, size_t off)
{
  return (unsigned long)buf[off] |
         ((unsigned long)buf[off + 1] << 8) |
         ((unsigned long)buf[off + 2] << 16) |
         ((unsigned long)buf[off + 3] << 24);
}

static void put_u16(unsigned char *buf, size_t off, unsigned short v)
{
  buf[off] = (unsigned char)(v & 0xFF);
  buf[off + 1] = (unsigned char)((v >> 8) & 0xFF);
}

static unsigned short get_u16(const unsigned char *buf, size_t off)
{
  return (unsigned short)((unsigned short)buf[off] |
                          ((unsigned short)buf[off + 1] << 8));
}

unsigned short bell_crc16(const unsigned char *buf, size_t len)
{
  unsigned short crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= (unsigned short)((unsigned short)buf[i] << 8);
    for (int b = 0; b < 8; b++) {
      if (crc & 0x8000)
        crc = (unsigned short)((crc << 1) ^ 0x1021);
      else
        crc = (unsigned short)(crc << 1);
    }
  }
  return crc;
}

/* ------------------------------------------------------------------ */
/* civil calendar math (days from/to epoch, no libc time dependence)   */
/* ------------------------------------------------------------------ */

/* Howard Hinnant's algorithms; valid for the whole practical range. */
static long days_from_civil(long y, unsigned m, unsigned d)
{
  y -= (m <= 2) ? 1 : 0;
  long era = (y >= 0 ? y : y - 399) / 400;
  long yoe = y - era * 400;                                  /* [0, 399] */
  long doy = (153L * (long)(m + (m > 2 ? -3 : 9)) + 2) / 5 + (long)d - 1;
  long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;          /* [0, 146096] */
  return era * 146097 + doe - 719468;
}

static void civil_from_days(long z, long *y, unsigned *m, unsigned *d)
{
  z += 719468;
  long era = (z >= 0 ? z : z - 146096) / 146097;
  long doe = z - era * 146097;                               /* [0, 146096] */
  long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  long yy = yoe + era * 400;
  long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);        /* [0, 365] */
  long mp = (5 * doy + 2) / 153;                             /* [0, 11] */
  long dd = doy - (153 * mp + 2) / 5 + 1;                    /* [1, 31] */
  long mm = mp + (mp < 10 ? 3 : -9);                         /* [1, 12] */
  *y = yy + ((mm <= 2) ? 1 : 0);
  *m = (unsigned)mm;
  *d = (unsigned)dd;
}

static bool valid_date(unsigned year, unsigned month, unsigned day)
{
  if (month < 1 || month > 12 || day < 1 || day > 31)
    return false;
  long y;
  unsigned m, d;
  civil_from_days(days_from_civil((long)year, month, day), &y, &m, &d);
  return y == (long)year && m == month && d == day;
}

static long date_to_days(const struct BellDate *dt)
{
  return days_from_civil((long)dt->year, dt->month, dt->day);
}

static bool date_in_range(const struct BellException *e, long day)
{
  return date_to_days(&e->from) <= day && day <= date_to_days(&e->to);
}

/* tm_wday: 0=sun..6=sat -> engine index 0=mon..6=sun */
static int weekday_index(int tm_wday)
{
  int w = tm_wday % 7;
  if (w < 0)
    w += 7;
  return (w + 6) % 7;
}

/* ------------------------------------------------------------------ */
/* validation                                                          */
/* ------------------------------------------------------------------ */

static int validate_pattern_fields(int count, int dur_s, int gap_s)
{
  if (count < 1 || count > 10)
    return BELL_ERR_CAPS;
  if (dur_s < 1 || dur_s > 30)
    return BELL_ERR_CAPS;
  if (gap_s < 0 || gap_s > 60)
    return BELL_ERR_CAPS;
  return BELL_OK;
}

int bell_pattern_total_s(const struct BellPattern *pat)
{
  return (int)pat->count * (int)pat->dur_s +
         ((int)pat->count - 1) * (int)pat->gap_s;
}

int bell_validate_program(const struct BellProgram *p)
{
  if (p->pattern_count > BELL_MAX_PATTERNS)
    return BELL_ERR_CAPS;

  for (int i = 0; i < p->pattern_count; i++) {
    const struct BellPattern *pat = &p->patterns[i];
    int err = validate_pattern_fields(pat->count, pat->dur_s, pat->gap_s);
    if (err != BELL_OK)
      return err;
    if (bell_pattern_total_s(pat) > BELL_MAX_RING_TOTAL_S)
      return BELL_ERR_CAPS;
  }

  for (int d = 0; d < BELL_DAYS; d++) {
    if (p->event_count[d] > BELL_MAX_EVENTS_PER_DAY)
      return BELL_ERR_CAPS;
    for (int i = 0; i < p->event_count[d]; i++) {
      const struct BellEvent *ev = &p->weekly[d][i];
      if (ev->min_of_day > 1439)
        return BELL_ERR_TIME;
      if (ev->pattern >= p->pattern_count)
        return BELL_ERR_PATTERN_REF;
      if (!p->patterns[ev->pattern].enabled)
        return BELL_ERR_PATTERN_REF;
      for (int j = 0; j < i; j++) {
        if (p->weekly[d][j].min_of_day == ev->min_of_day)
          return BELL_ERR_DUP_TIME;
      }
    }
  }
  return BELL_OK;
}

int bell_validate_calendar(const struct BellCalendar *c)
{
  if (c->exception_count > BELL_MAX_EXCEPTIONS)
    return BELL_ERR_CAPS;
  for (int i = 0; i < c->exception_count; i++) {
    const struct BellException *e = &c->exceptions[i];
    if (!valid_date(e->from.year, e->from.month, e->from.day))
      return BELL_ERR_DATE;
    if (!valid_date(e->to.year, e->to.month, e->to.day))
      return BELL_ERR_DATE;
    if (date_to_days(&e->from) > date_to_days(&e->to))
      return BELL_ERR_DATE_ORDER;
    if (e->type != 0)
      return BELL_ERR_TYPE;
  }
  return BELL_OK;
}

static int validate_name(const char *s, size_t cap)
{
  size_t len = strnlen(s, cap + 1);
  if (len > cap)
    return BELL_ERR_CAPS;
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)s[i];
    if (c < 0x20)
      return BELL_ERR_CAPS;
  }
  return BELL_OK;
}

int bell_validate_alias(const struct BellAlias *a)
{
  int err = validate_name(a->bell, BELL_MAX_ALIAS_NAME);
  if (err != BELL_OK)
    return err;
  if (a->pattern_count > BELL_MAX_PATTERNS)
    return BELL_ERR_CAPS;
  for (int i = 0; i < a->pattern_count; i++) {
    err = validate_name(a->patterns[i], BELL_MAX_ALIAS_NAME);
    if (err != BELL_OK)
      return err;
  }
  return BELL_OK;
}

/* ------------------------------------------------------------------ */
/* builders                                                            */
/* ------------------------------------------------------------------ */

void bell_program_builder_init(struct BellProgramBuilder *b, int v, int enabled)
{
  memset(&b->prog, 0, sizeof(b->prog));
  b->prog.v = (unsigned char)v;
  b->prog.enabled = enabled ? 1 : 0;
}

int bell_builder_add_pattern(struct BellProgramBuilder *b, int count, int dur_s,
                             int gap_s, int enabled)
{
  if (b->prog.pattern_count >= BELL_MAX_PATTERNS)
    return BELL_ERR_CAPS;
  int err = validate_pattern_fields(count, dur_s, gap_s);
  if (err != BELL_OK)
    return err;
  struct BellPattern ref = { (unsigned char)count, (unsigned char)dur_s,
                             (unsigned char)gap_s, (unsigned char)(enabled ? 1 : 0) };
  if (bell_pattern_total_s(&ref) > BELL_MAX_RING_TOTAL_S)
    return BELL_ERR_CAPS;
  b->prog.patterns[b->prog.pattern_count++] = ref;
  return BELL_OK;
}

int bell_builder_add_event(struct BellProgramBuilder *b, int weekday,
                           int min_of_day, int pattern)
{
  if (weekday < 0 || weekday >= BELL_DAYS)
    return BELL_ERR_CAPS;
  if (min_of_day < 0 || min_of_day > 1439)
    return BELL_ERR_TIME;
  if (pattern < 0 || pattern >= b->prog.pattern_count)
    return BELL_ERR_PATTERN_REF;
  if (!b->prog.patterns[pattern].enabled)
    return BELL_ERR_PATTERN_REF;
  if (b->prog.event_count[weekday] >= BELL_MAX_EVENTS_PER_DAY)
    return BELL_ERR_CAPS;
  for (int i = 0; i < b->prog.event_count[weekday]; i++) {
    if (b->prog.weekly[weekday][i].min_of_day == (unsigned short)min_of_day)
      return BELL_ERR_DUP_TIME;
  }
  struct BellEvent ev = { (unsigned short)min_of_day, (unsigned char)pattern };
  b->prog.weekly[weekday][b->prog.event_count[weekday]++] = ev;
  return BELL_OK;
}

int bell_program_builder_finish(struct BellProgramBuilder *b)
{
  return bell_validate_program(&b->prog);
}

void bell_calendar_builder_init(struct BellCalendarBuilder *b, int v)
{
  memset(&b->cal, 0, sizeof(b->cal));
  b->cal.v = (unsigned char)v;
}

int bell_builder_add_exception(struct BellCalendarBuilder *b,
                               int from_year, int from_month, int from_day,
                               int to_year, int to_month, int to_day, int type)
{
  if (b->cal.exception_count >= BELL_MAX_EXCEPTIONS)
    return BELL_ERR_CAPS;
  if (type != 0)
    return BELL_ERR_TYPE;
  if (!valid_date((unsigned)from_year, (unsigned)from_month, (unsigned)from_day))
    return BELL_ERR_DATE;
  if (!valid_date((unsigned)to_year, (unsigned)to_month, (unsigned)to_day))
    return BELL_ERR_DATE;
  struct BellException e;
  memset(&e, 0, sizeof(e));
  e.from.year = (unsigned short)from_year;
  e.from.month = (unsigned char)from_month;
  e.from.day = (unsigned char)from_day;
  e.to.year = (unsigned short)to_year;
  e.to.month = (unsigned char)to_month;
  e.to.day = (unsigned char)to_day;
  e.type = (unsigned char)type;
  if (date_to_days(&e.from) > date_to_days(&e.to))
    return BELL_ERR_DATE_ORDER;
  b->cal.exceptions[b->cal.exception_count++] = e;
  return BELL_OK;
}

int bell_calendar_builder_finish(struct BellCalendarBuilder *b)
{
  return bell_validate_calendar(&b->cal);
}

/* ------------------------------------------------------------------ */
/* queries                                                             */
/* ------------------------------------------------------------------ */

bool bell_is_day_off(const struct BellCalendar *c, const struct tm *day)
{
  long d = days_from_civil((long)day->tm_year + 1900,
                           (unsigned)day->tm_mon + 1, (unsigned)day->tm_mday);
  for (int i = 0; i < c->exception_count; i++) {
    if (date_in_range(&c->exceptions[i], d))
      return true;
  }
  return false;
}

int bell_materialize_day(const struct BellProgram *p, const struct BellCalendar *c,
                         const struct tm *day, struct BellDayEvent *out, int out_max)
{
  if (!p->enabled)
    return 0;
  if (bell_is_day_off(c, day))
    return 0;

  int idx = weekday_index(day->tm_wday);
  int n = p->event_count[idx];
  if (n > out_max)
    n = out_max;

  for (int i = 0; i < n; i++) {
    out[i].min_of_day = p->weekly[idx][i].min_of_day;
    out[i].pattern = p->weekly[idx][i].pattern;
  }

  /* insertion sort by min_of_day (n <= 32) */
  for (int i = 1; i < n; i++) {
    struct BellDayEvent key = out[i];
    int j = i - 1;
    while (j >= 0 && out[j].min_of_day > key.min_of_day) {
      out[j + 1] = out[j];
      j--;
    }
    out[j + 1] = key;
  }
  return n;
}

bool bell_next_event(const struct BellProgram *p, const struct BellCalendar *c,
                     const struct tm *now, struct tm *out_day,
                     unsigned short *out_min_of_day, unsigned char *out_pattern)
{
  if (!p->enabled)
    return false;

  long day = days_from_civil((long)now->tm_year + 1900,
                             (unsigned)now->tm_mon + 1, (unsigned)now->tm_mday);
  int wday_idx = weekday_index(now->tm_wday);
  int now_min = now->tm_hour * 60 + now->tm_min;

  for (int off = 0; off <= BELL_NEXT_SCAN_DAYS; off++) {
    long y;
    unsigned m, d;
    civil_from_days(day + off, &y, &m, &d);

    struct tm cand;
    memset(&cand, 0, sizeof(cand));
    cand.tm_year = (int)y - 1900;
    cand.tm_mon = (int)m - 1;
    cand.tm_mday = (int)d;
    if (bell_is_day_off(c, &cand))
      continue;

    int idx = (wday_idx + off) % BELL_DAYS;
    int best = -1;
    for (int i = 0; i < p->event_count[idx]; i++) {
      unsigned short ev_min = p->weekly[idx][i].min_of_day;
      if (off == 0 && (int)ev_min <= now_min)
        continue;
      if (best < 0 || ev_min < p->weekly[idx][best].min_of_day)
        best = i;
    }
    if (best >= 0) {
      unsigned short ev_min = p->weekly[idx][best].min_of_day;
      cand.tm_wday = (now->tm_wday + off) % 7;
      cand.tm_hour = (int)ev_min / 60;
      cand.tm_min = (int)ev_min % 60;
      cand.tm_isdst = -1; /* let mktime resolve dst for the local timestamp */
      if (out_day)
        *out_day = cand;
      if (out_min_of_day)
        *out_min_of_day = ev_min;
      if (out_pattern)
        *out_pattern = p->weekly[idx][best].pattern;
      return true;
    }
  }
  return false;
}

/* ------------------------------------------------------------------ */
/* NVS blob codec                                                      */
/* ------------------------------------------------------------------ */

/* program payload: v,enabled,pat_cnt(3) + ev_cnt(7) + patterns(8*4) + weekly(7*32*3) */
#define PROGRAM_PAYLOAD_LEN (3 + BELL_DAYS + (BELL_MAX_PATTERNS * 4) + (BELL_DAYS * BELL_MAX_EVENTS_PER_DAY * 3))
/* calendar payload: v,exc_cnt(2) + exceptions(32*9) */
#define CALENDAR_PAYLOAD_LEN (2 + (BELL_MAX_EXCEPTIONS * 9))
/* alias payload: bell name + count + pattern names */
#define ALIAS_PAYLOAD_LEN ((BELL_MAX_ALIAS_NAME + 1) + 1 + (BELL_MAX_PATTERNS * (BELL_MAX_ALIAS_NAME + 1)))
#define ALIAS_BLOB_MAGIC 0x334C4542UL /* "BEL3" */

size_t bell_program_blob_size(void)
{
  return BELL_BLOB_HEADER_LEN + PROGRAM_PAYLOAD_LEN;
}

size_t bell_calendar_blob_size(void)
{
  return BELL_BLOB_HEADER_LEN + CALENDAR_PAYLOAD_LEN;
}

size_t bell_alias_blob_size(void)
{
  return BELL_BLOB_HEADER_LEN + ALIAS_PAYLOAD_LEN;
}

static void write_blob_header(unsigned char *buf, unsigned long magic,
                              const unsigned char *payload, size_t payload_len)
{
  put_u32(buf, 0, magic);
  buf[4] = (unsigned char)BELL_BLOB_SCHEMA;
  buf[5] = 0;
  put_u16(buf, 6, bell_crc16(payload, payload_len));
}

static bool blob_header_ok(const unsigned char *buf, size_t len, unsigned long magic)
{
  if (len < BELL_BLOB_HEADER_LEN)
    return false;
  if (get_u32(buf, 0) != magic)
    return false;
  if (buf[4] != (unsigned char)BELL_BLOB_SCHEMA)
    return false;
  return true;
}

size_t bell_encode_program(const struct BellProgram *p, unsigned char *buf, size_t buf_len)
{
  size_t total = BELL_BLOB_HEADER_LEN + PROGRAM_PAYLOAD_LEN;
  if (buf_len < total)
    return 0;

  unsigned char *pl = buf + BELL_BLOB_HEADER_LEN;
  pl[0] = p->v;
  pl[1] = p->enabled;
  pl[2] = p->pattern_count;
  for (int d = 0; d < BELL_DAYS; d++)
    pl[3 + d] = p->event_count[d];

  size_t off = 3 + BELL_DAYS;
  for (int i = 0; i < BELL_MAX_PATTERNS; i++) {
    pl[off] = p->patterns[i].count;
    pl[off + 1] = p->patterns[i].dur_s;
    pl[off + 2] = p->patterns[i].gap_s;
    pl[off + 3] = p->patterns[i].enabled;
    off += 4;
  }
  for (int d = 0; d < BELL_DAYS; d++) {
    for (int i = 0; i < BELL_MAX_EVENTS_PER_DAY; i++) {
      put_u16(pl, off, p->weekly[d][i].min_of_day);
      pl[off + 2] = p->weekly[d][i].pattern;
      off += 3;
    }
  }

  write_blob_header(buf, BELL_PROGRAM_BLOB_MAGIC, pl, PROGRAM_PAYLOAD_LEN);
  return total;
}

size_t bell_encode_calendar(const struct BellCalendar *c, unsigned char *buf, size_t buf_len)
{
  size_t total = BELL_BLOB_HEADER_LEN + CALENDAR_PAYLOAD_LEN;
  if (buf_len < total)
    return 0;

  unsigned char *pl = buf + BELL_BLOB_HEADER_LEN;
  pl[0] = c->v;
  pl[1] = c->exception_count;

  size_t off = 2;
  for (int i = 0; i < BELL_MAX_EXCEPTIONS; i++) {
    const struct BellException *e = &c->exceptions[i];
    put_u16(pl, off, e->from.year);
    pl[off + 2] = e->from.month;
    pl[off + 3] = e->from.day;
    put_u16(pl, off + 4, e->to.year);
    pl[off + 6] = e->to.month;
    pl[off + 7] = e->to.day;
    pl[off + 8] = e->type;
    off += 9;
  }

  write_blob_header(buf, BELL_CALENDAR_BLOB_MAGIC, pl, CALENDAR_PAYLOAD_LEN);
  return total;
}

bool bell_decode_program(const unsigned char *buf, size_t len, struct BellProgram *out)
{
  memset(out, 0, sizeof(*out));
  if (!blob_header_ok(buf, len, BELL_PROGRAM_BLOB_MAGIC))
    return false;
  if (get_u16(buf, 6) != bell_crc16(buf + BELL_BLOB_HEADER_LEN, PROGRAM_PAYLOAD_LEN))
    return false;

  const unsigned char *pl = buf + BELL_BLOB_HEADER_LEN;
  out->v = pl[0];
  out->enabled = pl[1];
  out->pattern_count = pl[2];
  for (int d = 0; d < BELL_DAYS; d++)
    out->event_count[d] = pl[3 + d];

  size_t off = 3 + BELL_DAYS;
  for (int i = 0; i < BELL_MAX_PATTERNS; i++) {
    out->patterns[i].count = pl[off];
    out->patterns[i].dur_s = pl[off + 1];
    out->patterns[i].gap_s = pl[off + 2];
    out->patterns[i].enabled = pl[off + 3];
    off += 4;
  }
  for (int d = 0; d < BELL_DAYS; d++) {
    for (int i = 0; i < BELL_MAX_EVENTS_PER_DAY; i++) {
      out->weekly[d][i].min_of_day = get_u16(pl, off);
      out->weekly[d][i].pattern = pl[off + 2];
      off += 3;
    }
  }

  if (bell_validate_program(out) != BELL_OK) {
    memset(out, 0, sizeof(*out));
    return false;
  }
  return true;
}

bool bell_decode_calendar(const unsigned char *buf, size_t len, struct BellCalendar *out)
{
  memset(out, 0, sizeof(*out));
  if (!blob_header_ok(buf, len, BELL_CALENDAR_BLOB_MAGIC))
    return false;
  if (get_u16(buf, 6) != bell_crc16(buf + BELL_BLOB_HEADER_LEN, CALENDAR_PAYLOAD_LEN))
    return false;

  const unsigned char *pl = buf + BELL_BLOB_HEADER_LEN;
  out->v = pl[0];
  out->exception_count = pl[1];

  size_t off = 2;
  for (int i = 0; i < BELL_MAX_EXCEPTIONS; i++) {
    struct BellException *e = &out->exceptions[i];
    e->from.year = get_u16(pl, off);
    e->from.month = pl[off + 2];
    e->from.day = pl[off + 3];
    e->to.year = get_u16(pl, off + 4);
    e->to.month = pl[off + 6];
    e->to.day = pl[off + 7];
    e->type = pl[off + 8];
    off += 9;
  }

  if (bell_validate_calendar(out) != BELL_OK) {
    memset(out, 0, sizeof(*out));
    return false;
  }
  return true;
}

size_t bell_encode_alias(const struct BellAlias *a, unsigned char *buf, size_t buf_len)
{
  size_t total = BELL_BLOB_HEADER_LEN + ALIAS_PAYLOAD_LEN;
  if (buf_len < total)
    return 0;

  unsigned char *pl = buf + BELL_BLOB_HEADER_LEN;
  memcpy(pl, a->bell, BELL_MAX_ALIAS_NAME + 1);
  pl[BELL_MAX_ALIAS_NAME + 1] = a->pattern_count;
  for (int i = 0; i < BELL_MAX_PATTERNS; i++)
    memcpy(pl + (BELL_MAX_ALIAS_NAME + 1) + 1 + i * (BELL_MAX_ALIAS_NAME + 1),
           a->patterns[i], BELL_MAX_ALIAS_NAME + 1);

  write_blob_header(buf, ALIAS_BLOB_MAGIC, pl, ALIAS_PAYLOAD_LEN);
  return total;
}

bool bell_decode_alias(const unsigned char *buf, size_t len, struct BellAlias *out)
{
  memset(out, 0, sizeof(*out));
  if (!blob_header_ok(buf, len, ALIAS_BLOB_MAGIC))
    return false;
  if (get_u16(buf, 6) != bell_crc16(buf + BELL_BLOB_HEADER_LEN, ALIAS_PAYLOAD_LEN))
    return false;

  const unsigned char *pl = buf + BELL_BLOB_HEADER_LEN;
  memcpy(out->bell, pl, BELL_MAX_ALIAS_NAME + 1);
  out->bell[BELL_MAX_ALIAS_NAME] = 0;
  out->pattern_count = pl[BELL_MAX_ALIAS_NAME + 1];
  for (int i = 0; i < BELL_MAX_PATTERNS; i++) {
    memcpy(out->patterns[i],
           pl + (BELL_MAX_ALIAS_NAME + 1) + 1 + i * (BELL_MAX_ALIAS_NAME + 1),
           BELL_MAX_ALIAS_NAME + 1);
    out->patterns[i][BELL_MAX_ALIAS_NAME] = 0;
  }

  if (bell_validate_alias(out) != BELL_OK) {
    memset(out, 0, sizeof(*out));
    return false;
  }
  return true;
}

/* ------------------------------------------------------------------ */
/* canonical JSON serializers                                          */
/* ------------------------------------------------------------------ */

static const char *const day_keys[BELL_DAYS] = {
  "mon", "tue", "wed", "thu", "fri", "sat", "sun"
};

static size_t json_append(char *buf, size_t buf_len, size_t pos, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(buf + pos, buf_len - pos, fmt, ap);
  va_end(ap);
  if (n < 0 || (size_t)n >= buf_len - pos)
    return 0;
  return pos + (size_t)n;
}

size_t bell_program_to_json(const struct BellProgram *p, char *buf, size_t buf_len)
{
  size_t pos = 0;

  pos = json_append(buf, buf_len, pos, "{\"v\":%d,\"enabled\":%s,\"patterns\":[",
                    (int)p->v, p->enabled ? "true" : "false");
  if (!pos)
    return 0;

  for (int i = 0; i < p->pattern_count; i++) {
    const struct BellPattern *pat = &p->patterns[i];
    pos = json_append(buf, buf_len, pos,
                      "%s{\"count\":%d,\"dur_s\":%d,\"gap_s\":%d,\"enabled\":%s}",
                      i ? "," : "", (int)pat->count, (int)pat->dur_s,
                      (int)pat->gap_s, pat->enabled ? "true" : "false");
    if (!pos)
      return 0;
  }

  pos = json_append(buf, buf_len, pos, "],\"weekly\":{");
  if (!pos)
    return 0;

  for (int d = 0; d < BELL_DAYS; d++) {
    pos = json_append(buf, buf_len, pos, "%s\"%s\":[", d ? "," : "", day_keys[d]);
    if (!pos)
      return 0;
    for (int i = 0; i < p->event_count[d]; i++) {
      const struct BellEvent *ev = &p->weekly[d][i];
      pos = json_append(buf, buf_len, pos, "%s{\"t\":\"%02d:%02d\",\"p\":%d}",
                        i ? "," : "", (int)ev->min_of_day / 60,
                        (int)ev->min_of_day % 60, (int)ev->pattern);
      if (!pos)
        return 0;
    }
    pos = json_append(buf, buf_len, pos, "]");
    if (!pos)
      return 0;
  }

  pos = json_append(buf, buf_len, pos, "}}");
  if (!pos)
    return 0;
  return pos;
}

size_t bell_calendar_to_json(const struct BellCalendar *c, char *buf, size_t buf_len)
{
  size_t pos = json_append(buf, buf_len, 0, "{\"v\":%d,\"exceptions\":[", (int)c->v);
  if (!pos)
    return 0;

  for (int i = 0; i < c->exception_count; i++) {
    const struct BellException *e = &c->exceptions[i];
    pos = json_append(buf, buf_len, pos,
                      "%s{\"from\":\"%04d-%02d-%02d\",\"to\":\"%04d-%02d-%02d\"}",
                      i ? "," : "", (int)e->from.year, (int)e->from.month,
                      (int)e->from.day, (int)e->to.year, (int)e->to.month, (int)e->to.day);
    if (!pos)
      return 0;
  }

  pos = json_append(buf, buf_len, pos, "]}");
  if (!pos)
    return 0;
  return pos;
}

static size_t json_append_escaped(char *buf, size_t buf_len, size_t pos, const char *s)
{
  size_t tmp_len = (BELL_MAX_ALIAS_NAME * 6) + 1;
  char tmp[BELL_MAX_ALIAS_NAME * 6 + 1];
  size_t t = 0;

  for (const char *p = s; *p; p++) {
    if (*p == '"' || *p == '\\') {
      if (t + 2 >= tmp_len)
        return 0;
      tmp[t++] = '\\';
      tmp[t++] = *p;
    } else {
      if (t + 1 >= tmp_len)
        return 0;
      tmp[t++] = *p;
    }
  }
  tmp[t] = 0;

  return json_append(buf, buf_len, pos, "%s", tmp);
}

size_t bell_alias_to_json(const struct BellAlias *a, char *buf, size_t buf_len)
{
  int count = (int)a->pattern_count;
  while (count > 0 && a->patterns[count - 1][0] == 0)
    count--;

  size_t pos = json_append(buf, buf_len, 0, "{\"bell\":\"");
  if (!pos)
    return 0;
  pos = json_append_escaped(buf, buf_len, pos, a->bell);
  if (!pos)
    return 0;
  pos = json_append(buf, buf_len, pos, "\",\"patterns\":[");
  if (!pos)
    return 0;

  for (int i = 0; i < count; i++) {
    pos = json_append(buf, buf_len, pos, "%s", i ? ",\"" : "\"");
    if (!pos)
      return 0;
    pos = json_append_escaped(buf, buf_len, pos, a->patterns[i]);
    if (!pos)
      return 0;
    pos = json_append(buf, buf_len, pos, "\"");
    if (!pos)
      return 0;
  }

  pos = json_append(buf, buf_len, pos, "]}");
  if (!pos)
    return 0;
  return pos;
}
