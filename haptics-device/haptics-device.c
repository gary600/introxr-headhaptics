#include <stdbool.h>
#include <limits.h>
#include <stdint.h>
#include <pico.h>
#include <stdio.h>
#include <pico/stdlib.h>
#include <hardware/timer.h>
#include <hardware/gpio.h>
#include <hardware/pwm.h>
#include <hardware/uart.h>
#include <hardware/sync.h>

#define WRAP 1024
#define BUFSIZE 256

// The points are numbered as follows:
// The right side's 7 points are indices 1-7
// The left side's 7 points are indices 9-15
// Points 0 and 8 don't exist
// Slices are distributed in pairs for each side:
// The right side starts at slice N=0
// The left side starts at slice N=4
// Points 1-2 are slice N, 3-4 N+1, 5-6 N+2, 7 N+3
// The lower-numbered point is channel A, higher is B

#define NUM_POINTS 16

const bool POINTS_VALID[NUM_POINTS] = {
  0, 1, 1, 1, 1, 1, 1, 1,
  0, 1, 1, 1, 1, 1, 1, 1,
};

const unsigned POINTS_SLICES[NUM_POINTS] = {
  UINT_MAX, 0, 0, 1, 1, 2, 2, 3,
  UINT_MAX, 4, 4, 5, 5, 6, 6, 7,
};

const unsigned POINTS_CHANNELS[NUM_POINTS] = {
  UINT_MAX, 0, 1, 0, 1, 0, 1, 0,
  UINT_MAX, 0, 1, 0, 1, 0, 1, 0,
};

// SAFETY: values accessed in interrupt, use critical section
uint16_t points_at[NUM_POINTS] = {0};
uint16_t points_ramp[NUM_POINTS] = {0};
uint16_t points_target[NUM_POINTS] = {0};

// SAFETY: this only has a valid value within a critical section, which
// is safe because it is in a critical section.
uint32_t _interrupts;
#define CRITICAL_BEGIN() _interrupts = save_and_disable_interrupts()
#define CRITICAL_END() restore_interrupts(_interrupts)

void cmd_reset() {
  // Configure PWM
  for (unsigned slice = 0; slice < 8; ++slice) {
    // 125 MHz / 65536 ~= 2 kHz
    pwm_set_clkdiv(slice, 1.0f);
    // pwm_set_wrap(slice, 65535);
    pwm_set_wrap(slice, WRAP);
    pwm_set_chan_level(slice, 0, 0);
    pwm_set_chan_level(slice, 1, 0);
    pwm_set_enabled(slice, true);
  }
  // Set pins to PWM output
  for (unsigned gpio = 0; gpio < 16; ++gpio) {
    gpio_set_function(gpio, GPIO_FUNC_PWM);
  }

  // Reset setpoints
  CRITICAL_BEGIN();
  for (size_t point = 0; point < NUM_POINTS; ++point) {
    points_at[point] = 0;
    points_ramp[point] = 0;
    points_target[point] = 0;
  }
  CRITICAL_END();
}

void cmd_set(uint8_t point, uint16_t target, uint16_t ramp) {
  if (point >= NUM_POINTS) return;
  if (!POINTS_VALID[point]) return;

  CRITICAL_BEGIN();
  points_target[point] = target;
  points_ramp[point] = ramp;
  CRITICAL_END();
};

void cmd_parameters(uint8_t point, float clkdiv, uint16_t wrap) {
  if (point >= NUM_POINTS) return;
  if (!POINTS_VALID[point]) return;

  unsigned slice = POINTS_SLICES[point];
  pwm_set_clkdiv(slice, clkdiv);
  pwm_set_wrap(slice, wrap);
}

void cmd_query() {
  for (size_t point = 0; point < NUM_POINTS; ++point) {
    if (!POINTS_VALID[point]) continue;
    CRITICAL_BEGIN();
    uint16_t at = points_at[point];
    uint16_t ramp = points_ramp[point];
    uint16_t target = points_target[point];
    CRITICAL_END();
    // printf can't be in critsec
    printf(
      "point %d: at %d, ramp %d, target %d\n",
      point, at, ramp, target
    );
  }
}

void cmd_test() {
  // Test each point individually
  for (size_t point = 0; point < NUM_POINTS; ++point) {
    if (!POINTS_VALID[point]) continue;
    cmd_set(point, WRAP, WRAP);
    sleep_ms(300);
    cmd_set(point, 0, WRAP);
    sleep_ms(300);
  }

  // Test all points at once
  for (size_t point = 0; point < NUM_POINTS; ++point) {
    if (!POINTS_VALID[point]) continue;
    cmd_set(point, WRAP, WRAP);
  }
  sleep_ms(1000);
  for (size_t point = 0; point < NUM_POINTS; ++point) {
    if (!POINTS_VALID[point]) continue;
    cmd_set(point, 0, WRAP);
  }
}

bool timer_update(__unused struct repeating_timer* t) {
  // printf("p9: %d\n", points_at[9]);
  for (size_t point = 0; point < NUM_POINTS; ++point) {
    if (!POINTS_VALID[point]) continue;
    CRITICAL_BEGIN();
    uint16_t at = points_at[point];
    int32_t ramp = points_ramp[point];
    uint16_t target = points_target[point];
    CRITICAL_END();
    if (ramp == 0) continue;
    if (at == target) continue;
    if (at < target && (int32_t)at + (int32_t)ramp > target) ramp = target - at;
    if (at > target) {
      if ((int32_t)at - (int32_t)ramp < target) ramp = (int32_t)target - (int32_t)at;
      else ramp = -ramp;
    }
    pwm_set_chan_level(POINTS_SLICES[point], POINTS_CHANNELS[point], at + ramp);
    CRITICAL_BEGIN();
    points_at[point] = at + ramp;
    CRITICAL_END();
  }
  return true;
}


void main() {
  stdio_init_all();

  cmd_reset();

  struct repeating_timer timer;
  add_repeating_timer_ms(1, timer_update, NULL, &timer);

  // Main thread: blocking main loop: read serial + handle commands
  while (true) {
    char buf [BUFSIZE];
    size_t bufptr;
    line_start:
    bufptr = 0;
    do {
      int c;
      while ((c = getchar()) != EOF) {
        // echo for terminal
        putchar(c);
        if (bufptr >= 255) goto line_start; // if buffer full, give up and try again
        buf[bufptr] = c;
        bufptr++;
        if (c == '\n') goto line_end;
      }
    } while (bufptr == 0 || buf[bufptr-1] != '\n');
    line_end:
    if (bufptr == 1 || buf[0] == '\r') continue;
    buf[bufptr-1] = '\0';

    // Command
    switch (buf[0]) {
      case 'r': {
        cmd_reset();
        break;
      }

      case 's': {
        unsigned point;
        unsigned target;
        unsigned ramp;
        if (sscanf(buf + 1, "%d, %d, %d", &point, &target, &ramp) != 3) {
          printf("invalid args\n");
          break;
        }
        // Serial.println("pass");
        cmd_set((uint8_t)point, (uint16_t)target, (uint16_t)ramp);
        break;
      }

      case 'q': {
        cmd_query();
        break;
      }
      case 't': {
        cmd_test();
        break;
      }

      default: {
        printf("unknown command\n");
        break;
      }
    }
  }
}