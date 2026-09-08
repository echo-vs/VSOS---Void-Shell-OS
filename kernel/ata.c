/* ata.c — minimal ATA PIO driver (primary bus, SLAVE drive, LBA28) —
 * see ata.h for why this targets the slave (fs.img) and not the master
 * (os-image.bin, the boot disk) */
#include "ata.h"
#include "io.h"

#define ATA_DATA       0x1F0
#define ATA_SECCOUNT   0x1F2
#define ATA_LBA_LO     0x1F3
#define ATA_LBA_MID    0x1F4
#define ATA_LBA_HI     0x1F5
#define ATA_DRIVE_HEAD 0x1F6
#define ATA_STATUS     0x1F7
#define ATA_COMMAND    0x1F7

#define ATA_CMD_READ   0x20
#define ATA_CMD_WRITE  0x30
#define ATA_CMD_FLUSH  0xE7

#define ATA_SR_ERR 0x01
#define ATA_SR_DRQ 0x08
#define ATA_SR_BSY 0x80

/* bounded poll: real/emulated PIO responds within microseconds, so this
 * only ever gets exhausted if there's no working controller at all. */
#define ATA_TIMEOUT 100000

static int ata_wait_bsy_clear(void) {
    for (int i = 0; i < ATA_TIMEOUT; i++) {
        if (!(inb(ATA_STATUS) & ATA_SR_BSY)) return 1;
    }
    return 0;
}

static int ata_wait_drq_set(void) {
    for (int i = 0; i < ATA_TIMEOUT; i++) {
        unsigned char st = inb(ATA_STATUS);
        if (st & ATA_SR_ERR) return 0;
        if (st & ATA_SR_DRQ) return 1;
    }
    return 0;
}

static void ata_select_lba(unsigned int lba) {
    /* 0xF0 = LBA mode + slave drive (bit4=1); 0xE0 would select the
     * master (the boot disk) instead - see the header comment. */
    outb(ATA_DRIVE_HEAD, (unsigned char) (0xF0 | ((lba >> 24) & 0x0F)));
    outb(ATA_SECCOUNT, 1);
    outb(ATA_LBA_LO, (unsigned char) (lba & 0xFF));
    outb(ATA_LBA_MID, (unsigned char) ((lba >> 8) & 0xFF));
    outb(ATA_LBA_HI, (unsigned char) ((lba >> 16) & 0xFF));
}

int ata_read_sector(unsigned int lba, unsigned char *buf) {
    if (!ata_wait_bsy_clear()) return 0;

    ata_select_lba(lba);
    outb(ATA_COMMAND, ATA_CMD_READ);

    if (!ata_wait_bsy_clear()) return 0;
    if (!ata_wait_drq_set()) return 0;

    unsigned short *p = (unsigned short *) buf;
    for (int i = 0; i < 256; i++) {
        p[i] = inw(ATA_DATA);
    }
    return 1;
}

int ata_write_sector(unsigned int lba, const unsigned char *buf) {
    if (!ata_wait_bsy_clear()) return 0;

    ata_select_lba(lba);
    outb(ATA_COMMAND, ATA_CMD_WRITE);

    if (!ata_wait_bsy_clear()) return 0;
    if (!ata_wait_drq_set()) return 0;

    const unsigned short *p = (const unsigned short *) buf;
    for (int i = 0; i < 256; i++) {
        outw(ATA_DATA, p[i]);
    }

    outb(ATA_COMMAND, ATA_CMD_FLUSH);
    ata_wait_bsy_clear();
    return 1;
}
