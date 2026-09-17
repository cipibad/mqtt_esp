extern "C" {
#include "app_bell_engine.h"
}

#include "catch.hpp"

/*
 * 10 host tests for the pure bell engine, per the frozen bell contract.
 * Calendar anchors: 2026-09-13 is a Sunday, 2026-09-14 a Monday,
 * 2026-09-21 the Monday after.
 */

static struct tm make_tm(int year, int month, int day, int wday,
                         int hour = 0, int min = 0)
{
  struct tm t;
  memset(&t, 0, sizeof(t));
  t.tm_year = year - 1900;
  t.tm_mon = month - 1;
  t.tm_mday = day;
  t.tm_wday = wday;
  t.tm_hour = hour;
  t.tm_min = min;
  return t;
}

/* program: pattern 0 = {count 1, dur 3s, gap 2s, enabled}, mon 08:00 */
static void build_simple_program(struct BellProgramBuilder *b)
{
  bell_program_builder_init(b, 3, 1);
  REQUIRE(bell_builder_add_pattern(b, 1, 3, 2, 1) == BELL_OK);
  REQUIRE(bell_builder_add_event(b, 0, 8 * 60, 0) == BELL_OK);
}

/* 1. calendar range -> OFF day, empty materialization, next skips it */
TEST_CASE("bell_range_off_day_empty_and_next_skips", "[bell]")
{
  struct BellProgramBuilder pb;
  build_simple_program(&pb);

  struct BellCalendarBuilder cb;
  bell_calendar_builder_init(&cb, 5);
  REQUIRE(bell_builder_add_exception(&cb, 2026, 9, 14, 2026, 9, 18, 0) == BELL_OK);

  struct tm monday = make_tm(2026, 9, 14, 1);
  REQUIRE(bell_is_day_off(&cb.cal, &monday));

  struct BellDayEvent evs[BELL_MAX_EVENTS_PER_DAY];
  int n = bell_materialize_day(&pb.prog, &cb.cal, &monday, evs, BELL_MAX_EVENTS_PER_DAY);
  REQUIRE(n == 0);

  struct tm next_day;
  unsigned short next_min;
  unsigned char next_pat;
  struct tm sunday_night = make_tm(2026, 9, 13, 0, 23, 30);
  REQUIRE(bell_next_event(&pb.prog, &cb.cal, &sunday_night, &next_day, &next_min, &next_pat));
  REQUIRE(next_day.tm_year == 2026 - 1900);
  REQUIRE(next_day.tm_mon == 9 - 1);
  REQUIRE(next_day.tm_mday == 21); /* next Monday, off week skipped */
  REQUIRE(next_min == 8 * 60);
  REQUIRE(next_pat == 0);
}

/* 2. exact-date match (from == to) */
TEST_CASE("bell_exact_date_match", "[bell]")
{
  struct BellCalendarBuilder cb;
  bell_calendar_builder_init(&cb, 5);
  REQUIRE(bell_builder_add_exception(&cb, 2026, 9, 15, 2026, 9, 15, 0) == BELL_OK);

  struct tm d_before = make_tm(2026, 9, 14, 1);
  struct tm d_exact = make_tm(2026, 9, 15, 2);
  struct tm d_after = make_tm(2026, 9, 16, 3);
  REQUIRE_FALSE(bell_is_day_off(&cb.cal, &d_before));
  REQUIRE(bell_is_day_off(&cb.cal, &d_exact));
  REQUIRE_FALSE(bell_is_day_off(&cb.cal, &d_after));
}

/* 3. range across month boundary */
TEST_CASE("bell_range_month_boundary", "[bell]")
{
  struct BellCalendarBuilder cb;
  bell_calendar_builder_init(&cb, 5);
  REQUIRE(bell_builder_add_exception(&cb, 2026, 9, 28, 2026, 10, 5, 0) == BELL_OK);

  struct tm sep27 = make_tm(2026, 9, 27, 0);
  struct tm sep30 = make_tm(2026, 9, 30, 3);
  struct tm oct1 = make_tm(2026, 10, 1, 4);
  struct tm oct5 = make_tm(2026, 10, 5, 1);
  struct tm oct6 = make_tm(2026, 10, 6, 2);
  REQUIRE_FALSE(bell_is_day_off(&cb.cal, &sep27));
  REQUIRE(bell_is_day_off(&cb.cal, &sep30));
  REQUIRE(bell_is_day_off(&cb.cal, &oct1));
  REQUIRE(bell_is_day_off(&cb.cal, &oct5));
  REQUIRE_FALSE(bell_is_day_off(&cb.cal, &oct6));
}

/* 4. range across year boundary */
TEST_CASE("bell_range_year_boundary", "[bell]")
{
  struct BellCalendarBuilder cb;
  bell_calendar_builder_init(&cb, 5);
  REQUIRE(bell_builder_add_exception(&cb, 2026, 12, 28, 2027, 1, 4, 0) == BELL_OK);

  struct tm dec27 = make_tm(2026, 12, 27, 0);
  struct tm dec31 = make_tm(2026, 12, 31, 4);
  struct tm jan1 = make_tm(2027, 1, 1, 5);
  struct tm jan4 = make_tm(2027, 1, 4, 1);
  struct tm jan5 = make_tm(2027, 1, 5, 2);
  REQUIRE_FALSE(bell_is_day_off(&cb.cal, &dec27));
  REQUIRE(bell_is_day_off(&cb.cal, &dec31));
  REQUIRE(bell_is_day_off(&cb.cal, &jan1));
  REQUIRE(bell_is_day_off(&cb.cal, &jan4));
  REQUIRE_FALSE(bell_is_day_off(&cb.cal, &jan5));
}

/* 5. materialize sorted + 32-event cap ok */
TEST_CASE("bell_materialize_sorted_cap32", "[bell]")
{
  struct BellProgramBuilder pb;
  bell_program_builder_init(&pb, 3, 1);
  REQUIRE(bell_builder_add_pattern(&pb, 1, 3, 2, 1) == BELL_OK);

  /* 32 distinct times, added in descending order */
  for (int i = 31; i >= 0; i--)
    REQUIRE(bell_builder_add_event(&pb, 0, i * 45, 0) == BELL_OK);
  REQUIRE(pb.prog.event_count[0] == 32);
  REQUIRE(bell_program_builder_finish(&pb) == BELL_OK);

  struct BellCalendarBuilder cb;
  bell_calendar_builder_init(&cb, 5);

  struct tm monday = make_tm(2026, 9, 14, 1);
  struct BellDayEvent evs[BELL_MAX_EVENTS_PER_DAY];
  int n = bell_materialize_day(&pb.prog, &cb.cal, &monday, evs, BELL_MAX_EVENTS_PER_DAY);
  REQUIRE(n == 32);
  for (int i = 1; i < n; i++)
    REQUIRE(evs[i - 1].min_of_day < evs[i].min_of_day);
  REQUIRE(evs[0].min_of_day == 0);
  REQUIRE(evs[31].min_of_day == 31 * 45);

  /* blob fits the NVS budget from the contract */
  REQUIRE(bell_program_blob_size() <= 1024);
  REQUIRE(bell_calendar_blob_size() <= 400);
}

/* 6. 33 events/day -> reject */
TEST_CASE("bell_33_events_rejected", "[bell]")
{
  struct BellProgramBuilder pb;
  bell_program_builder_init(&pb, 3, 1);
  REQUIRE(bell_builder_add_pattern(&pb, 1, 3, 2, 1) == BELL_OK);

  for (int i = 0; i < 32; i++)
    REQUIRE(bell_builder_add_event(&pb, 0, i * 45, 0) == BELL_OK);

  REQUIRE(bell_builder_add_event(&pb, 0, 1439, 0) == BELL_ERR_CAPS);
  REQUIRE(pb.prog.event_count[0] == 32);
}

/* 7. pattern total > 30 s / disabled pattern ref -> reject */
TEST_CASE("bell_pattern_rules", "[bell]")
{
  struct BellProgramBuilder pb;
  bell_program_builder_init(&pb, 3, 1);

  /* count*dur + (count-1)*gap: 5*7 + 4*2 = 43 > 30 */
  REQUIRE(bell_builder_add_pattern(&pb, 5, 7, 2, 1) == BELL_ERR_CAPS);
  REQUIRE(bell_builder_add_pattern(&pb, 11, 3, 2, 1) == BELL_ERR_CAPS); /* count > 10 */
  REQUIRE(bell_builder_add_pattern(&pb, 1, 0, 2, 1) == BELL_ERR_CAPS);  /* dur < 1 */
  REQUIRE(bell_builder_add_pattern(&pb, 1, 31, 2, 1) == BELL_ERR_CAPS); /* dur > 30 */
  REQUIRE(bell_builder_add_pattern(&pb, 1, 3, 61, 1) == BELL_ERR_CAPS); /* gap > 60 */
  REQUIRE(pb.prog.pattern_count == 0);

  /* exact boundary total == 30 is accepted */
  REQUIRE(bell_builder_add_pattern(&pb, 2, 10, 10, 1) == BELL_OK);

  /* disabled pattern cannot be referenced */
  REQUIRE(bell_builder_add_pattern(&pb, 1, 3, 2, 0) == BELL_OK); /* pattern 1, disabled */
  REQUIRE(bell_builder_add_event(&pb, 0, 8 * 60, 1) == BELL_ERR_PATTERN_REF);

  /* bad time and unknown pattern index */
  REQUIRE(bell_builder_add_event(&pb, 0, 1440, 0) == BELL_ERR_TIME);
  REQUIRE(bell_builder_add_event(&pb, 0, 8 * 60, 5) == BELL_ERR_PATTERN_REF);
  REQUIRE(bell_builder_add_event(&pb, 0, 8 * 60, 0) == BELL_OK);
}

/* 8. bad dates (Feb 30, from > to) -> reject */
TEST_CASE("bell_bad_dates_rejected", "[bell]")
{
  struct BellCalendarBuilder cb;
  bell_calendar_builder_init(&cb, 5);

  REQUIRE(bell_builder_add_exception(&cb, 2026, 2, 30, 2026, 3, 2, 0) == BELL_ERR_DATE);
  REQUIRE(bell_builder_add_exception(&cb, 2027, 2, 29, 2027, 3, 2, 0) == BELL_ERR_DATE); /* non-leap */
  REQUIRE(bell_builder_add_exception(&cb, 2026, 13, 1, 2026, 13, 2, 0) == BELL_ERR_DATE);
  REQUIRE(bell_builder_add_exception(&cb, 2026, 10, 5, 2026, 10, 1, 0) == BELL_ERR_DATE_ORDER);
  REQUIRE(bell_builder_add_exception(&cb, 2026, 9, 1, 2026, 9, 2, 1) == BELL_ERR_TYPE);
  REQUIRE(cb.cal.exception_count == 0);

  /* leap day on a leap year is fine */
  REQUIRE(bell_builder_add_exception(&cb, 2028, 2, 29, 2028, 2, 29, 0) == BELL_OK);
}

/* 9. calendar-only update rematerializes, pv untouched */
TEST_CASE("bell_calendar_only_update", "[bell]")
{
  struct BellProgramBuilder pb;
  build_simple_program(&pb);

  unsigned char prog_blob[BELL_MAX_PAYLOAD];
  size_t prog_len = bell_encode_program(&pb.prog, prog_blob, sizeof(prog_blob));
  REQUIRE(prog_len > 0);

  struct BellProgram decoded;
  REQUIRE(bell_decode_program(prog_blob, prog_len, &decoded));
  REQUIRE(decoded.v == 3);

  struct tm monday = make_tm(2026, 9, 14, 1);

  /* calendar A: monday off */
  struct BellCalendarBuilder ca;
  bell_calendar_builder_init(&ca, 5);
  REQUIRE(bell_builder_add_exception(&ca, 2026, 9, 14, 2026, 9, 14, 0) == BELL_OK);
  unsigned char cal_blob[BELL_MAX_PAYLOAD];
  size_t cal_len = bell_encode_calendar(&ca.cal, cal_blob, sizeof(cal_blob));
  REQUIRE(cal_len > 0);
  struct BellCalendar cal_a;
  REQUIRE(bell_decode_calendar(cal_blob, cal_len, &cal_a));

  struct BellDayEvent evs[BELL_MAX_EVENTS_PER_DAY];
  REQUIRE(bell_materialize_day(&decoded, &cal_a, &monday, evs, BELL_MAX_EVENTS_PER_DAY) == 0);

  /* calendar B: empty -> same program rings again, v untouched */
  struct BellCalendarBuilder cb;
  bell_calendar_builder_init(&cb, 5);
  cal_len = bell_encode_calendar(&cb.cal, cal_blob, sizeof(cal_blob));
  REQUIRE(cal_len > 0);
  struct BellCalendar cal_b;
  REQUIRE(bell_decode_calendar(cal_blob, cal_len, &cal_b));

  REQUIRE(bell_materialize_day(&decoded, &cal_b, &monday, evs, BELL_MAX_EVENTS_PER_DAY) == 1);
  REQUIRE(evs[0].min_of_day == 8 * 60);
  REQUIRE(decoded.v == 3);
}

/* 10. corrupted bellexc -> empty calendar fallback, program intact */
TEST_CASE("bell_corrupted_calendar_fallback", "[bell]")
{
  struct BellProgramBuilder pb;
  build_simple_program(&pb);
  unsigned char prog_blob[BELL_MAX_PAYLOAD];
  size_t prog_len = bell_encode_program(&pb.prog, prog_blob, sizeof(prog_blob));

  struct BellCalendarBuilder cb;
  bell_calendar_builder_init(&cb, 5);
  REQUIRE(bell_builder_add_exception(&cb, 2026, 9, 14, 2026, 9, 18, 0) == BELL_OK);
  unsigned char cal_blob[BELL_MAX_PAYLOAD];
  size_t cal_len = bell_encode_calendar(&cb.cal, cal_blob, sizeof(cal_blob));
  REQUIRE(cal_len == bell_calendar_blob_size());

  /* corrupt one payload byte; header (first 8 bytes) stays intact */
  cal_blob[9] ^= 0xFF;

  struct BellCalendar cal;
  REQUIRE_FALSE(bell_decode_calendar(cal_blob, cal_len, &cal));
  REQUIRE(cal.exception_count == 0); /* empty fallback: ring per weekly */
  struct tm tue = make_tm(2026, 9, 15, 2);
  REQUIRE_FALSE(bell_is_day_off(&cal, &tue));

  /* program blob is an independent object and stays intact */
  struct BellProgram prog;
  REQUIRE(bell_decode_program(prog_blob, prog_len, &prog));
  REQUIRE(prog.v == 3);
  REQUIRE(prog.event_count[0] == 1);
}

/* 11. canonical serializers produce the contract json exactly */
TEST_CASE("bell_json_serializers_canonical", "[bell]")
{
  struct BellProgramBuilder pb;
  bell_program_builder_init(&pb, 3, 1);
  REQUIRE(bell_builder_add_pattern(&pb, 1, 3, 2, 1) == BELL_OK);
  REQUIRE(bell_builder_add_pattern(&pb, 2, 5, 1, 0) == BELL_OK); /* disabled, unreferenced */
  REQUIRE(bell_builder_add_event(&pb, 0, 8 * 60, 0) == BELL_OK);
  REQUIRE(bell_builder_add_event(&pb, 0, 14 * 60 + 30, 0) == BELL_OK);

  char buf[1024];
  size_t n = bell_program_to_json(&pb.prog, buf, sizeof(buf));
  REQUIRE(n > 0);
  REQUIRE(strcmp(buf,
                 "{\"v\":3,\"enabled\":true,\"patterns\":["
                 "{\"count\":1,\"dur_s\":3,\"gap_s\":2,\"enabled\":true},"
                 "{\"count\":2,\"dur_s\":5,\"gap_s\":1,\"enabled\":false}],"
                 "\"weekly\":{\"mon\":[{\"t\":\"08:00\",\"p\":0},{\"t\":\"14:30\",\"p\":0}],"
                 "\"tue\":[],\"wed\":[],\"thu\":[],\"fri\":[],\"sat\":[],\"sun\":[]}}") == 0);

  struct BellCalendarBuilder cb;
  bell_calendar_builder_init(&cb, 5);
  REQUIRE(bell_builder_add_exception(&cb, 2026, 12, 21, 2027, 1, 7, 0) == BELL_OK);

  n = bell_calendar_to_json(&cb.cal, buf, sizeof(buf));
  REQUIRE(n > 0);
  REQUIRE(strcmp(buf,
                 "{\"v\":5,\"exceptions\":["
                 "{\"from\":\"2026-12-21\",\"to\":\"2027-01-07\"}]}") == 0);

  n = bell_calendar_to_json(&cb.cal, buf, sizeof(buf));
  REQUIRE(n == strlen(buf));
}

/* 12. serializer returns 0 when the buffer is too small */
TEST_CASE("bell_json_serializer_buffer_guard", "[bell]")
{
  struct BellProgramBuilder pb;
  build_simple_program(&pb);

  char buf[1024];
  size_t full = bell_program_to_json(&pb.prog, buf, sizeof(buf));
  REQUIRE(full > 10);

  char small[32];
  REQUIRE(bell_program_to_json(&pb.prog, small, sizeof(small)) == 0);

  char exact[256];
  REQUIRE(bell_program_to_json(&pb.prog, exact, full + 1) == full);
  REQUIRE(bell_program_to_json(&pb.prog, exact, full) == 0);
}

/* 13. alias round-trip: encode/decode/json with trailing empty names trimmed */
TEST_CASE("bell_alias_roundtrip", "[bell]")
{
  struct BellAlias a;
  memset(&a, 0, sizeof(a));
  snprintf(a.bell, sizeof(a.bell), "Parter");
  a.pattern_count = 3;
  snprintf(a.patterns[0], sizeof(a.patterns[0]), "Scurt");
  snprintf(a.patterns[1], sizeof(a.patterns[1]), "Dublu");
  /* patterns[2] left empty -> trimmed from json */

  REQUIRE(bell_validate_alias(&a) == BELL_OK);

  unsigned char blob[BELL_MAX_PAYLOAD];
  size_t len = bell_encode_alias(&a, blob, sizeof(blob));
  REQUIRE(len == bell_alias_blob_size());
  REQUIRE(len <= 400);

  struct BellAlias b;
  REQUIRE(bell_decode_alias(blob, len, &b));
  REQUIRE(strcmp(b.bell, "Parter") == 0);
  REQUIRE(b.pattern_count == 3);
  REQUIRE(strcmp(b.patterns[0], "Scurt") == 0);
  REQUIRE(strcmp(b.patterns[1], "Dublu") == 0);
  REQUIRE(b.patterns[2][0] == 0);

  char json[256];
  size_t n = bell_alias_to_json(&a, json, sizeof(json));
  REQUIRE(n > 0);
  REQUIRE(strcmp(json, "{\"bell\":\"Parter\",\"patterns\":[\"Scurt\",\"Dublu\"]}") == 0);
}

/* 14. alias validation: oversize name and count rejected */
TEST_CASE("bell_alias_validation", "[bell]")
{
  struct BellAlias a;
  memset(&a, 0, sizeof(a));
  memset(a.bell, 'x', BELL_MAX_ALIAS_NAME + 2); /* not terminated within cap */
  REQUIRE(bell_validate_alias(&a) != BELL_OK);

  memset(&a, 0, sizeof(a));
  a.pattern_count = BELL_MAX_PATTERNS + 1;
  REQUIRE(bell_validate_alias(&a) == BELL_ERR_CAPS);

  memset(&a, 0, sizeof(a));
  snprintf(a.patterns[0], sizeof(a.patterns[0]), "nume cu \"ghilimele\" ok");
  a.pattern_count = 1;
  REQUIRE(bell_validate_alias(&a) == BELL_OK);

  char json[128];
  size_t n = bell_alias_to_json(&a, json, sizeof(json));
  REQUIRE(n > 0);
  REQUIRE(strcmp(json, "{\"bell\":\"\",\"patterns\":[\"nume cu \\\"ghilimele\\\" ok\"]}") == 0);
}

/* 15. corrupted alias blob -> empty defaults, other blobs intact */
TEST_CASE("bell_alias_corrupt_fallback", "[bell]")
{
  struct BellAlias a;
  memset(&a, 0, sizeof(a));
  snprintf(a.bell, sizeof(a.bell), "Etaj 1");
  a.pattern_count = 1;
  snprintf(a.patterns[0], sizeof(a.patterns[0]), "Lung");

  unsigned char blob[BELL_MAX_PAYLOAD];
  size_t len = bell_encode_alias(&a, blob, sizeof(blob));
  blob[9] ^= 0xFF;

  struct BellAlias out;
  REQUIRE_FALSE(bell_decode_alias(blob, len, &out));
  REQUIRE(out.bell[0] == 0);
  REQUIRE(out.pattern_count == 0);

  struct BellProgramBuilder pb;
  build_simple_program(&pb);
  size_t plen = bell_encode_program(&pb.prog, blob, sizeof(blob));
  struct BellProgram prog;
  REQUIRE(bell_decode_program(blob, plen, &prog));
  REQUIRE(prog.v == 3);
}
