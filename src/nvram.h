/**
 * @file nvram.h
 * @brief nvram access utility for powerpc platforms.
 *
 * Copyright (c) 2003, 2004 International Business Machines
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * @author Nathan Fontenot <nfont@linux.vnet.ibm.com>
 * @author Michael Strosaker <strosake@us.ibm.com>
 * @author Todd Inglett <tinglett@us.ibm.com>
 */

#ifndef _DEV_NVRAM_H_
#define _DEV_NVRAM_H_

#define NVRAM_SIG_SP	0x02	/**< support processor signature */
#define NVRAM_SIG_OF	0x50	/**< open firmware config signature */
#define NVRAM_SIG_FW	0x51	/**< general firmware signature */
#define NVRAM_SIG_HW	0x52	/**< hardware (VPD) signature */
#define NVRAM_SIG_SYS	0x70	/**< system env vars signature */
#define NVRAM_SIG_CFG	0x71	/**< config data signature */
#define NVRAM_SIG_ELOG	0x72	/**< error log signature */
#define NVRAM_SIG_VEND	0x7e	/**< vendor defined signature */
#define NVRAM_SIG_FREE	0x7f	/**< Free space signature */
#define NVRAM_SIG_OS	0xa0	/**< OS defined signature */

/**
 * @def printmap(ch)
 * @brief dertermines if 'ch' is a printable character
 */
#define printmap(ch)	(isgraph(ch) ? (ch) : '.')

#define NVRAM_BLOCK_SIZE	16
#define NVRAM_READ_SIZE		512
#define NVRAM_FILENAME1		"/dev/nvram"
#define NVRAM_FILENAME2		"/dev/misc/nvram"

#define DEFAULT_NVRAM_SZ	(1024 * 1024)
#define OOPS_PARTITION_SZ	4000

/**
 * @def MAX_CPUS
 * @brief maximum number of CPUS for errlog dumps
 */
#define MAX_CPUS 128

/**
 * @def MAX_PART_NAME
 * @brief maximum number of bytes in partition name
 */
#define MAX_PART_NAME 12

/**
 * @struct partition_header
 * @brief nvram partition header data
 */
struct partition_header {
    unsigned char	signature;		/**< partition signature */
    unsigned char 	checksum;		/**< partition checksum */
    unsigned short 	length;			/**< partition length */
    char 		name[MAX_PART_NAME];	/**< partition name */
};

/* sub-header for error-log partitions */
struct err_log_info {
    int			error_type;
    unsigned int	seq_num;
};

/* lnx,oops-log header */
struct oops_log_info {
   unsigned short	version;
   unsigned short	report_length;
   unsigned long long	timestamp;
}__attribute__((packed));

/* Internal representation of NVRAM. */
#define MAX_PARTITIONS 50
/**
 * @struct nvram
 * @brief internal representation of nvram data
 */
struct nvram {
    char 	*filename;		/**< original filename */
    int		fd;			/**< file descriptor */
    int 	nparts;			/**< number of partitions */
    int 	nbytes;			/**< size of data in bytes.  This 
                                         *   cannot be changed 
                                         *   (i.e. hardware size) 
					 */
    struct partition_header *parts[MAX_PARTITIONS]; 
                                        /**< partition header pointers 
                                         *   into data 
                                         */
    char	*data;                  /**< nvram contents */
};

/**
 * @var descs
 * @brief Array of VPD field names and descriptions
 */
static struct { char *name; char *desc; } descs[] = {
    {"PN", "Part Number"},
    {"FN", "FRU Number"},
    {"EC", "EC Level"},
    {"MN", "Manufacture ID"},
    {"SN", "Serial Number"},
    {"LI", "Load ID"},
    {"RL", "ROM Level"},
    {"RM", "Alterable ROM Level"},
    {"NA", "Network Address"},
    {"DD", "Device Driver Level"},
    {"DG", "Diagnostic Level"},
    {"LL", "Loadable Microcode Level"},
    {"VI", "Vendor ID/Device ID"},
    {"FU", "Function Number"},
    {"SI", "Subsystem Vendor ID/Device ID"},
    {"VK", "Platform"},			/**< "RS6K" => VPD is present */
    {"TM", "Model"},			/**< IBM specific? */
    {"YL", "Location Code"},		/**< IBM specific? */
    {"BR", "Brand"},			/**< IBM specific */
    {"CI", "CEC ID"},			/**< IBM specific */
    {"RD", "Rack ID"},			/**< IBM specific */
    {"PA", "Op Panel Installed"},	/**< IBM specific */
    {"NN", "Node Name"},		/**< IBM specific */
};

#endif
