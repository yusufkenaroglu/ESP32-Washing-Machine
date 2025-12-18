/*
 * ULP program to debounce and count edges on both power and start/stop buttons
 * and wake the main SoC after a confirmed transition. The same routine loops
 * over both buttons using per-button arrays in RTC_SLOW_MEM to minimise code
 * size while keeping the debounce logic identical.
 */
#include "sdkconfig.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/rtc_io_reg.h"
#include "soc/soc_ulp.h"
#include "soc/sens_reg.h"

#define BUTTON_COUNT 2
#define MASK_POWER   1
#define MASK_START   2

    /* Zero-initialised variables in RTC_SLOW_MEM */
    .bss
    /* Per-button state (index 0 = power, 1 = start/stop) */
    .global next_edge
next_edge:
    .long 0
    .long 0

    .global debounce_counter
debounce_counter:
    .long 0
    .long 0

    .global debounce_max_count
debounce_max_count:
    .long 0
    .long 0

    .global edge_count_buttons
edge_count_buttons:
    .long 0
    .long 0

    .global io_numbers
io_numbers:
    .long 0
    .long 0

    /* Shared configuration */
    .global edge_count_to_wake_up
edge_count_to_wake_up:
    .long 0

    .global button_enable_mask
button_enable_mask:
    .long 0 /* default: only power button active */

    /* One-time init guard */
    .global init_done
init_done:
    .long 0

    .text
    .global entry
    entry:
        /* Run one-time seeding of counters */
        move r3, init_done
        ld   r0, r3, 0
        add  r0, r0, 0
        jump init_state, eq

        /* Iterate over both buttons (0 = power, 1 = start/stop) */
        move r0, 0              /* r0: button index */

    button_loop:
        move r3, r0             /* keep index in r3 for later */

        /* Check enable mask: skip if disabled */
        move r1, button_enable_mask
        ld   r1, r1, 0
        rsh  r1, r1, r3
        and  r1, r1, 1
        jump next_button, eq

        /* Load RTC IO number for this button */
        move r1, io_numbers
        add  r1, r1, r3
        ld   r1, r1, 0          /* r1 = io_number */

        /* Read RTC GPIO level into r0 (macros place result in r0) */
        move r0, r1             /* r0 = io_number for comparison in JUMPR */
        move r2, r1             /* r2 = io_number (scratch for high-half case) */
        jumpr read_high, 16, ge
        READ_RTC_REG(RTC_GPIO_IN_REG, RTC_GPIO_IN_NEXT_S, 16)
        rsh  r0, r0, r1
        jump read_done

    read_high:
        READ_RTC_REG(RTC_GPIO_IN_REG, RTC_GPIO_IN_NEXT_S + 16, 2)
        sub  r2, r2, 16
        rsh  r0, r0, r2

    read_done:
        and  r0, r0, 1          /* r0 = level */

        /* Compare against expected polarity */
        move r1, next_edge
        add  r1, r1, r3
        ld   r2, r1, 0          /* r2 = expected */
        add  r2, r2, r0
        and  r2, r2, 1
        jump unchanged, eq

        /* Input toggled: check debounce counter */
        move r1, debounce_counter
        add  r1, r1, r3
        ld   r2, r1, 0
        add  r2, r2, 0
        jump edge_detected, eq
        sub  r2, r2, 1
        st   r2, r1, 0
        jump next_button

    edge_detected:
        /* Reset debounce counter */
        move r1, debounce_max_count
        add  r1, r1, r3
        ld   r2, r1, 0
        move r1, debounce_counter
        add  r1, r1, r3
        st   r2, r1, 0

        /* Flip expected edge */
        move r1, next_edge
        add  r1, r1, r3
        ld   r2, r1, 0
        add  r2, r2, 1
        and  r2, r2, 1
        st   r2, r1, 0

        /* Increment edge counter */
        move r1, edge_count_buttons
        add  r1, r1, r3
        ld   r2, r1, 0
        add  r2, r2, 1
        st   r2, r1, 0

        /* Wake main CPU if threshold reached */
        move r1, edge_count_to_wake_up
        ld   r1, r1, 0
        sub  r1, r1, r2
        jump wake_up, eq
        jump next_button

    unchanged:
        /* Reset debounce window when signal is stable */
        move r1, debounce_max_count
        add  r1, r1, r3
        ld   r2, r1, 0
        move r1, debounce_counter
        add  r1, r1, r3
        st   r2, r1, 0

    next_button:
        move r0, r3
        add  r0, r0, 1
        jumpr button_loop, BUTTON_COUNT, lt

        halt
init_state:
    /* Seed counters once for both buttons */
    move r0, 0
init_loop:
    move r1, debounce_max_count
    add  r1, r1, r0
    ld   r2, r1, 0
    move r3, debounce_counter
    add  r3, r3, r0
    st   r2, r3, 0

    /* Seed next_edge with current value (default 0) */
    move r1, next_edge
    add  r1, r1, r0
    ld   r2, r1, 0
    st   r2, r1, 0

    /* Clear edge counters */
    move r1, edge_count_buttons
    add  r1, r1, r0
    move r2, 0
    st   r2, r1, 0

    add  r0, r0, 1
    jumpr init_loop, BUTTON_COUNT, lt

    move r0, 1
    move r1, init_done
    st   r0, r1, 0
    jump entry
