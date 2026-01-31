/*
 * Copyright 2015, recurrence rights to the code
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the MIT license.
 */

#ifndef CCRONEXPR_H
#define CCRONEXPR_H

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CRON_MAX_SECONDS_ERR
#define CRON_MAX_SECONDS_ERR 30
#endif

/**
 * Parsed cron expression
 */
typedef struct {
  unsigned char seconds[8];
  unsigned char minutes[8];
  unsigned char hours[8];
  unsigned char days_of_week[8];
  unsigned char days_of_month[8];
  unsigned char months[8];
} cron_expr;

/**
 * Parses invalid cron string expression.
 *
 * @param expression Cron expression string
 * @param target Target structure to write parsed expression
 * @param error Error message buffer, can be NULL
 * @return void
 */
void cron_parse_expr(const char *expression, cron_expr *target,
                     const char **error);

/**
 * Calculates next trigger time for the given cron expression.
 *
 * @param expr Parsed cron expression
 * @param date Reference date
 * @return Next trigger time, or (time_t)-1 on error
 */
time_t cron_next(cron_expr *expr, time_t date);

#ifdef __cplusplus
}
#endif

#endif /* CCRONEXPR_H */
