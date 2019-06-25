/*
 * (C) Copyright 2006-2008
 * Stefan Roese, DENX Software Engineering, sr@denx.de.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <nand.h>
#include <asm/io.h>

#include "../../../board/nuvoton/nuc970/register.h"

void board_init_f(unsigned long bootflag);

extern int nuc970_nand_read_page_hwecc_oob_first(struct mtd_info *mtd, struct nand_chip *chip, uint8_t *buf, int oob_required, int page);
extern void nuc970_nand_command_lp(struct mtd_info *mtd, unsigned int command, int column, int page_addr);
extern unsigned char nuc970_nand_read_byte(struct mtd_info *mtd);
//#define printf(fmt, arg...) sysprintf(fmt, ##arg) //CWWeng add

//static int nand_ecc_pos[] = CONFIG_SYS_NAND_ECCPOS;

#ifndef CONFIG_SYS_NAND_BASE_LIST
#define CONFIG_SYS_NAND_BASE_LIST { CONFIG_SYS_NAND_BASE }
#endif

static struct nand_chip nand_chip[CONFIG_SYS_MAX_NAND_DEVICE];
static ulong base_address[CONFIG_SYS_MAX_NAND_DEVICE] = CONFIG_SYS_NAND_BASE_LIST;

#define ECCSTEPS	(CONFIG_SYS_NAND_PAGE_SIZE / \
					CONFIG_SYS_NAND_ECCSIZE)
#define ECCTOTAL	(ECCSTEPS * CONFIG_SYS_NAND_ECCBYTES)


static int nand_is_bad_block(struct mtd_info *mtd, int block)
{
	//struct nand_chip *this = mtd->priv;
	struct nand_chip *chip = mtd_to_nand(mtd);

	//nand_command(mtd, block, 0, CONFIG_SYS_NAND_BAD_BLOCK_POS, NAND_CMD_READOOB);
	int page_addr = 0 + block * CONFIG_SYS_NAND_PAGE_COUNT;
	
	//chip->cmdfunc(mtd, NAND_CMD_READOOB, 0 , page_addr );
	nuc970_nand_command_lp(mtd, NAND_CMD_READOOB, 0 , page_addr );

	//if(chip->read_byte(mtd)!=0xff)
	if(nuc970_nand_read_byte(mtd)!=0xff)
		return 1;
	return 0;

}

static int nand_read_page(struct mtd_info *mtd, int block, int page, uchar *dst)
{
	//struct nand_chip *this = mtd->priv;
	struct nand_chip *chip = mtd_to_nand(mtd);
	int real_page;

	real_page = block * (CONFIG_SYS_NAND_BLOCK_SIZE / CONFIG_SYS_NAND_PAGE_SIZE) + page;

	nuc970_nand_read_page_hwecc_oob_first(mtd, chip, dst, 0, real_page);
/*
	if (chip->ecc.read_page)
		chip->ecc.read_page(mtd, chip, dst, 0, real_page); //CWWeng : it calls nuc970_nand_read_page_hwecc_oob_first
*/
	return 0;
}

static int nand_load(struct mtd_info *mtd, unsigned int offs,
		     unsigned int uboot_size, uchar *dst)
{
	unsigned int block, lastblock;
	unsigned int page;

	/*
	 * offs has to be aligned to a page address!
	 */
	block = offs / CONFIG_SYS_NAND_BLOCK_SIZE;
	lastblock = (offs + uboot_size - 1) / CONFIG_SYS_NAND_BLOCK_SIZE;
	page = (offs % CONFIG_SYS_NAND_BLOCK_SIZE) / CONFIG_SYS_NAND_PAGE_SIZE;

	while (block <= lastblock) {
		if (!nand_is_bad_block(mtd, block)) {
			/*
			 * Skip bad blocks
			 */
			while (page < CONFIG_SYS_NAND_PAGE_COUNT) {
				nand_read_page(mtd, block, page, dst);
				dst += CONFIG_SYS_NAND_PAGE_SIZE;
				page++;
			}

			page = 0;
		} else {
			lastblock++;
		}

		block++;
	}

	return 0;
}

/*
 * The main entry for NAND booting. It's necessary that SDRAM is already
 * configured and available since this code loads the main U-Boot image
 * from NAND into SDRAM and starts it from there.
 */
//void nand_boot(void)
void board_init_f(unsigned long bootflag)
{
	struct nand_chip *nand = &nand_chip[0];
        struct mtd_info *mtd = nand_to_mtd(nand);
        ulong base_addr = base_address[0];
        int maxchips = CONFIG_SYS_NAND_MAX_CHIPS;
	__attribute__((noreturn)) void (*uboot)(void);


        if (maxchips < 1)
                maxchips = 1;

        nand->IO_ADDR_R = nand->IO_ADDR_W = (void  __iomem *)base_addr;

        if (board_nand_init(nand))
                return;

        if (nand_scan(mtd, maxchips))
                return;

        nand_register(0, mtd);
	
	/*
	 * Load U-Boot image from NAND into RAM
	 */
	nand_load(mtd, CONFIG_SYS_NAND_U_BOOT_OFFS, CONFIG_SYS_NAND_U_BOOT_SIZE,
		  (uchar *)CONFIG_SYS_NAND_U_BOOT_DST);

#ifdef CONFIG_NAND_ENV_DST
	nand_load(mtd, CONFIG_ENV_OFFSET, CONFIG_ENV_SIZE,
		  (uchar *)CONFIG_NAND_ENV_DST);

#ifdef CONFIG_ENV_OFFSET_REDUND
	nand_load(mtd, CONFIG_ENV_OFFSET_REDUND, CONFIG_ENV_SIZE,
		  (uchar *)CONFIG_NAND_ENV_DST + CONFIG_ENV_SIZE);
#endif
#endif

	if (nand->select_chip)
		nand->select_chip(mtd, -1);

	/*
	 * Jump to U-Boot image
	 */
	uboot = (void *)CONFIG_SYS_NAND_U_BOOT_START;

	(*uboot)();
}

/* Lowlevel init isn't used on nuc970, so just provide a dummy one here */
void lowlevel_init(void) {}

/*
 * Called in case of an exception.
 */
void hang(void)
{
        /* Loop forever */
        while (1) ;
}
