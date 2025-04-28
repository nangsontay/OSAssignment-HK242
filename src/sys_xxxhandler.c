//
// Created by Mai Thiện Ngôn on 13/4/25.
//

#include "syscall.h"
#include "common.h"
#include "stdlib.h"

int __sys_xxxhandler(struct pcb_t* caller, struct sc_regs* reg)
{
    printf("sys_xxxhandler: %d\n", reg->a1);
    printf("tung tung tung os\n");
    printf("cappuchino ASSasinmentsystem\n");
    return 0;
}
