/* startup.s - Cortex-M3 startup for the bootloader. Same shape as the
 * blink test's startup.s, but the SysTick vector now points at our real
 * SysTick_Handler (system_init.cpp) instead of Default_Handler, since we
 * use it for millisecond timeouts on the UART link.
 */
.syntax unified
.cpu cortex-m3
.thumb

.section .isr_vector, "a", %progbits
.global vector_table
vector_table:
    .word _estack
    .word Reset_Handler
    .word Default_Handler   /* NMI */
    .word Default_Handler   /* HardFault */
    .word Default_Handler   /* MemManage */
    .word Default_Handler   /* BusFault */
    .word Default_Handler   /* UsageFault */
    .word 0
    .word 0
    .word 0
    .word 0
    .word Default_Handler   /* SVCall */
    .word Default_Handler   /* DebugMon */
    .word 0
    .word Default_Handler   /* PendSV */
    .word SysTick_Handler   /* SysTick - defined in system_init.cpp */

.section .text.Reset_Handler, "ax", %progbits
.global Reset_Handler
.type Reset_Handler, %function
Reset_Handler:
    ldr r0, =_sidata
    ldr r1, =_sdata
    ldr r2, =_edata
copy_loop:
    cmp r1, r2
    bge copy_done
    ldr r3, [r0]
    str r3, [r1]
    adds r0, r0, #4
    adds r1, r1, #4
    b copy_loop
copy_done:

    ldr r1, =_sbss
    ldr r2, =_ebss
    movs r3, #0
zero_loop:
    cmp r1, r2
    bge zero_done
    str r3, [r1]
    adds r1, r1, #4
    b zero_loop
zero_done:

    bl main
    b .

.section .text.Default_Handler, "ax", %progbits
.global Default_Handler
.type Default_Handler, %function
Default_Handler:
    b .

.end
