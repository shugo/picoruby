#include <mrubyc.h>

#include <stdlib.h>
#include <stdio.h>
#include <time.h>

typedef struct {
  struct tm  tm;
  time_t     unixtime;
  int32_t    timezone;
} PICORUBY_TIME;


static mrbc_int_t
mrbc_Integer(struct VM *vm, mrbc_value value)
{
  if (value.tt == MRBC_TT_INTEGER) return value.i;
  if (value.tt == MRBC_TT_STRING) {
    int sign = 0;
    size_t len = value.string->size;
    uint8_t *data = value.string->data;
    if (data[0] == '-') sign = -1;
    if (data[0] == '+') sign = 1;
    if (data[0] == '_' || data[len - 1] == '_') goto error;
    char str[len];
    int c;
    int i = (sign == 0 ? 0 : 1);
    int j = 0;
    for (; i < len; i++) {
      c = data[i];
      if (c == '_') continue;
      if (47 < c && c < 58) {
        str[j++] = c;
      } else {
        goto error;
      }
    }
    if (str[0] == '\0') goto error;
    str[j] = '\0';
    return (sign < 0 ? mrbc_atoi(str, 10) * -1 : mrbc_atoi(str, 10));
  }
error:
  mrbc_raise(vm, MRBC_CLASS(ArgumentError), "invalid value for Integer()");
  return 0;
}
#define Integer(n) mrbc_Integer(vm, n)


static time_t unixtime_offset = 0;

static void
tz_env_set(struct VM *vm)
{
#ifdef PICORUBY_NO_ENV
  mrbc_value *env = mrbc_get_const(mrbc_search_symid("ENV"));
  if (env == NULL || env->tt != MRBC_TT_HASH) return;
  mrbc_value key = mrbc_string_new_cstr(vm, "TZ");
  mrbc_value tz = mrbc_hash_get(env, &key);
  if (tz.tt != MRBC_TT_STRING) return;
  setenv("TZ", (const char *)tz.string->data, 1);
  tzset();
#endif
}

/*
 * Singleton methods
 */

static void
c_hwclock_eq(struct VM *vm, mrbc_value v[], int argc)
{
  /*
   * Usage: Time.hwclock = Time.local(2023,1,1,0,0,0)
   * Usage: Time.set_hwclock(2023,1,1,0,0,0)
   */
  mrbc_value value = GET_ARG(1);
  mrbc_class *class_Time = mrbc_get_class_by_name("Time");
  if (value.tt != MRBC_TT_OBJECT || value.instance->cls != class_Time) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "value is not a Time");
    return;
  }
  time_t unixtime = ((PICORUBY_TIME *)value.instance->data)->unixtime;
  unixtime_offset = unixtime - time(NULL);
}

static mrbc_value
new_from_unixtime(struct VM *vm, mrbc_value v[], uint32_t unixtime)
{
  tz_env_set(vm);
  mrbc_value value = mrbc_instance_new(vm, v->cls, sizeof(PICORUBY_TIME));
  PICORUBY_TIME *data = (PICORUBY_TIME *)value.instance->data;
  data->unixtime = unixtime + unixtime_offset;
  localtime_r(&data->unixtime, &data->tm);
#ifdef _POSIX_VERSION
  data->timezone = (int32_t)timezone;  /* global variable from time.h */
#else
  data->timezone = (int32_t)_timezone;
#endif
  return value;
}

inline static mrbc_value
new_from_tm(struct VM *vm, mrbc_value v[], struct tm *tm)
{
  tz_env_set(vm);
  mrbc_value value = mrbc_instance_new(vm, v->cls, sizeof(PICORUBY_TIME));
  PICORUBY_TIME *data = (PICORUBY_TIME *)value.instance->data;
  data->unixtime = mktime(tm);
  memcpy(&data->tm, tm, sizeof(struct tm));
#ifdef MRBC_USE_HAL_POSIX
  data->timezone = (int32_t)timezone;
#else
  data->timezone = (int32_t)_timezone;
#endif
  return value;
}

static void
c_local(struct VM *vm, mrbc_value v[], int argc)
{
  if (argc < 1 || 6 < argc) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments (expected 1..6)");
    return;
  }
  struct tm tm;
  tm = (struct tm){0};
  tm.tm_year = Integer(GET_ARG(1)) - 1900;
  if (1 < argc) {
    int mon = Integer(GET_ARG(2));
    if (mon < 1 || 12 < mon) mrbc_raise(vm, MRBC_CLASS(ArgumentError), "mon out of range");
    tm.tm_mon = mon - 1;
  } else tm.tm_mon = 0;
  if (2 < argc) {
    int mday = Integer(GET_ARG(3));
    if (mday < 1 || 31 < mday) mrbc_raise(vm, MRBC_CLASS(ArgumentError), "mday out of range");
    tm.tm_mday = mday;
  } else tm.tm_mday = 1;
  if (3 < argc) {
    int hour = Integer(GET_ARG(4));
    if (hour < 0 || 23 < hour) mrbc_raise(vm, MRBC_CLASS(ArgumentError), "hour out of range");
    tm.tm_hour = hour;
  } else tm.tm_hour = 0;
  if (4 < argc) {
    int min = Integer(GET_ARG(5));
    if (min < 0 || 59 < min) mrbc_raise(vm, MRBC_CLASS(ArgumentError), "min out of range");
    tm.tm_min = min;
  } else tm.tm_min = 0;
  if (5 < argc) {
    int sec = Integer(GET_ARG(6));
    if (sec < 0 || 60 < sec) mrbc_raise(vm, MRBC_CLASS(ArgumentError), "sec out of range");
    tm.tm_sec = sec;
  } else tm.tm_sec = 0;
  SET_RETURN(new_from_tm(vm, v, &tm));
}

static void
c_at(struct VM *vm, mrbc_value v[], int argc)
{
  if (argc != 1) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments (expected 1)");
    return;
  }
  SET_RETURN(new_from_unixtime(vm, v, GET_INT_ARG(1) - unixtime_offset));
}

static void
c_now(struct VM *vm, mrbc_value v[], int argc)
{
  if (0 < argc) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments");
    return;
  }
  SET_RETURN(new_from_unixtime(vm, v, time(NULL)));
}


static void
c_new(struct VM *vm, mrbc_value v[], int argc)
{
  if (argc == 0) {
    c_now(vm, v, 0);
  } else if (6 < argc) {
    mrbc_raise(vm, MRBC_CLASS(ArgumentError), "wrong number of arguments (expected 0..6)");
  } else {
    c_local(vm, v, argc);
  }
}


/*
 * Instance methods
 */

static void
c_to_i(struct VM *vm, mrbc_value v[], int argc)
{
  PICORUBY_TIME *data = (PICORUBY_TIME *)v->instance->data;
  SET_INT_RETURN((uint32_t)data->unixtime);
}

#define MINIMUN_INSPECT_LENGTH 25

static void
c_inspect(struct VM *vm, mrbc_value v[], int argc)
{
  PICORUBY_TIME *data = (PICORUBY_TIME *)v->instance->data;
  struct tm *tm = &data->tm;
  char str[MINIMUN_INSPECT_LENGTH + 10];
  int a = abs(data->timezone) / 60;
  int year = tm->tm_year + 1900;
  if (year < 0) {
    sprintf(str, "%05d", year);
  } else if (year < 10000) {
    sprintf(str, "%04d", year);
  } else {
    sprintf(str, "%d", year);
  }
  sprintf(str + strlen(str), "-%02d-%02d %02d:%02d:%02d %c%02d%02d",
    tm->tm_mon + 1,
    tm->tm_mday,
    tm->tm_hour,
    tm->tm_min,
    tm->tm_sec,
    (0 < data->timezone ? '-' : (data->timezone == 0 ? ' ' : '+')),
    (a / 60),
    (a % 60)
  );
  SET_RETURN(mrbc_string_new_cstr(vm, str));
}

static void
c_year(struct VM *vm, mrbc_value v[], int argc)
{
  SET_INT_RETURN(((PICORUBY_TIME *)v->instance->data)->tm.tm_year + 1900);
}
static void
c_mon(struct VM *vm, mrbc_value v[], int argc)
{
  SET_INT_RETURN(((PICORUBY_TIME *)v->instance->data)->tm.tm_mon + 1);
}
static void
c_mday(struct VM *vm, mrbc_value v[], int argc)
{
  SET_INT_RETURN(((PICORUBY_TIME *)v->instance->data)->tm.tm_mday);
}
static void
c_hour(struct VM *vm, mrbc_value v[], int argc)
{
  SET_INT_RETURN(((PICORUBY_TIME *)v->instance->data)->tm.tm_hour);
}
static void
c_min(struct VM *vm, mrbc_value v[], int argc)
{
  SET_INT_RETURN(((PICORUBY_TIME *)v->instance->data)->tm.tm_min);
}
static void
c_sec(struct VM *vm, mrbc_value v[], int argc)
{
  SET_INT_RETURN(((PICORUBY_TIME *)v->instance->data)->tm.tm_sec);
}
static void
c_wday(struct VM *vm, mrbc_value v[], int argc)
{
  SET_INT_RETURN(((PICORUBY_TIME *)v->instance->data)->tm.tm_wday);
}

static int
mrbc_time_compare(mrbc_value *self, mrbc_value *other)
{
  if (other->tt != MRBC_TT_OBJECT || self->instance->cls != other->instance->cls) {
    return -2;
  }
  time_t other_unixtime = ((PICORUBY_TIME *)other->instance->data)->unixtime;
  time_t self_unixtime = ((PICORUBY_TIME *)self->instance->data)->unixtime;
  if (self_unixtime < other_unixtime) {
    return -1;
  } else if (other_unixtime < self_unixtime) {
    return 1;
  } else {
    return 0;
  }
}

static void
mrbc_time_comparison_failed(struct VM *vm)
{
  mrbc_raise(vm, MRBC_CLASS(ArgumentError), "comparison of Time failed");
}

static void
c_compare(struct VM *vm, mrbc_value v[], int argc)
{
  SET_INT_RETURN(mrbc_time_compare(v, &v[1]));
}

static void
c_eq(struct VM *vm, mrbc_value v[], int argc)
{
  switch (mrbc_time_compare(v, &v[1])) {
    case 0:
      SET_TRUE_RETURN();
      break;
    default:
      SET_FALSE_RETURN();
  }
}

static void
c_lt(struct VM *vm, mrbc_value v[], int argc)
{
  switch (mrbc_time_compare(v, &v[1])) {
    case -1:
      SET_TRUE_RETURN();
      break;
    case 0:
    case 1:
      SET_FALSE_RETURN();
      break;
    default:
      mrbc_time_comparison_failed(vm);
  }
}

static void
c_lte(struct VM *vm, mrbc_value v[], int argc)
{
  switch (mrbc_time_compare(v, &v[1])) {
    case -1:
    case 0:
      SET_TRUE_RETURN();
      break;
    case 1:
      SET_FALSE_RETURN();
      break;
    default:
      mrbc_time_comparison_failed(vm);
  }
}

static void
c_gt(struct VM *vm, mrbc_value v[], int argc)
{
  switch (mrbc_time_compare(v, &v[1])) {
    case -1:
    case 0:
      SET_FALSE_RETURN();
      break;
    case 1:
      SET_TRUE_RETURN();
      break;
    default:
      mrbc_time_comparison_failed(vm);
  }
}

static void
c_gte(struct VM *vm, mrbc_value v[], int argc)
{
  switch (mrbc_time_compare(v, &v[1])) {
    case -1:
      SET_FALSE_RETURN();
      break;
    case 0:
    case 1:
      SET_TRUE_RETURN();
      break;
    default:
      mrbc_time_comparison_failed(vm);
  }
}

void
mrbc_time_class_init(void)
{
  mrbc_class *class_Time = mrbc_define_class(0, "Time", mrbc_class_object);
  mrbc_define_method(0, class_Time, "hwclock=", c_hwclock_eq);
  mrbc_define_method(0, class_Time, "mktime", c_local);
  mrbc_define_method(0, class_Time, "local", c_local);
  mrbc_define_method(0, class_Time, "at", c_at);
  mrbc_define_method(0, class_Time, "now", c_now);
  mrbc_define_method(0, class_Time, "new", c_new);
  mrbc_define_method(0, class_Time, "to_i", c_to_i);
  mrbc_define_method(0, class_Time, "to_s", c_inspect);
  mrbc_define_method(0, class_Time, "inspect", c_inspect);
  mrbc_define_method(0, class_Time, "year", c_year);
  mrbc_define_method(0, class_Time, "mon",  c_mon);
  mrbc_define_method(0, class_Time, "mday", c_mday);
  mrbc_define_method(0, class_Time, "hour", c_hour);
  mrbc_define_method(0, class_Time, "min",  c_min);
  mrbc_define_method(0, class_Time, "sec",  c_sec);
  mrbc_define_method(0, class_Time, "wday", c_wday);
  mrbc_define_method(0, class_Time, "<=>", c_compare);
  mrbc_define_method(0, class_Time, "==", c_eq);
  mrbc_define_method(0, class_Time, "<",  c_lt);
  mrbc_define_method(0, class_Time, "<=", c_lte);
  mrbc_define_method(0, class_Time, ">",  c_gt);
  mrbc_define_method(0, class_Time, ">=", c_gte);
}
