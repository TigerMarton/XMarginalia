#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../../src/activities/reader/ReadingStatsUtils.h"

static int g_checks = 0;
#define CHECK(cond)                                                    \
  do {                                                                 \
    g_checks++;                                                        \
    if (!(cond)) {                                                     \
      fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      return 1;                                                        \
    }                                                                  \
  } while (0)

int test_leap_year() {
  CHECK(isLeapYear(2000));
  CHECK(isLeapYear(2020));
  CHECK(isLeapYear(2024));
  CHECK(!isLeapYear(1900));
  CHECK(!isLeapYear(2021));
  CHECK(!isLeapYear(2022));
  CHECK(!isLeapYear(2023));
  CHECK(!isLeapYear(2100));
  return 0;
}

int test_days_in_month() {
  CHECK(daysInMonth(2024, 2) == 29);
  CHECK(daysInMonth(2023, 2) == 28);
  CHECK(daysInMonth(2024, 1) == 31);
  CHECK(daysInMonth(2024, 4) == 30);
  CHECK(daysInMonth(2024, 12) == 31);
  CHECK(daysInMonth(2024, 0) == 0);
  CHECK(daysInMonth(2024, 13) == 0);
  return 0;
}

int test_day_index_roundtrip() {
  for (uint16_t y = 2020; y <= 2030; ++y) {
    for (uint8_t m = 1; m <= 12; ++m) {
      uint8_t maxD = daysInMonth(y, m);
      for (uint8_t d = 1; d <= maxD; ++d) {
        ReadingStatsDate orig = {y, m, d};
        CHECK(orig.isValid());
        uint32_t idx = readingStatsDayIndex(orig);
        ReadingStatsDate recovered = {};
        CHECK(readingStatsDateFromDayIndex(idx, recovered));
        CHECK(recovered.year == orig.year);
        CHECK(recovered.month == orig.month);
        CHECK(recovered.day == orig.day);
      }
    }
  }
  return 0;
}

int test_day_of_week() {
  // 2026-11-06 is Friday (index 4: Mon=0, Tue=1, Wed=2, Thu=3, Fri=4, Sat=5, Sun=6)
  ReadingStatsDate nov6_2026 = {2026, 11, 6};
  CHECK(readingStatsDayOfWeekIndex(nov6_2026) == 4);

  // 2024-01-01 was Monday (index 0)
  ReadingStatsDate jan1_2024 = {2024, 1, 1};
  CHECK(readingStatsDayOfWeekIndex(jan1_2024) == 0);

  // 2024-01-07 was Sunday (index 6)
  ReadingStatsDate jan7_2024 = {2024, 1, 7};
  CHECK(readingStatsDayOfWeekIndex(jan7_2024) == 6);
  return 0;
}

int test_time_buckets() {
  CHECK(readingTimeBucketForHour(0) == ReadingTimeBucket::Night);
  CHECK(readingTimeBucketForHour(4) == ReadingTimeBucket::Night);
  CHECK(readingTimeBucketForHour(5) == ReadingTimeBucket::Morning);
  CHECK(readingTimeBucketForHour(11) == ReadingTimeBucket::Morning);
  CHECK(readingTimeBucketForHour(12) == ReadingTimeBucket::Afternoon);
  CHECK(readingTimeBucketForHour(16) == ReadingTimeBucket::Afternoon);
  CHECK(readingTimeBucketForHour(17) == ReadingTimeBucket::Evening);
  CHECK(readingTimeBucketForHour(20) == ReadingTimeBucket::Evening);
  CHECK(readingTimeBucketForHour(21) == ReadingTimeBucket::Night);
  CHECK(readingTimeBucketForHour(23) == ReadingTimeBucket::Night);
  return 0;
}

int test_streak_calculations() {
  uint32_t anchor = 1000;
  std::array<uint8_t, READING_HISTORY_BYTES> bits{};

  // Mark 5 consecutive days: 1000, 1001, 1002, 1003, 1004
  for (uint32_t d = 1000; d <= 1004; ++d) {
    markReadingHistoryDay(anchor, bits, d);
  }
  CHECK(computeReadingHistoryLongestStreak(anchor, bits) == 5);

  ReadingStatsDate today{};
  readingStatsDateFromDayIndex(1004, today);
  CHECK(computeReadingHistoryCurrentStreak(anchor, bits, &today) == 5);

  // Yesterday was 1004, today is 1005 (unread yet): current streak should still be 5
  ReadingStatsDate tomorrow{};
  readingStatsDateFromDayIndex(1005, tomorrow);
  CHECK(computeReadingHistoryCurrentStreak(anchor, bits, &tomorrow) == 5);

  // Day after tomorrow 1006 (no reading on 1005): streak reset to 0
  ReadingStatsDate dayAfter{};
  readingStatsDateFromDayIndex(1006, dayAfter);
  CHECK(computeReadingHistoryCurrentStreak(anchor, bits, &dayAfter) == 0);

  // Add 1006..1015 (10 days streak): anchor will shift
  for (uint32_t d = 1006; d <= 1015; ++d) {
    markReadingHistoryDay(anchor, bits, d);
  }
  CHECK(computeReadingHistoryLongestStreak(anchor, bits) == 10);
  return 0;
}

int test_history_merge() {
  uint32_t anchorA = 1000;
  std::array<uint8_t, READING_HISTORY_BYTES> bitsA{};
  markReadingHistoryDay(anchorA, bitsA, 1000);
  markReadingHistoryDay(anchorA, bitsA, 1001);

  uint32_t anchorB = 1000;
  std::array<uint8_t, READING_HISTORY_BYTES> bitsB{};
  markReadingHistoryDay(anchorB, bitsB, 1002);
  markReadingHistoryDay(anchorB, bitsB, 1003);

  mergeReadingHistory(anchorA, bitsA, anchorB, bitsB);
  CHECK(computeReadingHistoryLongestStreak(anchorA, bitsA) == 4);
  return 0;
}

int main() {
  printf("Running Reading Stats host tests...\n");
  if (test_leap_year() != 0) return 1;
  if (test_days_in_month() != 0) return 1;
  if (test_day_index_roundtrip() != 0) return 1;
  if (test_day_of_week() != 0) return 1;
  if (test_time_buckets() != 0) return 1;
  if (test_streak_calculations() != 0) return 1;
  if (test_history_merge() != 0) return 1;

  printf("PASS: All %d Reading Stats checks passed!\n", g_checks);
  return 0;
}
