#include <stdio.h>

extern char metal_segment_itim_target_start;
extern char metal_segment_itim_target_end;

extern char metal_segment_data_target_start;
extern char metal_segment_data_target_end;

extern char metal_segment_bss_target_start;
extern char metal_segment_bss_target_end;

extern char metal_segment_heap_target_start;
extern char metal_segment_heap_target_end;

extern char metal_segment_stack_begin;
extern char metal_segment_stack_end;

void print_mem(void) {
    printf("ITIM:  %p - %p\n", &metal_segment_itim_target_start, &metal_segment_itim_target_end);
    printf("DATA:  %p - %p\n", &metal_segment_data_target_start, &metal_segment_data_target_end);
    printf("BSS:   %p - %p\n", &metal_segment_bss_target_start, &metal_segment_bss_target_end);
    printf("HEAP:  %p - %p\n", &metal_segment_heap_target_start, &metal_segment_heap_target_end);
    printf("STACK: %p - %p\n", &metal_segment_stack_begin, &metal_segment_stack_end);
}
