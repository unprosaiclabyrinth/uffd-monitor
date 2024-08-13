SECTIONS
{
    . = 0x400000; /* Set starting address for the first segment */

    .text : {
        *(.text)
    }

    .log_access_page : {
        . = ALIGN(0x1000); /* Align to page boundary */
        *(.log_access_section)
        . = ALIGN(0x1000); /* Ensure the section ends on a page boundary */
    }

    .data : {
        *(.data)
    }
    
    .bss : {
        *(.bss)
    }

    /DISCARD/ : { *(.note.GNU-stack) }
}