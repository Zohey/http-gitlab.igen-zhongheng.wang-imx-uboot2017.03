/*
 * i.MX nand boot control block(bcb).
 *
 * Based on the common/imx-bbu-nand-fcb.c from barebox and imx kobs-ng
 *
 * Copyright (C) 2017 Jagan Teki <jagan at amarulasolutions.com>
 * Copyright (C) 2016 Sergey Kubushyn <ksi at koi8.net>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <nand.h>

#include <asm/io.h>
#include <jffs2/jffs2.h>
#include <linux/mtd/mtd.h>

#include <asm/mach-imx/mxs-nand.h>
//#include <asm/mach-imx/imx-nandbcb.h>
#include <asm/mach-imx/BootControlBlocks.h>
#include <asm/mach-imx/imximage.cfg>

//#include "fcb2.c"

struct nfc_geometry {
	unsigned int  gf_len;
	unsigned int  ecc_strength;
	unsigned int  page_size_in_bytes;
	unsigned int  metadata_size_in_bytes;
	unsigned int  ecc_chunk0_size_in_bytes;
	unsigned int  ecc_chunkn_size_in_bytes;
	unsigned int  ecc_chunk_count;
	unsigned int  payload_size;
	unsigned int  auxiliary_size;
	unsigned int  auxiliary_status_offset;
	unsigned int  block_mark_byte_offset;
	unsigned int  block_mark_bit_offset;
	unsigned int  ecc_for_meta; /* separate ECC for meta data */

};

/*
NFC geometry :
		gf_len = 0x0000000d
		ecc_strength = 0x00000004
		page_size_in_bytes = 0x00000824
		metadata_size_in_bytes = 0x0000000a
		ecc_chunk0_size_in_bytes = 0x00000200
		ecc_chunkn_size_in_bytes = 0x00000200
		ecc_chunk_count = 0x00000004
		payload_size = 0x00000800
		auxiliary_size = 0x00000010
		auxiliary_status_offset = 0x0000000c
		block_mark_byte_offset = 0x000007e2
		block_mark_bit_offset = 0x00000004
		ecc_for_meta = 0x00000000
*/

struct nfc_geometry nfc_geo_tmp = {
	.gf_len = 0x0000000d,
	.ecc_strength = 0x00000004,
	.page_size_in_bytes = 0x00000824,
	.metadata_size_in_bytes = 0x0000000a,
	.ecc_chunk0_size_in_bytes = 0x00000200,
	.ecc_chunkn_size_in_bytes = 0x00000200,
	.ecc_chunk_count = 0x00000004,
	.payload_size = 0x00000800,
	.auxiliary_size = 0x00000010,
	.auxiliary_status_offset = 0x0000000c,
	.block_mark_byte_offset = 0x000007e2,
	.block_mark_bit_offset = 0x00000004,
	.ecc_for_meta = 0x00000000,
};

struct mtd_config {
	int chip_count;
	const char *chip_0_device_path;
	int chip_0_offset;
	int chip_0_size;
	const char *chip_1_device_path;
	int chip_1_offset;
	int chip_1_size;
	int search_exponent;
	int data_setup_time;
	int data_hold_time;
	int address_setup_time;
	int data_sample_time;
	int row_address_size;
	int column_address_size;
	int read_command_code1;
	int read_command_code2;
	int boot_stream_major_version;
	int boot_stream_minor_version;
	int boot_stream_sub_version;
	int ncb_version;
	int boot_stream_1_address;
	int boot_stream_2_address;

	/* for rom boot */
	int stride_size_in_bytes;
	int search_area_size_in_bytes;
	int search_area_size_in_pages;
};

struct mtd_config default_mtd_config = {
	.chip_count = 1,
	.chip_0_device_path = "/dev/mtd0",
	.chip_0_offset = 0,
	.chip_0_size = 0,
	.chip_1_device_path = NULL,
	.chip_1_offset = 0,
	.chip_1_size = 0,
	.search_exponent = 2,
	.data_setup_time = 80,
	.data_hold_time = 60,
	.address_setup_time = 25,
	.data_sample_time = 6,
	.row_address_size = 3,
	.column_address_size = 2,
	.read_command_code1 = 0x00,
	.read_command_code2 = 0x30,
	.boot_stream_major_version = 1,
	.boot_stream_minor_version = 0,
	.boot_stream_sub_version = 0,
	.ncb_version = 3,
	.boot_stream_1_address = 0,
	.boot_stream_2_address = 0,
};

#define BF_VAL(v, bf)		(((v) & bf##_MASK) >> bf##_OFFSET)
#define GETBIT(v, n)		(((v) >> (n)) & 0x1)

static u8 calculate_parity_13_8(u8 d)
{
	u8 p = 0;

	p |= (GETBIT(d, 6) ^ GETBIT(d, 5) ^ GETBIT(d, 3) ^ GETBIT(d, 2)) << 0;
	p |= (GETBIT(d, 7) ^ GETBIT(d, 5) ^ GETBIT(d, 4) ^ GETBIT(d, 2) ^ GETBIT(d, 1)) << 1;
	p |= (GETBIT(d, 7) ^ GETBIT(d, 6) ^ GETBIT(d, 5) ^ GETBIT(d, 1) ^ GETBIT(d, 0)) << 2;
	p |= (GETBIT(d, 7) ^ GETBIT(d, 4) ^ GETBIT(d, 3) ^ GETBIT(d, 0)) << 3;
	p |= (GETBIT(d, 6) ^ GETBIT(d, 4) ^ GETBIT(d, 3) ^ GETBIT(d, 2) ^ GETBIT(d, 1) ^ GETBIT(d, 0)) << 4;

	return p;
}

static void encode_hamming_13_8(void *_src, void *_ecc, size_t size)
{
	int i;
	u8 *src = _src;
	u8 *ecc = _ecc;

	for (i = 0; i < size; i++)
		ecc[i] = calculate_parity_13_8(src[i]);
}

static u32 calc_chksum(void *buf, size_t size)
{
	u32 chksum = 0;
	u8 *bp = buf;
	size_t i;

	for (i = 0; i < size; i++)
		chksum += bp[i];

	return ~chksum;
}

static int dbbt_fill_data(struct mtd_info *mtd, void *buf, int num_blocks)
{
	int n, n_bad_blocks = 0;
	u32 *bb = buf + 0x8;
	u32 *n_bad_blocksp = buf + 0x4;

	for (n = 0; n < num_blocks; n++) {
		loff_t offset = n * mtd->erasesize;
			if (mtd_block_isbad(mtd, offset)) {
				n_bad_blocks++;
				*bb = n;
				bb++;
		}
	}

	*n_bad_blocksp = n_bad_blocks;

	return n_bad_blocks;
}


/*
 * reverse bit for byte
 */
static uint8_t reverse_bit(uint8_t in_byte)
{
	int i;
	uint8_t out_byte = 0;

	for (i = 0; i < 8; i++) {
		if (in_byte & ((0x80) >> i)) {
			out_byte |= 1 << i;
		}
	}

	return out_byte;
}

static int encode_hamming_code_13_8(void *source_block, size_t source_size,
			       void *target_block, size_t target_size)
{
	////////////////////////////////////////////////////////////////////////////////
	// Definitions
	////////////////////////////////////////////////////////////////////////////////
	//!< Bytes per NCB data block
#define NAND_HC_ECC_SIZEOF_DATA_BLOCK_IN_BYTES        (512) 
	//! Size of a parity block in bytes for all 16-bit data blocks present inside one 512 byte NCB block.
#define NAND_HC_ECC_SIZEOF_PARITY_BLOCK_IN_BYTES      ((((512*8)/16)*6)/8) 
	//! Offset to first copy of NCB in a NAND page
#define NAND_HC_ECC_OFFSET_FIRST_DATA_COPY            (0) 
	//! Offset to second copy of NCB in a NAND page
#define NAND_HC_ECC_OFFSET_SECOND_DATA_COPY           (NAND_HC_ECC_OFFSET_FIRST_DATA_COPY+NAND_HC_ECC_SIZEOF_DATA_BLOCK_IN_BYTES) 
	//! Offset to third copy of NCB in a NAND page
#define NAND_HC_ECC_OFFSET_THIRD_DATA_COPY            (NAND_HC_ECC_OFFSET_SECOND_DATA_COPY+NAND_HC_ECC_SIZEOF_DATA_BLOCK_IN_BYTES)
	//! Offset to first copy of Parity block in a NAND page
#define NAND_HC_ECC_OFFSET_FIRST_PARITY_COPY          (NAND_HC_ECC_OFFSET_THIRD_DATA_COPY+NAND_HC_ECC_SIZEOF_DATA_BLOCK_IN_BYTES)
	//! Offset to second copy of Parity block in a NAND page
#define NAND_HC_ECC_OFFSET_SECOND_PARITY_COPY         (NAND_HC_ECC_OFFSET_FIRST_PARITY_COPY+NAND_HC_ECC_SIZEOF_PARITY_BLOCK_IN_BYTES)
	//! Offset to third copy of Parity block in a NAND page
#define NAND_HC_ECC_OFFSET_THIRD_PARITY_COPY          (NAND_HC_ECC_OFFSET_SECOND_PARITY_COPY+NAND_HC_ECC_SIZEOF_PARITY_BLOCK_IN_BYTES)
		
#define BITMASK_HAMMINGCHECKED_ALL_THREE_COPIES 0x7	//!< to indicate all three copies of NCB in first page are processed with Hamming codes.
#define BITMASK_HAMMINGCHECKED_FIRST_COPY       0x1	//!< to indicate first copy of NCB is processed with Hamming codes.
#define BITMASK_HAMMINGCHECKED_SECOND_COPY      0x2	//!< to indicate second copy of NCB is processed with Hamming codes.
#define BITMASK_HAMMINGCHECKED_THIRD_COPY       0x4	//!< to indicate third copy of NCB is processed with Hamming codes.

	uint8_t ecc[NAND_HC_ECC_SIZEOF_DATA_BLOCK_IN_BYTES];
	uint8_t data[NAND_HC_ECC_SIZEOF_DATA_BLOCK_IN_BYTES];
	int i;

	memset(ecc, 0, ARRAY_SIZE(ecc));
	memset(data, 0, ARRAY_SIZE(data));
	memcpy(data, source_block, source_size);

	for (i = 0; i < source_size; i ++)
		ecc[i] = calculate_parity_13_8(data[i]);

	memcpy((uint8_t*)target_block + BCB_MAGIC_OFFSET, data, NAND_HC_ECC_SIZEOF_DATA_BLOCK_IN_BYTES);
	memcpy((uint8_t*)target_block + BCB_MAGIC_OFFSET + NAND_HC_ECC_SIZEOF_DATA_BLOCK_IN_BYTES,
			ecc, NAND_HC_ECC_SIZEOF_DATA_BLOCK_IN_BYTES);

	return 0;
}

static int encode_bch_ecc(void *source_block, size_t source_size,
				   void *target_block, size_t target_size,
				   int version)
{

	struct bch_control *bch;
	uint8_t *ecc_buf;
	int ecc_buf_size;
	uint8_t *tmp_buf;
	int tmp_buf_size;
	int real_buf_size;
	int i, j;
	int ecc_bit_off;
	int data_ecc_blk_size;
	int low_byte_off, low_bit_off;
	int high_byte_off, high_bit_off;
	uint8_t byte_low, byte_high;

	/* define the variables for bch algorithm*/
	/* m:  METADATABYTE */
	/* b0: BLOCK0BYTE */
	/* e0: BLOCK0ECC */
	/* bn: BLOCKNBYTE */
	/* en: BLOCKNECC */
	/* n : NUMOFBLOCKN */
	/* gf: FCB_GF */
	int m, b0, e0, bn, en, n, gf;

	switch (version) {
		/* 62 bit BCH, for i.MX6SX and i.MX7D */
		case 2:
			m  = 32;
			b0 = 128;
			e0 = 62;
			bn = 128;
			en = 62;
			n  = 7;
			gf = 13;
			break;
		/* 40 bit BCH, for i.MX6UL */
		case 3:
			m  = 32;
			b0 = 128;
			e0 = 40;
			bn = 128;
			en = 40;
			n  = 7;
			gf = 13;
			break;
		default:
			printf("!!!!ERROR, bch version not defined\n");
			return -EINVAL;
			break;
	}

	/* sanity check */
	/* nand data block must be large enough for FCB structure */
	if (source_size > b0 + n * bn)
		return -EINVAL;
	/* nand page need to be large enough to contain Meta, FCB and ECC */
	if (target_size < m + b0 + e0*gf/8 + n*bn + n*en*gf/8)
		return -EINVAL;

	/* init bch, using default polynomial */
	bch = init_bch(gf, en, 0);
	if(!bch)
		return -EINVAL;

	/* buffer for ecc */
	ecc_buf_size = (gf * en + 7)/8;
	ecc_buf = malloc(ecc_buf_size);
	if(!ecc_buf)
		return -EINVAL;

	/* temp buffer to store data and ecc */
	tmp_buf_size = b0 + (e0 * gf + 7)/8 + (bn + (en * gf + 7)/8) * 7;
	tmp_buf = malloc(tmp_buf_size);
	if(!tmp_buf)
		return -EINVAL;
	memset(tmp_buf, 0, tmp_buf_size);

	/* generate ecc code for each data block and store in temp buffer */

	for (i = 0; i < n+1; i++) {
		memset(ecc_buf, 0, ecc_buf_size);
		encode_bch(bch, source_block + i * bn, bn, ecc_buf);

		memcpy(tmp_buf + i * (bn + ecc_buf_size), source_block + i * bn, bn);

		/* reverse ecc bit */
		for (j = 0; j < ecc_buf_size; j++) {
			ecc_buf[j] = reverse_bit(ecc_buf[j]);
		}

		memcpy(tmp_buf + (i+1)*bn + i*ecc_buf_size, ecc_buf, ecc_buf_size);
	}

	/* store Metadata for taget block with randomizer*/
	/*memcpy(target_block, RandData, m);*/
	memset(target_block, 0, m);

	/* shift the bit to combine the source data and ecc */
	real_buf_size = (b0*8 + gf*e0 + (bn*8 + gf*en)*n)/8;

	if (!((gf * en)%8)) {
		/* ecc data is byte aligned, just copy it. */
		memcpy(target_block + m, tmp_buf, real_buf_size);
	} else {
		/* bit offset for each ecc block */
		ecc_bit_off = 8 - (gf * en)%8;
		/* size of a data block plus ecc block */
		data_ecc_blk_size = bn +(gf*en+7)/8;

		for (i = 0; i < real_buf_size; i++) {
			low_bit_off = ((i/data_ecc_blk_size) * ecc_bit_off)%8;
			low_byte_off = ((i/data_ecc_blk_size) * ecc_bit_off)/8;
			high_bit_off = (((i+1)/data_ecc_blk_size) * ecc_bit_off)%8;
			high_byte_off = (((i+1)/data_ecc_blk_size) * ecc_bit_off)/8;

			byte_low = tmp_buf[i+low_byte_off] >> low_bit_off;
			byte_high = tmp_buf[i+1+high_byte_off] << (8 - high_bit_off);

			*(uint8_t *)(target_block + i + m) = (byte_low | byte_high);
		}
	}

	free(ecc_buf);
	free(tmp_buf);
	return 0;
}


/*
* copy_bits - copy bits from one memory region to another
* @dst: destination buffer
* @dst_bit_off: bit offset we're starting to write at
* @src: source buffer
* @src_bit_off: bit offset we're starting to read from
* @nbits: number of bits to copy
*
* This functions copies bits from one memory region to another, and is used by
* the GPMI driver to copy ECC sections which are not guaranteed to be byte
* aligned.
*
* src and dst should not overlap.
*
*/
void copy_bits(uint8_t *dst, size_t dst_bit_off,
		   uint8_t *src, size_t src_bit_off,
		   size_t nbits)
{
   size_t i;
   size_t nbytes;
   uint32_t src_buffer = 0;
   size_t bits_in_src_buffer = 0;

   if (!nbits)
	   return;

   /*
	* Move src and dst pointers to the closest byte pointer and store bit
	* offsets within a byte.
	*/
   src += src_bit_off / 8;
   src_bit_off %= 8;

   dst += dst_bit_off / 8;
   dst_bit_off %= 8;

   /*
	* Initialize the src_buffer value with bits available in the first
	* byte of data so that we end up with a byte aligned src pointer.
	*/
   if (src_bit_off) {
	   src_buffer = src[0] >> src_bit_off;
	   if (nbits >= (8 - src_bit_off)) {
		   bits_in_src_buffer += 8 - src_bit_off;
	   } else {
		   src_buffer &= GENMASK(nbits - 1, 0);
		   bits_in_src_buffer += nbits;
	   }
	   nbits -= bits_in_src_buffer;
	   src++;
   }

   /* Calculate the number of bytes that can be copied from src to dst. */
   nbytes = nbits / 8;

   /* Try to align dst to a byte boundary. */
   if (dst_bit_off) {
	   if (bits_in_src_buffer < (8 - dst_bit_off) && nbytes) {
		   src_buffer |= src[0] << bits_in_src_buffer;
		   bits_in_src_buffer += 8;
		   src++;
		   nbytes--;
	   }

	   if (bits_in_src_buffer >= (8 - dst_bit_off)) {
		   dst[0] &= GENMASK(dst_bit_off - 1, 0);
		   dst[0] |= src_buffer << dst_bit_off;
		   src_buffer >>= (8 - dst_bit_off);
		   bits_in_src_buffer -= (8 - dst_bit_off);
		   dst_bit_off = 0;
		   dst++;
		   if (bits_in_src_buffer > 7) {
			   bits_in_src_buffer -= 8;
			   dst[0] = src_buffer;
			   dst++;
			   src_buffer >>= 8;
		   }
	   }
   }

   if (!bits_in_src_buffer && !dst_bit_off) {
	   /*
		* Both src and dst pointers are byte aligned, thus we can
		* just use the optimized memcpy function.
		*/
	   if (nbytes)
		   memcpy(dst, src, nbytes);
   } else {
	   /*
		* src buffer is not byte aligned, hence we have to copy each
		* src byte to the src_buffer variable before extracting a byte
		* to store in dst.
		*/
	   for (i = 0; i < nbytes; i++) {
		   src_buffer |= src[i] << bits_in_src_buffer;
		   dst[i] = src_buffer;
		   src_buffer >>= 8;
	   }
   }
   /* Update dst and src pointers */
   dst += nbytes;
   src += nbytes;

   /*
	* nbits is the number of remaining bits. It should not exceed 8 as
	* we've already copied as much bytes as possible.
	*/
   nbits %= 8;

   /*
	* If there's no more bits to copy to the destination and src buffer
	* was already byte aligned, then we're done.
	*/
   if (!nbits && !bits_in_src_buffer)
	   return;

   /* Copy the remaining bits to src_buffer */
   if (nbits)
	   src_buffer |= (*src & GENMASK(nbits - 1, 0)) <<
				 bits_in_src_buffer;
   bits_in_src_buffer += nbits;

   /*
	* In case there were not enough bits to get a byte aligned dst buffer
	* prepare the src_buffer variable to match the dst organization (shift
	* src_buffer by dst_bit_off and retrieve the least significant bits
	* from dst).
	*/
   if (dst_bit_off)
	   src_buffer = (src_buffer << dst_bit_off) |
				(*dst & GENMASK(dst_bit_off - 1, 0));
   bits_in_src_buffer += dst_bit_off;

   /*
	* Keep most significant bits from dst if we end up with an unaligned
	* number of bits.
	*/
   nbytes = bits_in_src_buffer / 8;
   if (bits_in_src_buffer % 8) {
	   src_buffer |= (dst[nbytes] &
				  GENMASK(7, bits_in_src_buffer % 8)) <<
				 (nbytes * 8);
	   nbytes++;
   }

   /* Copy the remaining bytes to dst */
   for (i = 0; i < nbytes; i++) {
	   dst[i] = src_buffer;
	   src_buffer >>= 8;
   }
}


/*
* swap_block_mark - swap bbm
* @data_off: pointer to the data address
* @oob_off: pointer to the oob address
* @nfc_geo: nfc_geometry structure
* @wr_flag: 1 for write, 0 for read
*/

void swap_bad_block_mark(void *data, void *oob,
		   struct nfc_geometry* nfc_geo, int wr_flag)
{
/*
*		  The situation is a little bit complicate since the it is not
*		  symmetric swap behavior.
*
*		  Basic idea is swapping the data at block_mark_byte_offset,
*		  block_mark_bit_offset, denotes as dataX, with meta[0]. Since
*		  all related FCB data, including meta, FCB, parity check code,
*		  won't exceed NAND writesize, dataX is useless. But to protect
*		  the bad block mark, the correct behavior should be
*
*		  +----------+------------------------+------------------------+
*		  | 		 |			dataX		  | 		meta[0] 	   |
*		  +----------+------------------------+------------------------+
*		  |   WRITE  |		swap to meta[0]   | 	must set to 0xff   |
*		  +----------+------------------------+------------------------+
*		  |    READ  |			meta[0] 	  | 	must be 0xff	   |
*		  +----------+------------------------+------------------------+
*
*		  the original value of dataX doesn't matter, the only thing need
*		  to save/restore is meta[0]
*/

   int byte_off = nfc_geo->block_mark_byte_offset;
   int bit_off = nfc_geo->block_mark_bit_offset;
   uint8_t *data_off = data;
   uint8_t *oob_off = oob;

   if (wr_flag) {
	   data_off[byte_off] =
		   (data_off[byte_off] & GENMASK(bit_off - 1, 0)) |
		   (oob_off[0] << bit_off);
	   data_off[byte_off + 1] =
		   (data_off[byte_off + 1] << bit_off) |
		   (oob_off[0] & GENMASK(7, bit_off - 1) >> (8- bit_off));
	   oob_off[0] = 0xff;
   } else {
	   oob_off[0] =
		   ((data_off[byte_off] & GENMASK(7, bit_off - 1)) >> bit_off) |
		   ((data_off[byte_off + 1] & GENMASK(bit_off - 1, 0)) << (8 - bit_off));
   }
}

void process_data_for_raw_mode(struct mtd_info *mtd, int ecc, int new_raw_mode, char *fcb_raw, char *data, char *oobdata)
{
		
	struct nfc_geometry *nfc_geo = &nfc_geo_tmp;
	
	if ((!ecc) && new_raw_mode) {
		int i;
		int eccbits = nfc_geo->ecc_strength * nfc_geo->gf_len;
		int chunksize = nfc_geo->ecc_chunkn_size_in_bytes;
		int src_bit_off = 0;
		int oob_bit_off;
		int oob_bit_left;
		int ecc_chunk_count;


		/* copy meta first */
		memcpy(oobdata, fcb_raw, nfc_geo->metadata_size_in_bytes);
		src_bit_off += nfc_geo->metadata_size_in_bytes * 8;
		oob_bit_off = src_bit_off;
		ecc_chunk_count = nfc_geo->ecc_chunk_count;

		/* if bch requires dedicate ecc for meta */
		if (nfc_geo->ecc_for_meta) {
			copy_bits(oobdata, oob_bit_off,
				       fcb_raw, src_bit_off,
				       eccbits);
			src_bit_off += eccbits;
			oob_bit_off += eccbits;
			ecc_chunk_count = nfc_geo->ecc_chunk_count - 1;
		}

		/* copy others */
		for (i = 0; i < ecc_chunk_count; i++) {
			copy_bits(data, i * chunksize * 8,
				       fcb_raw, src_bit_off,
				       chunksize * 8);
			src_bit_off += chunksize * 8;
			copy_bits(oobdata, oob_bit_off,
				       fcb_raw, src_bit_off,
				       eccbits);
			src_bit_off += eccbits;
			oob_bit_off += eccbits;
		}

		oob_bit_left = (mtd->writesize + mtd->oobsize) * 8 - src_bit_off;
		if (oob_bit_left) {
			copy_bits(oobdata, oob_bit_off,
				       fcb_raw, src_bit_off,
				       oob_bit_left);
		}

		/* all gpmi controller need to do bi swap, may use flag to do this later */
		swap_bad_block_mark(data, oobdata, nfc_geo, 1);
	} else {
		memcpy(data, fcb_raw, mtd->writesize);
		memcpy(oobdata, fcb_raw + mtd->writesize, mtd->oobsize);
	}

}

/**
 * fcb_encrypt - Encrypt the FCB block, assuming that target system uses NCB
 * version 'version'
 *
 * fcb:     Points to valid imx28_BootBlockStruct_t structure.
 * target:  Points to a buffer large enough to contain an entire NAND Flash page
 *          (both data and OOB).
 * size:    The size of an entire NAND Flash page (both data and OOB).
 * version: The version number of the NCB.
 *
 */
static int fcb_encrypt(BCB_ROM_BootBlockStruct_t *fcb, void *target, size_t size, int version)
{
	uint32_t  accumulator;
	uint8_t   *p;
	uint8_t   *q;
	int fcb_size;

	//----------------------------------------------------------------------
	// Check for nonsense.
	//----------------------------------------------------------------------


	//----------------------------------------------------------------------
	// Clear out the target.
	//----------------------------------------------------------------------

	memset(target, 0, size);

	//----------------------------------------------------------------------
	// Compute the checksum.
	//
	// Note that we're computing the checksum only over the FCB itself,
	// whereas it's actually supposed to reflect the entire 508 bytes
	// in the FCB page between the base of the of FCB and the base of the
	// ECC bytes. However, the entire space between the top of the FCB and
	// the base of the ECC bytes will be all zeros, so this is OK.
	//----------------------------------------------------------------------

	p = ((uint8_t *) fcb) + 4;
	q = (uint8_t *) (fcb + 1);

	accumulator = 0;

	for (; p < q; p++) {
		accumulator += *p;
	}

	accumulator ^= 0xffffffff;

	fcb->m_u32Checksum = accumulator;

#define MAX_HAMMING_FCB_SZ 220
	fcb_size = MAX_HAMMING_FCB_SZ < sizeof(*fcb) ? MAX_HAMMING_FCB_SZ : sizeof(*fcb);

	//----------------------------------------------------------------------
	// Compute the ECC bytes.
	//----------------------------------------------------------------------

	switch (version)
	{
	case 0:
		memcpy(target, fcb, sizeof(*fcb));
		return size;
	case 1:
		return encode_hamming_code_13_8(fcb, fcb_size, target, size);
	case 2:
		return encode_bch_ecc(fcb, sizeof(*fcb), target, size, version);
	case 3:
		return encode_bch_ecc(fcb, sizeof(*fcb), target, size, version);
	default:
		fprintf(stderr, "FCB version == %d? Something is wrong!\n", version);
		return -EINVAL;
	}
}

#define PAGES_PER_STRIDE	64
#define ROM_MAX_BAD_BLOCKS	425	/* WTF? */

static void rom_boot_setting(struct mtd_info *mtd, struct mtd_config *cfg)
{
	cfg->stride_size_in_bytes = PAGES_PER_STRIDE * mtd->writesize;  // 0x20000
	cfg->search_area_size_in_bytes =
		(1 << cfg->search_exponent) * cfg->stride_size_in_bytes;    // 0x80000
	cfg->search_area_size_in_pages =
		(1 << cfg->search_exponent) * PAGES_PER_STRIDE;             // 0x100
}


static int fill_fcb(BCB_ROM_BootBlockStruct_t *fcb, struct mtd_info *mtd, unsigned int file_size)
{
	struct mtd_config *cfg   = &default_mtd_config;
	struct nfc_geometry *geo = &nfc_geo_tmp;
	unsigned int  max_boot_stream_size_in_bytes;
	unsigned int  boot_stream_size_in_bytes;
	unsigned int  boot_stream_size_in_pages;
	unsigned int  boot_stream1_pos;
	unsigned int  boot_stream2_pos;

	struct fcb_block *b      = &fcb->FCB_Block;
	FCB_ROM_NAND_Timing_t *t = &b->m_NANDTiming;

	/* Set up booting parameters */
	rom_boot_setting(mtd, cfg);

	if ((cfg->search_area_size_in_bytes * 2) > mtd->size) {
		printf("mtd: mtd size too small\n");
		return -1;
	}

	/*
	 * Figure out how large a boot stream the target MTD could possibly
	 * hold.
	 *
	 * The boot area will contain both search areas and two copies of the
	 * boot stream.
	 */
	max_boot_stream_size_in_bytes =
		(mtd->size - cfg->search_area_size_in_bytes * 2) / 2;

	/* Figure out how large the boot stream is. */
	boot_stream_size_in_bytes = file_size;

	boot_stream_size_in_pages =
		(boot_stream_size_in_bytes + (mtd->writesize - 1)) /
					mtd->writesize;
	/* Check if the boot stream will fit. */
	if (boot_stream_size_in_bytes >= max_boot_stream_size_in_bytes) {
		printf("mtd: bootstream too large\n");
		return -1;
	}

	/* Compute the positions of the boot stream copies. */
	boot_stream1_pos = 2 * cfg->search_area_size_in_bytes;  // 2 * 0x80000 = 0x100000
	boot_stream2_pos = 0x300000; // boot_stream1_pos + max_boot_stream_size_in_bytes;

	printf("mtd: max_boot_stream_size_in_bytes = %d\n"
		"mtd: boot_stream_size_in_bytes = %d\n"
		"mtd: boot_stream_size_in_pages = %d\n",
			max_boot_stream_size_in_bytes,
			boot_stream_size_in_bytes,
			boot_stream_size_in_pages);
	printf("mtd: #1 0x%08x - 0x%08x (0x%08x)\n"
		"mtd: #2 0x%08x - 0x%08x (0x%08x)\n",
			boot_stream1_pos,
			boot_stream1_pos + max_boot_stream_size_in_bytes,
			boot_stream1_pos + boot_stream_size_in_bytes,
			boot_stream2_pos,
			boot_stream2_pos + max_boot_stream_size_in_bytes,
			boot_stream2_pos + boot_stream_size_in_bytes);

	memset(fcb, 0, sizeof(*fcb));

	fcb->m_u32FingerPrint	= FCB_FINGERPRINT;
	fcb->m_u32Version	= FCB_VERSION_1;

	/* timing */
	t->m_u8DataSetup    = cfg->data_setup_time;
	t->m_u8DataHold     = cfg->data_hold_time;
	t->m_u8AddressSetup = cfg->address_setup_time;
	t->m_u8DSAMPLE_TIME = cfg->data_sample_time;

	/* fcb block */
	b->m_u32PageDataSize	= mtd->writesize;
	b->m_u32TotalPageSize	= mtd->writesize + mtd->oobsize;
	b->m_u32SectorsPerBlock	= mtd->erasesize / mtd->writesize;

	b->m_u32EccBlockNEccType = b->m_u32EccBlock0EccType =
					geo->ecc_strength >> 1;
	if (geo->ecc_for_meta)
		b->m_u32EccBlock0Size	= 0;
	else
		b->m_u32EccBlock0Size	= geo->ecc_chunk0_size_in_bytes;
	b->m_u32EccBlockNSize	= geo->ecc_chunkn_size_in_bytes;
	b->m_u32MetadataBytes	= geo->metadata_size_in_bytes;
	b->m_u32NumEccBlocksPerPage = geo->ecc_chunk_count - 1;

	b->m_u32Firmware1_startingPage = boot_stream1_pos / mtd->writesize;
	b->m_u32Firmware2_startingPage = boot_stream2_pos / mtd->writesize;
	b->m_u32PagesInFirmware1       = boot_stream_size_in_pages;
	b->m_u32PagesInFirmware2       = boot_stream_size_in_pages;

	b->m_u32DBBTSearchAreaStartAddress = cfg->search_area_size_in_pages;
	b->m_u32BadBlockMarkerByte     = geo->block_mark_byte_offset;
	b->m_u32BadBlockMarkerStartBit = geo->block_mark_bit_offset;
	b->m_u32BBMarkerPhysicalOffset = mtd->writesize;
	b->m_u32BCHType = geo->gf_len == 14 ? 1 : 0;

	return 0;
}



static int nandbcb_update(struct mtd_info *mtd, loff_t off, size_t size,
			  size_t maxsize, const u_char *buf)
{
	nand_erase_options_t opts;
	BCB_ROM_BootBlockStruct_t *fcb;
	struct dbbt_block *dbbt;
	loff_t fw1_off, fw2_off;
	void *fwbuf, *fcb_raw_page, *dbbt_page, *dbbt_data_page;
	int nr_blks, nr_blks_fcb, nr_blks_fw, fw1_blk, fw2_blk;
	size_t fwsize, dummy;
	int i, j, ret, len;
	char *tmp_buf;
	char *databuf;
	char *oobbuf;

	/* erase */
	memset(&opts, 0, sizeof(opts));
	opts.offset = off;
	opts.length = maxsize - 1;
	ret = nand_erase_opts(mtd, &opts);
	if (ret) {
		printf("%s: erase failed\n", __func__);
		return ret;
	}

	
	/* fill fcb */
	printf("fill fcb ...\n");
	fcb = kzalloc(sizeof(*fcb), GFP_KERNEL);
	fill_fcb(fcb, mtd, size + FLASH_OFFSET_STANDARD);
	//memcpy(fcb, g_data+32, sizeof(*fcb));

#undef P3
#define P3(x)	printf("  %s = 0x%08x\n", #x, fcb->x)
			printf("FCB\n");
			P3(m_u32Checksum);
			P3(m_u32FingerPrint);
			P3(m_u32Version);
#undef P3
#define P3(x)	printf("  %s = %d\n", #x, fcb->FCB_Block.x)
			P3(m_NANDTiming.m_u8DataSetup);
			P3(m_NANDTiming.m_u8DataHold);
			P3(m_NANDTiming.m_u8AddressSetup);
			P3(m_NANDTiming.m_u8DSAMPLE_TIME);
			P3(m_u32PageDataSize);
			P3(m_u32TotalPageSize);
			P3(m_u32SectorsPerBlock);
			P3(m_u32NumberOfNANDs);
			P3(m_u32TotalInternalDie);
			P3(m_u32CellType);
			P3(m_u32EccBlockNEccType);
			P3(m_u32EccBlock0Size);
			P3(m_u32EccBlockNSize);
			P3(m_u32EccBlock0EccType);
			P3(m_u32MetadataBytes);
			P3(m_u32NumEccBlocksPerPage);
			P3(m_u32EccBlockNEccLevelSDK);
			P3(m_u32EccBlock0SizeSDK);
			P3(m_u32EccBlockNSizeSDK);
			P3(m_u32EccBlock0EccLevelSDK);
			P3(m_u32NumEccBlocksPerPageSDK);
			P3(m_u32MetadataBytesSDK);
			P3(m_u32EraseThreshold);
			P3(m_u32Firmware1_startingPage);
			P3(m_u32Firmware2_startingPage);
			P3(m_u32PagesInFirmware1);
			P3(m_u32PagesInFirmware2);
			P3(m_u32DBBTSearchAreaStartAddress);
			P3(m_u32BadBlockMarkerByte);
			P3(m_u32BadBlockMarkerStartBit);
			P3(m_u32BBMarkerPhysicalOffset);
			P3(m_u32BCHType);
			P3(m_NANDTMTiming.m_u32TMTiming2_ReadLatency);
			P3(m_NANDTMTiming.m_u32TMTiming2_PreambleDelay);
			P3(m_NANDTMTiming.m_u32TMTiming2_CEDelay);
			P3(m_NANDTMTiming.m_u32TMTiming2_PostambleDelay);
			P3(m_NANDTMTiming.m_u32TMTiming2_CmdAddPause);
			P3(m_NANDTMTiming.m_u32TMTiming2_DataPause);
			P3(m_NANDTMTiming.m_u32TMSpeed);
			P3(m_NANDTMTiming.m_u32TMTiming1_BusyTimeout);
			P3(m_u32DISBBM);
			P3(m_u32BBMarkerPhysicalOffsetInSpareData);
	
		if(1) {
			P3(m_u32OnfiSyncEnable);
			P3(m_NANDONFITiming.m_u32ONFISpeed);
			P3(m_NANDONFITiming.m_u32ONFITiming_ReadLatency);
			P3(m_NANDONFITiming.m_u32ONFITiming_CEDelay);
			P3(m_NANDONFITiming.m_u32ONFITiming_PreambleDelay);
			P3(m_NANDONFITiming.m_u32ONFITiming_PostambleDelay);
			P3(m_NANDONFITiming.m_u32ONFITiming_CmdAddPause);
			P3(m_NANDONFITiming.m_u32ONFITiming_DataPause);
			P3(m_NANDONFITiming.m_u32ONFITiming_BusyTimeout);
			P3(m_u32DISBBSearch);
		}
	
		if(1) {
			P3(m_u32RandomizerEnable);
			P3(m_u32ReadRetryEnable);
			P3(m_u32ReadRetrySeqLength);
		}

	/* fill dbbt */
	printf("fill dbbt ...\n");
	dbbt_page = kzalloc(mtd->writesize, GFP_KERNEL);
	dbbt_data_page = kzalloc(mtd->writesize, GFP_KERNEL);
	dbbt = dbbt_page;
	dbbt->checksum = 0;
	dbbt->fingerprint = DBBT_FINGERPRINT2;
	dbbt->version = DBBT_VERSION_1;

	ret = dbbt_fill_data(mtd, dbbt_data_page, nr_blks);
	if (ret < 0)
		goto dbbt_err;
	else if (ret > 0)
		dbbt->dbbtnumofpages = 1;

	/*
	 * Reference documentation from i.MX6DQRM section 8.5.2.2
	 *
	 * Nand Boot Control Block(BCB) contains two data structures,
	 * - Firmware Configuration Block(FCB)
	 * - Discovered Bad Block Table(DBBT)
	 *
	 * FCB contains,
	 * - nand timings
	 * - DBBT search page address,
	 * - start page address of primary firmware
	 * - start page address of secondary firmware
	 *
	 * setup fcb:
	 * - number of blocks = mtd partition size / mtd erasesize
	 * - two firmware blocks, primary and secondary
	 * - first 4 block for FCB/DBBT
	 * - rest split in half for primary and secondary firmware
	 * - same firmware will write two times
	 */
	nr_blks_fcb = 4;

	/* write fw */
	fw1_off = fcb->FCB_Block.m_u32Firmware1_startingPage * mtd->writesize;
	fwsize  = fcb->FCB_Block.m_u32PagesInFirmware1 * mtd->writesize ;
	fwbuf = kzalloc(fwsize, GFP_KERNEL);
	memcpy(fwbuf + FLASH_OFFSET_STANDARD, buf, size);
	ret = nand_write_skip_bad(mtd, fw1_off, &fwsize, NULL, maxsize,
				  (u_char *)fwbuf, WITH_WR_VERIFY);
	printf("NAND fw write: 0x%llx offset, 0x%x bytes written: %s\n",
	       fw1_off, fwsize, ret ? "ERROR" : "OK");
	if (ret)
		goto fw_err;

	/* write fw, 2nd time */
	fw2_off = fcb->FCB_Block.m_u32Firmware2_startingPage * mtd->writesize;
	fwsize  = fcb->FCB_Block.m_u32PagesInFirmware2 * mtd->writesize ;
	ret = nand_write_skip_bad(mtd, fw2_off, &fwsize, NULL, maxsize,
				  (u_char *)fwbuf, WITH_WR_VERIFY);
	printf("NAND fw write: 0x%llx offset, 0x%x bytes written: %s\n",
	       fw2_off, fwsize, ret ? "ERROR" : "OK");
	if (ret)
		goto fw_err;

	/* write fcb/dbbt */
	fcb_raw_page = kzalloc(mtd->writesize + mtd->oobsize, GFP_KERNEL);
	databuf = kzalloc(10+mtd->writesize + mtd->oobsize, GFP_KERNEL);
	databuf += 10;
	oobbuf = databuf + mtd->writesize;
#if 0
	printf("before fcb_encrypt, len = %d:\n", sizeof(*fcb));
	tmp_buf = fcb;
	for (i = 0; i < sizeof(*fcb); i++)
	{
		printf("%02x ", tmp_buf[i]);
		if ((i+1)%16 == 0)
			printf("\n");
	}
#endif
	/* [1] Write the FCB search area. */
	len = mtd->writesize + mtd->oobsize;
	//printf("mtd->writesize = %d, mtd->oobsize = %d\n", mtd->writesize, mtd->oobsize);
	ret = fcb_encrypt(fcb, fcb_raw_page, len, 3);
	if (ret < 0)
		return ret;
#if 0
	printf("fcb_encrypt:\n");
	tmp_buf = fcb_raw_page;
	for (i = 0; i < len; i++)
	{
		printf("%02x ", tmp_buf[i]);
		if ((i+1)%16 == 0)
			printf("\n");
	}
#endif
	
	process_data_for_raw_mode(mtd, 0, 1, fcb_raw_page, databuf, oobbuf);

	/*
	 * Set the first and second byte of OOB data to 0xFF, not 0x00. These
	 * bytes are used as the Manufacturers Bad Block Marker (MBBM). Since
	 * the FCB is mostly written to the first page in a block, a scan for
	 * factory bad blocks will detect these blocks as bad, e.g. when
	 * function nand_scan_bbt() is executed to build a new bad block table.
	 */
	//memset(fcb_raw_page + mtd->writesize, 0xFF, 2);


	printf("NAND write fcb/dbbt: \n");
	for (i = 0; i < nr_blks_fcb; i++) {
		if (mtd_block_isbad(mtd, off)) {
			printf("Block %d is bad, skipped\n", i);
			continue;
		}

		/* raw write */
		mtd_oob_ops_t ops = {
			.datbuf = databuf - 10, // (u8 *)fcb_raw_page,
			.oobbuf = oobbuf, //((u8 *)fcb_raw_page) + mtd->writesize,
			.len = mtd->writesize,
			.ooblen = mtd->oobsize,
			.mode = MTD_OPS_RAW
		};
#if 0
		if (i == 0)
		{
			tmp_buf = databuf;
			printf("mtd write data:\n");
			for (j = 0; j < ops.len; j++)
			{
				printf("0x%02x, ", tmp_buf[j]);
				if ((j+1)%16 == 0)
					printf("\n");
			}

			tmp_buf = ops.oobbuf;
			printf("oobdata:\n");
			for (j = 0; j < ops.ooblen; j++)
			{
				printf("0x%02x, ", tmp_buf[j]);
				if ((j+1)%16 == 0)
					printf("\n");
			}
		}
#endif
		ret = mtd_write_oob(mtd, mtd->erasesize * i, &ops);
		if (ret) {
			printf("NAND fcb write: err %s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
			goto fcb_err;
		}
		printf("NAND fcb write: 0x%x offset, 0x%x bytes written: %s\n",
		      mtd->erasesize * i, ops.len, ret ? "ERROR" : "OK");

		ret = mtd_write(mtd, mtd->erasesize * i + mtd->writesize,
				mtd->writesize, &dummy, dbbt_page);
		if (ret) {
			printf("NAND fcb write: err %s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
			goto fcb_err;
		}
		printf("NAND dbbt write: 0x%x offset, 0x%x bytes written: %s\n",
		      mtd->erasesize * i + mtd->writesize, dummy,
		      ret ? "ERROR" : "OK");

		/* dbbtnumofpages == 0 if no bad blocks */
		if (dbbt->dbbtnumofpages > 0) {
			ret = mtd_write(mtd, mtd->erasesize * i + mtd->writesize * 5,
					mtd->writesize, &dummy, dbbt_data_page);
			if (ret) {
				printf("NAND bbt write: err %s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
				goto fcb_err;
			}
		}
	}

fcb_err:
	kfree(fcb_raw_page);
dbbt_err:
	kfree(dbbt_page);
	kfree(dbbt_data_page);
fw_err:
	kfree(fwbuf);

	return ret;
}

/* nandbcb update addr off|partition len 
 * nandbcb update ${fastboot_buffer} boot ${fastboot_bytes}
 */
static int do_nandbcb_update(int argc, char * const argv[])
{
	struct mtd_info *mtd;
	loff_t addr, offset, size, maxsize;
	char *endp;
	u_char *buf;
	int dev;
	int ret;

	if (argc != 4)
		return CMD_RET_USAGE;

	dev = nand_curr_device;
	if (dev < 0) {
		printf("failed to get nand_curr_device, run nand device");
		return CMD_RET_FAILURE;
	}

	addr = simple_strtoul(argv[1], &endp, 16);
	if (*argv[1] == 0 || *endp != 0)
		return CMD_RET_FAILURE;

	mtd = get_nand_dev_by_index(dev);
	if (mtd_arg_off_size(argc - 2, argv + 2, &dev, &offset, &size,
			     &maxsize, MTD_DEV_TYPE_NAND, mtd->size))
		return CMD_RET_FAILURE;

	buf = map_physmem(addr, size, MAP_WRBACK);
	if (!buf) {
		puts("failed to map physical memory\n");
		return CMD_RET_FAILURE;
	}

	ret = nandbcb_update(mtd, offset, size, maxsize, buf);

	return ret == 0 ? CMD_RET_SUCCESS : CMD_RET_FAILURE;
}

static int do_nandbcb(cmd_tbl_t *cmdtp, int flag, int argc,
		      char * const argv[])
{
	const char *cmd;
	int ret = 0;

	if (argc < 5)
		goto usage;

	cmd = argv[1];
	--argc;
	++argv;

	if (strcmp(cmd, "update") == 0) {
		ret = do_nandbcb_update(argc, argv);
		goto done;
	}

done:
	if (ret != -1)
		return ret;
usage:
	return CMD_RET_USAGE;
}

static char nandbcb_help_text[] =
	"update addr off|partition len	- update 'len' bytes starting at\n"
	"	'off|part' to memory address 'addr', skipping  bad blocks";

U_BOOT_CMD(
	nandbcb, 5, 1, do_nandbcb,
	"i.MX6 Nand BCB",
	nandbcb_help_text
);
