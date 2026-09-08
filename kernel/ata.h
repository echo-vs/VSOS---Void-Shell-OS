#ifndef ATA_H
#define ATA_H

/* ata.c — minimal ATA PIO driver, primary bus, SLAVE drive, LBA28.
 * Deliberately targets the slave, not the master: the master is
 * os-image.bin, the disk the real-mode bootloader reads via BIOS CHS
 * calls to load the kernel - growing or otherwise touching that disk
 * risks perturbing the CHS geometry translation it depends on. The
 * filesystem instead lives on fs.img, the second `-drive` QEMU attaches
 * as the primary slave (see the Makefile's `run` target), addressed here
 * purely via LBA so its size/geometry is irrelevant. Reads/writes are
 * polled (no IRQs) and timeout-bounded: if the controller never
 * responds, these return 0 instead of hanging forever, so callers can
 * fall back to RAM-only behavior instead of bricking the boot.
 */

/* reads/writes exactly one 512-byte sector. buf must point to at least
 * 512 bytes. returns 1 on success, 0 on timeout/error. */
int ata_read_sector(unsigned int lba, unsigned char *buf);
int ata_write_sector(unsigned int lba, const unsigned char *buf);

#endif
