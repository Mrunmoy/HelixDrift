/* startup_nrf52840.S
 *
 * Minimal ARM Cortex-M4F startup for nRF52840.
 * Provides the interrupt vector table and Reset_Handler which:
 *   1. Enables the FPU (CP10/CP11 full access via CPACR)
 *   2. Copies .data from flash (LMA) to RAM (VMA)
 *   3. Zeros .bss
 *   4. Calls __libc_init_array() (C++ global constructors)
 *   5. Calls main()
 *
 * All IRQ handlers are defined as weak aliases to Default_Handler so they
 * can be overridden by C/C++ code.
 */

    .syntax unified
    .cpu cortex-m4
    .fpu fpv4-sp-d16
    .thumb

/* Defined by the linker script */
    .extern _stack_top
    .extern _sidata
    .extern _sdata
    .extern _edata
    .extern _sbss
    .extern _ebss
    .extern __libc_init_array

/* -------------------------------------------------------------------------
 * Vector table
 * ------------------------------------------------------------------------- */
    .section .isr_vector, "a", %progbits
    .type  g_pfnVectors, %object

g_pfnVectors:
    /* Cortex-M system exceptions */
    .word  _stack_top
    .word  Reset_Handler
    .word  NMI_Handler
    .word  HardFault_Handler
    .word  MemManage_Handler
    .word  BusFault_Handler
    .word  UsageFault_Handler
    .word  0                        /* Reserved */
    .word  0                        /* Reserved */
    .word  0                        /* Reserved */
    .word  0                        /* Reserved */
    .word  SVC_Handler
    .word  DebugMon_Handler
    .word  0                        /* Reserved */
    .word  PendSV_Handler
    .word  SysTick_Handler

    /* nRF52840 peripheral IRQs (0–47) */
    .word  POWER_CLOCK_IRQHandler
    .word  RADIO_IRQHandler
    .word  UARTE0_UART0_IRQHandler
    .word  SPIM0_SPIS0_TWIM0_TWIS0_SPI0_TWI0_IRQHandler
    .word  SPIM1_SPIS1_TWIM1_TWIS1_SPI1_TWI1_IRQHandler
    .word  NFCT_IRQHandler
    .word  GPIOTE_IRQHandler
    .word  SAADC_IRQHandler
    .word  TIMER0_IRQHandler
    .word  TIMER1_IRQHandler
    .word  TIMER2_IRQHandler
    .word  RTC0_IRQHandler
    .word  TEMP_IRQHandler
    .word  RNG_IRQHandler
    .word  ECB_IRQHandler
    .word  CCM_AAR_IRQHandler
    .word  WDT_IRQHandler
    .word  RTC1_IRQHandler
    .word  QDEC_IRQHandler
    .word  COMP_LPCOMP_IRQHandler
    .word  SWI0_EGU0_IRQHandler
    .word  SWI1_EGU1_IRQHandler
    .word  SWI2_EGU2_IRQHandler
    .word  SWI3_EGU3_IRQHandler
    .word  SWI4_EGU4_IRQHandler
    .word  SWI5_EGU5_IRQHandler
    .word  TIMER3_IRQHandler
    .word  TIMER4_IRQHandler
    .word  PWM0_IRQHandler
    .word  PDM_IRQHandler
    .word  0                        /* Reserved */
    .word  0                        /* Reserved */
    .word  MWU_IRQHandler
    .word  PWM1_IRQHandler
    .word  PWM2_IRQHandler
    .word  SPIM2_SPIS2_SPI2_IRQHandler
    .word  RTC2_IRQHandler
    .word  I2S_IRQHandler
    .word  FPU_IRQHandler
    .word  USBD_IRQHandler
    .word  UARTE1_IRQHandler
    .word  QSPI_IRQHandler
    .word  CRYPTOCELL_IRQHandler
    .word  0                        /* Reserved */
    .word  0                        /* Reserved */
    .word  PWM3_IRQHandler
    .word  0                        /* Reserved */
    .word  SPIM3_IRQHandler

    .size  g_pfnVectors, .-g_pfnVectors

/* -------------------------------------------------------------------------
 * Reset_Handler
 * ------------------------------------------------------------------------- */
    .section .text.Reset_Handler
    .weak  Reset_Handler
    .type  Reset_Handler, %function

Reset_Handler:
    /* Enable FPU: grant full access to CP10 and CP11 (Cortex-M4F TRM §4.6.3).
     * CPACR is at 0xE000ED88; bits [23:20] = 1111b. */
    ldr   r0, =0xE000ED88
    ldr   r1, [r0]
    orr   r1, r1, #(0xF << 20)
    str   r1, [r0]
    dsb
    isb

    /* Copy .data from flash (LMA = _sidata) to RAM (VMA = _sdata.._edata) */
    ldr   r0, =_sdata
    ldr   r1, =_edata
    ldr   r2, =_sidata
    movs  r3, #0
    b     copy_data_loop

copy_data_body:
    ldr   r4, [r2, r3]
    str   r4, [r0, r3]
    adds  r3, r3, #4

copy_data_loop:
    adds  r4, r0, r3
    cmp   r4, r1
    bcc   copy_data_body

    /* Zero .bss */
    ldr   r2, =_sbss
    ldr   r4, =_ebss
    movs  r3, #0
    b     zero_bss_loop

zero_bss_body:
    str   r3, [r2]
    adds  r2, r2, #4

zero_bss_loop:
    cmp   r2, r4
    bcc   zero_bss_body

    /* Call C++ global constructors (.init_array) */
    bl    __libc_init_array

    /* Call main() */
    bl    main

    /* Trap if main returns */
    b     .
    .size Reset_Handler, .-Reset_Handler

/* -------------------------------------------------------------------------
 * Default_Handler — infinite loop, all unhandled IRQs land here
 * ------------------------------------------------------------------------- */
    .section .text.Default_Handler, "ax", %progbits

Default_Handler:
    b     Default_Handler
    .size Default_Handler, .-Default_Handler

/* -------------------------------------------------------------------------
 * Weak aliases — override any handler by defining it in C/C++
 * ------------------------------------------------------------------------- */
    .macro WEAK_IRQ name
    .weak  \name
    .thumb_set \name, Default_Handler
    .endm

    WEAK_IRQ NMI_Handler
    WEAK_IRQ HardFault_Handler
    WEAK_IRQ MemManage_Handler
    WEAK_IRQ BusFault_Handler
    WEAK_IRQ UsageFault_Handler
    WEAK_IRQ SVC_Handler
    WEAK_IRQ DebugMon_Handler
    WEAK_IRQ PendSV_Handler
    WEAK_IRQ SysTick_Handler

    WEAK_IRQ POWER_CLOCK_IRQHandler
    WEAK_IRQ RADIO_IRQHandler
    WEAK_IRQ UARTE0_UART0_IRQHandler
    WEAK_IRQ SPIM0_SPIS0_TWIM0_TWIS0_SPI0_TWI0_IRQHandler
    WEAK_IRQ SPIM1_SPIS1_TWIM1_TWIS1_SPI1_TWI1_IRQHandler
    WEAK_IRQ NFCT_IRQHandler
    WEAK_IRQ GPIOTE_IRQHandler
    WEAK_IRQ SAADC_IRQHandler
    WEAK_IRQ TIMER0_IRQHandler
    WEAK_IRQ TIMER1_IRQHandler
    WEAK_IRQ TIMER2_IRQHandler
    WEAK_IRQ RTC0_IRQHandler
    WEAK_IRQ TEMP_IRQHandler
    WEAK_IRQ RNG_IRQHandler
    WEAK_IRQ ECB_IRQHandler
    WEAK_IRQ CCM_AAR_IRQHandler
    WEAK_IRQ WDT_IRQHandler
    WEAK_IRQ RTC1_IRQHandler
    WEAK_IRQ QDEC_IRQHandler
    WEAK_IRQ COMP_LPCOMP_IRQHandler
    WEAK_IRQ SWI0_EGU0_IRQHandler
    WEAK_IRQ SWI1_EGU1_IRQHandler
    WEAK_IRQ SWI2_EGU2_IRQHandler
    WEAK_IRQ SWI3_EGU3_IRQHandler
    WEAK_IRQ SWI4_EGU4_IRQHandler
    WEAK_IRQ SWI5_EGU5_IRQHandler
    WEAK_IRQ TIMER3_IRQHandler
    WEAK_IRQ TIMER4_IRQHandler
    WEAK_IRQ PWM0_IRQHandler
    WEAK_IRQ PDM_IRQHandler
    WEAK_IRQ MWU_IRQHandler
    WEAK_IRQ PWM1_IRQHandler
    WEAK_IRQ PWM2_IRQHandler
    WEAK_IRQ SPIM2_SPIS2_SPI2_IRQHandler
    WEAK_IRQ RTC2_IRQHandler
    WEAK_IRQ I2S_IRQHandler
    WEAK_IRQ FPU_IRQHandler
    WEAK_IRQ USBD_IRQHandler
    WEAK_IRQ UARTE1_IRQHandler
    WEAK_IRQ QSPI_IRQHandler
    WEAK_IRQ CRYPTOCELL_IRQHandler
    WEAK_IRQ PWM3_IRQHandler
    WEAK_IRQ SPIM3_IRQHandler
