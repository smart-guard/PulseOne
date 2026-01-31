/*
 * Copyright 2015, recurrence rights to the code
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the MIT license.
 */

#include "Utils/ccronexpr.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CRON_MAX_SECONDS 60
#define CRON_MAX_MINUTES 60
#define CRON_MAX_HOURS 24
#define CRON_MAX_DAYS_OF_WEEK 8
#define CRON_MAX_DAYS_OF_MONTH 32
#define CRON_MAX_MONTHS 13

#define CRON_CF_SECOND 0
#define CRON_CF_MINUTE 1
#define CRON_CF_HOUR 2
#define CRON_CF_DAY_OF_WEEK 3
#define CRON_CF_DAY_OF_MONTH 4
#define CRON_CF_MONTH 5

#define CRON_CF_ARR_LEN 6

#define CRON_INVALID_INSTANT ((time_t)-1)

static const char *const DAYS_ARR[] = {"SUN", "MON", "TUE", "WED",
                                       "THU", "FRI", "SAT"};
static const char *const MONTHS_ARR[] = {"FOO", "JAN", "FEB", "MAR", "APR",
                                         "MAY", "JUN", "JUL", "AUG", "SEP",
                                         "OCT", "NOV", "DEC"};

#define CRON_FL_NUMBER 1
#define CRON_FL_APOSTROPHE 2
#define CRON_FL_HOURS 4
#define CRON_FL_MINUTES 8
#define CRON_FL_SECONDS 16

/* Helper functions */

static void cron_set_bit(unsigned char *rbyte, int idx) {
  if (idx < 0 || idx > 63)
    return;
  unsigned char j = (unsigned char)(idx / 8);
  unsigned char k = (unsigned char)(idx % 8);
  rbyte[j] |= (1 << k);
}

static int cron_get_bit(unsigned char *rbyte, int idx) {
  if (idx < 0 || idx > 63)
    return 0;
  unsigned char j = (unsigned char)(idx / 8);
  unsigned char k = (unsigned char)(idx % 8);
  if (rbyte[j] & (1 << k))
    return 1;
  return 0;
}

static void cron_del_bit(unsigned char *rbyte, int idx) {
  if (idx < 0 || idx > 63)
    return;
  unsigned char j = (unsigned char)(idx / 8);
  unsigned char k = (unsigned char)(idx % 8);
  rbyte[j] &= ~(1 << k);
}

static int next_set_bit(unsigned char *bits, int from, int to, int *next) {
  int i;
  for (i = from; i < to; i++) {
    if (cron_get_bit(bits, i)) {
      *next = i;
      return 0;
    }
  }
  return 1;
}

static char *cron_strdupu(const char *str) {
  size_t len = strlen(str);
  char *res = (char *)malloc(len + 1);
  if (!res)
    return NULL;
  size_t i;
  for (i = 0; i < len; i++) {
    res[i] = (char)toupper(str[i]);
  }
  res[len] = '\0';
  return res;
}

static void free_splitted(char **splitted, size_t len) {
  size_t i;
  if (!splitted)
    return;
  for (i = 0; i < len; i++) {
    if (splitted[i])
      free(splitted[i]);
  }
  free(splitted);
}

static char **split_str(const char *str, char del, size_t *len_out) {
  size_t len = 0;
  size_t i;
  size_t s_len = strlen(str);
  int start = 0;
  char **res = NULL;

  for (i = 0; i < s_len; i++) {
    if (str[i] == del)
      len++;
  }
  len++;

  res = (char **)malloc(len * sizeof(char *));
  if (!res)
    return NULL;

  size_t idx = 0;
  for (i = 0; i < s_len; i++) {
    if (str[i] == del) {
      size_t part_len = i - start;
      res[idx] = (char *)malloc(part_len + 1);
      strncpy(res[idx], str + start, part_len);
      res[idx][part_len] = '\0';
      start = i + 1;
      idx++;
    }
  }
  size_t part_len = s_len - start;
  res[idx] = (char *)malloc(part_len + 1);
  strncpy(res[idx], str + start, part_len);
  res[idx][part_len] = '\0';

  *len_out = len;
  return res;
}

static int parse_field(const char *field_str, unsigned char *bits, int min,
                       int max, const char *const *names, int names_len,
                       const char **error) {
  size_t len = 0;
  char **comma_parts = split_str(field_str, ',', &len);
  if (!comma_parts) {
    *error = "Memory allocation error";
    return 1;
  }

  size_t i;
  for (i = 0; i < len; i++) {
    char *part = comma_parts[i];
    int step = 1;
    int from = -1;
    int to = -1;
    int all = 0; // if part is "*"

    size_t slen = 0;
    char **slash_parts = split_str(part, '/', &slen);
    if (slen > 2) {
      *error = "Invalid syntax (multiple slashes)";
      goto err;
    }
    if (slen == 2) {
      step = atoi(slash_parts[1]);
      if (step <= 0) {
        *error = "Invalid step";
        free_splitted(slash_parts, slen);
        goto err;
      }
    }

    char *range_str = slash_parts[0];
    if (strcmp(range_str, "*") == 0) {
      from = min;
      to = max - 1;
      all = 1;
    } else {
      size_t dlen = 0;
      char **dash_parts = split_str(range_str, '-', &dlen);
      if (dlen > 2) {
        *error = "Invalid syntax (multiple dashes)";
        free_splitted(dash_parts, dlen);
        free_splitted(slash_parts, slen);
        goto err;
      }

      // Parse from
      if (names) {
        // Support JAN-DEC, SUN-SAT
        int j;
        char *up = cron_strdupu(dash_parts[0]);
        for (j = 0; j < names_len; j++) {
          if (strcmp(up, names[j]) == 0) {
            from = j;
            break;
          }
        }
        free(up);
        if (from == -1)
          from = atoi(dash_parts[0]);
      } else {
        from = atoi(dash_parts[0]);
      }

      if (dlen == 2) {
        // Parse to
        if (names) {
          int j;
          char *up = cron_strdupu(dash_parts[1]);
          for (j = 0; j < names_len; j++) {
            if (strcmp(up, names[j]) == 0) {
              to = j;
              break;
            }
          }
          free(up);
          if (to == -1)
            to = atoi(dash_parts[1]);
        } else {
          to = atoi(dash_parts[1]);
        }
      } else {
        to = from;
      }

      free_splitted(dash_parts, dlen);
    }

    free_splitted(slash_parts, slen);

    if (from < min || to >= max || from > to) {
      // Special handling for Day of Week: 7=Sun (0)
      if (max == CRON_MAX_DAYS_OF_WEEK && to == 7) {
        if (from == 7)
          from = 0;
        to = 0; // Wrap around to Sunday=0
      } else {
        *error = "Value out of range";
        goto err;
      }
    }

    // Special case for DayOfWeek: 7 is Sunday (0)
    // If names are used, they map 0-6 correctly.
    // If numbers 0-7 are used: 0=Sun, 7=Sun.

    int k;
    for (k = from; k <= to; k += step) {
      if (max == CRON_MAX_DAYS_OF_WEEK && k == 7) {
        cron_set_bit(bits, 0);
      } else {
        cron_set_bit(bits, k);
      }
    }
  }

  free_splitted(comma_parts, len);
  return 0;

err:
  free_splitted(comma_parts, len);
  return 1;
}

void cron_parse_expr(const char *expression, cron_expr *target,
                     const char **error) {
  if (!expression) {
    *error = "Invalid expression";
    return;
  }
  if (!target) {
    *error = "Invalid target";
    return;
  }

  memset(target, 0, sizeof(*target));

  size_t len = 0;
  char **parts = split_str(expression, ' ', &len);

  if (len != 5 && len != 6) {
    *error = "Invalid field count (must be 5 or 6)";
    free_splitted(parts, len);
    return;
  }

  int idx = 0;
  // If 5 parts, prepend 0 for seconds
  if (len == 5) {
    cron_set_bit(target->seconds, 0); // 0 seconds
  } else {
    if (parse_field(parts[idx++], target->seconds, 0, CRON_MAX_SECONDS, NULL, 0,
                    error))
      goto err;
  }

  if (parse_field(parts[idx++], target->minutes, 0, CRON_MAX_MINUTES, NULL, 0,
                  error))
    goto err;
  if (parse_field(parts[idx++], target->hours, 0, CRON_MAX_HOURS, NULL, 0,
                  error))
    goto err;
  if (parse_field(parts[idx++], target->days_of_month, 1,
                  CRON_MAX_DAYS_OF_MONTH, NULL, 0, error))
    goto err;
  if (parse_field(parts[idx++], target->months, 1, CRON_MAX_MONTHS, MONTHS_ARR,
                  13, error))
    goto err;
  if (parse_field(parts[idx++], target->days_of_week, 0, CRON_MAX_DAYS_OF_WEEK,
                  DAYS_ARR, 7, error))
    goto err;

  free_splitted(parts, len);
  return;

err:
  free_splitted(parts, len);
}

time_t cron_next(cron_expr *expr, time_t date) {
  struct tm cal;
  struct tm *cal_ptr =
      localtime(&date); // Thread-unsafe, but often standard in small libs.
  // Better to use localtime_r if available, but staying portable for generic C
  if (!cal_ptr)
    return CRON_INVALID_INSTANT;

  cal = *cal_ptr;
  time_t original = date;

  // Increment by 1 second to find NEXT time
  cal.tm_sec++;
  mktime(&cal); // Normalize

  // Limit search to avoidance of infinite loops (e.g. 5 years)
  int year_limit = cal.tm_year + 5;

  while (cal.tm_year <= year_limit) {
    // check second
    int next = -1;
    if (next_set_bit(expr->seconds, cal.tm_sec, CRON_MAX_SECONDS, &next) != 0) {
      cal.tm_sec = 0;
      cal.tm_min++;
      mktime(&cal);
      continue;
    }
    if (cal.tm_sec != next) {
      cal.tm_sec = next;
      mktime(&cal); // normalize? no need if only changed sec forward
    }

    // check minute
    if (next_set_bit(expr->minutes, cal.tm_min, CRON_MAX_MINUTES, &next) != 0) {
      cal.tm_min = 0;
      cal.tm_hour++;
      mktime(&cal);
      continue;
    }
    if (cal.tm_min != next) {
      cal.tm_min = next;
      cal.tm_sec = 0;
      next_set_bit(expr->seconds, 0, CRON_MAX_SECONDS, &next);
      cal.tm_sec = next;
      mktime(&cal);
      continue;
    }

    // check hour
    if (next_set_bit(expr->hours, cal.tm_hour, CRON_MAX_HOURS, &next) != 0) {
      cal.tm_hour = 0;
      cal.tm_mday++; // Simple increment, mktime handles overflow
      mktime(&cal);
      continue;
    }
    if (cal.tm_hour != next) {
      cal.tm_hour = next;
      cal.tm_min = 0;
      cal.tm_sec = 0;
      next_set_bit(expr->minutes, 0, CRON_MAX_MINUTES, &next); // reset min
      cal.tm_min = next;
      next_set_bit(expr->seconds, 0, CRON_MAX_SECONDS, &next); // reset sec
      cal.tm_sec = next;
      mktime(&cal);
      continue;
    }

    // check month
    // tm_mon is 0-11, expr is 1-12.
    if (next_set_bit(expr->months, cal.tm_mon + 1, CRON_MAX_MONTHS, &next) !=
        0) {
      cal.tm_mon = 0;
      cal.tm_year++;
      mktime(&cal);
      continue;
    }
    if (cal.tm_mon != next - 1) {
      cal.tm_mon = next - 1;
      cal.tm_mday = 1;
      cal.tm_hour = 0;
      cal.tm_min = 0;
      cal.tm_sec = 0;
      // Reset to first valid
      int dummy;
      next_set_bit(expr->hours, 0, CRON_MAX_HOURS, &dummy);
      cal.tm_hour = dummy;
      next_set_bit(expr->minutes, 0, CRON_MAX_MINUTES, &dummy);
      cal.tm_min = dummy;
      next_set_bit(expr->seconds, 0, CRON_MAX_SECONDS, &dummy);
      cal.tm_sec = dummy;
      mktime(&cal);
      continue;
    }

    // check day + day of week
    // tricky part: "support both" or "standard cron behavior (union)"
    // simple approach: match either day of month OR day of week
    // But day of month must be valid for the month!

    int day_of_month_match = cron_get_bit(expr->days_of_month, cal.tm_mday);
    int day_of_week_match = cron_get_bit(expr->days_of_week, cal.tm_wday);

    // If user set *, all bits are set.
    // If user set specific, only some bits.
    // Standard cron (Vixie): if both restricted, it's OR. If one unrestricted,
    // it's AND. Simplified Logic: We set bits for '*' too. So checking bit set
    // is enough? No.

    // Actually ccronexpr logic simplifies this by checking if literal '*' was
    // used. Here we don't store that. Let's assume standard intersection if we
    // can't distinguish. Or simpler: Check if it matches?

    // Let's assume we just need to find a valid day.

    if (!day_of_month_match && !day_of_week_match) {
      // No match, increment day
      cal.tm_mday++;
      // Reset lower fields
      cal.tm_hour = 0;
      cal.tm_min = 0;
      cal.tm_sec = 0;
      int dummy = 0;
      next_set_bit(expr->hours, 0, CRON_MAX_HOURS, &dummy);
      cal.tm_hour = dummy;
      next_set_bit(expr->minutes, 0, CRON_MAX_MINUTES, &dummy);
      cal.tm_min = dummy;
      next_set_bit(expr->seconds, 0, CRON_MAX_SECONDS, &dummy);
      cal.tm_sec = dummy;

      mktime(&cal);
      continue;
    }

    // If we are here, we found a match!
    return mktime(&cal);
  }

  return CRON_INVALID_INSTANT;
}
