.section .init
.global _start
_start:
    
    ldr r0, =0x3F200000
    ldr r1, =0x3F003000

    mov r2, #1
    lsl r2, #27
    str r2, [r0, #8]

    lsl r2, #2
    mov r5, #0  @old
    ldr r6, =1000000

    led_on:

        ldr r3, [r1, #4] @now
        add r8, r5, r6
        cmp r3, r8
        blo led_on

        str r2, [r0, #28]
        mov r5, r3

        noblink:

        ldr r3, [r1, #4] @now
        add r8, r5, r6
        cmp r3, r8
        blo noblink

        str r2, [r0, #40]
        mov r5, r3

        skip:

    b led_on
        
