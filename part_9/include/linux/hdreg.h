#ifndef _HDREG_H
#define _HDREG_H

#define HD_DATA         0x1f0
#define HD_ERROR        0x1f1
#define HD_NSECTOR      0x1f2
#define HD_SECTOR       0x1f3
#define HD_LCYL         0x1f4
#define HD_HCYL         0x1f5
#define HD_CURRENT      0x1f6
#define HD_STATUS       0x1f7
#define HD_PRECOMP HD_ERROR
#define HD_COMMAND HD_STATUS

#define HD_CMD          0x3f6

/* Bits of HD_STATUS */
#define ERR_STAT        0x01
#define INDEX_STAT      0x02
#define ECC_STAT        0x04
#define DRQ_STAT        0x08
#define SEEK_STAT       0x10
#define WRERR_STAT      0x20
#define READY_STAT      0x40
#define BUSY_STAT       0x80

#define WIN_RESTORE             0x10
#define WIN_READ                0x20
#define WIN_WRITE               0x30
#define WIN_VERIFY              0x40
#define WIN_FORMAT              0x50
#define WIN_INIT                0x60
#define WIN_SEEK                0x70
#define WIN_DIAGNOSE            0x90
#define WIN_SPECIFY             0x91
#define WIN_IDENTIFY            0xEC

struct partition {
    unsigned char boot_ind;
    unsigned char head;
    unsigned char sector;
    unsigned char cyl;
    unsigned char sys_ind;
    unsigned char end_head;
    unsigned char end_sector;
    unsigned char end_cyl;
    unsigned int start_sect;
    unsigned int nr_sects;
};

#endif

