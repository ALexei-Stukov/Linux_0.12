.code16                             

.text                               

.globl _start_setup

_start_setup:
    movw    %cs,    %ax
    movw    %ax,    %dx
    movw    %ax,    %es

    movw    $setup_msg,    %ax            
    movw    %ax,    %bp             
    movw    $0x01301,   %ax         

    movw    $0x0c,  %bx             
    movw    $16,    %cx             
    movb    $9, %dl                 
    movb    $12, %dh                 
    int $0x010

setup_msg:                                
.ascii  "Setup is running"
                                    





