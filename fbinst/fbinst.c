#define _CRT_SECURE_NO_WARNINGS

#include "../libfbfs/fbfs.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#include <direct.h>
#include <fcntl.h>
#include <io.h>
#define mkdir_one(path)	_mkdir(path)
#else
#include <sys/stat.h>
#include <strings.h>
#include <unistd.h>
#define mkdir_one(path)	mkdir(path, 0777)
#endif

#if defined(_MSC_VER)
#define fbinst_strcasecmp	_stricmp
#define fbinst_strncasecmp	_strnicmp
#else
#define fbinst_strcasecmp	strcasecmp
#define fbinst_strncasecmp	strncasecmp
#endif

#if defined(_MSC_VER)
#define fb_strdup	_strdup
#else
#define fb_strdup	strdup
#endif

#define FBINST_VERSION		"1.7"
#define BUILD_NUMBER		1
#define MENU_BUFFER_SIZE	(FBFS_SECTOR_SIZE * 64u)

#define FB_MENU_FILE		"fb.cfg"

#define FBM_TYPE_MENU		1
#define FBM_TYPE_TEXT		2
#define FBM_TYPE_TIMEOUT	3
#define FBM_TYPE_DEFAULT	4
#define FBM_TYPE_COLOR		5

#define FBS_TYPE_MENU		1
#define FBS_TYPE_BULDR		2
#define FBS_TYPE_SYSLINUX	3
#define FBS_TYPE_LINUX		4
#define FBS_TYPE_MSDOS		5
#define FBS_TYPE_FREEDOS	6
#define FBS_TYPE_CHAIN		7
#define FBS_TYPE_GRLDR		8

#define COLOR_NORMAL		7

#pragma pack(push, 1)
struct fbm_text
{
	uint8_t size;
	uint8_t type;
	char title[1];
};

struct fbm_menu
{
	uint8_t size;
	uint8_t type;
	uint16_t key;
	uint8_t sys_type;
	char name[1];
};

struct fbm_timeout
{
	uint8_t size;
	uint8_t type;
	uint8_t timeout;
};
#pragma pack(pop)

struct key_entry
{
	uint16_t code[4];
	const char *name;
};

static const struct key_entry key_table[] =
{
	{{0x1e61, 0x1e41, 0x1e01, 0x1e00}, "A"},
	{{0x3062, 0x3042, 0x3002, 0x3000}, "B"},
	{{0x2e63, 0x2e43, 0x2e03, 0x2e00}, "C"},
	{{0x2064, 0x2044, 0x2004, 0x2000}, "D"},
	{{0x1265, 0x1245, 0x1205, 0x1200}, "E"},
	{{0x2166, 0x2146, 0x2106, 0x2100}, "F"},
	{{0x2267, 0x2247, 0x2207, 0x2200}, "G"},
	{{0x2368, 0x2348, 0x2308, 0x2300}, "H"},
	{{0x1769, 0x1749, 0x1709, 0x1700}, "I"},
	{{0x246a, 0x244a, 0x240a, 0x2400}, "J"},
	{{0x256b, 0x254b, 0x250b, 0x2500}, "K"},
	{{0x266c, 0x264c, 0x260c, 0x2600}, "L"},
	{{0x326d, 0x324d, 0x320d, 0x3200}, "M"},
	{{0x316e, 0x314e, 0x310e, 0x3100}, "N"},
	{{0x186f, 0x184f, 0x180f, 0x1800}, "O"},
	{{0x1970, 0x1950, 0x1910, 0x1900}, "P"},
	{{0x1071, 0x1051, 0x1011, 0x1000}, "Q"},
	{{0x1372, 0x1352, 0x1312, 0x1300}, "R"},
	{{0x1f73, 0x1f53, 0x1f13, 0x1f00}, "S"},
	{{0x1474, 0x1454, 0x1414, 0x1400}, "T"},
	{{0x1675, 0x1655, 0x1615, 0x1600}, "U"},
	{{0x2f76, 0x2f56, 0x2f16, 0x2f00}, "V"},
	{{0x1177, 0x1157, 0x1117, 0x1100}, "W"},
	{{0x2d78, 0x2d58, 0x2d18, 0x2d00}, "X"},
	{{0x1579, 0x1559, 0x1519, 0x1500}, "Y"},
	{{0x2c7a, 0x2c5a, 0x2c1a, 0x2c00}, "Z"},
	{{0x0231, 0x0221, 0x0000, 0x7800}, "1"},
	{{0x0332, 0x0340, 0x0300, 0x7900}, "2"},
	{{0x0433, 0x0423, 0x0000, 0x7a00}, "3"},
	{{0x0534, 0x0524, 0x0000, 0x7b00}, "4"},
	{{0x0635, 0x0625, 0x0000, 0x7c00}, "5"},
	{{0x0736, 0x075e, 0x071e, 0x7d00}, "6"},
	{{0x0837, 0x0826, 0x0000, 0x7e00}, "7"},
	{{0x0938, 0x092a, 0x0000, 0x7f00}, "8"},
	{{0x0a39, 0x0a28, 0x0000, 0x8000}, "9"},
	{{0x0b30, 0x0b29, 0x0000, 0x8100}, "0"},
	{{0x3920, 0x3920, 0x3920, 0x3920}, "SPACE"},
	{{0x011b, 0x011b, 0x011b, 0x0100}, "ESC"},
	{{0x1c0d, 0x1c0d, 0x1c0a, 0xa600}, "ENTER"},
	{{0x0f09, 0x0f00, 0x9400, 0xa500}, "TAB"},
	{{0x0e08, 0x0e08, 0x0e7f, 0x0e00}, "BACKSPACE"},
	{{0x4800, 0x4838, 0x8d00, 0x9800}, "UP"},
	{{0x5000, 0x5032, 0x9100, 0xa000}, "DOWN"},
	{{0x4b00, 0x4b34, 0x7300, 0x9b00}, "LEFT"},
	{{0x4d00, 0x4d36, 0x7400, 0x9d00}, "RIGHT"},
	{{0x4700, 0x4737, 0x7700, 0x9700}, "HOME"},
	{{0x4f00, 0x4f31, 0x7500, 0x9f00}, "END"},
	{{0x4900, 0x4939, 0x8400, 0x9900}, "PGUP"},
	{{0x5100, 0x5133, 0x7600, 0xa100}, "PGDN"},
	{{0x5200, 0x5230, 0x9200, 0xa200}, "INS"},
	{{0x5300, 0x532e, 0x9300, 0xa300}, "DEL"},
	{{0x3b00, 0x5400, 0x5e00, 0x6800}, "F1"},
	{{0x3c00, 0x5500, 0x5f00, 0x6900}, "F2"},
	{{0x3d00, 0x5600, 0x6000, 0x6a00}, "F3"},
	{{0x3e00, 0x5700, 0x6100, 0x6b00}, "F4"},
	{{0x3f00, 0x5800, 0x6200, 0x6c00}, "F5"},
	{{0x4000, 0x5900, 0x6300, 0x6d00}, "F6"},
	{{0x4100, 0x5a00, 0x6400, 0x6e00}, "F7"},
	{{0x4200, 0x5b00, 0x6500, 0x6f00}, "F8"},
	{{0x4300, 0x5c00, 0x6600, 0x7000}, "F9"},
	{{0x4400, 0x5d00, 0x6700, 0x7100}, "F10"},
	{{0x8500, 0x8700, 0x8900, 0x8b00}, "F11"},
	{{0x8600, 0x8800, 0x8a00, 0x8c00}, "F12"},
	{{0, 0, 0, 0}, NULL}
};

static const char *progname = "fbinst";
static int verbosity;
static int debug_boot;

static void
quit(const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "%s: error: ", progname);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	exit(1);
}

static void
check(int err)
{
	if (err != FBFS_OK)
		quit("%s", fbfs_last_error());
}

static void
info(const char *fmt, ...)
{
	va_list ap;

	if (!verbosity)
		return;

	fprintf(stderr, "%s: info: ", progname);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
}

static void
help(void)
{
	printf("Usage:\n"
		"\tfbinst [OPTIONS] DEVICE_OR_FILE COMMAND [PARAMETERS]\n\n"
		"Global Options:\n"
		"  --help,-h\t\tDisplay this message and exit\n"
		"  --version,-V\t\tPrint version information and exit\n"
		"  --list,-l\t\tList disks and exit\n"
		"  --verbose,-v\t\tPrint verbose messages\n"
		"  --debug,-d\t\tUse debug version of mbr\n\n"
		"Commands:\n"
		"  format\t\tFormat disk or image\n"
		"    --raw,-r\t\tFormat with normal FAT layout\n"
		"    --force,-f\t\tForce layout creation\n"
		"    --zip,-z\t\tFormat as USB-ZIP\n"
		"    --fat16|--fat32\tSelect FAT type for main partition\n"
		"    --ntfs\t\tFormat main partition as NTFS (Windows only, --force required)\n"
		"    --exfat\t\tFormat main partition as exFAT (Windows only, --force required)\n"
		"    --refs\t\tFormat main partition as ReFS (Windows only, --force required)\n"
		"    --align,-a\t\tAlign FAT data to cluster boundary\n"
		"    --nalign,-n NUM\tNAND alignment\n"
		"    --unit-size,-u NUM\tFAT unit size in sectors\n"
		"    --base,-b NUM\tSet base boot sector\n"
		"    --size,-s NUM\tSet size of data partition\n"
		"    --primary,-p NUM\tSet primary data size\n"
		"    --extended,-e NUM\tSet extended data size\n"
		"    --list-size,-l NUM\tSet file list size\n"
		"    --max-sectors NUM\tSet maximum sectors per read\n"
		"    --chs\t\tForce CHS mode\n"
		"    --archive FILE\tInitialize from archive\n"
		"    --fb-version 1.6|1.7\tSet created fbfs version\n"
		"    --mid SIZE[:TYPE]\tAdd middle FAT32/NTFS/exFAT/ReFS partition between main and tail\n"
		"                \t  SIZE: size with unit suffix (e.g. 300m, 1g)\n"
		"                \t  TYPE: fat32(default)|fat16|ntfs|exfat|refs\n"
		"    --mid-label LABEL\tSet volume label for middle partition\n"
		"    --tail SIZE[:TYPE]\tAdd tail FAT32/NTFS/exFAT/ReFS partition at disk end (SIZE can be 'max')\n"
		"                \t  SIZE: size with unit suffix (e.g. 300m, 1g)\n"
		"                \t  TYPE: fat32(default)|fat16|ntfs|exfat|refs\n"
		"    --label,-L LABEL\tSet volume label for main partition\n"
		"    --tail-label LABEL\tSet volume label for tail partition\n"
		"    --part[1-4] SIZE:TYPE[:LABEL]\tCreate raw MBR partition (no UD, e.g. 300m:fat32:EFI)\n"
		"    --active SLOT\tSet active boot partition slot (1-4) for Raw MBR mode\n"
		"  restore\t\tTry to restore fb mbr\n"
		"  update\t\tUpdate boot code when available\n"
		"  sync\t\t\tSynchronize disk information\n"
		"  info\t\t\tShow disk information\n"
		"  clear\t\t\tClear files\n"
		"  add NAME [FILE]\tAdd/update file item\n"
		"    --extended,-e\tStore in extended data area\n"
		"    --syslinux,-s\tPatch syslinux boot file\n"
		"  add-menu NAME FILE\tAdd/update menu file\n"
		"    --append,-a\t\tAppend to existing menu file\n"
		"    --string,-s\t\tMenu items are command arguments\n"
		"  resize NAME SIZE\tResize/create file item\n"
		"    --extended,-e\tStore in extended data area\n"
		"    --fill,-f NUM\tFill byte for expansion\n"
		"  copy OLD NEW\t\tCopy file item\n"
		"  move OLD NEW\t\tMove file item\n"
		"  export NAME [FILE]\tExport file item\n"
		"  remove NAME\t\tRemove file item\n"
		"  map NAME SLOT TYPE\tMap UD image file to MBR partition slot (1-4). TYPE: fat32|fat16|ntfs|exfat|refs|esp|0x0c|...\n"
		"  unmap SLOT\t\tUnmap partition slot (1-4)\n"
		"  format-file NAME TYPE\tFormat a UD image file as FAT16/FAT32/NTFS/exFAT/ReFS\n"
		"    --label,-L LABEL\tSet volume label for the file\n"
		"  cat NAME\t\tShow text file content\n"
		"  cat-menu NAME\t\tShow menu file content\n"
		"  pack\t\t\tPack free space\n"
		"  check\t\t\tCheck primary data marks\n"
		"  save FILE\t\tSave to archive file\n"
		"    --list-size,-l NUM\tSet archive file list size\n"
		"  load FILE\t\tLoad from archive file\n"
		"  create\t\tCreate archive file\n"
		"    --primary,-p NUM\tSet original primary data size\n"
		"    --extended,-e NUM\tSet original extended data size\n"
		"    --list-size,-l NUM\tSet file list size\n"
		"    --fb-version 1.6|1.7\tSet archive version\n"
		"  import-mbr FILE [SEC]\tImport custom MBR from a file (restores bootstrap code)\n"
		"  import-pbr SLOT FILE\tImport custom partition boot record (PBR) to SLOT (1-4)\n"
		"  write-mbr TYPE\tWrite high-compatibility built-in MBR (TYPE: nt60, nt52)\n"
		"  write-pbr SLOT TYPE\tWrite high-compatibility PBR to SLOT (1-4) (TYPE: bootmgr, ntldr)\n\n"
		"  BOOTICE Emulation Switches (Supports '/' to allow drop-in legacy script integration):\n"
		"    /device=DISKn[:part]\tSpecify target disk (supports '1', '(hd1)', '(hd:1)' formats) and slot\n"
		"    /mbr\t\tPerform MBR level operations\n"
		"    /pbr\t\tPerform PBR level operations\n"
		"    /install\t\tWrite built-in boot record (needs /type=)\n"
		"    /backup\t\tBackup boot record to file (needs /file=)\n"
		"    /restore\t\tImport boot record from file (needs /file=)\n"
		"    /type=TYPE\t\tBoot record type (nt60, nt52, bootmgr, ntldr)\n"
		"    /file=FILE\t\tSource backup binary file path\n"
		"    /sectors=NUM\tSectors to restore for MBR\n"
		"    /keep_bpb\t\tRetain original BIOS Parameter Block on restore\n\n"
		"    BOOTICE Emulation Examples:\n"
		"      # Install standard Windows NT 6.x MBR to Physical Disk 1:\n"
		"      fbinst /device=1 /mbr /install /type=nt60\n\n"
		"      # Backup MBR (1 sector) of Disk 1 to backup file:\n"
		"      fbinst /device=1 /mbr /backup /file=C:\\path\\to\\mbr_bak.bin /sectors=1\n\n"
		"      # Restore 1 sector MBR from binary backup file:\n"
		"      fbinst /device=1 /mbr /restore /file=C:\\path\\to\\mbr.bin /sectors=1\n\n"
		"      # Install BOOTMGR Partition Boot Record to Physical Disk 1, Partition index 0 (Slot 1):\n"
		"      fbinst /device=1:0 /pbr /install /type=bootmgr\n\n"
		"      # Backup Partition Boot Record of Disk 1 Slot 1 to backup file:\n"
		"      fbinst /device=1:0 /pbr /backup /file=C:\\path\\to\\pbr_bak.bin /sectors=1\n\n"
		"      # Restore Partition Boot Record from file, keeping BPB metadata intact:\n"
		"      fbinst /device=1:0 /pbr /restore /file=C:\\path\\to\\pbr.bin /keep_bpb\n\n"
		"Examples:\n"
		"  # Step 1: List all disks in the system and find your U-disk number\n"
		"  fbinst --list\n\n"
		"  # Step 2: Format an 8G U-disk with custom volume labels (Supports Multi-Partition!):\n"
		"  #   Layout: UD Hidden Boot Area (1400M) + Main NTFS Partition (Label: 'U_DISK') + Tail FAT32 Partition (Label: 'GRUB2')\n"
		"  fbinst (hdX) format --force --primary 8m --extended 1392m --ntfs --label \"U_DISK\" --tail 100m:fat32 --tail-label \"GRUB2\"\n\n"
		"  # Step 3: Create, map, and format a PE launch image file inside the UD hidden area:\n"
		"  #   A. Create a 600M empty contiguous file inside the UD hidden space\n"
		"  fbinst (hdX) resize -e \"fat32_pe.img\" 600m\n\n"
		"  #   B. Map \"fat32_pe.img\" to Partition Slot 3 as a FAT32 partition (supports string type aliases!)\n"
		"  fbinst (hdX) map \"fat32_pe.img\" 3 fat32\n\n"
		"  #   C. Directly format the mapped file as FAT32 and set volume label to 'EFI'\n"
		"  fbinst (hdX) format-file \"fat32_pe.img\" fat32 --label \"EFI\"\n\n"
		"  #   D. Unmap the partition slot to hide it from Windows (if needed)\n"
		"  fbinst (hdX) unmap 3\n\n"
		"  # Other Common Formats:\n"
		"  # Format as: UD boot + NTFS main (labeled as 'DATA') + NTFS tail (1G, labeled as 'BACKUP')\n"
		"  fbinst (hdX) format --force --ntfs -L \"DATA\" --tail 1g:ntfs --tail-label \"BACKUP\"\n\n"
		"  # Format as: UD boot + exFAT main (full remaining, no tail, labeled as 'EXFAT_DISK')\n"
		"  fbinst (hdX) format --force --exfat -L \"EXFAT_DISK\"\n\n"
		"  # Format a disk image file as FAT32 and set volume label\n"
		"  fbinst myimage.img format --force --fat32 --label \"DISK_IMG\"\n\n"
		"  # Add a file to an existing fbinst disk\n"
		"  fbinst (hdX) add myfile.bin C:\\path\\to\\myfile.bin\n\n"
		"  Partition layout after format with custom labels:\n"
		"  [MBR+fbinst bootloader]  [UD/fbfs data area]  [NTFS (Label: 'U_DISK')]  [FAT32 100M (Label: 'GRUB2')]\n"
		"  Sector 0 (63 sectors)    ^base                ^total                 ^disk_end-100M\n\n"
		"  # Non-UD Raw MBR Formatting (Natively creates standard MBR partition layout on disk):\n"
		"  #   Example: Format disk into standard dual-partition (Part1: 300M FAT32 'EFI', Part2: Remaining NTFS 'DATA')\n"
		"  #            and set Partition 1 active bootable:\n"
		"  fbinst (hdX) format --force --part1 300m:fat32:EFI --part2 max:ntfs:DATA --active 1\n\n");
}

static uint32_t
parse_size_arg(const char *arg)
{
	int ok;
	uint32_t value;

	value = fbfs_parse_size(arg, &ok);
	if (!ok)
		quit("invalid size %s", arg);
	return value;
}

static uint8_t
parse_fb_version(const char *arg)
{
	if (!strcmp(arg, "1.6"))
		return FBFS_VERSION_MINOR_16;
	if (!strcmp(arg, "1.7"))
		return FBFS_VERSION_MINOR_17;
	quit("invalid fb version %s", arg);
	return 0;
}

static void
need_arg(int argc, int i, const char *opt)
{
	if (i + 1 >= argc)
		quit("no parameter for %s", opt);
}

static void
list_devs(void)
{
	struct fbfs_disk_entry entries[32];
	size_t count;
	size_t i;

	check(fbfs_disk_list(entries, sizeof(entries) / sizeof(entries[0]), &count));
	for (i = 0; i < count && i < sizeof(entries) / sizeof(entries[0]); i++)
	{
		uint64_t sectors;
		uint64_t rounded;
		char unit;

		sectors = entries[i].sectors;
		if (sectors >= (3ull << 20))
		{
			rounded = (sectors + (1ull << 20)) >> 21;
			unit = 'g';
		}
		else
		{
			rounded = (sectors + (1ull << 10)) >> 11;
			unit = 'm';
		}

		printf("%s: %llu (%llu%c)%s\n", entries[i].name,
			(unsigned long long)sectors, (unsigned long long)rounded,
			unit, entries[i].has_fbinst ? " *" : "");
	}
}

static void
local_time32(uint32_t value, struct tm *out)
{
	time_t t;

	t = (time_t)value;
#if defined(_WIN32)
	localtime_s(out, &t);
#else
	{
		struct tm *tmp;

		tmp = localtime(&t);
		if (tmp)
			*out = *tmp;
		else
			memset(out, 0, sizeof(*out));
	}
#endif
}

static void
print_info(struct fbfs *fs)
{
	struct fbfs_info fs_info;
	size_t count;
	size_t i;
	uint32_t primary_used;
	uint32_t extended_used;
	uint32_t next;

	check(fbfs_get_info(fs, &fs_info));
	printf("version: %u.%u\n", fs_info.ver_major, fs_info.ver_minor);
	if (fs_info.archive_mode)
	{
		printf("file list size: %u\n", fs_info.list_sectors);
		printf("original primary data size: %u\n", fs_info.original_primary_size);
		printf("original extended data size: %u\n", fs_info.original_extended_size);
		printf("total sectors: %u\n", fs_info.archive_size);
	}
	else
	{
		printf("base boot sector: %u\n", fs_info.boot_base);
		printf("boot code size: %u\n", fs_info.boot_size);
		printf("primary data size: %u\n", fs_info.primary_size);
		printf("extended data size: %u\n", fs_info.extended_size);
	}

	printf("file list size: %u\n", fs_info.list_sectors);
	printf("file list used: %u\n", fs_info.list_used);
	printf("files:\n");

	count = fbfs_file_count(fs);
	primary_used = 0;
	extended_used = 0;
	next = fs_info.list_start + fs_info.list_sectors;
	for (i = 0; i < count; i++)
	{
		struct fbfs_file_info file;
		struct tm tm;
		uint32_t sectors;

		check(fbfs_get_file(fs, i, &file));
		sectors = file.extended_area ?
			((file.data_size + 511u) >> 9) :
			((file.data_size + 509u) / 510u);
		if (file.extended_area)
			extended_used += sectors;
		else
			primary_used += sectors;

		if (file.data_start != next)
		{
			if (next >= fs_info.primary_size || file.data_start <= fs_info.primary_size)
				printf("  %d*   0x%x 0x%x\n",
					next >= fs_info.primary_size, next,
					file.data_start - next);
			else
			{
				printf("  0*   0x%x 0x%x\n", next,
					fs_info.primary_size - next);
				printf("  1*   0x%x 0x%x\n", fs_info.primary_size,
					file.data_start - fs_info.primary_size);
			}
		}

		next = file.data_start + sectors;
		local_time32(file.data_time, &tm);
		printf("  %d%c%c  \"%s\" 0x%x %u "
			"(%d-%02d-%02d %02d:%02d:%02d)\n",
			file.extended_area,
			(file.flag & FBFS_FILE_EXTENDED) ? 'e' : ' ',
			(file.flag & FBFS_FILE_SYSLINUX) ? 's' : ' ',
			file.name, file.data_start, file.data_size,
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec);
	}

	if (!fs_info.archive_mode)
	{
		uint64_t free_ext;

		if (next != fs_info.total_size)
			printf("  1*   0x%x 0x%x\n", next, fs_info.total_size - next);
		printf("primary area free space: %u\n",
			(fs_info.primary_size - fs_info.list_start -
			fs_info.list_sectors - primary_used) * 510u);
		free_ext = fs_info.extended_size - extended_used;
		free_ext <<= 9;
		printf("extended area free space: %llu\n",
			(unsigned long long)free_ext);
	}
}

static uint8_t *
read_stdin(uint32_t *size)
{
	uint8_t temp[8192];
	uint8_t *buffer;
	size_t used;
	size_t capacity;

#if defined(_WIN32)
	_setmode(_fileno(stdin), _O_BINARY);
#endif

	buffer = NULL;
	used = 0;
	capacity = 0;
	for (;;)
	{
		size_t got;

		got = fread(temp, 1, sizeof(temp), stdin);
		if (!got)
			break;
		if (used + got > capacity)
		{
			size_t new_capacity;
			uint8_t *new_buffer;

			new_capacity = capacity ? capacity * 2u : 8192u;
			while (new_capacity < used + got)
				new_capacity *= 2u;
			new_buffer = realloc(buffer, new_capacity);
			if (!new_buffer)
			{
				free(buffer);
				quit("not enough memory");
			}
			buffer = new_buffer;
			capacity = new_capacity;
		}
		memcpy(buffer + used, temp, got);
		used += got;
	}

	if (used > UINT32_MAX)
	{
		free(buffer);
		quit("input too large");
	}

	*size = (uint32_t)used;
	return buffer;
}

static void
create_parent_dirs(char *path)
{
	char *p;

	for (p = path; *p; p++)
	{
		if (*p == '/' || *p == '\\')
		{
			char saved;

			saved = *p;
			*p = 0;
			if (path[0])
				(void)mkdir_one(path);
			*p = saved;
		}
	}
}

static char *
upcase(char *str)
{
	char *p;

	for (p = str; *p; p++)
		*p = (char)toupper((unsigned char)*p);
	return str;
}

static uint16_t
get_keycode(const char *key)
{
	char buffer[32];
	const char *name;
	int modifier;
	size_t i;

	if (!fbinst_strncasecmp(key, "0x", 2))
	{
		unsigned long code;

		code = strtoul(key, NULL, 0);
		return (code > 0 && code < 0xffffu) ? (uint16_t)code : 0;
	}

	snprintf(buffer, sizeof(buffer), "%s", key);
	upcase(buffer);
	name = buffer;
	modifier = 0;
	if (!strncmp(name, "SHIFT-", 6))
	{
		modifier = 1;
		name += 6;
	}
	else if (!strncmp(name, "CTRL-", 5))
	{
		modifier = 2;
		name += 5;
	}
	else if (!strncmp(name, "ALT-", 4))
	{
		modifier = 3;
		name += 4;
	}

	for (i = 0; key_table[i].name; i++)
	{
		if (!strcmp(name, key_table[i].name))
			return key_table[i].code[modifier];
	}

	return 0;
}

static const char *
get_keyname(uint16_t code)
{
	static char buffer[32];
	size_t i;

	for (i = 0; key_table[i].name; i++)
	{
		size_t j;

		for (j = 0; j < 4; j++)
		{
			if (key_table[i].code[j] == code)
			{
				const char *prefix;

				prefix = "";
				if (j == 1)
					prefix = "shift-";
				else if (j == 2)
					prefix = "ctrl-";
				else if (j == 3)
					prefix = "alt-";
				snprintf(buffer, sizeof(buffer), "%s%s", prefix,
					key_table[i].name);
				return buffer;
			}
		}
	}

	snprintf(buffer, sizeof(buffer), "0x%x", code);
	return buffer;
}

static const char *
clean_name(const char *name)
{
	while (*name == '/' || *name == '\\')
		name++;
	if (!fbinst_strncasecmp(name, "ud:/", 4))
		name += 4;
	while (*name == '/' || *name == '\\')
		name++;
	if (!*name)
		quit("empty file name");
	return name;
}

static int
parse_line(char *line, char ***args)
{
	char **values;
	int count;
	int capacity;

	values = NULL;
	count = 0;
	capacity = 0;
	while (*line)
	{
		char *p;

		while (*line == ' ' || *line == '\t' || *line == '\r' || *line == '\n')
			line++;
		if (!*line)
			break;

		p = line;
		if (*line == '"')
		{
			p++;
			line++;
			while (*line && *line != '"')
				line++;
		}
		else
		{
			while (*line && *line != ' ' && *line != '\t' &&
				*line != '\r' && *line != '\n')
				line++;
		}

		if (*line)
		{
			*line = 0;
			line++;
		}

		if (count == capacity)
		{
			char **new_values;

			capacity += 10;
			new_values = realloc(values, (size_t)capacity * sizeof(values[0]));
			if (!new_values)
				quit("not enough memory");
			values = new_values;
		}
		values[count++] = p;
	}

	*args = values;
	return count;
}

static int
append_menu_item(uint8_t *buffer, int offset, int size)
{
	if (size > 255 || offset + size >= MENU_BUFFER_SIZE - FBFS_SECTOR_SIZE)
		quit("menu item too long");
	return offset + size;
}

static int
add_item_menu(uint8_t *buffer, int offset, int argc, char **argv)
{
	struct fbm_menu *item;
	int size;
	int type;

	if (argc < 2)
		quit("not enough parameters");

	type = 0;
	size = (int)(sizeof(struct fbm_menu) - 1u);
	if (!strcmp(argv[1], "grldr") || !strcmp(argv[1], "syslinux") ||
		!strcmp(argv[1], "msdos") || !strcmp(argv[1], "freedos") ||
		!strcmp(argv[1], "chain") || !strcmp(argv[1], "buldr"))
	{
		if (argc < 3)
			quit("not enough parameters");
		switch (argv[1][0])
		{
		case 'g':
			type = FBS_TYPE_GRLDR;
			break;
		case 's':
			type = FBS_TYPE_SYSLINUX;
			break;
		case 'm':
			type = FBS_TYPE_MSDOS;
			break;
		case 'f':
			type = FBS_TYPE_FREEDOS;
			break;
		case 'c':
			type = FBS_TYPE_CHAIN;
			break;
		case 'b':
			type = FBS_TYPE_BULDR;
			break;
		default:
			quit("invalid system type %s", argv[1]);
		}
		size += (int)strlen(clean_name(argv[2])) + 1;
	}
	else if (!strcmp(argv[1], "linux"))
	{
		if (argc < 3)
			quit("not enough parameters");
		type = FBS_TYPE_LINUX;
		size += (int)strlen(clean_name(argv[2])) + 1;
		size += (argc >= 4) ? (int)strlen(argv[3]) + 1 : 1;
		size += (argc >= 5) ? (int)strlen(argv[4]) + 1 : 1;
	}
	else
		quit("invalid system type %s", argv[1]);

	offset = append_menu_item(buffer, offset, size);
	item = (struct fbm_menu *)(buffer + offset - size);
	memset(item, 0, (size_t)size);
	item->size = (uint8_t)(size - 2);
	item->type = FBM_TYPE_MENU;
	item->sys_type = (uint8_t)type;
	item->key = get_keycode(argv[0]);
	if (!item->key)
		quit("invalid hotkey %s", argv[0]);

	if (type == FBS_TYPE_LINUX)
	{
		char *p;

		p = item->name;
		strcpy(p, clean_name(argv[2]));
		p += strlen(p) + 1;
		if (argc >= 4)
			strcpy(p, argv[3]);
		p += strlen(p) + 1;
		if (argc >= 5)
			strcpy(p, argv[4]);
	}
	else
		strcpy(item->name, clean_name(argv[2]));

	return offset;
}

static int
add_item_text(uint8_t *buffer, int offset, int argc, char **argv)
{
	struct fbm_text *item;
	char *p;
	int has_newline;
	int size;
	int i;

	has_newline = 1;
	if (argc > 0 && !strcmp(argv[0], "-n"))
	{
		has_newline = 0;
		argc--;
		argv++;
	}

	size = (int)(sizeof(struct fbm_text) - 1u);
	for (i = 0; i < argc; i++)
		size += (int)strlen(argv[i]) + 1;
	if (has_newline)
		size += 2;

	offset = append_menu_item(buffer, offset, size);
	item = (struct fbm_text *)(buffer + offset - size);
	memset(item, 0, (size_t)size);
	item->size = (uint8_t)(size - 2);
	item->type = FBM_TYPE_TEXT;
	p = item->title;
	for (i = 0; i < argc; i++)
	{
		strcpy(p, argv[i]);
		p += strlen(argv[i]);
		*p++ = ' ';
	}
	if (argc)
		p--;
	if (has_newline)
	{
		*p++ = '\r';
		*p++ = '\n';
	}
	*p = 0;
	return offset;
}

static const char *color_list[16] =
{
	"black", "blue", "green", "cyan", "red", "magenta", "brown",
	"light-gray", "dark-gray", "light-blue", "light-green",
	"light-cyan", "light-red", "light-magenta", "yellow", "white"
};

static int
name_to_color(const char *name)
{
	int i;

	for (i = 0; i < 16; i++)
		if (!strcmp(name, color_list[i]))
			return i;
	return -1;
}

static int
get_color_value(const char *name)
{
	char buffer[64];
	char *slash;
	int fg;
	int bg;

	if (!strcmp(name, "normal"))
		return COLOR_NORMAL;

	snprintf(buffer, sizeof(buffer), "%s", name);
	slash = strchr(buffer, '/');
	if (slash)
	{
		*slash++ = 0;
		bg = name_to_color(slash);
		if (bg < 0)
			quit("invalid background color %s", slash);
	}
	else
		bg = 0;

	fg = name_to_color(buffer);
	if (fg < 0)
		quit("invalid foreground color %s", buffer);
	return (bg << 4) + fg;
}

static const char *
get_color_name(uint8_t color)
{
	static char buffer[32];
	int fg;
	int bg;

	if (color == COLOR_NORMAL)
		return "normal";

	fg = color & 0xf;
	bg = color >> 4;
	if (!bg)
		return color_list[fg];

	snprintf(buffer, sizeof(buffer), "%s/%s", color_list[fg], color_list[bg]);
	return buffer;
}

static int
add_item_timeout(uint8_t *buffer, int offset, int argc, char **argv, uint8_t type)
{
	struct fbm_timeout *item;
	int size;

	if (argc < 1)
		quit("not enough parameters");

	size = sizeof(struct fbm_timeout);
	offset = append_menu_item(buffer, offset, size);
	item = (struct fbm_timeout *)(buffer + offset - size);
	item->size = (uint8_t)(size - 2);
	item->type = type;
	item->timeout = (uint8_t)((type == FBM_TYPE_COLOR) ?
		get_color_value(argv[0]) : strtoul(argv[0], NULL, 0));
	return offset;
}

static int
add_menu_line(uint8_t *buffer, int offset, char *line)
{
	char **args;
	int count;

	if (line[0] == '#')
		return offset;

	count = parse_line(line, &args);
	if (!count)
	{
		free(args);
		return offset;
	}

	if (!strcmp(args[0], "menu"))
		offset = add_item_menu(buffer, offset, count - 1, args + 1);
	else if (!strcmp(args[0], "text"))
		offset = add_item_text(buffer, offset, count - 1, args + 1);
	else if (!strcmp(args[0], "timeout"))
		offset = add_item_timeout(buffer, offset, count - 1, args + 1,
			FBM_TYPE_TIMEOUT);
	else if (!strcmp(args[0], "default"))
		offset = add_item_timeout(buffer, offset, count - 1, args + 1,
			FBM_TYPE_DEFAULT);
	else if (!strcmp(args[0], "color"))
		offset = add_item_timeout(buffer, offset, count - 1, args + 1,
			FBM_TYPE_COLOR);
	else
		quit("unknown menu command");

	free(args);
	return offset;
}

static void
command_add_menu(struct fbfs *fs, int argc, char **argv)
{
	uint8_t *buffer;
	int append;
	int direct;
	int offset;
	int i;

	append = 0;
	direct = 0;
	for (i = 0; i < argc; i++)
	{
		if (argv[i][0] != '-')
			break;
		if (!strcmp(argv[i], "--append") || !strcmp(argv[i], "-a"))
			append = 1;
		else if (!strcmp(argv[i], "--string") || !strcmp(argv[i], "-s"))
			direct = 1;
		else
			quit("invalid option %s for add-menu", argv[i]);
	}

	argc -= i;
	argv += i;
	if (argc < 2)
		quit("not enough parameters");

	buffer = calloc(1, MENU_BUFFER_SIZE);
	if (!buffer)
		quit("not enough memory");

	offset = 0;
	if (append)
	{
		struct fbfs_file_info existing;
		uint32_t used;

		if (fbfs_find_file(fs, argv[0], &existing) == FBFS_OK)
		{
			check(fbfs_read_file(fs, argv[0], buffer, MENU_BUFFER_SIZE, &used));
			offset = (int)used - 1;
		}
	}

	if (direct)
	{
		for (i = 1; i < argc; i++)
			offset = add_menu_line(buffer, offset, argv[i]);
	}
	else
	{
		FILE *file;
		char line[512];

		file = fopen(argv[1], "r");
		if (!file)
			quit("can't open file %s", argv[1]);
		while (fgets(line, sizeof(line), file))
			offset = add_menu_line(buffer, offset, line);
		fclose(file);
	}

	buffer[offset] = 0;
	check(fbfs_add_buffer(fs, argv[0], buffer, (uint32_t)offset + 1u, 0));
	free(buffer);
}

static void
command_cat_menu(struct fbfs *fs, int argc, char **argv)
{
	struct fbfs_file_info file;
	uint8_t *buffer;
	uint32_t used;
	uint32_t offset;

	if (argc < 1)
		quit("not enough parameters");

	check(fbfs_find_file(fs, argv[0], &file));
	buffer = malloc(file.data_size + 1u);
	if (!buffer)
		quit("not enough memory");
	check(fbfs_read_file(fs, argv[0], buffer, file.data_size, &used));
	buffer[used] = 0;

	offset = 0;
	while (offset < used && buffer[offset])
	{
		switch (buffer[offset + 1])
		{
		case FBM_TYPE_MENU:
			{
				struct fbm_menu *item;

				item = (struct fbm_menu *)(buffer + offset);
				printf("menu %s ", get_keyname(item->key));
				switch (item->sys_type)
				{
				case FBS_TYPE_BULDR:
					printf("buldr \"%s\"\n", item->name);
					break;
				case FBS_TYPE_GRLDR:
					printf("grldr \"%s\"\n", item->name);
					break;
				case FBS_TYPE_SYSLINUX:
					printf("syslinux \"%s\"\n", item->name);
					break;
				case FBS_TYPE_LINUX:
					{
						char *p1;
						char *p2;

						p1 = item->name + strlen(item->name) + 1;
						p2 = p1 + strlen(p1) + 1;
						printf("linux \"%s\" \"%s\" \"%s\"\n",
							item->name, p1, p2);
					}
					break;
				case FBS_TYPE_MSDOS:
					printf("msdos \"%s\"\n", item->name);
					break;
				case FBS_TYPE_FREEDOS:
					printf("freedos \"%s\"\n", item->name);
					break;
				case FBS_TYPE_CHAIN:
					printf("chain \"%s\"\n", item->name);
					break;
				default:
					quit("invalid system type %d", item->sys_type);
				}
			}
			break;
		case FBM_TYPE_TEXT:
			{
				struct fbm_text *item;
				size_t len;
				int newline;

				item = (struct fbm_text *)(buffer + offset);
				len = strlen(item->title);
				newline = len >= 2 && item->title[len - 1] == '\n' &&
					item->title[len - 2] == '\r';
				if (newline)
					item->title[len - 2] = 0;
				printf("text %s\"%s\"\n", newline ? "" : "-n ", item->title);
			}
			break;
		case FBM_TYPE_TIMEOUT:
		case FBM_TYPE_DEFAULT:
			{
				struct fbm_timeout *item;
				int is_timeout;

				item = (struct fbm_timeout *)(buffer + offset);
				is_timeout = buffer[offset + 1] == FBM_TYPE_TIMEOUT;
				printf("%s %d\n", is_timeout ? "timeout" : "default",
					item->timeout);
			}
			break;
		case FBM_TYPE_COLOR:
			{
				struct fbm_timeout *item;

				item = (struct fbm_timeout *)(buffer + offset);
				printf("color %s\n", get_color_name(item->timeout));
			}
			break;
		default:
			quit("invalid menu type %d", buffer[offset + 1]);
		}

		offset += buffer[offset] + 2u;
		if (offset > used)
			quit("invalid menu");
	}

	free(buffer);
}

static void
parse_format_options(int argc, char **argv, struct fbfs_format_options *opt)
{
	int i;

	memset(opt, 0, sizeof(*opt));
	opt->primary_size = FBFS_MIN_PRI_SIZE;
	opt->list_size = FBFS_DEFAULT_LIST_SIZE;
	opt->base = FBFS_DEFAULT_BASE_SIZE;
	opt->fat_type = FBFS_FORMAT_FAT_AUTO;
	opt->nand_align = FBFS_MIN_NAND_ALIGN;
	opt->version_minor = FBFS_VERSION_MINOR_16;
	opt->tail_fat_type = FBFS_FORMAT_FAT32; /* default tail partition type */
	opt->mid_fat_type = FBFS_FORMAT_FAT32;  /* default mid partition type */
	opt->label = NULL;
	opt->tail_label = NULL;
	opt->mid_label = NULL;

	for (i = 0; i < argc; i++)
	{
		if (!strcmp(argv[i], "--size") || !strcmp(argv[i], "-s"))
		{
			need_arg(argc, i, argv[i]);
			opt->partition_size = parse_size_arg(argv[++i]);
		}
		else if (!strcmp(argv[i], "--primary") || !strcmp(argv[i], "-p"))
		{
			need_arg(argc, i, argv[i]);
			opt->primary_size = parse_size_arg(argv[++i]);
		}
		else if (!strcmp(argv[i], "--extended") || !strcmp(argv[i], "-e"))
		{
			need_arg(argc, i, argv[i]);
			i++;
			if (!strcmp(argv[i], "max") || !strcmp(argv[i], "all"))
				opt->extended_max = 1;
			else
				opt->extended_size = parse_size_arg(argv[i]);
		}
		else if (!strcmp(argv[i], "--base") || !strcmp(argv[i], "-b"))
		{
			need_arg(argc, i, argv[i]);
			opt->base = parse_size_arg(argv[++i]);
		}
		else if (!strcmp(argv[i], "--list-size") || !strcmp(argv[i], "-l"))
		{
			need_arg(argc, i, argv[i]);
			opt->list_size = parse_size_arg(argv[++i]);
		}
		else if (!strcmp(argv[i], "--force") || !strcmp(argv[i], "-f"))
			opt->force = 1;
		else if (!strcmp(argv[i], "--zip") || !strcmp(argv[i], "-z"))
			opt->zip = 1;
		else if (!strcmp(argv[i], "--raw") || !strcmp(argv[i], "-r"))
			opt->raw = 1;
		else if (!strcmp(argv[i], "--align") || !strcmp(argv[i], "-a"))
			opt->align = 1;
		else if (!strcmp(argv[i], "--fat16"))
			opt->fat_type = FBFS_FORMAT_FAT16;
		else if (!strcmp(argv[i], "--fat32"))
			opt->fat_type = FBFS_FORMAT_FAT32;
		else if (!strcmp(argv[i], "--ntfs"))
			opt->fat_type = FBFS_FORMAT_NTFS;
		else if (!strcmp(argv[i], "--exfat"))
			opt->fat_type = FBFS_FORMAT_EXFAT;
		else if (!strcmp(argv[i], "--refs"))
			opt->fat_type = FBFS_FORMAT_REFS;
		else if (!strcmp(argv[i], "--label") || !strcmp(argv[i], "-L"))
		{
			need_arg(argc, i, argv[i]);
			opt->label = argv[++i];
		}
		else if (!strcmp(argv[i], "--tail-label"))
		{
			need_arg(argc, i, argv[i]);
			opt->tail_label = argv[++i];
		}
		else if (!strcmp(argv[i], "--mid-label"))
		{
			need_arg(argc, i, argv[i]);
			opt->mid_label = argv[++i];
		}
		else if (!strcmp(argv[i], "--mid"))
		{
			char *colon;
			char mid_arg[64];

			need_arg(argc, i, argv[i]);
			i++;
			snprintf(mid_arg, sizeof(mid_arg), "%s", argv[i]);
			colon = strchr(mid_arg, ':');
			if (colon)
			{
				*colon = 0;
				colon++;
				if (!fbinst_strcasecmp(colon, "fat16"))
					opt->mid_fat_type = FBFS_FORMAT_FAT16;
				else if (!fbinst_strcasecmp(colon, "fat32"))
					opt->mid_fat_type = FBFS_FORMAT_FAT32;
				else if (!fbinst_strcasecmp(colon, "ntfs"))
					opt->mid_fat_type = FBFS_FORMAT_NTFS;
				else if (!fbinst_strcasecmp(colon, "exfat"))
					opt->mid_fat_type = FBFS_FORMAT_EXFAT;
				else if (!fbinst_strcasecmp(colon, "refs"))
					opt->mid_fat_type = FBFS_FORMAT_REFS;
				else
					quit("invalid mid partition type '%s' (valid: fat16, fat32, ntfs, exfat, refs)", colon);
			}
			opt->mid_size = parse_size_arg(mid_arg);
			if (!opt->mid_size)
				quit("mid partition size must be > 0");
		}
		else if (!strcmp(argv[i], "--tail"))
		{
			char *colon;
			char tail_arg[64];

			need_arg(argc, i, argv[i]);
			i++;
			snprintf(tail_arg, sizeof(tail_arg), "%s", argv[i]);
			colon = strchr(tail_arg, ':');
			if (colon)
			{
				*colon = 0;
				colon++;
				if (!fbinst_strcasecmp(colon, "fat16"))
					opt->tail_fat_type = FBFS_FORMAT_FAT16;
				else if (!fbinst_strcasecmp(colon, "fat32"))
					opt->tail_fat_type = FBFS_FORMAT_FAT32;
				else if (!fbinst_strcasecmp(colon, "ntfs"))
					opt->tail_fat_type = FBFS_FORMAT_NTFS;
				else if (!fbinst_strcasecmp(colon, "exfat"))
					opt->tail_fat_type = FBFS_FORMAT_EXFAT;
				else if (!fbinst_strcasecmp(colon, "refs"))
					opt->tail_fat_type = FBFS_FORMAT_REFS;
				else
					quit("invalid tail partition type '%s' (valid: fat16, fat32, ntfs, exfat, refs)", colon);
			}
			if (!fbinst_strcasecmp(tail_arg, "max") || !fbinst_strcasecmp(tail_arg, "all"))
				opt->tail_max = 1;
			else
			{
				opt->tail_size = parse_size_arg(tail_arg);
				if (!opt->tail_size)
					quit("tail partition size must be > 0");
			}
		}
		else if (!strcmp(argv[i], "--archive"))
		{
			need_arg(argc, i, argv[i]);
			opt->archive_path = argv[++i];
		}
		else if (!strcmp(argv[i], "--nalign") || !strcmp(argv[i], "-n"))
		{
			need_arg(argc, i, argv[i]);
			opt->nand_align = strtoul(argv[++i], NULL, 0) - 1u;
		}
		else if (!strcmp(argv[i], "--unit-size") || !strcmp(argv[i], "-u"))
		{
			need_arg(argc, i, argv[i]);
			opt->unit_size = strtoul(argv[++i], NULL, 0);
		}
		else if (!strcmp(argv[i], "--max-sectors"))
		{
			need_arg(argc, i, argv[i]);
			opt->max_sectors = strtoul(argv[++i], NULL, 0);
		}
		else if (!strcmp(argv[i], "--chs"))
			opt->chs_mode = 1;
		else if (!strcmp(argv[i], "--fb-version"))
		{
			need_arg(argc, i, argv[i]);
			opt->version_minor = parse_fb_version(argv[++i]);
		}
		else if (!strncmp(argv[i], "--part", 6) && strlen(argv[i]) == 7 && isdigit((unsigned char)argv[i][6]))
		{
			int slot = argv[i][6] - '0';
			if (slot >= 1 && slot <= 4)
			{
				char part_arg[128];
				need_arg(argc, i, argv[i]);
				i++;
				snprintf(part_arg, sizeof(part_arg), "%s", argv[i]);
				
				opt->raw_mbr_mode = 1; // Mark that we are using raw MBR formatting mode!
				
				char *colon1 = strchr(part_arg, ':');
				if (!colon1)
					quit("invalid partition argument '%s' (format: SIZE:TYPE[:LABEL])", argv[i]);
				*colon1 = 0;
				char *size_str = part_arg;
				char *type_str = colon1 + 1;
				char *label_str = NULL;
				
				char *colon2 = strchr(type_str, ':');
				if (colon2)
				{
					*colon2 = 0;
					label_str = colon2 + 1;
				}
				
				// Parse size
				if (!fbinst_strcasecmp(size_str, "max") || !fbinst_strcasecmp(size_str, "all"))
				{
					opt->raw_parts[slot - 1].size = UINT32_MAX; // Use UINT32_MAX as a placeholder for max
				}
				else
				{
					opt->raw_parts[slot - 1].size = parse_size_arg(size_str);
				}
				
				// Parse type
				if (!fbinst_strcasecmp(type_str, "fat16"))
					opt->raw_parts[slot - 1].fat_type = FBFS_FORMAT_FAT16;
				else if (!fbinst_strcasecmp(type_str, "fat32"))
					opt->raw_parts[slot - 1].fat_type = FBFS_FORMAT_FAT32;
				else if (!fbinst_strcasecmp(type_str, "ntfs"))
					opt->raw_parts[slot - 1].fat_type = FBFS_FORMAT_NTFS;
				else if (!fbinst_strcasecmp(type_str, "exfat"))
					opt->raw_parts[slot - 1].fat_type = FBFS_FORMAT_EXFAT;
				else if (!fbinst_strcasecmp(type_str, "refs"))
					opt->raw_parts[slot - 1].fat_type = FBFS_FORMAT_REFS;
				else
					quit("invalid partition type '%s' (valid: fat16, fat32, ntfs, exfat, refs)", type_str);
					
				// Parse label
				if (label_str)
				{
					opt->raw_parts[slot - 1].label = fb_strdup(label_str);
				}
			}
			else
			{
				quit("invalid partition slot index '%d' (must be 1-4)", slot);
			}
		}
		else if (!strcmp(argv[i], "--active"))
		{
			need_arg(argc, i, argv[i]);
			opt->active_slot = strtoul(argv[++i], NULL, 0);
			if (opt->active_slot < 1 || opt->active_slot > 4)
				quit("active slot must be 1, 2, 3, or 4");
		}
		else
			quit("invalid option %s for format", argv[i]);
	}
}

static void
parse_archive_options(int argc, char **argv, struct fbfs_archive_options *opt)
{
	int i;

	memset(opt, 0, sizeof(*opt));
	opt->primary_size = FBFS_MIN_PRI_SIZE;
	opt->list_size = FBFS_DEFAULT_LIST_SIZE;
	opt->version_minor = FBFS_VERSION_MINOR_16;
	for (i = 0; i < argc; i++)
	{
		if (!strcmp(argv[i], "--primary") || !strcmp(argv[i], "-p"))
		{
			need_arg(argc, i, argv[i]);
			opt->primary_size = parse_size_arg(argv[++i]);
		}
		else if (!strcmp(argv[i], "--extended") || !strcmp(argv[i], "-e"))
		{
			need_arg(argc, i, argv[i]);
			opt->extended_size = parse_size_arg(argv[++i]);
		}
		else if (!strcmp(argv[i], "--list-size") || !strcmp(argv[i], "-l"))
		{
			need_arg(argc, i, argv[i]);
			opt->list_size = parse_size_arg(argv[++i]);
		}
		else if (!strcmp(argv[i], "--fb-version"))
		{
			need_arg(argc, i, argv[i]);
			opt->version_minor = parse_fb_version(argv[++i]);
		}
		else
			quit("invalid option %s for create", argv[i]);
	}
}

static void
parse_sync_options(int argc, char **argv, struct fbfs_sync_options *opt)
{
	int i;

	memset(opt, 0, sizeof(*opt));
	opt->copy_bpb = -1;
	for (i = 0; i < argc; i++)
	{
		if (!strcmp(argv[i], "--copy-bpb"))
			opt->copy_bpb = 2;
		else if (!strcmp(argv[i], "--reset-bpb"))
			opt->copy_bpb = 1;
		else if (!strcmp(argv[i], "--clear-bpb"))
			opt->copy_bpb = 0;
		else if (!strcmp(argv[i], "--bpb-size"))
		{
			need_arg(argc, i, argv[i]);
			opt->bpb_size = strtoul(argv[++i], NULL, 0);
		}
		else if (!strcmp(argv[i], "--zip") || !strcmp(argv[i], "-z"))
			opt->zip = 1;
		else if (!strcmp(argv[i], "--max-sectors"))
		{
			need_arg(argc, i, argv[i]);
			opt->max_sectors = strtoul(argv[++i], NULL, 0);
		}
		else if (!strcmp(argv[i], "--chs"))
			opt->chs_mode = 1;
		else
			quit("invalid option %s for sync", argv[i]);
	}
}

static uint8_t
parse_partition_type(const char *arg)
{
	if (!fbinst_strcasecmp(arg, "fat32") || !fbinst_strcasecmp(arg, "fat32x"))
		return 0x0c;
	if (!fbinst_strcasecmp(arg, "fat16") || !fbinst_strcasecmp(arg, "fat16x"))
		return 0x0e;
	if (!fbinst_strcasecmp(arg, "ntfs") || !fbinst_strcasecmp(arg, "exfat") || !fbinst_strcasecmp(arg, "refs"))
		return 0x07;
	if (!fbinst_strcasecmp(arg, "esp") || !fbinst_strcasecmp(arg, "efi"))
		return 0xef;
	if (!fbinst_strcasecmp(arg, "linux") || !fbinst_strcasecmp(arg, "ext4"))
		return 0x83;

	return (uint8_t)strtoul(arg, NULL, 0);
}

static int
is_read_only_command(const char *command)
{
	return !strcmp(command, "info") || !strcmp(command, "cat") ||
		!strcmp(command, "cat-menu") || !strcmp(command, "export");
}

int
main(int argc, char **argv)
{
	int bootice_mode = 0;
	int j;
	for (j = 1; j < argc; j++) {
		if (argv[j][0] == '/' || !fbinst_strcasecmp(argv[j], "import-mbr") || 
			!fbinst_strcasecmp(argv[j], "import-pbr") || 
			!fbinst_strcasecmp(argv[j], "write-mbr") || 
			!fbinst_strcasecmp(argv[j], "write-pbr")) {
			bootice_mode = 1;
			break;
		}
	}

	if (bootice_mode) {
		const char *bootice_device = NULL;
		const char *bootice_file = NULL;
		const char *bootice_type = NULL;
		int is_mbr = 0;
		int is_pbr = 0;
		int is_install = 0;
		int is_restore = 0;
		int is_backup = 0;
		int is_sectors_mode = 0;
		int is_partitions_mode = 0;
		uint32_t bootice_slot = 1;
		uint32_t bootice_sectors = 0;
		uint32_t bootice_lba = 0;
		int keep_bpb = 1;
		int keep_dpt = 0;
		const char *part_op = NULL;
		const char *part_param = NULL;

		for (j = 1; j < argc; j++) {
			if (!fbinst_strncasecmp(argv[j], "/device=", 8)) {
				const char *val = argv[j] + 8;
				static char dev_name[64];
				memset(dev_name, 0, sizeof(dev_name));
				bootice_slot = 1;

				const char *colon = strchr(val, ':');
				if (colon) {
					bootice_slot = strtoul(colon + 1, NULL, 0) + 1;
				}

				char temp_val[64];
				memset(temp_val, 0, sizeof(temp_val));
				if (colon) {
					size_t len = colon - val;
					if (len >= sizeof(temp_val)) len = sizeof(temp_val) - 1;
					memcpy(temp_val, val, len);
				} else {
					strncpy(temp_val, val, sizeof(temp_val) - 1);
				}

				char *start_p = strchr(temp_val, '(');
				char *end_p = strchr(temp_val, ')');
				if (start_p && end_p && end_p > start_p) {
					*end_p = '\0';
					memmove(temp_val, start_p + 1, strlen(start_p + 1) + 1);
				}

				char *hd_colon = strchr(temp_val, ':');
				if (hd_colon) {
					int dev_idx = atoi(hd_colon + 1);
					snprintf(dev_name, sizeof(dev_name), "(hd%d)", dev_idx);
				} else if (!fbinst_strncasecmp(temp_val, "hd", 2)) {
					snprintf(dev_name, sizeof(dev_name), "(%s)", temp_val);
				} else {
					if (isdigit((unsigned char)temp_val[0])) {
						snprintf(dev_name, sizeof(dev_name), "(hd%s)", temp_val);
					} else {
						snprintf(dev_name, sizeof(dev_name), "%s", temp_val);
					}
				}
				bootice_device = dev_name;
			} else if (!fbinst_strcasecmp(argv[j], "/mbr")) {
				is_mbr = 1;
			} else if (!fbinst_strcasecmp(argv[j], "/pbr")) {
				is_pbr = 1;
			} else if (!fbinst_strcasecmp(argv[j], "/sectors")) {
				is_sectors_mode = 1;
			} else if (!fbinst_strcasecmp(argv[j], "/partitions")) {
				is_partitions_mode = 1;
			} else if (!fbinst_strcasecmp(argv[j], "/install")) {
				is_install = 1;
			} else if (!fbinst_strcasecmp(argv[j], "/restore")) {
				is_restore = 1;
			} else if (!fbinst_strcasecmp(argv[j], "/backup")) {
				is_backup = 1;
			} else if (!fbinst_strncasecmp(argv[j], "/type=", 6)) {
				bootice_type = argv[j] + 6;
			} else if (!fbinst_strncasecmp(argv[j], "/file=", 6)) {
				bootice_file = argv[j] + 6;
			} else if (!fbinst_strncasecmp(argv[j], "/sectors=", 9)) {
				bootice_sectors = strtoul(argv[j] + 9, NULL, 0);
			} else if (!fbinst_strncasecmp(argv[j], "/lba=", 5)) {
				bootice_lba = strtoul(argv[j] + 5, NULL, 0);
			} else if (!fbinst_strcasecmp(argv[j], "/keep_bpb")) {
				keep_bpb = 1;
			} else if (!fbinst_strcasecmp(argv[j], "/keep_dpt")) {
				keep_dpt = 1;
			} else if (!fbinst_strcasecmp(argv[j], "/hide")) {
				part_op = "hide";
			} else if (!fbinst_strcasecmp(argv[j], "/unhide")) {
				part_op = "unhide";
			} else if (!fbinst_strcasecmp(argv[j], "/activate")) {
				part_op = "activate";
			} else if (!fbinst_strncasecmp(argv[j], "/set_id=", 8)) {
				part_op = "set_id";
				part_param = argv[j] + 8;
			} else if (!fbinst_strncasecmp(argv[j], "/backup_dpt=", 12)) {
				part_op = "backup_dpt";
				part_param = argv[j] + 12;
			} else if (!fbinst_strncasecmp(argv[j], "/restore_dpt=", 13)) {
				part_op = "restore_dpt";
				part_param = argv[j] + 13;
			}
		}

		if (!bootice_device) {
			for (j = 1; j < argc; j++) {
				if (argv[j][0] != '/' && fbinst_strcasecmp(argv[j], "import-mbr") != 0 &&
					fbinst_strcasecmp(argv[j], "import-pbr") != 0 &&
					fbinst_strcasecmp(argv[j], "write-mbr") != 0 &&
					fbinst_strcasecmp(argv[j], "write-pbr") != 0 &&
					fbinst_strcasecmp(argv[j], "export-mbr") != 0 &&
					fbinst_strcasecmp(argv[j], "export-pbr") != 0 &&
					fbinst_strcasecmp(argv[j], "backup-sectors") != 0 &&
					fbinst_strcasecmp(argv[j], "restore-sectors") != 0 &&
					fbinst_strcasecmp(argv[j], "partition-op") != 0) {
					bootice_device = argv[j];
					break;
				}
			}
		}

		if (bootice_device) {
			struct fbfs_disk *disk_inst;
			uint32_t disk_flags = FBFS_DISK_WRITABLE;
			check(fbfs_disk_open(bootice_device, disk_flags, &disk_inst));

			if (is_mbr) {
				if (is_install && bootice_type) {
					check(fbfs_write_mbr(disk_inst, bootice_type));
					printf("Successfully wrote MBR code (%s) to %s\n", bootice_type, bootice_device);
				} else if (is_restore && bootice_file) {
					check(fbfs_import_mbr(disk_inst, bootice_file, bootice_sectors));
					printf("Successfully imported MBR from file %s to %s\n", bootice_file, bootice_device);
				} else if (is_backup && bootice_file) {
					check(fbfs_export_mbr(disk_inst, bootice_file, bootice_sectors));
					printf("Successfully backed up MBR to file %s from %s\n", bootice_file, bootice_device);
				} else {
					quit("invalid Bootice parameters for /mbr");
				}
			} else if (is_pbr) {
				if (is_install && bootice_type) {
					check(fbfs_write_pbr(disk_inst, bootice_slot, bootice_type));
					printf("Successfully wrote PBR code (%s) to %s (Slot %u)\n", bootice_type, bootice_device, bootice_slot);
				} else if (is_restore && bootice_file) {
					check(fbfs_import_pbr(disk_inst, bootice_slot, bootice_file, keep_bpb));
					printf("Successfully imported PBR from file %s to %s (Slot %u)\n", bootice_file, bootice_device, bootice_slot);
				} else if (is_backup && bootice_file) {
					check(fbfs_export_pbr(disk_inst, bootice_slot, bootice_file, bootice_sectors));
					printf("Successfully backed up PBR to file %s (Slot %u) from %s\n", bootice_file, bootice_slot, bootice_device);
				} else {
					quit("invalid Bootice parameters for /pbr");
				}
			} else if (is_sectors_mode) {
				if (is_backup && bootice_file) {
					check(fbfs_backup_sectors(disk_inst, bootice_file, bootice_lba, bootice_sectors));
					printf("Successfully backed up %u sectors starting at LBA %u to %s\n", bootice_sectors, bootice_lba, bootice_file);
				} else if (is_restore && bootice_file) {
					check(fbfs_restore_sectors(disk_inst, bootice_file, bootice_lba, bootice_sectors, keep_dpt, keep_bpb));
					printf("Successfully restored %u sectors to LBA %u from %s\n", bootice_sectors, bootice_lba, bootice_file);
				} else {
					quit("invalid Bootice parameters for /sectors");
				}
			} else if (is_partitions_mode) {
				if (part_op) {
					if (!fbinst_strcasecmp(part_op, "backup_dpt")) {
						check(fbfs_partition_op(disk_inst, 0, part_op, part_param));
						printf("Successfully backed up DPT to file %s\n", part_param);
					} else if (!fbinst_strcasecmp(part_op, "restore_dpt")) {
						check(fbfs_partition_op(disk_inst, 0, part_op, part_param));
						printf("Successfully restored DPT from file %s\n", part_param);
					} else {
						check(fbfs_partition_op(disk_inst, bootice_slot, part_op, part_param));
						printf("Successfully performed '%s' partition operation on Slot %u\n", part_op, bootice_slot);
					}
				} else {
					quit("invalid Bootice partition operations");
				}
			}

			fbfs_disk_close(disk_inst);
			fbfs_disk_unlock_all();
			return 0;
		}
	}
	struct fbfs_disk *disk;
	struct fbfs *fs;
	const char *device;
	const char *command;
	uint32_t disk_flags;
	int i;
	int modified;

	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] != '-')
			break;
		if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h"))
		{
			help();
			return 0;
		}
		else if (!strcmp(argv[i], "--version") || !strcmp(argv[i], "-V"))
		{
			fprintf(stderr, "%s version : %s build %d\n", progname,
				FBINST_VERSION, BUILD_NUMBER);
			return 0;
		}
		else if (!strcmp(argv[i], "--verbose") || !strcmp(argv[i], "-v"))
			verbosity++;
		else if (!strcmp(argv[i], "--list") || !strcmp(argv[i], "-l"))
		{
			list_devs();
			return 0;
		}
		else if (!strcmp(argv[i], "--debug") || !strcmp(argv[i], "-d"))
			debug_boot = 1;
		else
			quit("invalid option %s", argv[i]);
	}

	if (i >= argc - 1)
		quit("no device name or command");

	device = argv[i++];
	command = argv[i++];
	argc -= i;
	argv += i;
	modified = 0;

	disk_flags = is_read_only_command(command) ? 0 : FBFS_DISK_WRITABLE;
	if (!strcmp(command, "create"))
		disk_flags |= FBFS_DISK_CREATE | FBFS_DISK_TRUNCATE;

	check(fbfs_disk_open(device, disk_flags, &disk));

	if (!strcmp(command, "import-mbr"))
	{
		uint32_t sectors = 0;
		if (argc < 1)
			quit("not enough parameters: import-mbr FILE [SECTORS]");
		if (argc >= 2)
			sectors = strtoul(argv[1], NULL, 0);
		check(fbfs_import_mbr(disk, argv[0], sectors));
		fbfs_disk_close(disk);
		return 0;
	}
	else if (!strcmp(command, "import-pbr"))
	{
		if (argc < 2)
			quit("not enough parameters: import-pbr SLOT FILE");
		uint32_t slot = strtoul(argv[0], NULL, 0);
		check(fbfs_import_pbr(disk, slot, argv[1], 1));
		fbfs_disk_close(disk);
		return 0;
	}
	else if (!strcmp(command, "write-mbr"))
	{
		if (argc < 1)
			quit("not enough parameters: write-mbr TYPE");
		check(fbfs_write_mbr(disk, argv[0]));
		fbfs_disk_close(disk);
		return 0;
	}
	else if (!strcmp(command, "write-pbr"))
	{
		if (argc < 2)
			quit("not enough parameters: write-pbr SLOT TYPE");
		uint32_t slot = strtoul(argv[0], NULL, 0);
		check(fbfs_write_pbr(disk, slot, argv[1]));
		fbfs_disk_close(disk);
		return 0;
	}
	else if (!strcmp(command, "export-mbr"))
	{
		uint32_t sectors = 0;
		if (argc < 1)
			quit("not enough parameters: export-mbr FILE [SECTORS]");
		if (argc >= 2)
			sectors = strtoul(argv[1], NULL, 0);
		check(fbfs_export_mbr(disk, argv[0], sectors));
		fbfs_disk_close(disk);
		return 0;
	}
	else if (!strcmp(command, "export-pbr"))
	{
		uint32_t sectors = 0;
		if (argc < 2)
			quit("not enough parameters: export-pbr SLOT FILE [SECTORS]");
		uint32_t slot = strtoul(argv[0], NULL, 0);
		if (argc >= 3)
			sectors = strtoul(argv[2], NULL, 0);
		check(fbfs_export_pbr(disk, slot, argv[1], sectors));
		fbfs_disk_close(disk);
		return 0;
	}
	else if (!strcmp(command, "backup-sectors"))
	{
		if (argc < 3)
			quit("not enough parameters: backup-sectors FILE LBA SECTORS");
		uint32_t lba = strtoul(argv[1], NULL, 0);
		uint32_t sectors = strtoul(argv[2], NULL, 0);
		check(fbfs_backup_sectors(disk, argv[0], lba, sectors));
		fbfs_disk_close(disk);
		return 0;
	}
	else if (!strcmp(command, "restore-sectors"))
	{
		if (argc < 3)
			quit("not enough parameters: restore-sectors FILE LBA SECTORS");
		uint32_t lba = strtoul(argv[1], NULL, 0);
		uint32_t sectors = strtoul(argv[2], NULL, 0);
		check(fbfs_restore_sectors(disk, argv[0], lba, sectors, 0, 0));
		fbfs_disk_close(disk);
		return 0;
	}
	else if (!strcmp(command, "partition-op"))
	{
		if (argc < 2)
			quit("not enough parameters: partition-op SLOT OP [PARAM]");
		uint32_t slot = strtoul(argv[0], NULL, 0);
		const char *op = argv[1];
		const char *param = (argc >= 3) ? argv[2] : NULL;
		check(fbfs_partition_op(disk, slot, op, param));
		fbfs_disk_close(disk);
		return 0;
	}

	if (!strcmp(command, "format"))
	{
		struct fbfs_format_options opt;

		parse_format_options(argc, argv, &opt);
		opt.debug_boot = debug_boot;
		check(fbfs_format(disk, &opt));
		fbfs_disk_close(disk);
		fbfs_disk_unlock_all();
		return 0;
	}
	else if (!strcmp(command, "restore"))
	{
		check(fbfs_restore(disk));
		fbfs_disk_close(disk);
		fbfs_disk_unlock_all();
		return 0;
	}
	else if (!strcmp(command, "create"))
	{
		struct fbfs_archive_options opt;

		parse_archive_options(argc, argv, &opt);
		check(fbfs_create_archive(disk, &opt));
		fbfs_disk_close(disk);
		return 0;
	}

	check(fbfs_mount(disk, FBFS_OPEN_ALLOW_ARCHIVE, &fs));

	if (!strcmp(command, "update"))
	{
		check(fbfs_update(fs, debug_boot));
		modified = 1;
	}
	else if (!strcmp(command, "sync"))
	{
		struct fbfs_sync_options opt;

		parse_sync_options(argc, argv, &opt);
		check(fbfs_sync(fs, &opt));
		modified = 1;
	}
	else if (!strcmp(command, "map"))
	{
		uint32_t slot;
		uint32_t type;

		if (argc < 3)
			quit("not enough parameters: map NAME SLOT TYPE");
		slot = strtoul(argv[1], NULL, 0);
		type = parse_partition_type(argv[2]);
		if (slot < 1 || slot > 4)
			quit("slot must be 1, 2, 3, or 4");
		check(fbfs_map_partition(fs, argv[0], slot, (uint8_t)type));
		modified = 1;
	}
	else if (!strcmp(command, "unmap"))
	{
		uint32_t slot;

		if (argc < 1)
			quit("not enough parameters: unmap SLOT");
		slot = strtoul(argv[0], NULL, 0);
		if (slot < 1 || slot > 4)
			quit("slot must be 1, 2, 3, or 4");
		check(fbfs_unmap_partition(fs, slot));
		modified = 1;
	}
	else if (!strcmp(command, "format-file"))
	{
		uint32_t unit_size = 0;
		int align = 0;
		const char *label = NULL;
		const char *file_name = NULL;
		const char *type_name = NULL;
		int i;

		for (i = 0; i < argc; i++)
		{
			if (!strcmp(argv[i], "--align") || !strcmp(argv[i], "-a"))
				align = 1;
			else if (!strcmp(argv[i], "--unit-size") || !strcmp(argv[i], "-u"))
			{
				need_arg(argc, i, argv[i]);
				unit_size = strtoul(argv[++i], NULL, 0);
			}
			else if (!strcmp(argv[i], "--label") || !strcmp(argv[i], "-L"))
			{
				need_arg(argc, i, argv[i]);
				label = argv[++i];
			}
			else if (argv[i][0] == '-')
			{
				quit("invalid option %s for format-file", argv[i]);
			}
			else
			{
				if (!file_name)
					file_name = argv[i];
				else if (!type_name)
					type_name = argv[i];
				else
					quit("too many parameters for format-file");
			}
		}

		if (!file_name || !type_name)
			quit("not enough parameters: format-file NAME TYPE");

		check(fbfs_format_file(fs, file_name, type_name, unit_size, align, label));
		modified = 1;
	}
	else if (!strcmp(command, "info"))
		print_info(fs);
	else if (!strcmp(command, "clear"))
	{
		check(fbfs_clear(fs));
		modified = 1;
	}
	else if (!strcmp(command, "add"))
	{
		uint32_t flags;
		int syslinux;

		flags = 0;
		syslinux = 0;
		for (i = 0; i < argc; i++)
		{
			if (argv[i][0] != '-')
				break;
			if (!strcmp(argv[i], "--extended") || !strcmp(argv[i], "-e"))
				flags |= FBFS_FILE_EXTENDED;
			else if (!strcmp(argv[i], "--syslinux") || !strcmp(argv[i], "-s"))
			{
				flags |= FBFS_FILE_EXTENDED | FBFS_FILE_SYSLINUX;
				syslinux = 1;
			}
			else
				quit("invalid option %s for add", argv[i]);
		}
		argc -= i;
		argv += i;
		if (argc < 1)
			quit("not enough parameters");
		if (argc >= 2)
			check(fbfs_add_file(fs, argv[0], argv[1], flags));
		else
		{
			uint8_t *buffer;
			uint32_t size;

			buffer = read_stdin(&size);
			if (!size)
				quit("no input");
			check(fbfs_add_buffer(fs, argv[0], buffer, size, flags));
			free(buffer);
		}
		if (syslinux)
			check(fbfs_syslinux_patch(fs, argv[0]));
		modified = 1;
	}
	else if (!strcmp(command, "add-menu"))
	{
		command_add_menu(fs, argc, argv);
		modified = 1;
	}
	else if (!strcmp(command, "resize"))
	{
		uint32_t flags;
		uint8_t fill;

		flags = 0;
		fill = 0;
		for (i = 0; i < argc; i++)
		{
			if (argv[i][0] != '-')
				break;
			if (!strcmp(argv[i], "--extended") || !strcmp(argv[i], "-e"))
				flags |= FBFS_FILE_EXTENDED;
			else if (!strcmp(argv[i], "--fill") || !strcmp(argv[i], "-f"))
			{
				need_arg(argc, i, argv[i]);
				i++;
				fill = (strlen(argv[i]) == 1) ?
					(uint8_t)argv[i][0] :
					(uint8_t)strtoul(argv[i], NULL, 0);
			}
			else
				quit("invalid option %s for resize", argv[i]);
		}
		argc -= i;
		argv += i;
		if (argc < 2)
			quit("not enough parameters");
		uint32_t size_bytes;
		if (!strcmp(argv[1], "max") || !strcmp(argv[1], "all"))
		{
			size_bytes = fbfs_get_max_free_size(fs, (flags & FBFS_FILE_EXTENDED) ? 1 : 0);
		}
		else
		{
			size_bytes = parse_size_arg(argv[1]) << 9;
		}
		check(fbfs_resize_file(fs, argv[0], size_bytes,
			fill, flags));
		modified = 1;
	}
	else if (!strcmp(command, "copy"))
	{
		if (argc < 2)
			quit("not enough parameters");
		check(fbfs_copy_file(fs, argv[0], argv[1]));
		modified = 1;
	}
	else if (!strcmp(command, "move"))
	{
		if (argc < 2)
			quit("not enough parameters");
		check(fbfs_move_file(fs, argv[0], argv[1]));
		modified = 1;
	}
	else if (!strcmp(command, "export"))
	{
		if (argc < 1)
			quit("not enough parameters");
		if (argc >= 2)
		{
			char *path;

			path = malloc(strlen(argv[1]) + 1u);
			if (!path)
				quit("not enough memory");
			strcpy(path, argv[1]);
			create_parent_dirs(path);
			check(fbfs_export_file(fs, argv[0], argv[1]));
			free(path);
		}
		else
		{
			struct fbfs_file_info file;
			uint8_t *buffer;
			uint32_t used;

#if defined(_WIN32)
			_setmode(_fileno(stdout), _O_BINARY);
#endif
			check(fbfs_find_file(fs, argv[0], &file));
			buffer = malloc(file.data_size);
			if (!buffer)
				quit("not enough memory");
			check(fbfs_read_file(fs, argv[0], buffer, file.data_size, &used));
			if (fwrite(buffer, 1, used, stdout) != used)
				quit("stdout write failed");
			free(buffer);
		}
	}
	else if (!strcmp(command, "remove"))
	{
		if (argc < 1)
			quit("not enough parameters");
		check(fbfs_remove_file(fs, argv[0]));
		modified = 1;
	}
	else if (!strcmp(command, "cat"))
	{
		struct fbfs_file_info file;
		uint8_t *buffer;
		uint32_t used;

		if (argc < 1)
			quit("not enough parameters");
		check(fbfs_find_file(fs, argv[0], &file));
		buffer = malloc(file.data_size + 1u);
		if (!buffer)
			quit("not enough memory");
		check(fbfs_read_file(fs, argv[0], buffer, file.data_size, &used));
		buffer[used] = 0;
		puts((char *)buffer);
		free(buffer);
	}
	else if (!strcmp(command, "cat-menu"))
		command_cat_menu(fs, argc, argv);
	else if (!strcmp(command, "pack"))
	{
		check(fbfs_pack(fs));
		modified = 1;
	}
	else if (!strcmp(command, "check"))
		check(fbfs_check(fs));
	else if (!strcmp(command, "save"))
	{
		uint32_t list_size;

		list_size = 0;
		for (i = 0; i < argc; i++)
		{
			if (argv[i][0] != '-')
				break;
			if (!strcmp(argv[i], "--list-size") || !strcmp(argv[i], "-l"))
			{
				need_arg(argc, i, argv[i]);
				list_size = parse_size_arg(argv[++i]);
			}
			else
				quit("invalid option %s for save", argv[i]);
		}
		argc -= i;
		argv += i;
		if (argc < 1)
			quit("not enough parameters");
		check(fbfs_save_archive(fs, argv[0], list_size));
	}
	else if (!strcmp(command, "load"))
	{
		if (argc < 1)
			quit("not enough parameters");
		check(fbfs_load_archive(fs, argv[0]));
		modified = 1;
	}
	else
		quit("unknown command %s", command);

	if (modified)
		check(fbfs_flush(fs));

	fbfs_close(fs);
	fbfs_disk_close(disk);
	fbfs_disk_unlock_all();
	return 0;
}
