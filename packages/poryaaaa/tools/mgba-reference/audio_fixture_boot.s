    .syntax unified
    .cpu arm7tdmi

    .section .text, "ax", %progbits
    .thumb
    .align 2
    .global fixture_boot
    .type fixture_boot, %function

@ Replaces AgbMain with the smallest loop that runs the configured MP2K song.
fixture_boot:
    bx pc
    nop

    .arm
    .align 2

fixture_arm:
    ldr r3, =M4A_SOUND_INIT
    mov lr, pc
    bx r3

    ldr r0, =FIXTURE_SONG_ID
    ldr r3, =M4A_SONG_NUM_START
    mov lr, pc
    bx r3

    ldr r4, =0x04000006

fixture_frame:
    ldrh r0, [r4]
    cmp r0, #150
    bhs fixture_frame

fixture_wait_vcount:
    ldrh r0, [r4]
    cmp r0, #150
    blo fixture_wait_vcount

    ldr r3, =M4A_SOUND_VSYNC
    mov lr, pc
    bx r3

fixture_wait_vblank:
    ldrh r0, [r4]
    cmp r0, #160
    blo fixture_wait_vblank

    ldr r3, =M4A_SOUND_MAIN
    mov lr, pc
    bx r3
    b fixture_frame

    .ltorg
    .size fixture_boot, . - fixture_boot
