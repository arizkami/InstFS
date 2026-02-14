.section .note.GNU-stack,"",%progbits

/*
 * InstFS Header - Assembly bootstrap for instrument filesystem
 * This provides the initial filesystem structure embedded in the binary
 */

.section .rodata
.align 8

.global instfs_header_start
.global instfs_header_end
.global instfs_header_size

instfs_header_start:
    /* Magic number: "INSTFS" */
    .ascii "INSTFS\0\0"
    
    /* Version: 1.0 */
    .long 0x00010000
    
    /* Number of instruments */
    .word 0
    
    /* Offset to instrument table */
    .quad 56
    
    /* Reserved for future use */
    .quad 0, 0, 0, 0

instrument_table:
    /* Instrument entries will be added here */
    /* Format: [name_offset, data_offset, data_size, format] */
    
instfs_header_end:

.global instfs_header_size
instfs_header_size:
    .quad instfs_header_end - instfs_header_start
