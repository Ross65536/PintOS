#ifndef __KERNEL_FIXED_POINT__H
#define __KERNEL_FIXED_POINT__H

#include <stdint.h>

#define __Q (14)
#define __F (1 << __Q)

/**
 * 17.14 fixed-point reals
 */
struct fixed_point {
  int32_t value;
};

static inline struct fixed_point fixed_point_build (int n) {
  struct fixed_point ret = {
    n * __F
  };

  return ret;
}

static inline int fixed_point_to_int (struct fixed_point real) {
  return real.value / __F;
}

static inline int fixed_point_to_nearest_int (struct fixed_point real) {
  if (real.value >= 0)
    return (real.value + __F / 2) / __F;
  else 
    return (real.value - __F / 2) / __F;
}

static inline struct fixed_point fixed_point_add_real (struct fixed_point x, struct fixed_point y) {
  struct fixed_point ret = {
    x.value + y.value
  };

  return ret;
}

static inline struct fixed_point fixed_point_sub_real (struct fixed_point x, struct fixed_point y) {
  struct fixed_point ret = {
    x.value - y.value
  };

  return ret;
}

static inline struct fixed_point fixed_point_mult_real (struct fixed_point x, struct fixed_point y) {
  struct fixed_point ret = {
    (((int64_t) x.value) * y.value / __F)
  };

  return ret;
}

static inline struct fixed_point fixed_point_div_real (struct fixed_point x, struct fixed_point y) {
  struct fixed_point ret = {
    (((int64_t) x.value) * __F / y.value) 
  };

  return ret;
}

static inline struct fixed_point fixed_point_add_int (struct fixed_point x, int n) {
  struct fixed_point ret = {
    x.value + n * __F
  };

  return ret;
}

static inline struct fixed_point fixed_point_sub_int (struct fixed_point x, int n) {
  struct fixed_point ret = {
    x.value - n * __F
  };

  return ret;
}

static inline struct fixed_point fixed_point_mult_int (struct fixed_point x, int n) {
  struct fixed_point ret = {
    x.value * n
  };

  return ret;
}

static inline struct fixed_point fixed_point_div_int (struct fixed_point x, int n) {
  struct fixed_point ret = {
    x.value / n
  };

  return ret;
}




#endif