/*
 * Copyright (C) 2017 Jagan Teki <jagan at amarulasolutions.com>
 * Copyright (C) 2016 Sergey Kubushyn <ksi at koi8.net>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef _MXS_NAND_
#define _MXS_NAND_

uint32_t mxs_nand_get_ecc_strength(uint32_t page_data_size,
				   uint32_t page_oob_size);
uint32_t mxs_nand_mark_byte_offset(struct mtd_info *mtd);
uint32_t mxs_nand_mark_bit_offset(struct mtd_info *mtd);

#endif	/* _MXS_NAND_ */