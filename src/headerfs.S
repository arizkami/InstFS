; InstFS Header - Assembly bootstrap for instrument filesystem
; This provides the initial filesystem structure embedded in the binary

section .rodata
align 8

global instfs_header_start
global instfs_header_end
global instfs_header_size

instfs_header_start:
    ; Magic number: "INSTFS"
    db "INSTFS", 0x00, 0x00
    
    ; Version: 1.0 (0x00010000)
    dd 0x00010000
    
    ; Number of instruments (0)
    dw 0
    
    ; Offset to instrument table (56)
    dq 56
    
    ; Reserved for future use (0, 0, 0, 0)
    dq 0, 0, 0, 0

instrument_table:
    ; Instrument entries will be added here
    ; Format: [name_offset, data_offset, data_size, format]
    
instfs_header_end:

global instfs_header_size
instfs_header_size:
    dq instfs_header_end - instfs_header_start
