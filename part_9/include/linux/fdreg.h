#ifndef _FDREG_H
#define _FDREG_H

extern int ticks_to_floppy_on(unsigned int nr);
extern void floppy_on(unsigned int nr);
extern void floppy_off(unsigned int nr);
extern void floppy_select(unsigned int nr);
extern void floppy_deselect(unsigned int nr);

#define FD_STATUS       0x3f4
#define FD_DATA         0x3f5
#define FD_DOR          0x3f2
#define FD_DIR          0x3f7
#define FD_DCR          0x3f7

#define STATUS_BUSYMASK 0x0F
#define STATUS_BUSY     0x10
#define STATUS_DMA      0x20
#define STATUS_DIR      0x40
#define STATUS_READY    0x80

#define FD_RECALIBRATE  0x07
#define FD_SEEK         0x0F
#define FD_READ         0xE6
#define FD_WRITE        0xC5
#define FD_SENSEI       0x08
#define FD_SPECIFY      0x03

#define DMA_READ        0x46
#define DMA_WRITE       0x4A

#endif

