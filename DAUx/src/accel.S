; DAUx Accelerated Audio Processing
; SIMD-optimized audio operations

section .text

; Platform-specific calling conventions
%ifdef _WIN32
    ; Windows x64 calling convention
    %define ARG1 rcx
    %define ARG2 rdx
    %define ARG3 r8
    %define ARG4 r9
%else
    ; System V AMD64 ABI (Linux, BSD)
    %define ARG1 rdi
    %define ARG2 rsi
    %define ARG3 rdx
    %define ARG4 rcx
%endif

; ========================================
; Float32 Mixing (SIMD)
; void daux_mix_f32(float* dest, const float* src, size_t frames, float gain)
; ========================================

global daux_mix_f32
daux_mix_f32:
    ; ARG1 = dest
    ; ARG2 = src
    ; ARG3 = frames
    ; ARG4 = gain (in xmm0 on both platforms)
    
    test ARG3, ARG3
    jz .done
    
    ; Broadcast gain to all 4 floats in xmm1
    shufps xmm0, xmm0, 0
    movaps xmm1, xmm0
    
    mov rax, ARG3
    shr rax, 2              ; Process 4 floats at a time
    jz .remainder
    
.loop:
    movups xmm2, [ARG2]     ; Load 4 source floats
    mulps xmm2, xmm1        ; Multiply by gain
    movups xmm3, [ARG1]     ; Load 4 dest floats
    addps xmm3, xmm2        ; Add to dest
    movups [ARG1], xmm3     ; Store result
    
    add ARG1, 16
    add ARG2, 16
    dec rax
    jnz .loop
    
.remainder:
    and ARG3, 3
    jz .done
    
.remainder_loop:
    movss xmm2, [ARG2]
    mulss xmm2, xmm1
    movss xmm3, [ARG1]
    addss xmm3, xmm2
    movss [ARG1], xmm3
    
    add ARG1, 4
    add ARG2, 4
    dec ARG3
    jnz .remainder_loop
    
.done:
    ret

; ========================================
; Float32 Gain (SIMD)
; void daux_gain_f32(float* buffer, size_t frames, float gain)
; ========================================

global daux_gain_f32
daux_gain_f32:
    ; ARG1 = buffer
    ; ARG2 = frames
    ; ARG3 = gain (in xmm0)
    
    test ARG2, ARG2
    jz .done
    
    ; Broadcast gain
    shufps xmm0, xmm0, 0
    
    mov rax, ARG2
    shr rax, 2
    jz .remainder
    
.loop:
    movups xmm1, [ARG1]
    mulps xmm1, xmm0
    movups [ARG1], xmm1
    
    add ARG1, 16
    dec rax
    jnz .loop
    
.remainder:
    and ARG2, 3
    jz .done
    
.remainder_loop:
    movss xmm1, [ARG1]
    mulss xmm1, xmm0
    movss [ARG1], xmm1
    
    add ARG1, 4
    dec ARG2
    jnz .remainder_loop
    
.done:
    ret

; ========================================
; Float32 Copy (SIMD)
; void daux_copy_f32(float* dest, const float* src, size_t frames)
; ========================================

global daux_copy_f32
daux_copy_f32:
    ; ARG1 = dest
    ; ARG2 = src
    ; ARG3 = frames
    
    test ARG3, ARG3
    jz .done
    
    mov rax, ARG3
    shr rax, 2
    jz .remainder
    
.loop:
    movups xmm0, [ARG2]
    movups [ARG1], xmm0
    
    add ARG1, 16
    add ARG2, 16
    dec rax
    jnz .loop
    
.remainder:
    and ARG3, 3
    jz .done
    
.remainder_loop:
    movss xmm0, [ARG2]
    movss [ARG1], xmm0
    
    add ARG1, 4
    add ARG2, 4
    dec ARG3
    jnz .remainder_loop
    
.done:
    ret

; ========================================
; Float32 Clear (SIMD)
; void daux_clear_f32(float* buffer, size_t frames)
; ========================================

global daux_clear_f32
daux_clear_f32:
    ; ARG1 = buffer
    ; ARG2 = frames
    
    test ARG2, ARG2
    jz .done
    
    xorps xmm0, xmm0        ; Zero register
    
    mov rax, ARG2
    shr rax, 2
    jz .remainder
    
.loop:
    movups [ARG1], xmm0
    add ARG1, 16
    dec rax
    jnz .loop
    
.remainder:
    and ARG2, 3
    jz .done
    
.remainder_loop:
    movss [ARG1], xmm0
    add ARG1, 4
    dec ARG2
    jnz .remainder_loop
    
.done:
    ret
