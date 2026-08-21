.syntax unified
.cpu arm7tdmi

.ifndef FIXTURE_BLOCK_COUNT
.equ FIXTURE_BLOCK_COUNT, 1
.endif
.ifndef FIXTURE_VOICE_COUNT
.equ FIXTURE_VOICE_COUNT, 1
.endif
.ifndef FIXTURE_MASTER_VOLUME
.equ FIXTURE_MASTER_VOLUME, 15
.endif
.ifndef FIXTURE_REVERB
.equ FIXTURE_REVERB, 0
.endif
.ifndef FIXTURE_RELEASE_BLOCK
.equ FIXTURE_RELEASE_BLOCK, 255
.endif
.ifndef FIXTURE_FLAGS
.equ FIXTURE_FLAGS, 1
.endif
.ifndef FIXTURE_STEP
.equ FIXTURE_STEP, 0x00800000
.endif
.ifndef FIXTURE_START_SEGMENT
.equ FIXTURE_START_SEGMENT, 0
.endif
.ifndef FIXTURE_START_OFFSET
.equ FIXTURE_START_OFFSET, 0
.endif
.ifndef FIXTURE_WAVE_TYPE
.equ FIXTURE_WAVE_TYPE, 0
.endif
.ifndef FIXTURE_WAVE_STATUS
.equ FIXTURE_WAVE_STATUS, 0
.endif
.ifndef FIXTURE_CHANNEL_MODE
.equ FIXTURE_CHANNEL_MODE, 0
.endif
.ifndef FIXTURE_VOLUME_RIGHT
.equ FIXTURE_VOLUME_RIGHT, 127
.endif
.ifndef FIXTURE_VOLUME_LEFT
.equ FIXTURE_VOLUME_LEFT, 127
.endif
.ifndef FIXTURE_ATTACK
.equ FIXTURE_ATTACK, 255
.endif
.ifndef FIXTURE_DECAY
.equ FIXTURE_DECAY, 255
.endif
.ifndef FIXTURE_SUSTAIN
.equ FIXTURE_SUSTAIN, 255
.endif
.ifndef FIXTURE_RELEASE
.equ FIXTURE_RELEASE, 255
.endif
.ifndef FIXTURE_ECHO_VOLUME
.equ FIXTURE_ECHO_VOLUME, 0
.endif
.ifndef FIXTURE_ECHO_LENGTH
.equ FIXTURE_ECHO_LENGTH, 0
.endif
.ifndef FIXTURE_LOOP_START
.equ FIXTURE_LOOP_START, 0
.endif
.ifndef FIXTURE_WAVE_LENGTH
.equ FIXTURE_WAVE_LENGTH, 1024
.endif
.ifndef FIXTURE_RING_RIGHT
.equ FIXTURE_RING_RIGHT, 0
.endif
.ifndef FIXTURE_RING_LEFT
.equ FIXTURE_RING_LEFT, 0
.endif
.ifndef FIXTURE_PCM_RATE_HZ
.equ FIXTURE_PCM_RATE_HZ, 5734
.endif
.ifndef FIXTURE_VOICE_VOLUME
.equ FIXTURE_VOICE_VOLUME, 127
.endif
.ifndef FIXTURE_PATTERN
.equ FIXTURE_PATTERN, 0
.endif

.equ SOUND_FIFO_A, 0x040000A0
.equ SOUND_FIFO_B, 0x040000A4
.equ SOUND_CNT_H, 0x04000082
.equ SOUND_CNT_X, 0x04000084
.equ SOUND_BIAS, 0x04000088
.equ TIMER0_DATA, 0x04000100
.equ TIMER0_CNT, 0x04000102
.equ VCOUNT, 0x04000006
.equ WORK, 0x02010000
.if FIXTURE_VOICE_COUNT > 12
.equ PCM, 0x02014000
.else
.equ PCM, WORK + 0x350
.endif
.equ STACK, 0x03007F00
.equ SCRATCH, 0x02010FFC
.equ FRAME, 0x60
.equ DMA_BUFFER_SIZE, 0x630
.equ LOGICAL_RING_SIZE, FRAME * 3
.equ CASE_HEADER_SIZE, 96
.equ CASE_FLAG_ROTATE_RING, 2

.section .text.header, "ax", %progbits
.arm
.align 2
.global fixture_entry
.type fixture_entry, %function
fixture_entry:
    b reset
    .byte 0x24, 0xFF, 0xAE, 0x51, 0x69, 0x9A, 0xA2, 0x21
    .byte 0x3D, 0x84, 0x82, 0x0A, 0x84, 0xE4, 0x09, 0xAD
    .byte 0x11, 0x24, 0x8B, 0x98, 0xC0, 0x81, 0x7F, 0x21
    .byte 0xA3, 0x52, 0xBE, 0x19, 0x93, 0x09, 0xCE, 0x20
    .byte 0x10, 0x46, 0x4A, 0x4A, 0xF8, 0x27, 0x31, 0xEC
    .byte 0x58, 0xC7, 0xE8, 0x33, 0x82, 0xE3, 0xCE, 0xBF
    .byte 0x85, 0xF4, 0xDF, 0x94, 0xCE, 0x4B, 0x09, 0xC1
    .byte 0x94, 0x56, 0x8A, 0xC0, 0x13, 0x72, 0xA7, 0xFC
    .byte 0x9F, 0x84, 0x4D, 0x73, 0xA3, 0xCA, 0x9A, 0x61
    .byte 0x58, 0x97, 0xA3, 0x27, 0xFC, 0x03, 0x98, 0x76
    .byte 0x23, 0x1D, 0xC7, 0x61, 0x03, 0x04, 0xAE, 0x56
    .byte 0xBF, 0x38, 0x84, 0x00, 0x40, 0xA7, 0x0E, 0xFD
    .byte 0xFF, 0x52, 0xFE, 0x03, 0x6F, 0x95, 0x30, 0xF1
    .byte 0x97, 0xFB, 0xC0, 0x85, 0x60, 0xD6, 0x80, 0x25
    .byte 0xA9, 0x63, 0xBE, 0x03, 0x01, 0x4E, 0x38, 0xE2
    .byte 0xF9, 0xA2, 0x34, 0xFF, 0xBB, 0x3E, 0x03, 0x44
    .byte 0x78, 0x00, 0x90, 0xCB, 0x88, 0x11, 0x3A, 0x94
    .byte 0x65, 0xC0, 0x7C, 0x63, 0x87, 0xF0, 0x3C, 0xAF
    .byte 0xD6, 0x25, 0xE4, 0x8B, 0x38, 0x0A, 0xAC, 0x72
    .byte 0x21, 0xD4, 0xF8, 0x07
    .ascii "SAPPYORACLE "
    .ascii "SPFX"
    .ascii "01"
    .byte 0x96, 0, 0
    .space 7
    .byte 0
    .byte 0x4C
    .byte 0, 0

.section .text.fixture, "ax", %progbits
.arm
.align 2
reset:
    mrs r0, cpsr
    orr r0, r0, #0xC0
    msr cpsr_c, r0
    ldr sp, =STACK
    ldr r12, =init_fixture + 1
    mov lr, pc
    bx r12

wait_vblank:
    ldr r0, =VCOUNT
1:
    ldrh r1, [r0]
    cmp r1, #160
    bne 1b
    ldr r12, =mix_once + 1
    mov lr, pc
    bx r12
2:
    ldrh r1, [r0]
    cmp r1, #160
    beq 2b
    b wait_vblank

.thumb
.thumb_func
mix_once:
    ldr r0, =WORK + 0x40
    ldr r1, [r0]
    cmp r1, #FIXTURE_BLOCK_COUNT
    bhs mix_return

    movs r2, #FIXTURE_RELEASE_BLOCK
    cmp r1, r2
    bne no_release
    ldr r3, =WORK + 0x50
    movs r2, #FIXTURE_VOICE_COUNT
release_loop:
    ldrb r4, [r3]
    movs r5, #0x40
    orrs r4, r5
    strb r4, [r3]
    adds r3, #0x40
    subs r2, #1
    bne release_loop
no_release:
    movs r2, #FIXTURE_START_SEGMENT
    movs r3, #CASE_FLAG_ROTATE_RING
    movs r4, #FIXTURE_FLAGS
    tst r4, r3
    beq segment_ready
    adds r2, r2, r1
segment_modulo:
    cmp r2, #3
    blo segment_ready
    subs r2, #3
    b segment_modulo
segment_ready:
    movs r3, #FRAME
    muls r3, r2
    ldr r5, =PCM
    adds r5, r5, r3
    ldr r0, =SCRATCH
    str r5, [r0]

    push {r0-r7, lr}
    sub sp, #0x1C
    movs r0, #FRAME
    str r0, [sp, #0x00]
    movs r0, #FIXTURE_VOICE_COUNT
    str r0, [sp, #0x04]
    str r5, [sp, #0x08]
    movs r0, #0
    str r0, [sp, #0x0C]
    str r0, [sp, #0x10]
    str r0, [sp, #0x14]
    ldr r0, =WORK
    str r0, [sp, #0x18]
    movs r4, #0
    cmp r2, #2
    bne mixer_registers_ready
    movs r4, #2
mixer_registers_ready:
    ldr r6, =DMA_BUFFER_SIZE
    movs r0, #FRAME
    mov r8, r0
    ldr r0, [sp, #0x3C]
    str r0, [sp, #0x40]
    ldr r0, =after_mixer + 1
    str r0, [sp, #0x3C]
    ldr r0, =WORK
    ldr r3, =SoundMainRAM
    mov lr, pc
    bx r3
after_mixer:
    ldr r0, [sp, #0x00]
    mov lr, r0
    ldr r5, =SCRATCH
    ldr r5, [r5]

    movs r0, r5
    ldr r2, =SOUND_FIFO_A
    movs r3, #(FRAME / 4)
write_fifo_a:
    ldr r1, [r0]
    str r1, [r2]
    adds r0, #4
    subs r3, #1
    bne write_fifo_a

    ldr r0, =DMA_BUFFER_SIZE
    adds r0, r0, r5
    ldr r2, =SOUND_FIFO_B
    movs r3, #(FRAME / 4)
write_fifo_b:
    ldr r1, [r0]
    str r1, [r2]
    adds r0, #4
    subs r3, #1
    bne write_fifo_b

    ldr r0, =WORK + 0x40
    ldr r1, [r0]
    adds r1, #1
    str r1, [r0]
    bx lr
mix_return:
    bx lr
    .pool

.macro INIT_CHANNEL index
    ldr r3, =WORK + 0x50 + (\index * 0x40)
    movs r1, #0x80
    strb r1, [r3, #0x00]
    movs r1, #FIXTURE_CHANNEL_MODE
    strb r1, [r3, #0x01]
    movs r1, #FIXTURE_VOLUME_RIGHT
    strb r1, [r3, #0x02]
    movs r1, #FIXTURE_VOLUME_LEFT
    strb r1, [r3, #0x03]
    movs r1, #FIXTURE_ATTACK
    strb r1, [r3, #0x04]
    movs r1, #FIXTURE_DECAY
    strb r1, [r3, #0x05]
    movs r1, #FIXTURE_SUSTAIN
    strb r1, [r3, #0x06]
    movs r1, #FIXTURE_RELEASE
    strb r1, [r3, #0x07]
    movs r1, #FIXTURE_ECHO_VOLUME
    strb r1, [r3, #0x0C]
    movs r1, #FIXTURE_ECHO_LENGTH
    strb r1, [r3, #0x0D]
    ldr r1, =FIXTURE_START_OFFSET
    str r1, [r3, #0x18]
    ldr r1, =FIXTURE_STEP
    str r1, [r3, #0x20]
    ldr r1, =fixture_wave
    str r1, [r3, #0x24]
.endm

.thumb_func
init_fixture:
    push {r4-r7, lr}
    ldr r0, =__mixer_load_start
    ldr r1, =__mixer_start
    ldr r2, =__mixer_end
copy_mixer:
    cmp r1, r2
    bhs mixer_copied
    ldrb r3, [r0]
    strb r3, [r1]
    adds r0, #1
    adds r1, #1
    b copy_mixer
mixer_copied:
    ldr r0, =__bss_start
    ldr r2, =__bss_end
    movs r1, #0
clear_bss:
    cmp r0, r2
    bhs bss_cleared
    strb r1, [r0]
    adds r0, #1
    b clear_bss
bss_cleared:
    ldr r0, =SOUND_BIAS
    ldr r1, =0x0200
    strh r1, [r0]
    ldr r0, =SOUND_CNT_X
    movs r1, #0x80
    strh r1, [r0]
    ldr r0, =SOUND_CNT_H
    ldr r1, =0x7700
    strh r1, [r0]
    ldr r0, =TIMER0_DATA
    ldr r1, =0xFF00
    strh r1, [r0]
    ldr r0, =TIMER0_CNT
    movs r1, #0x80
    strh r1, [r0]

    movs r1, #0
    ldr r0, =WORK
    ldr r2, =(0x500 / 4)
clear_work:
    stmia r0!, {r1}
    subs r2, #1
    bne clear_work
    ldr r0, =PCM
    ldr r2, =((DMA_BUFFER_SIZE * 2) / 4)
clear_pcm:
    stmia r0!, {r1}
    subs r2, #1
    bne clear_pcm

    ldr r0, =PCM
    ldr r1, =FIXTURE_RING_RIGHT
    ldr r2, =LOGICAL_RING_SIZE
seed_right:
    strb r1, [r0]
    adds r0, #1
    subs r2, #1
    bne seed_right
    ldr r0, =(PCM + DMA_BUFFER_SIZE)
    ldr r1, =FIXTURE_RING_LEFT
    ldr r2, =LOGICAL_RING_SIZE
seed_left:
    strb r1, [r0]
    adds r0, #1
    subs r2, #1
    bne seed_left

    ldr r0, =WORK
    movs r1, #FIXTURE_REVERB
    strb r1, [r0, #0x05]
    movs r1, #FIXTURE_VOICE_COUNT
    strb r1, [r0, #0x06]
    movs r1, #FIXTURE_MASTER_VOLUME
    strb r1, [r0, #0x07]
    movs r1, #1
    str r1, [r0, #0x18]

.if FIXTURE_VOICE_COUNT > 0
    INIT_CHANNEL 0
.endif
.if FIXTURE_VOICE_COUNT > 1
    INIT_CHANNEL 1
.endif
.if FIXTURE_VOICE_COUNT > 2
    INIT_CHANNEL 2
.endif
.if FIXTURE_VOICE_COUNT > 3
    INIT_CHANNEL 3
.endif
.if FIXTURE_VOICE_COUNT > 4
    INIT_CHANNEL 4
.endif
.if FIXTURE_VOICE_COUNT > 5
    INIT_CHANNEL 5
.endif
.if FIXTURE_VOICE_COUNT > 6
    INIT_CHANNEL 6
.endif
.if FIXTURE_VOICE_COUNT > 7
    INIT_CHANNEL 7
.endif
.if FIXTURE_VOICE_COUNT > 8
    INIT_CHANNEL 8
.endif
.if FIXTURE_VOICE_COUNT > 9
    INIT_CHANNEL 9
.endif
.if FIXTURE_VOICE_COUNT > 10
    INIT_CHANNEL 10
.endif
.if FIXTURE_VOICE_COUNT > 11
    INIT_CHANNEL 11
.endif
.if FIXTURE_VOICE_COUNT > 12
    INIT_CHANNEL 12
.endif
.if FIXTURE_VOICE_COUNT > 13
    INIT_CHANNEL 13
.endif
.if FIXTURE_VOICE_COUNT > 14
    INIT_CHANNEL 14
.endif

    ldr r0, [sp, #0x10]
    add sp, #0x14
    bx r0

.pool

.section .fixture_case, "a", %progbits
.align 2
.global fixture_case_descriptor
fixture_case_descriptor:
    .ascii "SPFD"
    .hword 1
    .hword CASE_HEADER_SIZE
    .word FRAME
    .word FIXTURE_BLOCK_COUNT
    .word FIXTURE_VOICE_COUNT
    .byte FIXTURE_MASTER_VOLUME
    .byte FIXTURE_REVERB
    .byte FIXTURE_RELEASE_BLOCK
    .byte FIXTURE_FLAGS
    .word FIXTURE_STEP
    .word LOGICAL_RING_SIZE
    .word FIXTURE_START_SEGMENT
    .word FIXTURE_START_OFFSET
    .byte FIXTURE_WAVE_TYPE
    .byte FIXTURE_WAVE_STATUS
    .byte FIXTURE_CHANNEL_MODE
    .byte FIXTURE_VOICE_COUNT
    .byte FIXTURE_VOLUME_RIGHT
    .byte FIXTURE_VOLUME_LEFT
    .byte FIXTURE_ATTACK
    .byte FIXTURE_DECAY
    .byte FIXTURE_SUSTAIN
    .byte FIXTURE_RELEASE
    .byte FIXTURE_ECHO_VOLUME
    .byte FIXTURE_ECHO_LENGTH
    .word FIXTURE_LOOP_START
    .word FIXTURE_WAVE_LENGTH
    .word fixture_wave_end - fixture_wave_data
    .word 0
    .byte FIXTURE_RING_RIGHT
    .byte FIXTURE_RING_LEFT
    .hword 0
    .word FIXTURE_PCM_RATE_HZ
    .word FIXTURE_VOICE_VOLUME
    .space CASE_HEADER_SIZE - (. - fixture_case_descriptor)

.align 2
fixture_wave:
    .byte FIXTURE_WAVE_TYPE, 0, 0, FIXTURE_WAVE_STATUS
    .word 0
    .word FIXTURE_LOOP_START
    .word FIXTURE_WAVE_LENGTH
fixture_wave_data:
.if FIXTURE_PATTERN == 0
    .rept 128
    .byte 127, 96, 64, 32, 0, -32, -64, -96
    .endr
.elseif FIXTURE_PATTERN == 1
    .rept 1024
    .byte 127
    .endr
.elseif FIXTURE_PATTERN == 2
    .rept 1024
    .byte -128
    .endr
.elseif FIXTURE_PATTERN == 3
    .rept 128
    .byte -120, -32, 48, 120, 96, 24, -48, -112
    .endr
.elseif FIXTURE_PATTERN == 4
    .byte 0
    .rept 32
    .byte 0x11
    .endr
    .byte 64
    .rept 32
    .byte 0xFF
    .endr
.if FIXTURE_WAVE_LENGTH > 128
    .byte -64
    .rept 32
    .byte 0
    .endr
.endif
.elseif FIXTURE_PATTERN == 5
    .rept 128
    .byte 7, 15, 31, 63, -7, -15, -31, -63
    .endr
.elseif FIXTURE_PATTERN == 6
    .byte 127
    .rept 1023
    .byte 0
    .endr
.else
    .error "unknown FIXTURE_PATTERN"
.endif
fixture_wave_end:
