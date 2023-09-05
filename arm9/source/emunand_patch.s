.section .emunand_patch, "aw", %progbits
.thumb
.align 4

@ Code originally by Normmatt

.global emunandPatch
emunandPatch:
    @ Original code that still needs to be executed
    mov r4, r0
    mov r5, r1
    mov r7, r2
    mov r6, r3
    @ End

    @ If we're already trying to access the SD, return
    ldr r2, [r4, #4]
    ldr r0, emunandPatchSdmmcStructPtr
    cmp r2, r0
    beq out

    ldr r2, [r4, #8] @ Get sector to read
    str r0, [r4, #4] @ Set object to be SD

    ldr r3, emunandPatchNandOffset
    add r2, r3 @ Add the offset to the NAND in the SD

    cmp r2, r3 @ For GW compatibility, see if we're trying to read the ncsd header (sector 0)
    bne skip_add

    ldr r3, emunandPatchNcsdHeaderOffset
    add r2, r3 @ If we're reading the ncsd header, add the offset of that sector

    skip_add:
        str r2, [r4, #8] @ Store sector to read
        
    out:

        @ Return 4 bytes behind where we got called,
        @ due to the offset of this function being stored there
        mov r2, lr
        add r2, #4

        @ More original code that might have been skipped depending on alignment;
        @ needs to be done at the end so CPSR is preserved
        lsl r0, r1, #0x17
        bx r2

.pool

.global emunandPatchSdmmcStructPtr
.global emunandPatchNandOffset
.global emunandPatchNcsdHeaderOffset
.balign 4

emunandPatchSdmmcStructPtr:     .word   0 @ Pointer to sdmmc struct
emunandPatchNandOffset:         .word   0 @ For rednand this should be 1
emunandPatchNcsdHeaderOffset:   .word   0 @ Depends on nand manufacturer + emunand type (GW/RED)

.pool
.balign 4

_emunandPatchEnd:

.global emunandPatchSize
emunandPatchSize:
    .word _emunandPatchEnd - emunandPatch
