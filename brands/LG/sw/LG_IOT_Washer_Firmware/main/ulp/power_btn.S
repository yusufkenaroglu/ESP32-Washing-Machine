/*
 * ULP program to debounce and count edges on the power button GPIO
 * and wake the main SoC after a confirmed transition.
 */
#include "sdkconfig.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/rtc_io_reg.h"
#include "soc/soc_ulp.h"
#include "soc/sens_reg.h"

    /* Zero-initialised variables in RTC_SLOW_MEM */
    .bss
    .global next_edge
next_edge:
    .long 0

    .global debounce_counter
debounce_counter:
    .long 0

    .global debounce_max_count
debounce_max_count:
    .long 0

    .global edge_count_power
edge_count_power:
    .long 0

    .global edge_count_to_wake_up
edge_count_to_wake_up:
    .long 0

    .global io_number_power
io_number_power:
    .long 0

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

    /* Load RTC IO number for the power button */
    move r2, io_number_power
    ld   r3, r2, 0

    /* Read RTC GPIO level into r0 */
    move r0, r3
    jumpr read_high, 16, ge
    READ_RTC_REG(RTC_GPIO_IN_REG, RTC_GPIO_IN_NEXT_S, 16)
    rsh r0, r0, r3
    jump read_done

read_high:
    READ_RTC_REG(RTC_GPIO_IN_REG, RTC_GPIO_IN_NEXT_S + 16, 2)
    sub r3, r3, 16
    rsh r0, r0, r3

read_done:
    and r0, r0, 1

    /* Compare against expected polarity */
    move r2, next_edge
    ld   r3, r2, 0
    add  r3, r3, r0
    and  r3, r3, 1
    jump unchanged, eq

    /* Input toggled: check debounce counter */
    move r2, debounce_counter
    ld   r3, r2, 0
    add  r3, r3, 0
    jump edge_detected, eq
    sub  r3, r3, 1
    st   r3, r2, 0
    jump end_program

edge_detected:
    /* Reset debounce counter */
    move r2, debounce_max_count
    ld   r3, r2, 0
    move r2, debounce_counter
    st   r3, r2, 0

    /* Flip expected edge */
    move r2, next_edge
    ld   r3, r2, 0
    add  r3, r3, 1
    and  r3, r3, 1
    st   r3, r2, 0

    /* Increment edge counter */
    move r2, edge_count_power
    ld   r3, r2, 0
    add  r3, r3, 1
    st   r3, r2, 0

    /* Wake main CPU if threshold reached */
    move r2, edge_count_to_wake_up
    ld   r2, r2, 0
    sub  r2, r2, r3
    jump wake_up, eq
    jump end_program

unchanged:
    /* Reset debounce window when signal is stable */
    move r2, debounce_max_count
    ld   r3, r2, 0
    move r2, debounce_counter
    st   r3, r2, 0

end_program:
    halt

init_state:
    /* Seed counters once */
    move r0, debounce_max_count
    ld   r0, r0, 0
    move r1, debounce_counter
    st   r0, r1, 0

    move r0, next_edge
    ld   r0, r0, 0
    move r1, next_edge
    st   r0, r1, 0

    move r0, 1
    move r1, init_done
    st   r0, r1, 0
    jump entry
