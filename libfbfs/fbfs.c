#define _CRT_SECURE_NO_WARNINGS

#include "fbfs.h"
#include "fb_mbr_rel.h"
#include "fb_mbr_dbg.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winioctl.h>
#include <sys/stat.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <strings.h>
#include <unistd.h>
#endif

#define FBFS_GLOB_SECTORS	64u
#define FBFS_DEF_FAT32_SIZE	(512u * 2048u)
#define FBFS_MIN_FAT16_SIZE	(8400u + 1u)
#define FBFS_AR_NAME_PREFIX	"ud:/"
#define FBFS_OFS_MAX_SEC	0x1adu

static const uint8_t fat32_nt60_bc[] = {
	0x0F, 0x18, 0x2E, 0x78, 0x14, 0x24, 0x01, 0x01, 0x01, 0x24, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x03, 0x02, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFA, 0x31, 0xC0, 0x8E, 0xD8, 0x8E, 0xD0, 0xBC, 0x00, 0x7C, 0xFB, 0x88, 0x16, 0x24, 0x01, 0xB4,
	0x08, 0xCD, 0x13, 0x73, 0x0A, 0xF6, 0xC2, 0x80, 0x74, 0x05, 0xFE, 0x06, 0x25, 0x01, 0x18, 0xE4,
	0x8A, 0xF1, 0x83, 0xE1, 0x3F, 0x89, 0x0E, 0x18, 0x01, 0x31, 0xC0, 0x8A, 0xF6, 0x40, 0x89, 0x05,
	0x1A, 0x01, 0x88, 0x16, 0x25, 0x01, 0xB8, 0x00, 0x90, 0x8E, 0xC0, 0x31, 0xDB, 0xBD, 0x00, 0x07,
	0x8B, 0x46, 0x1C, 0x8B, 0x56, 0x1E, 0x03, 0x46, 0x0E, 0x13, 0x56, 0x10, 0x1E, 0x03, 0x46, 0x24,
	0x13, 0x56, 0x26, 0x03, 0x46, 0x24, 0x13, 0x56, 0x26, 0x89, 0x46, 0x1C, 0x89, 0x56, 0x1E, 0xE8,
	0x5A, 0x00, 0x26, 0x8B, 0x07, 0x26, 0x8B, 0x4F, 0x02, 0x8D, 0x7E, 0x28, 0x26, 0x8B, 0x57, 0x04,
	0x26, 0x8B, 0x6F, 0x06, 0x55, 0x52, 0x51, 0x50, 0x53, 0x53, 0x1E, 0xE8, 0x0C, 0x00, 0x1F, 0x26,
	0x89, 0x47, 0x08, 0x26, 0x89, 0x57, 0x0A, 0x1F, 0xEB, 0x10, 0x1F, 0x89, 0x4E, 0x1C, 0x89, 0x56,
	0x1E, 0xE8, 0x2E, 0x00, 0x73, 0x05, 0xB0, 0xFF, 0x29, 0xC0, 0xC3, 0x83, 0xC5, 0x08, 0x55, 0xE8,
	0x12, 0x00, 0x26, 0x8B, 0x47, 0x10, 0x26, 0x8B, 0x57, 0x12, 0x1F, 0xC3, 0x55, 0xE8, 0x05, 0x00,
	0x26, 0x8B, 0x47, 0x1C, 0x1F, 0xC3, 0x55, 0xE8, 0x05, 0x00, 0x26, 0x8B, 0x47, 0x20, 0x1F, 0xC3,
	0x8B, 0x4E, 0x1C, 0x8B, 0x56, 0x1E, 0x49, 0x74, 0x3F, 0x41, 0x51, 0x52, 0xE8, 0xF9, 0xFF, 0x1F,
	0x59, 0x5A, 0x8B, 0x45, 0x08, 0x8B, 0x55, 0x0A, 0x01, 0x46, 0x1C, 0x11, 0x56, 0x1E, 0x8A, 0x45,
	0x02, 0x31, 0xD2, 0x3D, 0x00, 0x02, 0x74, 0x05, 0xF7, 0x36, 0x0B, 0x00, 0xF7, 0x26, 0x0D, 0x00,
	0x48, 0x89, 0x45, 0x10, 0x1F, 0xC3, 0x49, 0x74, 0xD4, 0x3D, 0x00, 0x02, 0x74, 0xC3, 0x31, 0xC0,
	0x8E, 0xD8, 0xBE, 0x6E, 0x7D, 0xE8, 0x81, 0xFF, 0x31, 0xC0, 0xCD, 0x16, 0xCD, 0x19, 0xB8, 0x01,
	0x02, 0xBB, 0x00, 0x05, 0x8B, 0x4E, 0x1C, 0x8B, 0x56, 0x1E, 0xCD, 0x13, 0x73, 0x05, 0x31, 0xC0,
	0xCD, 0x13, 0xF9, 0xC3, 0x00, 0x0D, 0x0A, 0x42, 0x4F, 0x4F, 0x54, 0x4D, 0x47, 0x52, 0x20, 0x69,
	0x73, 0x20, 0x6D, 0x69, 0x73, 0x73, 0x69, 0x6E, 0x67, 0x00, 0x0D, 0x0A, 0x44, 0x69, 0x73, 0x6b,
	0x20, 0x65, 0x72, 0x72, 0x6f, 0x72, 0x00, 0x0D, 0x0A, 0x50, 0x72, 0x65, 0x73, 0x73, 0x20, 0x61,
	0x6e, 0x79, 0x20, 0x6b, 0x65, 0x79, 0x20, 0x74, 0x6f, 0x20, 0x72, 0x65, 0x73, 0x74, 0x61, 0x72,
	0x74, 0x0D, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t fat32_nt52_bc[] = {
	0x0F, 0x18, 0x2E, 0x78, 0x14, 0x24, 0x01, 0x01, 0x01, 0x24, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x03, 0x02, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFA, 0x31, 0xC0, 0x8E, 0xD8, 0x8E, 0xD0, 0xBC, 0x00, 0x7C, 0xFB, 0x88, 0x16, 0x24, 0x01, 0xB4,
	0x08, 0xCD, 0x13, 0x73, 0x0A, 0xF6, 0xC2, 0x80, 0x74, 0x05, 0xFE, 0x06, 0x25, 0x01, 0x18, 0xE4,
	0x8A, 0xF1, 0x83, 0xE1, 0x3F, 0x89, 0x0E, 0x18, 0x01, 0x31, 0xC0, 0x8A, 0xF6, 0x40, 0x89, 0x05,
	0x1A, 0x01, 0x88, 0x16, 0x25, 0x01, 0xB8, 0x00, 0x90, 0x8E, 0xC0, 0x31, 0xDB, 0xBD, 0x00, 0x07,
	0x8B, 0x46, 0x1C, 0x8B, 0x56, 0x1E, 0x03, 0x46, 0x0E, 0x13, 0x56, 0x10, 0x1E, 0x03, 0x46, 0x24,
	0x13, 0x56, 0x26, 0x03, 0x46, 0x24, 0x13, 0x56, 0x26, 0x89, 0x46, 0x1C, 0x89, 0x56, 0x1E, 0xE8,
	0x5A, 0x00, 0x26, 0x8B, 0x07, 0x26, 0x8B, 0x4F, 0x02, 0x8D, 0x7E, 0x28, 0x26, 0x8B, 0x57, 0x04,
	0x26, 0x8B, 0x6F, 0x06, 0x55, 0x52, 0x51, 0x50, 0x53, 0x53, 0x1E, 0xE8, 0x0C, 0x00, 0x1F, 0x26,
	0x89, 0x47, 0x08, 0x26, 0x89, 0x57, 0x0A, 0x1F, 0xEB, 0x10, 0x1F, 0x89, 0x4E, 0x1C, 0x89, 0x56,
	0x1E, 0xE8, 0x2E, 0x00, 0x73, 0x05, 0xB0, 0xFF, 0x29, 0xC0, 0xC3, 0x83, 0xC5, 0x08, 0x55, 0xE8,
	0x12, 0x00, 0x26, 0x8B, 0x47, 0x10, 0x26, 0x8B, 0x57, 0x12, 0x1F, 0xC3, 0x55, 0xE8, 0x05, 0x00,
	0x26, 0x8B, 0x47, 0x1C, 0x1F, 0xC3, 0x55, 0xE8, 0x05, 0x00, 0x26, 0x8B, 0x47, 0x20, 0x1F, 0xC3,
	0x8B, 0x4E, 0x1C, 0x8B, 0x56, 0x1E, 0x49, 0x74, 0x3F, 0x41, 0x51, 0x52, 0xE8, 0xF9, 0xFF, 0x1F,
	0x59, 0x5A, 0x8B, 0x45, 0x08, 0x8B, 0x55, 0x0A, 0x01, 0x46, 0x1C, 0x11, 0x56, 0x1E, 0x8A, 0x45,
	0x02, 0x31, 0xD2, 0x3D, 0x00, 0x02, 0x74, 0x05, 0xF7, 0x36, 0x0B, 0x00, 0xF7, 0x26, 0x0D, 0x00,
	0x48, 0x89, 0x45, 0x10, 0x1F, 0xC3, 0x49, 0x74, 0xD4, 0x3D, 0x00, 0x02, 0x74, 0xC3, 0x31, 0xC0,
	0x8E, 0xD8, 0xBE, 0x6E, 0x7D, 0xE8, 0x81, 0xFF, 0x31, 0xC0, 0xCD, 0x16, 0xCD, 0x19, 0xB8, 0x01,
	0x02, 0xBB, 0x00, 0x05, 0x8B, 0x4E, 0x1C, 0x8B, 0x56, 0x1E, 0xCD, 0x13, 0x73, 0x05, 0x31, 0xC0,
	0xCD, 0x13, 0xF9, 0xC3, 0x00, 0x0D, 0x0A, 0x4E, 0x54, 0x4C, 0x44, 0x52, 0x20, 0x69, 0x73, 0x20,
	0x6D, 0x69, 0x73, 0x73, 0x69, 0x6E, 0x67, 0x00, 0x0D, 0x0A, 0x44, 0x69, 0x73, 0x6b, 0x20, 0x65,
	0x72, 0x72, 0x6f, 0x72, 0x00, 0x0D, 0x0A, 0x50, 0x72, 0x65, 0x73, 0x73, 0x20, 0x61, 0x6e, 0x79,
	0x20, 0x6b, 0x65, 0x79, 0x20, 0x74, 0x6f, 0x20, 0x72, 0x65, 0x73, 0x74, 0x61, 0x72, 0x74, 0x0d,
	0x0a, 0x00
};

#if defined(_MSC_VER)
#define fbfs_strcasecmp		_stricmp
#define fbfs_strncasecmp	_strnicmp
#define fbfs_strdup		_strdup
#define fbfs_stat		_stat64
#define fbfs_stat_t		struct _stat64
#else
#define fbfs_strcasecmp		strcasecmp
#define fbfs_strncasecmp	strncasecmp
#define fbfs_strdup		strdup
#define fbfs_stat		stat
#define fbfs_stat_t		struct stat
#endif

#pragma pack(push, 1)
struct fbfs_mbr
{
	uint8_t jmp_code;
	uint8_t jmp_ofs;
	uint8_t boot_code[0x1ab];
	uint8_t max_sec;
	uint16_t lba;
	uint8_t spt;
	uint8_t heads;
	uint16_t boot_base;
	uint32_t fb_magic;
	uint8_t mbr_table[0x46];
	uint16_t end_magic;
};

struct fbfs_data
{
	uint16_t boot_size;
	uint16_t flags;
	uint8_t ver_major;
	uint8_t ver_minor;
	uint16_t list_used;
	uint16_t list_size;
	uint16_t pri_size;
	uint32_t ext_size;
};

struct fbfs_ar_data
{
	uint32_t ar_magic;
	uint8_t ver_major;
	uint8_t ver_minor;
	uint16_t list_used;
	uint16_t list_size;
	uint16_t pri_size;
	uint32_t ext_size;
};

struct fbfs_file_record
{
	uint8_t size;
	uint8_t flag;
	uint32_t data_start;
	uint32_t data_size;
	uint32_t data_time;
	char name[1];
};

struct fbfs_fat_bs16
{
	uint8_t jb[3];
	char on[8];
	uint16_t bps;
	uint8_t spc;
	uint16_t nrs;
	uint8_t nf;
	uint16_t nrd;
	uint16_t ts16;
	uint8_t md;
	uint16_t fz16;
	uint16_t spt;
	uint16_t nh;
	uint32_t nhs;
	uint32_t ts32;
	uint8_t dn;
	uint8_t r1;
	uint8_t ebs;
	uint32_t vn;
	char vl[11];
	char fs[8];
	uint8_t bc[448];
	uint16_t bss;
};

struct fbfs_fat_bs32
{
	uint8_t jb[3];
	char on[8];
	uint16_t bps;
	uint8_t spc;
	uint16_t nrs;
	uint8_t nf;
	uint16_t nrd;
	uint16_t ts16;
	uint8_t md;
	uint16_t fz16;
	uint16_t spt;
	uint16_t nh;
	uint32_t nhs;
	uint32_t ts32;
	uint32_t fz32;
	uint16_t ef;
	uint16_t fsv;
	uint32_t rc;
	uint16_t fsi;
	uint16_t bbs;
	uint8_t r1[12];
	uint8_t dn;
	uint8_t r2;
	uint8_t ebs;
	uint32_t vn;
	char vl[11];
	char fs[8];
	uint8_t bc[420];
	uint16_t bss;
};

struct fbfs_menu_text
{
	uint8_t size;
	uint8_t type;
	char title[1];
};
#pragma pack(pop)

typedef char fbfs_mbr_size_must_be_512[(sizeof(struct fbfs_mbr) == 512) ? 1 : -1];
typedef char fbfs_fat16_size_must_be_512[(sizeof(struct fbfs_fat_bs16) == 512) ? 1 : -1];
typedef char fbfs_fat32_size_must_be_512[(sizeof(struct fbfs_fat_bs32) == 512) ? 1 : -1];

struct fbfs_disk
{
	int writable;
	int is_disk;
	int is_file;
	uint32_t partition_offset;
#if defined(_WIN32)
	HANDLE handle;
#else
	int fd;
#endif
};

struct fbfs
{
	struct fbfs_disk *disk;
	int archive_mode;
	uint8_t ver_major;
	uint8_t ver_minor;
	uint32_t name_offset;
	uint32_t boot_base;
	uint32_t boot_size;
	uint32_t list_start;
	uint32_t list_sectors;
	uint32_t list_size;
	uint32_t list_tail;
	uint32_t primary_size;
	uint32_t extended_size;
	uint32_t original_primary_size;
	uint32_t original_extended_size;
	uint32_t total_size;
	uint32_t partition_offset;
	uint32_t archive_size;
	uint8_t *list;
};

static uint32_t fbfs_archive_current_size(struct fbfs *fs);

static char fbfs_error_message[512];

static void
fbfs_get_boot_image(int debug_boot, const uint8_t **image, uint32_t *size)
{
	if (debug_boot)
	{
		*image = fb_mbr_dbg;
		*size = (uint32_t)sizeof(fb_mbr_dbg);
	}
	else
	{
		*image = fb_mbr_rel;
		*size = (uint32_t)sizeof(fb_mbr_rel);
	}
}

static int
fbfs_set_error(int err, const char *fmt, ...)
{
	va_list ap;

	if (fmt)
	{
		va_start(ap, fmt);
		vsnprintf(fbfs_error_message, sizeof(fbfs_error_message), fmt, ap);
		va_end(ap);
	}
	else
	{
		snprintf(fbfs_error_message, sizeof(fbfs_error_message), "%s",
			fbfs_strerror(err));
	}

	return err;
}

const char *
fbfs_strerror(int err)
{
	switch (err)
	{
	case FBFS_OK:
		return "success";
	case FBFS_ERR_INVALID_ARGUMENT:
		return "invalid argument";
	case FBFS_ERR_NO_MEMORY:
		return "not enough memory";
	case FBFS_ERR_OPEN:
		return "open failed";
	case FBFS_ERR_READ:
		return "read failed";
	case FBFS_ERR_WRITE:
		return "write failed";
	case FBFS_ERR_SEEK:
		return "seek failed";
	case FBFS_ERR_SIZE:
		return "invalid size";
	case FBFS_ERR_LOCK:
		return "lock failed";
	case FBFS_ERR_BAD_FS:
		return "not a fbinst filesystem";
	case FBFS_ERR_BAD_VERSION:
		return "unsupported fbinst version";
	case FBFS_ERR_BAD_LIST:
		return "invalid file list";
	case FBFS_ERR_NOT_FOUND:
		return "file not found";
	case FBFS_ERR_ALREADY_EXISTS:
		return "file already exists";
	case FBFS_ERR_NO_SPACE:
		return "not enough space";
	case FBFS_ERR_TOO_LARGE:
		return "item too large";
	case FBFS_ERR_UNSUPPORTED:
		return "unsupported operation";
	case FBFS_ERR_CORRUPT:
		return "corrupt data";
	case FBFS_ERR_IO:
		return "i/o error";
	default:
		return "unknown error";
	}
}

const char *
fbfs_last_error(void)
{
	return fbfs_error_message[0] ? fbfs_error_message : fbfs_strerror(FBFS_OK);
}

static int
fbfs_is_disk_name(const char *path)
{
	if (!path || !path[0])
		return 0;

	if (!strncmp(path, "\\\\.\\", 4))
		return 1;

	if ((strlen(path) == 2) && isalpha((unsigned char)path[0]) && path[1] == ':')
		return 1;

	if (path[0] == '(')
		return 1;

	return 0;
}

static int
fbfs_parse_bios_name(const char *path, char *device, size_t device_size,
	uint32_t *partition)
{
	char type;
	char *end;
	long disk;

	if (!path || path[0] != '(')
		return 0;

	if ((path[1] != 'h' && path[1] != 'f') || path[2] != 'd')
		return 0;

	type = path[1];
	errno = 0;
	disk = strtol(path + 3, &end, 10);
	if (errno || disk < 0)
		return 0;

	*partition = UINT32_MAX;
	if (*end == ',')
	{
		long part;

		errno = 0;
		part = strtol(end + 1, &end, 10);
		if (errno || part < 0)
			return 0;
		*partition = (uint32_t)part;
	}

	if (*end != ')' || end[1])
		return 0;

#if defined(_WIN32)
	if (type == 'h')
		snprintf(device, device_size, "\\\\.\\PhysicalDrive%ld", disk);
	else if (disk < 2)
		snprintf(device, device_size, "\\\\.\\%c:", (int)('A' + disk));
	else
		return 0;
#else
	if (type == 'h')
		snprintf(device, device_size, "/dev/sd%c", (int)('a' + disk));
	else
		snprintf(device, device_size, "/dev/fd%ld", disk);
#endif

	return 1;
}

static int
fbfs_read_partition_offset(struct fbfs_disk *disk, uint32_t partition,
	uint32_t *offset)
{
	uint8_t sector[FBFS_SECTOR_SIZE];
	uint32_t entries[4][2];
	size_t count;
	size_t i;

	if (partition == UINT32_MAX)
	{
		*offset = 0;
		return FBFS_OK;
	}

	if (fbfs_disk_read(disk, 0, sector, 1) != FBFS_OK)
		return FBFS_ERR_READ;

	if (*(uint16_t *)(sector + 0x1fe) != 0xaa55u)
		return fbfs_set_error(FBFS_ERR_BAD_FS, "invalid partition table");

	count = 0;
	for (i = 0; i < 4; i++)
	{
		uint8_t *p;

		p = sector + 0x1be + i * 16;
		if (!p[4])
			continue;

		entries[count][0] = *(uint32_t *)(p + 8);
		entries[count][1] = *(uint32_t *)(p + 12);
		count++;
	}

	if (partition >= count)
		return fbfs_set_error(FBFS_ERR_NOT_FOUND, "partition %u not found",
			partition);

	*offset = entries[partition][0];
	return FBFS_OK;
}

int
fbfs_disk_open(const char *path, uint32_t flags, struct fbfs_disk **out)
{
	struct fbfs_disk *disk;
	char device[260];
	const char *open_path;
	uint32_t partition;
	int writable;
	int create;
	int truncate_file;

	if (!path || !out)
		return fbfs_set_error(FBFS_ERR_INVALID_ARGUMENT, NULL);

	*out = NULL;
	writable = (flags & FBFS_DISK_WRITABLE) != 0;
	create = (flags & FBFS_DISK_CREATE) != 0;
	truncate_file = (flags & FBFS_DISK_TRUNCATE) != 0;
	partition = UINT32_MAX;
	open_path = path;

	if (fbfs_parse_bios_name(path, device, sizeof(device), &partition))
		open_path = device;
	else if ((strlen(path) == 2) && isalpha((unsigned char)path[0]) && path[1] == ':')
	{
#if defined(_WIN32)
		snprintf(device, sizeof(device), "\\\\.\\%c:", toupper((unsigned char)path[0]));
		open_path = device;
#endif
	}

	disk = calloc(1, sizeof(*disk));
	if (!disk)
		return fbfs_set_error(FBFS_ERR_NO_MEMORY, NULL);

	disk->writable = writable;
	disk->is_disk = fbfs_is_disk_name(open_path);
	disk->is_file = !disk->is_disk;

#if defined(_WIN32)
	{
		DWORD access;
		DWORD share;
		DWORD creation;

		access = GENERIC_READ | (writable ? GENERIC_WRITE : 0);
		share = FILE_SHARE_READ | FILE_SHARE_WRITE;
		creation = OPEN_EXISTING;
		if (disk->is_file && create)
			creation = truncate_file ? CREATE_ALWAYS : OPEN_ALWAYS;

		disk->handle = CreateFileA(open_path, access, share, NULL, creation,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, NULL);
		if (disk->handle == INVALID_HANDLE_VALUE)
		{
			free(disk);
			return fbfs_set_error(FBFS_ERR_OPEN, "can't open %s", path);
		}
	}
#else
	{
		int oflags;

		oflags = writable ? O_RDWR : O_RDONLY;
		if (disk->is_file && create)
			oflags |= O_CREAT;
		if (disk->is_file && truncate_file)
			oflags |= O_TRUNC;

		disk->fd = open(open_path, oflags, 0666);
		if (disk->fd < 0)
		{
			free(disk);
			return fbfs_set_error(FBFS_ERR_OPEN, "can't open %s", path);
		}
	}
#endif

	if (fbfs_read_partition_offset(disk, partition, &disk->partition_offset) != FBFS_OK)
	{
		fbfs_disk_close(disk);
		return FBFS_ERR_OPEN;
	}

	*out = disk;
	return FBFS_OK;
}

void
fbfs_disk_close(struct fbfs_disk *disk)
{
	if (!disk)
		return;

#if defined(_WIN32)
	if (disk->handle != INVALID_HANDLE_VALUE)
	{
		if (disk->writable && disk->is_disk)
		{
			DWORD done;
			// 写入新分区表或完成修改后，强制通知 Windows Mount Manager 刷新物理磁盘的分区映射
			DeviceIoControl(disk->handle, IOCTL_DISK_UPDATE_PROPERTIES,
				NULL, 0, NULL, 0, &done, NULL);
		}
		CloseHandle(disk->handle);
	}
#else
	if (disk->fd >= 0)
		close(disk->fd);
#endif

	free(disk);
}

static int
fbfs_disk_seek_bytes(struct fbfs_disk *disk, uint64_t offset)
{
#if defined(_WIN32)
	LARGE_INTEGER li;

	li.QuadPart = (LONGLONG)offset;
	if (!SetFilePointerEx(disk->handle, li, NULL, FILE_BEGIN))
		return fbfs_set_error(FBFS_ERR_SEEK, "seek failed at byte %llu",
			(unsigned long long)offset);
#else
	if (lseek(disk->fd, (off_t)offset, SEEK_SET) != (off_t)offset)
		return fbfs_set_error(FBFS_ERR_SEEK, "seek failed at byte %llu",
			(unsigned long long)offset);
#endif

	return FBFS_OK;
}

int
fbfs_disk_read(struct fbfs_disk *disk, uint32_t sector, void *buffer, uint32_t count)
{
	uint64_t offset;
	uint32_t bytes;

	if (!disk || !buffer)
		return fbfs_set_error(FBFS_ERR_INVALID_ARGUMENT, NULL);

	offset = ((uint64_t)disk->partition_offset + sector) * FBFS_SECTOR_SIZE;
	bytes = count * FBFS_SECTOR_SIZE;
	if (fbfs_disk_seek_bytes(disk, offset) != FBFS_OK)
		return FBFS_ERR_SEEK;

#if defined(_WIN32)
	{
		DWORD done;

		if (!ReadFile(disk->handle, buffer, bytes, &done, NULL) || done != bytes)
			return fbfs_set_error(FBFS_ERR_READ, "read failed at sector %u",
				sector);
	}
#else
	if (read(disk->fd, buffer, bytes) != (ssize_t)bytes)
		return fbfs_set_error(FBFS_ERR_READ, "read failed at sector %u", sector);
#endif

	return FBFS_OK;
}

int
fbfs_disk_write(struct fbfs_disk *disk, uint32_t sector, const void *buffer, uint32_t count)
{
	uint64_t offset;
	uint32_t bytes;

	if (!disk || !buffer)
		return fbfs_set_error(FBFS_ERR_INVALID_ARGUMENT, NULL);

	if (!disk->writable)
		return fbfs_set_error(FBFS_ERR_WRITE, "disk is read-only");

	offset = ((uint64_t)disk->partition_offset + sector) * FBFS_SECTOR_SIZE;
	bytes = count * FBFS_SECTOR_SIZE;
	if (fbfs_disk_seek_bytes(disk, offset) != FBFS_OK)
		return FBFS_ERR_SEEK;

#if defined(_WIN32)
	{
		DWORD done;

		if (!WriteFile(disk->handle, buffer, bytes, &done, NULL) || done != bytes)
			return fbfs_set_error(FBFS_ERR_WRITE, "write failed at sector %u",
				sector);
	}
#else
	if (write(disk->fd, buffer, bytes) != (ssize_t)bytes)
		return fbfs_set_error(FBFS_ERR_WRITE, "write failed at sector %u", sector);
#endif

	return FBFS_OK;
}

uint64_t
fbfs_disk_size(struct fbfs_disk *disk)
{
	if (!disk)
		return UINT64_MAX;

#if defined(_WIN32)
	if (disk->is_disk)
	{
		DISK_GEOMETRY_EX geometry;
		DWORD done;

		if (DeviceIoControl(disk->handle, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
			NULL, 0, &geometry, sizeof(geometry), &done, NULL))
			return ((uint64_t)geometry.DiskSize.QuadPart >> 9);
	}
	else
	{
		LARGE_INTEGER size;

		if (GetFileSizeEx(disk->handle, &size))
			return ((uint64_t)size.QuadPart >> 9);
	}
#else
	{
		struct stat st;

		if (!fstat(disk->fd, &st))
			return ((uint64_t)st.st_size >> 9);
	}
#endif

	return UINT64_MAX;
}

int
fbfs_disk_truncate(struct fbfs_disk *disk, uint32_t sectors)
{
	if (!disk || !disk->is_file)
		return FBFS_OK;

#if defined(_WIN32)
	if (fbfs_disk_seek_bytes(disk, (uint64_t)sectors * FBFS_SECTOR_SIZE) != FBFS_OK)
		return FBFS_ERR_SEEK;
	if (!SetEndOfFile(disk->handle))
		return fbfs_set_error(FBFS_ERR_WRITE, "truncate failed");
#else
	if (ftruncate(disk->fd, (off_t)sectors * FBFS_SECTOR_SIZE))
		return fbfs_set_error(FBFS_ERR_WRITE, "truncate failed");
#endif

	return FBFS_OK;
}

#if defined(_WIN32)
static HANDLE fbfs_locked_volumes[24];
#endif

int
fbfs_disk_lock(struct fbfs_disk *disk)
{
#if defined(_WIN32)
	STORAGE_DEVICE_NUMBER disk_number;
	DWORD done;
	char name[] = "\\\\.\\C:";
	char drive;

	if (!disk || !disk->is_disk)
		return FBFS_OK;

	if (!DeviceIoControl(disk->handle, IOCTL_STORAGE_GET_DEVICE_NUMBER,
		NULL, 0, &disk_number, sizeof(disk_number), &done, NULL))
		return fbfs_set_error(FBFS_ERR_LOCK, "can't query disk number");

	for (drive = 'C'; drive <= 'Z'; drive++)
	{
		HANDLE volume;
		STORAGE_DEVICE_NUMBER volume_number;

		name[4] = drive;
		volume = CreateFileA(name, GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
		if (volume == INVALID_HANDLE_VALUE)
			continue;

		if (!DeviceIoControl(volume, IOCTL_STORAGE_GET_DEVICE_NUMBER,
			NULL, 0, &volume_number, sizeof(volume_number), &done, NULL) ||
			disk_number.DeviceType != volume_number.DeviceType ||
			disk_number.DeviceNumber != volume_number.DeviceNumber)
		{
			CloseHandle(volume);
			continue;
		}

		if (DeviceIoControl(volume, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0,
			&done, NULL))
			fbfs_locked_volumes[drive - 'C'] = volume;
		else
			CloseHandle(volume);
	}
#else
	(void)disk;
#endif

	return FBFS_OK;
}

void
fbfs_disk_unlock_all(void)
{
#if defined(_WIN32)
	size_t i;

	for (i = 0; i < sizeof(fbfs_locked_volumes) / sizeof(fbfs_locked_volumes[0]); i++)
	{
		DWORD done;

		if (!fbfs_locked_volumes[i])
			continue;

		DeviceIoControl(fbfs_locked_volumes[i], IOCTL_DISK_UPDATE_PROPERTIES,
			NULL, 0, NULL, 0, &done, NULL);
		DeviceIoControl(fbfs_locked_volumes[i], FSCTL_DISMOUNT_VOLUME,
			NULL, 0, NULL, 0, &done, NULL);
		CloseHandle(fbfs_locked_volumes[i]);
		fbfs_locked_volumes[i] = NULL;
	}
#endif
}

int
fbfs_disk_list(struct fbfs_disk_entry *entries, size_t capacity, size_t *count)
{
	size_t found;
	int i;

	if (!count)
		return fbfs_set_error(FBFS_ERR_INVALID_ARGUMENT, NULL);

	found = 0;
	for (i = 0; i < 20; i++)
	{
		char name[32];
		struct fbfs_disk *disk;
		uint8_t sector[FBFS_SECTOR_SIZE];
		uint64_t sectors;
		int has_fbinst;

		snprintf(name, sizeof(name), "(hd%d)", i);
		if (fbfs_disk_open(name, 0, &disk) != FBFS_OK)
			continue;

		sectors = fbfs_disk_size(disk);
		has_fbinst = 0;
		if (fbfs_disk_read(disk, 0, sector, 1) == FBFS_OK)
		{
			struct fbfs_mbr *mbr;

			mbr = (struct fbfs_mbr *)sector;
			has_fbinst = (mbr->end_magic == 0xaa55u &&
				mbr->fb_magic == FBFS_MAGIC && mbr->lba == 0);
		}

		if (entries && found < capacity)
		{
			snprintf(entries[found].name, sizeof(entries[found].name), "%s",
				name);
			entries[found].sectors = sectors;
			entries[found].has_fbinst = has_fbinst;
		}

		found++;
		fbfs_disk_close(disk);
	}

	*count = found;
	return FBFS_OK;
}

static void
fbfs_mem_move(uint8_t *dst, const uint8_t *src, size_t size)
{
	memmove(dst, src, size);
}

static void
fbfs_add_mark(uint8_t *buffer, uint32_t sectors, uint32_t base, uint32_t size)
{
	uint8_t *src;
	uint32_t i;

	if (!sectors)
		return;

	memset(buffer + size, 0, sectors * FBFS_SECTOR_SIZE - size);
	src = buffer + (sectors - 1) * FBFS_PRIMARY_SECTOR_DATA;
	for (i = sectors; i > 0; i--)
	{
		uint32_t idx;

		idx = i - 1;
		fbfs_mem_move(buffer + idx * FBFS_SECTOR_SIZE, src,
			FBFS_PRIMARY_SECTOR_DATA);
		*(uint16_t *)(buffer + idx * FBFS_SECTOR_SIZE +
			FBFS_PRIMARY_SECTOR_DATA) = (uint16_t)(base + idx);
		src -= FBFS_PRIMARY_SECTOR_DATA;
	}
}

static void
fbfs_remove_mark(uint8_t *buffer, uint32_t sectors)
{
	uint32_t i;
	uint8_t *dst;

	dst = buffer;
	for (i = 0; i + 1 < sectors; i++)
	{
		dst += FBFS_PRIMARY_SECTOR_DATA;
		buffer += FBFS_SECTOR_SIZE;
		fbfs_mem_move(dst, buffer, FBFS_PRIMARY_SECTOR_DATA);
	}
}

static int
fbfs_is_supported_version(uint8_t major, uint8_t minor)
{
	return major == FBFS_VERSION_MAJOR &&
		(minor == FBFS_VERSION_MINOR_16 || minor == FBFS_VERSION_MINOR_17);
}

static uint32_t
fbfs_name_offset_for_version(uint8_t minor)
{
	return (minor == FBFS_VERSION_MINOR_16) ? 0u : 4u;
}

static int
fbfs_validate_list(struct fbfs *fs)
{
	uint32_t offset;

	offset = 0;
	while (offset < fs->list_size && fs->list[offset])
	{
		uint32_t record_len;
		struct fbfs_file_record *record;
		uint32_t name_offset;
		uint32_t name_limit;

		record = (struct fbfs_file_record *)(fs->list + offset);
		record_len = record->size + 2u;
		if (record_len < 15u || offset + record_len > fs->list_size)
			return fbfs_set_error(FBFS_ERR_BAD_LIST, "invalid file list");

		name_offset = 14u + fs->name_offset;
		if (record_len <= name_offset)
			return fbfs_set_error(FBFS_ERR_BAD_LIST, "invalid file name");

		name_limit = record_len - name_offset;
		if (!memchr(record->name + fs->name_offset, 0, name_limit))
			return fbfs_set_error(FBFS_ERR_BAD_LIST, "unterminated file name");

		offset += record_len;
	}

	if (offset >= fs->list_size)
		return fbfs_set_error(FBFS_ERR_BAD_LIST, "file list is not terminated");

	fs->list_tail = offset;
	return FBFS_OK;
}

int
fbfs_mount(struct fbfs_disk *disk, uint32_t flags, struct fbfs **out)
{
	struct fbfs *fs;
	uint8_t sector[FBFS_SECTOR_SIZE];
	struct fbfs_data data;
	uint32_t list_bytes;

	if (!disk || !out)
		return fbfs_set_error(FBFS_ERR_INVALID_ARGUMENT, NULL);

	*out = NULL;
	if (fbfs_disk_read(disk, 0, sector, 1) != FBFS_OK)
		return FBFS_ERR_READ;

	fs = calloc(1, sizeof(*fs));
	if (!fs)
		return fbfs_set_error(FBFS_ERR_NO_MEMORY, NULL);

	fs->disk = disk;
	if (*(uint32_t *)sector == FBFS_AR_MAGIC)
	{
		struct fbfs_ar_data *ar;

		if (!(flags & FBFS_OPEN_ALLOW_ARCHIVE))
		{
			free(fs);
			return fbfs_set_error(FBFS_ERR_BAD_FS,
				"archive is not allowed for this command");
		}

		ar = (struct fbfs_ar_data *)sector;
		if (!fbfs_is_supported_version(ar->ver_major, ar->ver_minor))
		{
			free(fs);
			return fbfs_set_error(FBFS_ERR_BAD_VERSION,
				"unsupported fbinst version %u.%u",
				ar->ver_major, ar->ver_minor);
		}

		fs->archive_mode = 1;
		fs->ver_major = ar->ver_major;
		fs->ver_minor = ar->ver_minor;
		fs->list_start = 1;
		fs->list_sectors = ar->list_size;
		fs->primary_size = 1 + fs->list_sectors;
		fs->extended_size = FBFS_AR_MAX_SIZE - fs->primary_size;
		fs->original_primary_size = ar->pri_size;
		fs->original_extended_size = ar->ext_size;
		fs->total_size = FBFS_AR_MAX_SIZE;
	}
	else
	{
		struct fbfs_mbr *mbr;

		mbr = (struct fbfs_mbr *)sector;
		if (mbr->fb_magic != FBFS_MAGIC || mbr->end_magic != 0xaa55u)
		{
			free(fs);
			return fbfs_set_error(FBFS_ERR_BAD_FS,
				"fb mbr not initialized");
		}

		fs->archive_mode = 0;
		fs->boot_base = mbr->boot_base;
		fs->partition_offset = *(uint32_t *)(sector + 0x1c6);
		if (fbfs_disk_read(disk, fs->boot_base + 1, sector, 1) != FBFS_OK)
		{
			free(fs);
			return FBFS_ERR_READ;
		}

		memcpy(&data, sector, sizeof(data));
		if (!fbfs_is_supported_version(data.ver_major, data.ver_minor))
		{
			free(fs);
			return fbfs_set_error(FBFS_ERR_BAD_VERSION,
				"unsupported fbinst version %u.%u",
				data.ver_major, data.ver_minor);
		}

		fs->ver_major = data.ver_major;
		fs->ver_minor = data.ver_minor;
		fs->boot_size = data.boot_size;
		fs->list_start = fs->boot_base + 1 + data.boot_size;
		fs->list_sectors = data.list_size;
		fs->primary_size = data.pri_size;
		fs->extended_size = data.ext_size;
		fs->original_primary_size = data.pri_size;
		fs->original_extended_size = data.ext_size;
		fs->total_size = data.pri_size + data.ext_size;
	}

	fs->name_offset = fbfs_name_offset_for_version(fs->ver_minor);
	if (!fs->list_sectors || fs->list_sectors > FBFS_MAX_LIST_SECTORS)
	{
		free(fs);
		return fbfs_set_error(FBFS_ERR_BAD_LIST, "invalid file list size");
	}

	list_bytes = fs->list_sectors * FBFS_SECTOR_SIZE;
	fs->list_size = fs->list_sectors * FBFS_PRIMARY_SECTOR_DATA;
	fs->list = malloc(list_bytes);
	if (!fs->list)
	{
		free(fs);
		return fbfs_set_error(FBFS_ERR_NO_MEMORY, NULL);
	}

	if (fbfs_disk_read(disk, fs->list_start, fs->list, fs->list_sectors) != FBFS_OK)
	{
		fbfs_close(fs);
		return FBFS_ERR_READ;
	}

	fbfs_remove_mark(fs->list, fs->list_sectors);
	if (fbfs_validate_list(fs) != FBFS_OK)
	{
		fbfs_close(fs);
		return FBFS_ERR_BAD_LIST;
	}

	if (fs->archive_mode)
	{
		uint32_t last_start;
		uint32_t last_size;
		uint32_t offset;

		last_start = fs->list_start + fs->list_sectors;
		last_size = 0;
		offset = 0;
		while (offset < fs->list_tail)
		{
			struct fbfs_file_record *record;

			record = (struct fbfs_file_record *)(fs->list + offset);
			last_start = record->data_start;
			last_size = record->data_size;
			offset += record->size + 2u;
		}
		fs->archive_size = last_start + ((last_size + 511u) >> 9);
	}

	*out = fs;
	return FBFS_OK;
}

void
fbfs_close(struct fbfs *fs)
{
	if (!fs)
		return;

	free(fs->list);
	free(fs);
}

int
fbfs_flush(struct fbfs *fs)
{
	uint8_t sector[FBFS_SECTOR_SIZE];
	uint8_t *marked_list;
	uint32_t list_used;
	int err;

	if (!fs)
		return fbfs_set_error(FBFS_ERR_INVALID_ARGUMENT, NULL);

	marked_list = malloc(fs->list_sectors * FBFS_SECTOR_SIZE);
	if (!marked_list)
		return fbfs_set_error(FBFS_ERR_NO_MEMORY, NULL);

	memcpy(marked_list, fs->list, fs->list_sectors * FBFS_PRIMARY_SECTOR_DATA);
	fbfs_add_mark(marked_list, fs->list_sectors, fs->list_start, fs->list_tail);
	list_used = (fs->list_tail / FBFS_PRIMARY_SECTOR_DATA) + 1u;

	if (fs->archive_mode)
	{
		struct fbfs_ar_data *ar;

		memset(sector, 0, sizeof(sector));
		ar = (struct fbfs_ar_data *)sector;
		ar->ar_magic = FBFS_AR_MAGIC;
		ar->ver_major = fs->ver_major;
		ar->ver_minor = fs->ver_minor;
		ar->list_used = (uint16_t)list_used;
		ar->list_size = (uint16_t)fs->list_sectors;
		ar->pri_size = (uint16_t)fs->original_primary_size;
		ar->ext_size = fs->original_extended_size;
		err = fbfs_disk_write(fs->disk, 0, sector, 1);
	}
	else
	{
		struct fbfs_data *data;

		err = fbfs_disk_read(fs->disk, fs->boot_base + 1, sector, 1);
		if (err == FBFS_OK)
		{
			data = (struct fbfs_data *)sector;
			data->list_used = (uint16_t)list_used;
			data->list_size = (uint16_t)fs->list_sectors;
			data->pri_size = (uint16_t)fs->primary_size;
			data->ext_size = fs->extended_size;
			err = fbfs_disk_write(fs->disk, fs->boot_base + 1, sector, 1);
		}
	}

	if (err == FBFS_OK)
		err = fbfs_disk_write(fs->disk, fs->list_start, marked_list,
			fs->list_sectors);

	free(marked_list);

	if (err == FBFS_OK && fs->archive_mode && fs->disk->is_file)
	{
		uint32_t ar_size;

		ar_size = fbfs_archive_current_size(fs);
		fs->archive_size = ar_size;
		err = fbfs_disk_truncate(fs->disk, ar_size);
	}

	return err;
}

static const char *
fbfs_skip_name_prefix(struct fbfs *fs, struct fbfs_file_record *record)
{
	return record->name + fs->name_offset;
}

static const char *
fbfs_normalize_name(const char *name)
{
	if (!name)
		return NULL;

	while (*name == '/' || *name == '\\')
		name++;

	if (!fbfs_strcasecmp(name, FBFS_AR_NAME_PREFIX))
		name += 4;
	else if (!fbfs_strncasecmp(name, FBFS_AR_NAME_PREFIX, 4))
		name += 4;

	while (*name == '/' || *name == '\\')
		name++;

	return *name ? name : NULL;
}

static int
fbfs_make_record_name(struct fbfs *fs, const char *name, char *buffer,
	size_t buffer_size)
{
	const char *normalized;
	size_t needed;

	normalized = fbfs_normalize_name(name);
	if (!normalized)
		return fbfs_set_error(FBFS_ERR_INVALID_ARGUMENT, "empty file name");

	if (fs->name_offset)
		needed = strlen(FBFS_AR_NAME_PREFIX) + strlen(normalized) + 1u;
	else
		needed = strlen(normalized) + 1u;

	if (needed > buffer_size)
		return fbfs_set_error(FBFS_ERR_TOO_LARGE, "file name is too long");

	if (fs->name_offset)
	{
		strcpy(buffer, FBFS_AR_NAME_PREFIX);
		strcat(buffer, normalized);
	}
	else
		strcpy(buffer, normalized);

	return FBFS_OK;
}

static struct fbfs_file_record *
fbfs_record_at(struct fbfs *fs, uint32_t offset)
{
	if (!fs || offset >= fs->list_tail)
		return NULL;

	return (struct fbfs_file_record *)(fs->list + offset);
}

static int
fbfs_find_record_offset(struct fbfs *fs, const char *name, uint32_t *offset_out)
{
	const char *normalized;
	uint32_t offset;

	normalized = fbfs_normalize_name(name);
	if (!normalized)
		return fbfs_set_error(FBFS_ERR_INVALID_ARGUMENT, "empty file name");

	offset = 0;
	while (offset < fs->list_tail)
	{
		struct fbfs_file_record *record;

		record = fbfs_record_at(fs, offset);
		if (!fbfs_strcasecmp(fbfs_skip_name_prefix(fs, record), normalized))
		{
			if (offset_out)
				*offset_out = offset;
			return FBFS_OK;
		}

		offset += record->size + 2u;
	}

	return FBFS_ERR_NOT_FOUND;
}

static void
fbfs_fill_file_info(struct fbfs *fs, struct fbfs_file_record *record,
	struct fbfs_file_info *info)
{
	memset(info, 0, sizeof(*info));
	snprintf(info->name, sizeof(info->name), "%s", fbfs_skip_name_prefix(fs, record));
	info->flag = record->flag;
	info->data_start = record->data_start;
	info->data_size = record->data_size;
	info->data_time = record->data_time;
	info->extended_area = record->data_start >= fs->primary_size;
}

int
fbfs_get_info(struct fbfs *fs, struct fbfs_info *info)
{
	if (!fs || !info)
		return fbfs_set_error(FBFS_ERR_INVALID_ARGUMENT, NULL);

	memset(info, 0, sizeof(*info));
	info->archive_mode = fs->archive_mode;
	info->ver_major = fs->ver_major;
	info->ver_minor = fs->ver_minor;
	info->boot_base = fs->boot_base;
	info->boot_size = fs->boot_size;
	info->primary_size = fs->primary_size;
	info->extended_size = fs->extended_size;
	info->original_primary_size = fs->original_primary_size;
	info->original_extended_size = fs->original_extended_size;
	info->total_size = fs->total_size;
	info->list_start = fs->list_start;
	info->list_sectors = fs->list_sectors;
	info->list_size = fs->list_size;
	info->list_used = (fs->list_tail / FBFS_PRIMARY_SECTOR_DATA) + 1u;
	info->list_tail = fs->list_tail;
	info->archive_size = fs->archive_size;
	return FBFS_OK;
}

size_t
fbfs_file_count(struct fbfs *fs)
{
	size_t count;
	uint32_t offset;

	if (!fs)
		return 0;

	count = 0;
	offset = 0;
	while (offset < fs->list_tail)
	{
		struct fbfs_file_record *record;

		record = fbfs_record_at(fs, offset);
		offset += record->size + 2u;
		count++;
	}

	return count;
}

int
fbfs_get_file(struct fbfs *fs, size_t index, struct fbfs_file_info *info)
{
	size_t current;
	uint32_t offset;

	if (!fs || !info)
		return fbfs_set_error(FBFS_ERR_INVALID_ARGUMENT, NULL);

	current = 0;
	offset = 0;
	while (offset < fs->list_tail)
	{
		struct fbfs_file_record *record;

		record = fbfs_record_at(fs, offset);
		if (current == index)
		{
			fbfs_fill_file_info(fs, record, info);
			return FBFS_OK;
		}

		offset += record->size + 2u;
		current++;
	}

	return fbfs_set_error(FBFS_ERR_NOT_FOUND, "file index out of range");
}

int
fbfs_find_file(struct fbfs *fs, const char *name, struct fbfs_file_info *info)
{
	uint32_t offset;
	int err;

	if (!fs || !info)
		return fbfs_set_error(FBFS_ERR_INVALID_ARGUMENT, NULL);

	err = fbfs_find_record_offset(fs, name, &offset);
	if (err != FBFS_OK)
		return fbfs_set_error(FBFS_ERR_NOT_FOUND, "file %s not found", name);

	fbfs_fill_file_info(fs, fbfs_record_at(fs, offset), info);
	return FBFS_OK;
}

int
fbfs_clear(struct fbfs *fs)
{
	if (!fs)
		return fbfs_set_error(FBFS_ERR_INVALID_ARGUMENT, NULL);

	fs->list_tail = 0;
	fs->list[0] = 0;
	return FBFS_OK;
}

static uint32_t
fbfs_file_sector_count(struct fbfs *fs, struct fbfs_file_record *record)
{
	uint32_t block_size;

	block_size = (record->data_start >= fs->primary_size) ?
		FBFS_SECTOR_SIZE : FBFS_PRIMARY_SECTOR_DATA;
	return (record->data_size + block_size - 1u) / block_size;
}

static int
fbfs_check_space(struct fbfs *fs, uint32_t *start, uint32_t size,
	uint32_t *count, uint32_t begin, uint32_t end, int is_ext)
{
	(void)size;

	if (begin >= fs->primary_size || end <= fs->primary_size)
	{
		if ((begin >= fs->primary_size || !is_ext) && end - begin >= *count)
		{
			*start = begin;
			return 1;
		}
	}
	else
	{
		if (!is_ext && fs->primary_size - begin >= *count)
		{
			*start = begin;
			return 1;
		}

		*count = (size + FBFS_SECTOR_SIZE - 1u) >> 9;
		if (end - fs->primary_size >= *count)
		{
			*start = fs->primary_size;
			return 1;
		}
	}

	return 0;
}

static int
fbfs_find_space(struct fbfs *fs, uint32_t *start, uint32_t size, int is_ext,
	uint32_t *insert_offset)
{
	uint32_t begin;
	uint32_t count;
	uint32_t offset;

	begin = fs->list_start + fs->list_sectors;
	count = (size + FBFS_PRIMARY_SECTOR_DATA - 1u) / FBFS_PRIMARY_SECTOR_DATA;
	offset = 0;

	while (offset < fs->list_tail)
	{
		struct fbfs_file_record *record;

		record = fbfs_record_at(fs, offset);
		if (fbfs_check_space(fs, start, size, &count, begin, record->data_start,
			is_ext))
		{
			*insert_offset = offset;
			return FBFS_OK;
		}

		begin = record->data_start + fbfs_file_sector_count(fs, record);
		offset += record->size + 2u;
	}

	if (!fbfs_check_space(fs, start, size, &count, begin, fs->total_size, is_ext))
		return fbfs_set_error(FBFS_ERR_NO_SPACE, "not enough space");

	*insert_offset = offset;
	return FBFS_OK;
}

static int
fbfs_delete_record(struct fbfs *fs, const char *name)
{
	uint32_t offset;
	uint32_t length;
	int err;

	err = fbfs_find_record_offset(fs, name, &offset);
	if (err != FBFS_OK)
		return FBFS_ERR_NOT_FOUND;

	length = fs->list[offset] + 2u;
	memmove(fs->list + offset, fs->list + offset + length,
		fs->list_tail - offset - length);
	fs->list_tail -= length;
	fs->list[fs->list_tail] = 0;
	return FBFS_OK;
}

static int
fbfs_alloc_record(struct fbfs *fs, const char *name, uint32_t size,
	uint32_t timestamp, uint32_t flags, struct fbfs_file_record **record_out)
{
	char stored_name[260];
	uint32_t start;
	uint32_t insert_offset;
	uint32_t record_len;
	struct fbfs_file_record *record;
	int is_ext;
	int err;

	err = fbfs_make_record_name(fs, name, stored_name, sizeof(stored_name));
	if (err != FBFS_OK)
		return err;

	(void)fbfs_delete_record(fs, name);

	record_len = 14u + (uint32_t)strlen(stored_name) + 1u;
	if (record_len > 255u || fs->list_tail + record_len >= fs->list_size)
		return fbfs_set_error(FBFS_ERR_TOO_LARGE, "file item too long");

	is_ext = (flags & FBFS_FILE_EXTENDED) != 0;
	err = fbfs_find_space(fs, &start, size, is_ext, &insert_offset);
	if (err != FBFS_OK)
		return err;

	if (insert_offset < fs->list_tail)
		memmove(fs->list + insert_offset + record_len, fs->list + insert_offset,
			fs->list_tail - insert_offset);

	record = (struct fbfs_file_record *)(fs->list + insert_offset);
	memset(record, 0, record_len);
	record->size = (uint8_t)(record_len - 2u);
	record->flag = (uint8_t)(flags & 0xffu);
	record->data_start = start;
	record->data_size = size;
	record->data_time = timestamp;
	strcpy(record->name, stored_name);
	fs->list_tail += record_len;
	fs->list[fs->list_tail] = 0;

	if (record_out)
		*record_out = record;

	return FBFS_OK;
}

static int
fbfs_read_record_data(struct fbfs *fs, struct fbfs_file_record *record,
	uint32_t offset, void *buffer, uint32_t length)
{
	uint32_t block_size;
	uint32_t sector;
	uint32_t sector_offset;
	uint8_t local[FBFS_SECTOR_SIZE];
	uint8_t *out;

	if (offset > record->data_size || length > record->data_size - offset)
		return fbfs_set_error(FBFS_ERR_INVALID_ARGUMENT, NULL);

	block_size = (record->data_start >= fs->primary_size) ?
		FBFS_SECTOR_SIZE : FBFS_PRIMARY_SECTOR_DATA;
	sector = record->data_start + offset / block_size;
	sector_offset = offset % block_size;
	out = buffer;

	while (length)
	{
		uint32_t chunk;

		if (block_size == FBFS_SECTOR_SIZE && sector_offset == 0 && length >= FBFS_SECTOR_SIZE)
		{
			uint32_t read_sectors = length / FBFS_SECTOR_SIZE;
			if (fbfs_disk_read(fs->disk, sector, out, read_sectors) != FBFS_OK)
				return FBFS_ERR_READ;
			out += read_sectors * FBFS_SECTOR_SIZE;
			length -= read_sectors * FBFS_SECTOR_SIZE;
			sector += read_sectors;
			continue;
		}

		chunk = block_size - sector_offset;
		if (chunk > length)
			chunk = length;

		if (fbfs_disk_read(fs->disk, sector, local, 1) != FBFS_OK)
			return FBFS_ERR_READ;
		memcpy(out, local + sector_offset, chunk);

		out += chunk;
		length -= chunk;
		sector++;
		sector_offset = 0;
	}

	return FBFS_OK;
}

static void
fbfs_update_archive_size_for_record(struct fbfs *fs, struct fbfs_file_record *record)
{
	uint32_t end;

	if (!fs->archive_mode)
		return;

	end = record->data_start + ((record->data_size + FBFS_SECTOR_SIZE - 1u) >> 9);
	if (end > fs->archive_size)
		fs->archive_size = end;
}

static int
fbfs_write_record_data(struct fbfs *fs, struct fbfs_file_record *record,
	uint32_t offset, const void *buffer, uint32_t length)
{
	uint32_t block_size;
	uint32_t sector;
	uint32_t sector_offset;
	const uint8_t *in;
	uint8_t local[FBFS_SECTOR_SIZE];

	if (offset > record->data_size || length > record->data_size - offset)
		return fbfs_set_error(FBFS_ERR_INVALID_ARGUMENT, NULL);

	block_size = (record->data_start >= fs->primary_size) ?
		FBFS_SECTOR_SIZE : FBFS_PRIMARY_SECTOR_DATA;
	sector = record->data_start + offset / block_size;
	sector_offset = offset % block_size;
	in = buffer;

	while (length)
	{
		uint32_t chunk;

		if (block_size == FBFS_SECTOR_SIZE && sector_offset == 0 && length >= FBFS_SECTOR_SIZE)
		{
			uint32_t write_sectors = length / FBFS_SECTOR_SIZE;
			if (fbfs_disk_write(fs->disk, sector, in, write_sectors) != FBFS_OK)
				return FBFS_ERR_WRITE;
			in += write_sectors * FBFS_SECTOR_SIZE;
			length -= write_sectors * FBFS_SECTOR_SIZE;
			sector += write_sectors;
			continue;
		}

		chunk = block_size - sector_offset;
		if (chunk > length)
			chunk = length;

		memset(local, 0, sizeof(local));
		if (chunk != block_size || sector_offset != 0)
		{
			if (fbfs_disk_read(fs->disk, sector, local, 1) != FBFS_OK)
			{
				fbfs_error_message[0] = 0;
				memset(local, 0, sizeof(local));
				if (block_size == FBFS_PRIMARY_SECTOR_DATA)
					*(uint16_t *)(local + FBFS_PRIMARY_SECTOR_DATA) =
						(uint16_t)sector;
			}
		}

		memcpy(local + sector_offset, in, chunk);
		if (block_size == FBFS_PRIMARY_SECTOR_DATA)
			*(uint16_t *)(local + FBFS_PRIMARY_SECTOR_DATA) = (uint16_t)sector;
		if (fbfs_disk_write(fs->disk, sector, local, 1) != FBFS_OK)
			return FBFS_ERR_WRITE;

		in += chunk;
		length -= chunk;
		sector++;
		sector_offset = 0;
	}

	fbfs_update_archive_size_for_record(fs, record);
	return FBFS_OK;
}

static int
fbfs_zero_record_tail(struct fbfs *fs, struct fbfs_file_record *record,
	uint8_t fill, uint32_t offset)
{
	uint8_t sector[FBFS_SECTOR_SIZE];
	uint32_t block_size;
	uint32_t start_sector;
	uint32_t sector_offset;
	uint32_t remain;

	if (offset >= record->data_size)
		return FBFS_OK;

	block_size = (record->data_start >= fs->primary_size) ?
		FBFS_SECTOR_SIZE : FBFS_PRIMARY_SECTOR_DATA;
	start_sector = record->data_start + offset / block_size;
	sector_offset = offset % block_size;
	remain = record->data_size - offset;

	while (remain)
	{
		uint32_t chunk;

		// 核心吞吐优化：如果处于扇区对齐状态、且为标准512扇区大小的扩展区文件，
		// 采用 128 扇区 (64KB) 大缓冲区对齐合并写入，将物理写入次数减少 128 倍
		if (sector_offset == 0 && block_size == FBFS_SECTOR_SIZE && remain >= FBFS_SECTOR_SIZE * 128)
		{
			uint32_t num_sectors = remain / FBFS_SECTOR_SIZE;
			if (num_sectors > 128)
				num_sectors = 128;

			uint8_t *large_buf = malloc(FBFS_SECTOR_SIZE * num_sectors);
			if (large_buf)
			{
				memset(large_buf, fill, FBFS_SECTOR_SIZE * num_sectors);
				if (fbfs_disk_write(fs->disk, start_sector, large_buf, num_sectors) != FBFS_OK)
				{
					free(large_buf);
					return FBFS_ERR_WRITE;
				}
				free(large_buf);
				remain -= num_sectors * FBFS_SECTOR_SIZE;
				start_sector += num_sectors;
				continue;
			}
		}

		// 如果在具有 510 字节数据限制的主隐藏区 (Primary Area)
		// 仍保持安全的单扇区流式标记写入，由于主隐藏区文件不超过 32MB，性能不受影响
		chunk = block_size - sector_offset;
		if (chunk > remain)
			chunk = remain;

		memset(sector, fill, sizeof(sector));
		if (sector_offset)
		{
			if (fbfs_disk_read(fs->disk, start_sector, sector, 1) != FBFS_OK)
				memset(sector, fill, sizeof(sector));
		}

		memset(sector + sector_offset, fill, chunk);
		if (block_size == FBFS_PRIMARY_SECTOR_DATA)
			*(uint16_t *)(sector + FBFS_PRIMARY_SECTOR_DATA) =
				(uint16_t)start_sector;
		if (fbfs_disk_write(fs->disk, start_sector, sector, 1) != FBFS_OK)
			return FBFS_ERR_WRITE;

		remain -= chunk;
		start_sector++;
		sector_offset = 0;
	}

	return FBFS_OK;
}

int
fbfs_add_buffer(struct fbfs *fs, const char *name, const void *buffer,
	uint32_t size, uint32_t flags)
{
	struct fbfs_file_record *record;
	int err;

	if (!fs || !name || (!buffer && size))
		return fbfs_set_error(FBFS_ERR_INVALID_ARGUMENT, NULL);

	if (!size)
		return fbfs_set_error(FBFS_ERR_SIZE, "empty file");

	err = fbfs_alloc_record(fs, name, size, (uint32_t)time(NULL), flags, &record);
	if (err != FBFS_OK)
		return err;

	err = fbfs_write_record_data(fs, record, 0, buffer, size);
	if (err == FBFS_OK && fs->archive_mode)
	{
		uint32_t end;

		end = record->data_start + ((record->data_size + 511u) >> 9);
		if (end > fs->archive_size)
			fs->archive_size = end;
	}

	return err;
}

int
fbfs_add_stream(struct fbfs *fs, const char *name, FILE *stream, uint32_t size,
	uint32_t timestamp, uint32_t flags)
{
	uint8_t buffer[FBFS_SECTOR_SIZE * FBFS_GLOB_SECTORS];
	struct fbfs_file_record *record;
	uint32_t offset;
	int err;

	if (!fs || !name || !stream)
		return fbfs_set_error(FBFS_ERR_INVALID_ARGUMENT, NULL);

	if (!size)
		return fbfs_set_error(FBFS_ERR_SIZE, "empty file");

	err = fbfs_alloc_record(fs, name, size, timestamp ? timestamp :
		(uint32_t)time(NULL), flags, &record);
	if (err != FBFS_OK)
		return err;

	offset = 0;
	while (offset < size)
	{
		uint32_t chunk;

		chunk = size - offset;
		if (chunk > sizeof(buffer))
			chunk = sizeof(buffer);

		if (fread(buffer, 1, chunk, stream) != chunk)
			return fbfs_set_error(FBFS_ERR_READ, "file read failed");

		err = fbfs_write_record_data(fs, record, offset, buffer, chunk);
		if (err != FBFS_OK)
			return err;

		offset += chunk;
	}

	if (fs->archive_mode)
	{
		uint32_t end;

		end = record->data_start + ((record->data_size + 511u) >> 9);
		if (end > fs->archive_size)
			fs->archive_size = end;
	}

	return FBFS_OK;
}

uint32_t
fbfs_file_time(const char *path)
{
	fbfs_stat_t st;

	if (!path || fbfs_stat(path, &st))
		return (uint32_t)time(NULL);

	if (st.st_mtime < 0)
		return 0;
	if ((uint64_t)st.st_mtime > UINT32_MAX)
		return UINT32_MAX;

	return (uint32_t)st.st_mtime;
}

int
fbfs_add_file(struct fbfs *fs, const char *name, const char *path, uint32_t flags)
{
	FILE *stream;
	fbfs_stat_t st;
	uint32_t size;
	uint32_t timestamp;
	int err;

	if (!fs || !name || !path)
		return fbfs_set_error(FBFS_ERR_INVALID_ARGUMENT, NULL);

	if (fbfs_stat(path, &st))
		return fbfs_set_error(FBFS_ERR_OPEN, "can't stat %s", path);

	if (st.st_size <= 0 || (uint64_t)st.st_size > UINT32_MAX)
		return fbfs_set_error(FBFS_ERR_SIZE, "invalid file size");

	size = (uint32_t)st.st_size;
	timestamp = fbfs_file_time(path);
	stream = fopen(path, "rb");
	if (!stream)
		return fbfs_set_error(FBFS_ERR_OPEN, "can't open file %s", path);

	err = fbfs_add_stream(fs, name, stream, size, timestamp, flags);
	fclose(stream);
	return err;
}

int
fbfs_read_file(struct fbfs *fs, const char *name, void *buffer,
	uint32_t size, uint32_t *written)
{
	uint32_t offset;
	struct fbfs_file_record *record;
	int err;

	if (!fs || !name || !buffer)
		return fbfs_set_error(FBFS_ERR_INVALID_ARGUMENT, NULL);

	err = fbfs_find_record_offset(fs, name, &offset);
	if (err != FBFS_OK)
		return fbfs_set_error(FBFS_ERR_NOT_FOUND, "file %s not found", name);

	record = fbfs_record_at(fs, offset);
	if (size < record->data_size)
		return fbfs_set_error(FBFS_ERR_SIZE, "buffer too small");

	err = fbfs_read_record_data(fs, record, 0, buffer, record->data_size);
	if (err == FBFS_OK && written)
		*written = record->data_size;
	return err;
}

int
fbfs_export_file(struct fbfs *fs, const char *name, const char *path)
{
	uint8_t buffer[FBFS_SECTOR_SIZE * FBFS_GLOB_SECTORS];
	uint32_t offset;
	uint32_t done;
	struct fbfs_file_record *record;
	FILE *stream;
	int err;

	if (!fs || !name || !path)
		return fbfs_set_error(FBFS_ERR_INVALID_ARGUMENT, NULL);

	err = fbfs_find_record_offset(fs, name, &offset);
	if (err != FBFS_OK)
		return fbfs_set_error(FBFS_ERR_NOT_FOUND, "file %s not found", name);

	stream = fopen(path, "wb");
	if (!stream)
		return fbfs_set_error(FBFS_ERR_OPEN, "can't create file %s", path);

	record = fbfs_record_at(fs, offset);
	done = 0;
	while (done < record->data_size)
	{
		uint32_t chunk;

		chunk = record->data_size - done;
		if (chunk > sizeof(buffer))
			chunk = sizeof(buffer);

		err = fbfs_read_record_data(fs, record, done, buffer, chunk);
		if (err != FBFS_OK)
			break;

		if (fwrite(buffer, 1, chunk, stream) != chunk)
		{
			err = fbfs_set_error(FBFS_ERR_WRITE, "file write failed");
			break;
		}

		done += chunk;
	}

	fclose(stream);
	return err;
}

int
fbfs_remove_file(struct fbfs *fs, const char *name)
{
	if (!fs || !name)
		return fbfs_set_error(FBFS_ERR_INVALID_ARGUMENT, NULL);

	if (fbfs_delete_record(fs, name) != FBFS_OK)
		return fbfs_set_error(FBFS_ERR_NOT_FOUND, "file %s not found", name);

	return FBFS_OK;
}

static int
fbfs_copy_record_bytes(struct fbfs *dst_fs, struct fbfs_file_record *dst,
	struct fbfs *src_fs, struct fbfs_file_record *src, uint32_t size)
{
	uint8_t buffer[FBFS_SECTOR_SIZE * FBFS_GLOB_SECTORS];
	uint32_t done;
	int err;

	done = 0;
	while (done < size)
	{
		uint32_t chunk;

		chunk = size - done;
		if (chunk > sizeof(buffer))
			chunk = sizeof(buffer);

		err = fbfs_read_record_data(src_fs, src, done, buffer, chunk);
		if (err != FBFS_OK)
			return err;

		err = fbfs_write_record_data(dst_fs, dst, done, buffer, chunk);
		if (err != FBFS_OK)
			return err;

		done += chunk;
	}

	return FBFS_OK;
}

int
fbfs_resize_file(struct fbfs *fs, const char *name, uint32_t size,
	uint8_t fill, uint32_t flags)
{
	uint32_t offset;
	struct fbfs_file_record old_record;
	struct fbfs_file_record *record;
	uint32_t old_size;
	uint32_t old_flags;
	uint32_t timestamp;
	int err;

	if (!fs || !name || !size)
		return fbfs_set_error(FBFS_ERR_INVALID_ARGUMENT, NULL);

	err = fbfs_find_record_offset(fs, name, &offset);
	if (err == FBFS_OK)
	{
		record = fbfs_record_at(fs, offset);
		if (record->data_size >= size)
		{
			record->data_size = size;
			return FBFS_OK;
		}

		memcpy(&old_record, record, sizeof(old_record));
		old_size = record->data_size;
		old_flags = record->flag;
		timestamp = record->data_time;
		flags = old_flags | ((record->data_start >= fs->primary_size) ?
			FBFS_FILE_EXTENDED : 0u);
		(void)fbfs_delete_record(fs, name);
	}
	else
	{
		memset(&old_record, 0, sizeof(old_record));
		old_size = 0;
		old_flags = flags;
		timestamp = (uint32_t)time(NULL);
	}

	err = fbfs_alloc_record(fs, name, size, timestamp, flags, &record);
	if (err != FBFS_OK)
		return err;

	if (old_size)
	{
		err = fbfs_copy_record_bytes(fs, record, fs, &old_record, old_size);
		if (err != FBFS_OK)
			return err;
	}

	record->flag = (uint8_t)(old_flags & 0xffu);
	if (flags & FBFS_FILE_EXTENDED)
		record->flag |= FBFS_FILE_EXTENDED;

	return fbfs_zero_record_tail(fs, record, fill, old_size);
}

int
fbfs_copy_file(struct fbfs *fs, const char *old_name, const char *new_name)
{
	uint32_t offset;
	struct fbfs_file_record old_record;
	struct fbfs_file_record *new_record;
	int err;

	if (!fs || !old_name || !new_name)
		return fbfs_set_error(FBFS_ERR_INVALID_ARGUMENT, NULL);

	err = fbfs_find_record_offset(fs, old_name, &offset);
	if (err != FBFS_OK)
		return fbfs_set_error(FBFS_ERR_NOT_FOUND, "file %s not found",
			old_name);

	memcpy(&old_record, fbfs_record_at(fs, offset), sizeof(old_record));
	(void)fbfs_delete_record(fs, new_name);
	err = fbfs_alloc_record(fs, new_name, old_record.data_size,
		old_record.data_time, old_record.flag, &new_record);
	if (err != FBFS_OK)
		return err;

	new_record->flag = old_record.flag;
	return fbfs_copy_record_bytes(fs, new_record, fs, &old_record,
		old_record.data_size);
}

int
fbfs_move_file(struct fbfs *fs, const char *old_name, const char *new_name)
{
	char stored_name[260];
	uint32_t offset;
	uint32_t old_len;
	uint32_t new_len;
	struct fbfs_file_record *record;
	int err;

	if (!fs || !old_name || !new_name)
		return fbfs_set_error(FBFS_ERR_INVALID_ARGUMENT, NULL);

	(void)fbfs_delete_record(fs, new_name);
	err = fbfs_find_record_offset(fs, old_name, &offset);
	if (err != FBFS_OK)
		return fbfs_set_error(FBFS_ERR_NOT_FOUND, "file %s not found",
			old_name);

	err = fbfs_make_record_name(fs, new_name, stored_name, sizeof(stored_name));
	if (err != FBFS_OK)
		return err;

	record = fbfs_record_at(fs, offset);
	old_len = record->size + 2u;
	new_len = 14u + (uint32_t)strlen(stored_name) + 1u;
	if (new_len > 255u || fs->list_tail + new_len - old_len >= fs->list_size)
		return fbfs_set_error(FBFS_ERR_TOO_LARGE, "file item too long");

	if (new_len != old_len)
	{
		memmove(fs->list + offset + new_len, fs->list + offset + old_len,
			fs->list_tail - offset - old_len);
		fs->list_tail += new_len - old_len;
		fs->list[fs->list_tail] = 0;
		record = fbfs_record_at(fs, offset);
		record->size = (uint8_t)(new_len - 2u);
	}

	strcpy(record->name, stored_name);
	return FBFS_OK;
}

int
fbfs_pack(struct fbfs *fs)
{
	uint8_t buffer[FBFS_SECTOR_SIZE * FBFS_GLOB_SECTORS];
	uint32_t base;
	uint32_t offset;
	int err;

	if (!fs)
		return fbfs_set_error(FBFS_ERR_INVALID_ARGUMENT, NULL);

	base = fs->list_start + fs->list_sectors;
	offset = 0;
	while (offset < fs->list_tail)
	{
		struct fbfs_file_record *record;
		struct fbfs_file_record old_record;
		uint32_t sectors;
		uint32_t block_size;
		uint32_t copied;

		record = fbfs_record_at(fs, offset);
		block_size = (record->data_start >= fs->primary_size) ?
			FBFS_SECTOR_SIZE : FBFS_PRIMARY_SECTOR_DATA;
		sectors = (record->data_size + block_size - 1u) / block_size;

		if (base < fs->primary_size && record->data_start >= fs->primary_size)
			base = fs->primary_size;

		if (record->data_start != base)
		{
			memcpy(&old_record, record, sizeof(old_record));
			record->data_start = base;
			copied = 0;
			while (copied < old_record.data_size)
			{
				uint32_t chunk;

				chunk = old_record.data_size - copied;
				if (chunk > sizeof(buffer))
					chunk = sizeof(buffer);
				err = fbfs_read_record_data(fs, &old_record, copied,
					buffer, chunk);
				if (err != FBFS_OK)
					return err;
				err = fbfs_write_record_data(fs, record, copied,
					buffer, chunk);
				if (err != FBFS_OK)
					return err;
				copied += chunk;
			}
		}

		base = record->data_start + sectors;
		offset += record->size + 2u;
	}

	return FBFS_OK;
}

int
fbfs_check(struct fbfs *fs)
{
	uint8_t buffer[FBFS_SECTOR_SIZE * FBFS_GLOB_SECTORS];
	uint32_t start;

	if (!fs)
		return fbfs_set_error(FBFS_ERR_INVALID_ARGUMENT, NULL);

	if (fs->archive_mode)
		return fbfs_set_error(FBFS_ERR_UNSUPPORTED,
			"check is not supported for archive files");

	start = 0;
	while (start < fs->primary_size)
	{
		uint32_t count;
		uint32_t i;

		count = fs->primary_size - start;
		if (count > FBFS_GLOB_SECTORS)
			count = FBFS_GLOB_SECTORS;

		if (fbfs_disk_read(fs->disk, start, buffer, count) != FBFS_OK)
			return FBFS_ERR_READ;

		for (i = 0; i < count; i++, start++)
		{
			struct fbfs_mbr *mbr;
			int bad;

			mbr = (struct fbfs_mbr *)(buffer + i * FBFS_SECTOR_SIZE);
			if (start <= fs->boot_base)
				bad = (mbr->end_magic != 0xaa55u || mbr->lba != start);
			else
				bad = (*(uint16_t *)(buffer + i * FBFS_SECTOR_SIZE +
					FBFS_PRIMARY_SECTOR_DATA) != (uint16_t)start);

			if (bad)
				return fbfs_set_error(FBFS_ERR_CORRUPT,
					"check failed at sector %u", start);
		}
	}

	return FBFS_OK;
}

int
fbfs_syslinux_patch(struct fbfs *fs, const char *name)
{
	uint32_t offset;
	struct fbfs_file_record *record;
	uint8_t buffer[FBFS_SECTOR_SIZE * FBFS_GLOB_SECTORS];
	uint8_t *pa;
	uint16_t *pw;
	uint16_t *ps;
	uint32_t *pc;
	uint32_t *p;
	uint32_t start;
	uint32_t checksum;
	uint32_t dwords;
	uint32_t sectors;
	int ver;
	int err;

	if (!fs || !name)
		return fbfs_set_error(FBFS_ERR_INVALID_ARGUMENT, NULL);

	err = fbfs_find_record_offset(fs, name, &offset);
	if (err != FBFS_OK)
		return fbfs_set_error(FBFS_ERR_NOT_FOUND, "file %s not found", name);

	record = fbfs_record_at(fs, offset);
	record->flag |= FBFS_FILE_SYSLINUX | FBFS_FILE_EXTENDED;
	sectors = (record->data_size + FBFS_SECTOR_SIZE - 1u) >> 9;
	if (sectors <= 2 || sectors > FBFS_GLOB_SECTORS ||
		record->data_start < fs->primary_size)
		return fbfs_set_error(FBFS_ERR_SIZE, "invalid size for ldlinux.bin");

	if (fbfs_disk_read(fs->disk, record->data_start, buffer, sectors) != FBFS_OK)
		return FBFS_ERR_READ;

	if (memcmp(buffer + 0x202, "SYSLINUX", 8))
		return fbfs_set_error(FBFS_ERR_BAD_FS, "not a valid ldlinux.bin");

	pa = buffer + 0x200;
	while (pa < buffer + 0x400 && *(uint32_t *)pa != 0x3eb202feu)
		pa += 4;

	if (pa >= buffer + 0x400)
		return fbfs_set_error(FBFS_ERR_BAD_FS, "syslinux signature not found");

	start = record->data_start + 1u - fs->partition_offset;
	ver = buffer[0x20b] - '0';
	if (ver == 3)
	{
		*(uint32_t *)(buffer + 0x1f8) = start;
		*(uint16_t *)(buffer + 0x1fe) = 0xaa55u;
		pw = (uint16_t *)(pa + 8);
		ps = (uint16_t *)(pa + 10);
		pc = (uint32_t *)(pa + 12);
		p = (uint32_t *)(pa + 16);
		(void)p;
	}
	else if (ver == 4)
	{
		uint16_t epa_ofs;
		uint16_t ofs;

		epa_ofs = *(uint16_t *)(pa + 22);
		if (0x200u + epa_ofs + 18u >= sizeof(buffer))
			return fbfs_set_error(FBFS_ERR_BAD_FS, "invalid syslinux epa");

		ofs = *(uint16_t *)(buffer + 0x200 + epa_ofs + 14);
		if ((uint32_t)ofs + 4u > sizeof(buffer))
			return fbfs_set_error(FBFS_ERR_BAD_FS, "invalid syslinux offset");
		*(uint32_t *)(buffer + ofs) = start;

		ofs = *(uint16_t *)(buffer + 0x200 + epa_ofs + 16);
		if ((uint32_t)ofs + 4u > sizeof(buffer))
			return fbfs_set_error(FBFS_ERR_BAD_FS, "invalid syslinux offset");
		*(uint32_t *)(buffer + ofs) = 0xffffffffu;

		pw = (uint16_t *)(pa + 12);
		ps = (uint16_t *)(pa + 8);
		pc = (uint32_t *)(pa + 16);
		ofs = *(uint16_t *)(buffer + 0x200 + epa_ofs + 10);
		if (0x200u + ofs >= sizeof(buffer))
			return fbfs_set_error(FBFS_ERR_BAD_FS, "invalid syslinux offset");
		p = (uint32_t *)(buffer + 0x200 + ofs);
		(void)p;
	}
	else
		return fbfs_set_error(FBFS_ERR_BAD_FS, "invalid ldlinux.bin version %d",
			ver);

	dwords = (record->data_size - FBFS_SECTOR_SIZE) >> 2;
	*pw = (uint16_t)dwords;
	*ps = (ver == 3) ? 0u : 1u;
	*pc = 0;

	checksum = 0x3eb202feu;
	for (p = (uint32_t *)(buffer + 0x200); p < (uint32_t *)(buffer + 0x200) + dwords;
		p++)
		checksum -= *p;
	*pc = checksum;

	return fbfs_disk_write(fs->disk, record->data_start, buffer, 2);
}

static uint32_t
fbfs_archive_current_size(struct fbfs *fs)
{
	uint32_t offset;
	uint32_t last_start;
	uint32_t last_size;

	last_start = fs->list_start + fs->list_sectors;
	last_size = 0;
	offset = 0;
	while (offset < fs->list_tail)
	{
		struct fbfs_file_record *record;

		record = fbfs_record_at(fs, offset);
		last_start = record->data_start;
		last_size = record->data_size;
		offset += record->size + 2u;
	}

	return last_start + ((last_size + FBFS_SECTOR_SIZE - 1u) >> 9);
}

int
fbfs_create_archive(struct fbfs_disk *disk, const struct fbfs_archive_options *options)
{
	uint8_t sector[FBFS_SECTOR_SIZE];
	uint8_t *list;
	struct fbfs_ar_data *data;
	uint32_t primary_size;
	uint32_t extended_size;
	uint32_t list_size;
	uint32_t list_sectors;
	uint8_t version_minor;
	int err;

	if (!disk)
		return fbfs_set_error(FBFS_ERR_INVALID_ARGUMENT, NULL);

	primary_size = options && options->primary_size ?
		options->primary_size : FBFS_MIN_PRI_SIZE;
	extended_size = options ? options->extended_size : 0;
	list_size = options && options->list_size ?
		options->list_size : FBFS_DEFAULT_LIST_SIZE;
	version_minor = options && options->version_minor ?
		options->version_minor : FBFS_VERSION_MINOR_16;

	if (!fbfs_is_supported_version(FBFS_VERSION_MAJOR, version_minor))
		return fbfs_set_error(FBFS_ERR_BAD_VERSION, "invalid fbinst version");
	if (primary_size < FBFS_MIN_PRI_SIZE || primary_size > FBFS_MAX_PRI_SIZE)
		return fbfs_set_error(FBFS_ERR_SIZE, "invalid primary data size");

	list_sectors = (list_size + FBFS_PRIMARY_SECTOR_DATA - 1u) /
		FBFS_PRIMARY_SECTOR_DATA;
	if (!list_sectors || list_sectors > FBFS_MAX_LIST_SECTORS)
		return fbfs_set_error(FBFS_ERR_SIZE, "file list too large");

	memset(sector, 0, sizeof(sector));
	data = (struct fbfs_ar_data *)sector;
	data->ar_magic = FBFS_AR_MAGIC;
	data->ver_major = FBFS_VERSION_MAJOR;
	data->ver_minor = version_minor;
	data->list_used = 1;
	data->list_size = (uint16_t)list_sectors;
	data->pri_size = (uint16_t)primary_size;
	data->ext_size = extended_size;

	list = calloc(list_sectors, FBFS_SECTOR_SIZE);
	if (!list)
		return fbfs_set_error(FBFS_ERR_NO_MEMORY, NULL);
	fbfs_add_mark(list, list_sectors, 1, 0);

	err = fbfs_disk_write(disk, 0, sector, 1);
	if (err == FBFS_OK)
		err = fbfs_disk_write(disk, 1, list, list_sectors);
	if (err == FBFS_OK)
		err = fbfs_disk_truncate(disk, 1 + list_sectors);

	free(list);
	return err;
}

int
fbfs_save_archive(struct fbfs *fs, const char *path, uint32_t list_size)
{
	struct fbfs_disk *disk;
	struct fbfs_archive_options options;
	struct fbfs *archive;
	uint32_t offset;
	int err;

	if (!fs || !path)
		return fbfs_set_error(FBFS_ERR_INVALID_ARGUMENT, NULL);

	if (!list_size)
		list_size = fs->list_size;

	memset(&options, 0, sizeof(options));
	options.primary_size = fs->archive_mode ?
		fs->original_primary_size : fs->primary_size;
	options.extended_size = fs->archive_mode ?
		fs->original_extended_size : fs->extended_size;
	options.list_size = list_size;
	options.version_minor = fs->ver_minor;

	err = fbfs_disk_open(path, FBFS_DISK_WRITABLE | FBFS_DISK_CREATE |
		FBFS_DISK_TRUNCATE, &disk);
	if (err != FBFS_OK)
		return err;

	err = fbfs_create_archive(disk, &options);
	if (err != FBFS_OK)
	{
		fbfs_disk_close(disk);
		return err;
	}

	err = fbfs_mount(disk, FBFS_OPEN_ALLOW_ARCHIVE, &archive);
	if (err != FBFS_OK)
	{
		fbfs_disk_close(disk);
		return err;
	}

	offset = 0;
	while (offset < fs->list_tail)
	{
		struct fbfs_file_record *source;
		struct fbfs_file_record *target;
		const char *name;

		source = fbfs_record_at(fs, offset);
		name = fbfs_skip_name_prefix(fs, source);
		err = fbfs_alloc_record(archive, name, source->data_size,
			source->data_time, source->flag | FBFS_FILE_EXTENDED, &target);
		if (err != FBFS_OK)
			break;

		target->flag = source->flag;
		err = fbfs_copy_record_bytes(archive, target, fs, source,
			source->data_size);
		if (err != FBFS_OK)
			break;

		offset += source->size + 2u;
	}

	if (err == FBFS_OK)
	{
		archive->archive_size = fbfs_archive_current_size(archive);
		err = fbfs_flush(archive);
	}

	fbfs_close(archive);
	fbfs_disk_close(disk);
	return err;
}

int
fbfs_load_archive(struct fbfs *fs, const char *path)
{
	struct fbfs_disk *disk;
	struct fbfs *archive;
	uint32_t offset;
	int err;

	if (!fs || !path)
		return fbfs_set_error(FBFS_ERR_INVALID_ARGUMENT, NULL);

	err = fbfs_disk_open(path, 0, &disk);
	if (err != FBFS_OK)
		return err;

	err = fbfs_mount(disk, FBFS_OPEN_ALLOW_ARCHIVE, &archive);
	if (err != FBFS_OK)
	{
		fbfs_disk_close(disk);
		return err;
	}

	offset = 0;
	while (offset < archive->list_tail)
	{
		struct fbfs_file_record *source;
		struct fbfs_file_record *target;
		const char *name;
		uint32_t flags;

		source = fbfs_record_at(archive, offset);
		name = fbfs_skip_name_prefix(archive, source);
		flags = source->flag;
		err = fbfs_alloc_record(fs, name, source->data_size,
			source->data_time, flags, &target);
		if (err != FBFS_OK)
			break;

		target->flag = source->flag;
		err = fbfs_copy_record_bytes(fs, target, archive, source,
			source->data_size);
		if (err != FBFS_OK)
			break;

		if (target->flag & FBFS_FILE_SYSLINUX)
		{
			err = fbfs_syslinux_patch(fs, name);
			if (err != FBFS_OK)
				break;
		}

		offset += source->size + 2u;
	}

	fbfs_close(archive);
	fbfs_disk_close(disk);
	return err;
}

uint32_t
fbfs_parse_size(const char *text, int *ok)
{
	char *end;
	uint64_t value;

	if (ok)
		*ok = 0;
	if (!text || !*text)
		return 0;

	errno = 0;
	value = strtoull(text, &end, 0);
	if (errno)
		return 0;

	if (*end == 'k' || *end == 'K')
	{
		value <<= 1;
		end++;
	}
	else if (*end == 'm' || *end == 'M')
	{
		value <<= 11;
		end++;
	}
	else if (*end == 'g' || *end == 'G')
	{
		value <<= 21;
		end++;
	}

	if (*end || value > UINT32_MAX)
		return 0;

	if (ok)
		*ok = 1;
	return (uint32_t)value;
}

static int
fbfs_zero_range(struct fbfs_disk *disk, uint32_t start, uint32_t count)
{
	uint8_t zero[FBFS_SECTOR_SIZE * FBFS_GLOB_SECTORS];

	memset(zero, 0, sizeof(zero));
	while (count)
	{
		uint32_t chunk;

		chunk = count > FBFS_GLOB_SECTORS ? FBFS_GLOB_SECTORS : count;
		if (fbfs_disk_write(disk, start, zero, chunk) != FBFS_OK)
			return FBFS_ERR_WRITE;
		start += chunk;
		count -= chunk;
	}

	return FBFS_OK;
}

static void
fbfs_lba_to_chs(uint32_t lba, uint8_t *data)
{
	uint32_t cylinder;

	cylinder = (lba / (63u * 255u)) & 0x3ffu;
	lba %= 63u * 255u;
	data[0] = (uint8_t)(lba / 63u);
	data[1] = (uint8_t)((lba % 63u) + 1u);
	if (cylinder > 255u)
		data[1] = (uint8_t)(data[1] + ((cylinder >> 8) * 64u));
	data[2] = (uint8_t)cylinder;
}

static uint32_t
fbfs_get_part_offset(const uint8_t *sector)
{
	uint32_t min_offset;
	int i;

	min_offset = UINT32_MAX;
	for (i = 0x1be; i < 0x1fe; i += 16)
	{
		uint32_t lba;

		if (!sector[i + 4])
			continue;

		lba = *(const uint32_t *)(sector + i + 8);
		if (lba && lba < min_offset)
			min_offset = lba;
	}

	return min_offset;
}

static void
fbfs_config_mbr(uint8_t *sector, uint32_t max_sectors, int chs_mode, int is_zip)
{
	struct fbfs_mbr *mbr;

	mbr = (struct fbfs_mbr *)sector;
	if (!max_sectors)
		max_sectors = mbr->max_sec ? (mbr->max_sec & 0x7fu) : 63u;
	if (chs_mode)
		max_sectors |= 0x80u;
	mbr->max_sec = (uint8_t)max_sectors;

	if (is_zip)
	{
		sector[0x26] = 0x29;
		memcpy(sector + 3, "MSWIN4.1", 8);
	}
}

static void
fbfs_init_boot_mbr(uint8_t *sector, const uint8_t *boot_image,
	uint32_t boot_image_size, uint32_t base)
{
	struct fbfs_mbr *mbr;

	memset(sector, 0, FBFS_SECTOR_SIZE);
	if (boot_image_size >= FBFS_SECTOR_SIZE)
		memcpy(sector, boot_image, FBFS_SECTOR_SIZE);
	mbr = (struct fbfs_mbr *)sector;
	mbr->boot_base = (uint16_t)base;
	mbr->fb_magic = FBFS_MAGIC;
	mbr->end_magic = 0xaa55u;
}

static int
fbfs_sync_mbr_raw(struct fbfs_disk *disk, uint8_t *sector, uint32_t base,
	int copy_bpb, int force)
{
	uint32_t i;

	if (force && fbfs_disk_lock(disk) != FBFS_OK)
		return FBFS_ERR_LOCK;

	for (i = 0; i <= base; i++)
	{
		uint8_t temp[FBFS_SECTOR_SIZE];
		int j;

		memcpy(temp, sector, sizeof(temp));
		((struct fbfs_mbr *)temp)->lba = (uint16_t)i;
		if (i)
		{
			for (j = 0x1be; j < 0x1fe; j += 16)
			{
				uint32_t start;
				uint32_t size;

				if (!temp[j + 4])
					continue;

				start = *(uint32_t *)(temp + j + 8);
				size = *(uint32_t *)(temp + j + 12);
				if (start)
					start--;
				*(uint32_t *)(temp + j + 8) = start;
				fbfs_lba_to_chs(start, temp + j + 1);
				fbfs_lba_to_chs(start + size - 1u, temp + j + 5);
			}

			if (copy_bpb)
				((struct fbfs_fat_bs16 *)temp)->nrs--;
		}

		if (fbfs_disk_write(disk, i, temp, 1) != FBFS_OK)
			return FBFS_ERR_WRITE;
	}

	return FBFS_OK;
}



#if defined(_WIN32)
static BOOLEAN FormatExSuccess = FALSE;

static BOOLEAN WINAPI
FormatExCallback(
	int Command,
	DWORD Modifier,
	PVOID Argument
)
{
	switch (Command)
	{
	case 0: // PROGRESS
		if (Argument)
		{
			DWORD percent = *(DWORD *)Argument;
			printf("\rFormat progress: %u%%", percent);
			fflush(stdout);
		}
		break;
	case 1:  // DONEWITHSTRUCTURE (结构建立完毕)
	case 15: // STRUCTUREPROGRESS (元数据结构构建进度)
		// 静默处理，避免控制台打印 Unhandled 杂音
		break;
	case 8:  // VOLUMEINUSE (询问是否强制卸载)
	case 10: // UNKNOWNA / FORCEFORMAT
		if (Argument)
		{
			*(PBOOLEAN)Argument = TRUE; // 回应 TRUE 强制卸载并继续
		}
		break;
	case 14: // OUTPUT
		if (Argument)
		{
			typedef struct { ULONG Lines; PCHAR Output; } TEXTOUTPUT;
			TEXTOUTPUT *out = (TEXTOUTPUT *)Argument;
			if (out && out->Output)
			{
				printf("[FMIFS] %s", out->Output);
			}
		}
		break;
	case 11: // DONE
		if (Argument)
		{
			BOOLEAN status = *(BOOLEAN *)Argument;
			FormatExSuccess = status;
		}
		break;
	default:
		// 打印其他未处理的指令辅助诊断
		printf("[FMIFS] Unhandled Callback Command: %d\n", Command);
		break;
	}
	return TRUE;
}

typedef VOID (WINAPI *PFORMATEX)(
	PWCHAR DriveRoot,
	DWORD MediaFlag,
	PWCHAR Format,
	PWCHAR Label,
	BOOL QuickFormat,
	DWORD ClusterSize,
	PVOID Callback
);

#if defined(_WIN32)
static void fbfs_to_wchar(const char *src, WCHAR *dst, int max_chars)
{
	int len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, src, -1, dst, max_chars);
	if (len <= 0)
	{
		MultiByteToWideChar(CP_ACP, 0, src, -1, dst, max_chars);
	}
}
#endif

static void fbfs_set_fat_label(char *dest_vl, const char *label)
{
	int i;
	memset(dest_vl, ' ', 11);
	if (!label || !*label)
		return;
	for (i = 0; i < 11 && label[i]; i++)
	{
		unsigned char c = (unsigned char)label[i];
		if (c >= 'a' && c <= 'z')
			dest_vl[i] = (char)(c - 'a' + 'A');
		else
			dest_vl[i] = (char)c;
	}
}

static int fbfs_format_external(struct fbfs_disk *disk, int fs_type, uint32_t unit_size, uint32_t partition_start, const char *label)
{
	STORAGE_DEVICE_NUMBER disk_number;
	DWORD done;
	int found = 0;
	int retries = 5;
	HANDLE hFind;
	char volume_name[MAX_PATH];
	char target_volume[MAX_PATH] = {0};
	const WCHAR *fs_name;
	WCHAR w_label[128] = {0};

	if (!disk->is_disk)
		return fbfs_set_error(FBFS_ERR_UNSUPPORTED, "NTFS/exFAT/ReFS format is only supported on physical disks");

	switch (fs_type) {
	case FBFS_FORMAT_NTFS:  fs_name = L"NTFS";  break;
	case FBFS_FORMAT_EXFAT: fs_name = L"exFAT"; break;
	case FBFS_FORMAT_REFS:  fs_name = L"ReFS";  break;
	case FBFS_FORMAT_FAT32: fs_name = L"FAT32"; break;
	case FBFS_FORMAT_FAT16: fs_name = L"FAT";   break;
	default:
		return fbfs_set_error(FBFS_ERR_INVALID_ARGUMENT, "Unknown external filesystem type");
	}

	// 解除 fbinst 对全盘所有分区的锁定
	fbfs_disk_unlock_all();

	DeviceIoControl(disk->handle, IOCTL_DISK_UPDATE_PROPERTIES, NULL, 0, NULL, 0, &done, NULL);

	if (!DeviceIoControl(disk->handle, IOCTL_STORAGE_GET_DEVICE_NUMBER, NULL, 0, &disk_number, sizeof(disk_number), &done, NULL))
		return fbfs_set_error(FBFS_ERR_IO, "Can't query disk number");

	// 等待 Windows 底层创建对应的卷设备对象，并进行无死角枚举
	while (retries-- > 0 && !found) {
		Sleep(1000);
		hFind = FindFirstVolumeA(volume_name, sizeof(volume_name));
		if (hFind != INVALID_HANDLE_VALUE) {
			do {
				HANDLE volume;
				char open_name[MAX_PATH];
				snprintf(open_name, sizeof(open_name), "%s", volume_name);
				size_t len = strlen(open_name);
				if (len > 0 && open_name[len - 1] == '\\') {
					open_name[len - 1] = '\0'; // 移除尾部反斜杠用于 CreateFile 打开
				}

				volume = CreateFileA(open_name, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
				if (volume != INVALID_HANDLE_VALUE) {
					VOLUME_DISK_EXTENTS extents;
					if (DeviceIoControl(volume, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, NULL, 0, &extents, sizeof(extents), &done, NULL)) {
						if (extents.NumberOfDiskExtents > 0) {
							// 极高精密匹配：底层物理磁盘序号一致，且分区的 StartingOffset 字节物理起始地址完全一致！
							if (extents.Extents[0].DiskNumber == disk_number.DeviceNumber &&
								extents.Extents[0].StartingOffset.QuadPart == (LONGLONG)partition_start * FBFS_SECTOR_SIZE) {
								snprintf(target_volume, sizeof(target_volume), "%s", volume_name);
								found = 1;
								CloseHandle(volume);
								break;
							}
						}
					}
					CloseHandle(volume);
				}
			} while (FindNextVolumeA(hFind, volume_name, sizeof(volume_name)));
			FindVolumeClose(hFind);
		}
	}

	if (!found) {
		fprintf(stderr, "\nfbinst: warning: Could not auto-detect the Volume GUID of the new partition.\n"
						"Please format it manually as %S.\n", fs_name);
		return FBFS_OK;
	}

	HMODULE ifsModule = LoadLibraryA("fmifs.dll");
	if (!ifsModule) {
		return fbfs_set_error(FBFS_ERR_IO, "Failed to load fmifs.dll");
	}

	PFORMATEX pFormatEx = (PFORMATEX)GetProcAddress(ifsModule, "FormatEx");
	if (!pFormatEx) {
		FreeLibrary(ifsModule);
		return fbfs_set_error(FBFS_ERR_IO, "Failed to find FormatEx in fmifs.dll");
	}

	WCHAR w_target_volume[MAX_PATH];
	MultiByteToWideChar(CP_ACP, 0, target_volume, -1, w_target_volume, MAX_PATH);

	// 微软 FormatEx 接口限制：如果使用 Volume GUID 作为 DriveRoot，传入时绝不能携带尾部反斜杠，否则执行将立即失败
	size_t w_len = wcslen(w_target_volume);
	if (w_len > 0 && w_target_volume[w_len - 1] == L'\\') {
		w_target_volume[w_len - 1] = L'\0';
	}

	printf("\nFormatting volume %S to %S using FMIFS...\n", w_target_volume, fs_name);

	if (label) {
		fbfs_to_wchar(label, w_label, 128);
	}

	FormatExSuccess = FALSE;
	pFormatEx(w_target_volume, 0x0C, (PWCHAR)fs_name, w_label, TRUE, unit_size * FBFS_SECTOR_SIZE, FormatExCallback);

	FreeLibrary(ifsModule);

	if (!FormatExSuccess) {
		return fbfs_set_error(FBFS_ERR_WRITE, "FormatEx failed to format the volume");
	}

	printf("\nFormat completed successfully.\n");
	return FBFS_OK;
}
#else
#if defined(_WIN32)
static void fbfs_to_wchar(const char *src, WCHAR *dst, int max_chars)
{
	int len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, src, -1, dst, max_chars);
	if (len <= 0)
	{
		MultiByteToWideChar(CP_ACP, 0, src, -1, dst, max_chars);
	}
}
#endif

static void fbfs_set_fat_label(char *dest_vl, const char *label)
{
	int i;
	memset(dest_vl, ' ', 11);
	if (!label || !*label)
		return;
	for (i = 0; i < 11 && label[i]; i++)
	{
		unsigned char c = (unsigned char)label[i];
		if (c >= 'a' && c <= 'z')
			dest_vl[i] = (char)(c - 'a' + 'A');
		else
			dest_vl[i] = (char)c;
	}
}

static int fbfs_format_external(struct fbfs_disk *disk, int fs_type, uint32_t unit_size, uint32_t partition_start, const char *label)
{
	fprintf(stderr, "\nfbinst: warning: External format only supported on Windows. Please format manually.\n");
	return FBFS_OK;
}
#endif

static void fbfs_make_fat_label_entry(uint8_t *entry, const char *label)
{
	int i;
	memset(entry, 0, 32);
	memset(entry, ' ', 11);
	entry[0x0B] = 0x08; // ATTR_VOLUME_ID
	if (!label || !*label)
	{
		memcpy(entry, "NO NAME    ", 11);
		return;
	}
	for (i = 0; i < 11 && label[i]; i++)
	{
		unsigned char c = (unsigned char)label[i];
		if (c >= 'a' && c <= 'z')
			entry[i] = (char)(c - 'a' + 'A');
		else
			entry[i] = (char)c;
	}
}

static int
fbfs_format_fat16(struct fbfs_disk *disk, uint32_t start, uint32_t sectors,
	uint32_t unit_size, int align, const char *label, int vbr_type)
{
	uint8_t sector[FBFS_SECTOR_SIZE];
	uint8_t root_sector[FBFS_SECTOR_SIZE];
	struct fbfs_fat_bs16 *bs;
	uint32_t table[][2] = {
		{8400u, 0u}, {32680u, 2u}, {262144u, 4u},
		{524288u, 8u}, {1048576u, 16u}, {2097152u, 32u},
		{4194304u, 64u}, {0xffffffffu, 0u}
	};
	uint32_t i;
	uint32_t j;
	uint32_t fat_count;
	uint32_t fat_size;
	uint32_t root_dir;

	memset(sector, 0, sizeof(sector));
	bs = (struct fbfs_fat_bs16 *)sector;
	bs->jb[0] = 0xeb;
	bs->jb[1] = 0x3c;
	bs->jb[2] = 0x90;
	memcpy(bs->on, "MSWIN4.1", 8);
	bs->bps = FBFS_SECTOR_SIZE;
	bs->nrs = 1;
	bs->nf = 2;
	bs->nrd = (sectors < FBFS_MIN_FAT16_SIZE) ? 0xf0u :
		((start & 1u) ? 0x200u : 0x1f0u);
	bs->md = (sectors < FBFS_MIN_FAT16_SIZE) ? 0xf0u : 0xf8u;
	bs->spt = 63;
	bs->nh = 255;
	bs->nhs = start;
	bs->dn = 0x80;
	bs->ebs = 0x29;
	fbfs_set_fat_label(bs->vl, label);
	if (vbr_type == 1)
	{
		memcpy(bs->bc, fat32_nt60_bc, sizeof(fat32_nt60_bc) > sizeof(bs->bc) ? sizeof(bs->bc) : sizeof(fat32_nt60_bc));
	}
	else if (vbr_type == 2)
	{
		memcpy(bs->bc, fat32_nt52_bc, sizeof(fat32_nt52_bc) > sizeof(bs->bc) ? sizeof(bs->bc) : sizeof(fat32_nt52_bc));
	}
	memcpy(bs->fs, "FAT16   ", 8);
	if (sectors < FBFS_MIN_FAT16_SIZE)
		bs->fs[4] = '2';
	bs->bss = 0xaa55u;
	if (sectors < 65536u)
		bs->ts16 = (uint16_t)sectors;
	else
		bs->ts32 = sectors;

	if (sectors == 2880u)
	{
		bs->spc = 1;
		bs->fz16 = 9;
		bs->spt = 18;
		bs->nh = 2;
	}
	else if (sectors == 5760u)
	{
		bs->spc = 2;
		bs->fz16 = 9;
		bs->spt = 36;
		bs->nh = 2;
	}
	else
	{
		if (unit_size)
			bs->spc = (uint8_t)unit_size;
		else
		{
			i = 0;
			while (sectors > table[i][0])
				i++;
			if (!table[i][1])
				return fbfs_set_error(FBFS_ERR_SIZE, "invalid size for fat16");
			bs->spc = (uint8_t)table[i][1];
		}

		i = sectors - (bs->nrs + ((bs->nrd * 32u) + bs->bps - 1u) / bs->bps);
		j = (256u * bs->spc) + bs->nf;
		bs->fz16 = (uint16_t)((i + (j - 1u)) / j);

		if (align)
		{
			uint32_t begin;
			uint32_t next;

			begin = start + bs->nrs + bs->fz16 * 2u +
				(bs->nrd * 32u) / FBFS_SECTOR_SIZE;
			next = (begin + bs->spc - 1u) / bs->spc;
			bs->fz16 = (uint16_t)(bs->fz16 + (next * bs->spc - begin) / 2u);
		}

		i = sectors - (bs->nrs + ((bs->nrd * 32u) + bs->bps - 1u) / bs->bps);
		i /= bs->spc;
		if (i >= 65525u)
			return fbfs_set_error(FBFS_ERR_SIZE, "unit size invalid for fat16");
	}

	if (fbfs_disk_write(disk, start, sector, 1) != FBFS_OK)
		return FBFS_ERR_WRITE;
	if (fbfs_zero_range(disk, start + 1, bs->nrs - 1u) != FBFS_OK)
		return FBFS_ERR_WRITE;

	fat_count = bs->nf;
	fat_size = bs->fz16;
	root_dir = (bs->nrd * 32u + 511u) >> 9;
	memset(sector, 0, sizeof(sector));
	if (sectors < FBFS_MIN_FAT16_SIZE)
		*(uint32_t *)sector = 0x00fffff8u;
	else
		*(uint32_t *)sector = 0xfffffff8u;

	start += bs->nrs;
	for (i = 0; i < fat_count; i++)
	{
		if (fbfs_disk_write(disk, start, sector, 1) != FBFS_OK)
			return FBFS_ERR_WRITE;
		if (fbfs_zero_range(disk, start + 1, fat_size - 1u) != FBFS_OK)
			return FBFS_ERR_WRITE;
		start += fat_size;
	}

	// Create root directory with volume label entry
	memset(root_sector, 0, sizeof(root_sector));
	fbfs_make_fat_label_entry(root_sector, label);
	if (fbfs_disk_write(disk, start, root_sector, 1) != FBFS_OK)
		return FBFS_ERR_WRITE;

	return fbfs_zero_range(disk, start + 1, root_dir - 1u);
}

static int
fbfs_format_fat32(struct fbfs_disk *disk, uint32_t start, uint32_t sectors,
	uint32_t unit_size, int align, const char *label, int vbr_type)
{
	uint8_t sectors3[FBFS_SECTOR_SIZE * 3u];
	uint8_t sector[FBFS_SECTOR_SIZE];
	uint8_t root_sector[FBFS_SECTOR_SIZE];
	struct fbfs_fat_bs32 *bs;
	uint32_t table[][2] = {
		{66600u, 0u}, {532480u, 1u}, {16777216u, 8u},
		{33554432u, 16u}, {67108864u, 32u}, {0xffffffffu, 64u}
	};
	uint32_t i;
	uint32_t j;
	uint32_t fat;
	uint32_t spc;

	memset(sectors3, 0, sizeof(sectors3));
	bs = (struct fbfs_fat_bs32 *)sectors3;
	bs->jb[0] = 0xeb;
	bs->jb[1] = 0x58;
	bs->jb[2] = 0x90;
	memcpy(bs->on, "MSWIN4.1", 8);
	bs->bps = FBFS_SECTOR_SIZE;
	bs->nrs = (uint16_t)(32u + (start & 1u));
	bs->nf = 2;
	bs->md = 0xf8;
	bs->spt = 63;
	bs->nh = 255;
	bs->nhs = start;
	bs->ts32 = sectors;
	bs->rc = 2;
	bs->fsi = 1;
	bs->bbs = 6;
	bs->dn = 0x80;
	bs->ebs = 0x29;
	fbfs_set_fat_label(bs->vl, label);
	if (vbr_type == 1)
	{
		memcpy(bs->bc, fat32_nt60_bc, sizeof(fat32_nt60_bc) > sizeof(bs->bc) ? sizeof(bs->bc) : sizeof(fat32_nt60_bc));
	}
	else if (vbr_type == 2)
	{
		memcpy(bs->bc, fat32_nt52_bc, sizeof(fat32_nt52_bc) > sizeof(bs->bc) ? sizeof(bs->bc) : sizeof(fat32_nt52_bc));
	}
	memcpy(bs->fs, "FAT32   ", 8);
	bs->bss = 0xaa55u;

	*(uint32_t *)(sectors3 + 0x200) = 0x41615252u;
	*(uint32_t *)(sectors3 + 0x3e4) = 0x61417272u;
	*(uint32_t *)(sectors3 + 0x3e8) = 0xffffffffu;
	*(uint32_t *)(sectors3 + 0x3ec) = 0xffffffffu;
	*(uint16_t *)(sectors3 + 0x3fe) = 0xaa55u;
	*(uint16_t *)(sectors3 + 0x5fe) = 0xaa55u;

	if (unit_size)
		bs->spc = (uint8_t)unit_size;
	else
	{
		i = 0;
		while (sectors > table[i][0])
			i++;
		if (!table[i][1])
			return fbfs_set_error(FBFS_ERR_SIZE, "invalid size for fat32");
		bs->spc = (uint8_t)table[i][1];
	}

	i = sectors - (uint32_t)(bs->nrs + ((bs->nrd * 32u) + bs->bps - 1u) /
		bs->bps);
	j = ((256u * bs->spc) + bs->nf) >> 1;
	bs->fz32 = (i + (j - 1u)) / j;

	if (align)
	{
		uint32_t begin;
		uint32_t next;

		begin = start + bs->nrs + bs->fz32 * 2u;
		next = (begin + bs->spc - 1u) / bs->spc;
		bs->fz32 += (next * bs->spc - begin) / 2u;
	}

	i = sectors - (uint32_t)(bs->nrs + ((bs->nrd * 32u) + bs->bps - 1u) /
		bs->bps);
	i /= bs->spc;
	if (i < 65525u)
		return fbfs_set_error(FBFS_ERR_SIZE, "unit size invalid for fat32");

	if (fbfs_disk_write(disk, start, sectors3, 3) != FBFS_OK)
		return FBFS_ERR_WRITE;
	if (fbfs_zero_range(disk, start + 3, bs->bbs - 3u) != FBFS_OK)
		return FBFS_ERR_WRITE;
	if (fbfs_disk_write(disk, start + bs->bbs, sectors3, 3) != FBFS_OK)
		return FBFS_ERR_WRITE;
	if (fbfs_zero_range(disk, start + bs->bbs + 3,
		bs->nrs - bs->bbs - 3u) != FBFS_OK)
		return FBFS_ERR_WRITE;

	fat = bs->fz32;
	spc = bs->spc;
	memset(sector, 0, sizeof(sector));
	*(uint32_t *)(sector + 0) = 0x0ffffff8u;
	*(uint32_t *)(sector + 4) = 0x0fffffffu;
	*(uint32_t *)(sector + 8) = 0x0fffffffu;

	start += bs->nrs;
	for (i = 0; i < bs->nf; i++)
	{
		if (fbfs_disk_write(disk, start, sector, 1) != FBFS_OK)
			return FBFS_ERR_WRITE;
		if (fbfs_zero_range(disk, start + 1, fat - 1u) != FBFS_OK)
			return FBFS_ERR_WRITE;
		start += fat;
	}

	// Create root directory with volume label entry
	memset(root_sector, 0, sizeof(root_sector));
	fbfs_make_fat_label_entry(root_sector, label);
	if (fbfs_disk_write(disk, start, root_sector, 1) != FBFS_OK)
		return FBFS_ERR_WRITE;

	return fbfs_zero_range(disk, start + 1, spc - 1u);
}

int
fbfs_format(struct fbfs_disk *disk, const struct fbfs_format_options *options)
{
	struct fbfs_format_options opt;
	uint8_t sector[FBFS_SECTOR_SIZE];
	uint8_t *list;
	uint64_t disk_sectors;
	uint32_t max_size;
	uint32_t total_size;
	uint32_t partition_size;
	uint32_t list_sectors;
	uint32_t boot_sectors;
	uint32_t boot_tail_size;
	uint32_t data_start;
	uint32_t tail_start;
	uint32_t main_part_size;
	uint32_t avail_space;
	uint32_t mid_part_size;
	uint32_t tail_part_size;
	uint32_t main_start;
	uint32_t mid_start;
	const uint8_t *boot_image;
	uint32_t boot_image_size;
	int fat32;
	int is_ext_fs;
	uint8_t sys_id;
	int err;

	if (!disk)
		return fbfs_set_error(FBFS_ERR_INVALID_ARGUMENT, NULL);

	memset(&opt, 0, sizeof(opt));
	opt.primary_size = FBFS_MIN_PRI_SIZE;
	opt.list_size = FBFS_DEFAULT_LIST_SIZE;
	opt.base = FBFS_DEFAULT_BASE_SIZE;
	opt.fat_type = FBFS_FORMAT_FAT_AUTO;
	opt.nand_align = FBFS_MIN_NAND_ALIGN;
	opt.version_minor = FBFS_VERSION_MINOR_16;
	if (options)
	{
		uint32_t default_primary;
		uint32_t default_list;
		int default_fat;
		uint32_t default_align;
		uint8_t default_version;

		default_primary = opt.primary_size;
		default_list = opt.list_size;
		default_fat = opt.fat_type;
		default_align = opt.nand_align;
		default_version = opt.version_minor;
		opt = *options;
		if (!opt.primary_size)
			opt.primary_size = default_primary;
		if (!opt.list_size)
			opt.list_size = default_list;
		/* Only reset fat_type to default if it is explicitly unset (FAT_AUTO) */
		if (opt.fat_type == FBFS_FORMAT_FAT_AUTO)
			opt.fat_type = default_fat;
		if (!opt.nand_align)
			opt.nand_align = default_align;
		if (!opt.version_minor)
			opt.version_minor = default_version;
	}

	if (opt.raw_mbr_mode)
	{
		uint8_t sector[FBFS_SECTOR_SIZE];
		uint32_t disk_sectors;
		uint32_t total_sectors;
		uint32_t x_offset;
		uint32_t sizes[4] = {0};
		int max_idx = -1;
		int count = 0;
		int i;

		disk_sectors = (uint32_t)fbfs_disk_size(disk);
		if (disk_sectors == UINT32_MAX)
			return fbfs_set_error(FBFS_ERR_SIZE, "can't get disk size");

		total_sectors = disk_sectors;

		// 锁盘安全保护：在格式化写入任何物理扇区前，必须锁定全盘所有分区，防止被系统占用拦截
		if (fbfs_disk_lock(disk) != FBFS_OK)
			return FBFS_ERR_LOCK;

		// 1. Calculate explicit sizes and locate the 'max' partition
		uint32_t align_sectors = 2048;
		uint32_t used_sectors = align_sectors; // Initial MBR / offset padding
		for (i = 0; i < 4; i++)
		{
			if (opt.raw_parts[i].size != 0)
			{
				count++;
				if (opt.raw_parts[i].size == UINT32_MAX)
				{
					if (max_idx != -1)
						return fbfs_set_error(FBFS_ERR_INVALID_ARGUMENT, "only one partition can be marked as 'max'");
					max_idx = i;
				}
				else
				{
					used_sectors += opt.raw_parts[i].size;
				}
			}
		}

		if (count == 0)
			return fbfs_set_error(FBFS_ERR_INVALID_ARGUMENT, "no partitions defined for raw format");

		if (used_sectors >= total_sectors)
			return fbfs_set_error(FBFS_ERR_SIZE, "disk size is too small for the defined partitions");

		// Assign size to the 'max' partition
		if (max_idx != -1)
		{
			sizes[max_idx] = total_sectors - used_sectors;
		}

		for (i = 0; i < 4; i++)
		{
			if (opt.raw_parts[i].size != 0 && i != max_idx)
			{
				sizes[i] = opt.raw_parts[i].size;
			}
		}

		// 2. Clear first 2048 sectors to ensure clean installation
		if (fbfs_zero_range(disk, 0, align_sectors) != FBFS_OK)
			return FBFS_ERR_WRITE;

		// 3. Construct MBR
		memset(sector, 0, sizeof(sector));
		*(uint16_t *)(sector + 0x1fe) = 0xaa55u;

		x_offset = align_sectors;
		for (i = 0; i < 4; i++)
		{
			if (sizes[i] > 0)
			{
				uint32_t p = 0x1beu + i * 16u;
				uint8_t sys_id;
				
				switch (opt.raw_parts[i].fat_type)
				{
				case FBFS_FORMAT_FAT16:
					sys_id = 0x0e; break;
				case FBFS_FORMAT_FAT32:
					sys_id = 0x0c; break;
				case FBFS_FORMAT_NTFS:
				case FBFS_FORMAT_EXFAT:
				case FBFS_FORMAT_REFS:
				default:
					sys_id = 0x07; break;
				}

				if (opt.active_slot == i + 1)
				{
					sector[p + 0] = 0x80; // Active boot indicator
				}
				else
				{
					sector[p + 0] = 0x00;
				}

				fbfs_lba_to_chs(x_offset, sector + p + 1);
				sector[p + 4] = sys_id;
				fbfs_lba_to_chs(x_offset + sizes[i] - 1u, sector + p + 5);
				*(uint32_t *)(sector + p + 8) = x_offset;
				*(uint32_t *)(sector + p + 12) = sizes[i];

				x_offset += sizes[i];
			}
		}

		// Write MBR
		if (fbfs_disk_write(disk, 0, sector, 1) != FBFS_OK)
			return FBFS_ERR_WRITE;

		// 4. Format each partition
		x_offset = align_sectors;
		for (i = 0; i < 4; i++)
		{
			if (sizes[i] > 0)
			{
				err = FBFS_OK;
				switch (opt.raw_parts[i].fat_type)
				{
				case FBFS_FORMAT_FAT16:
					err = fbfs_format_fat16(disk, x_offset, sizes[i], 0, 0, opt.raw_parts[i].label, opt.vbr_type);
					break;
				case FBFS_FORMAT_FAT32:
					err = fbfs_format_fat32(disk, x_offset, sizes[i], 0, 0, opt.raw_parts[i].label, opt.vbr_type);
					break;
				case FBFS_FORMAT_NTFS:
				case FBFS_FORMAT_EXFAT:
				case FBFS_FORMAT_REFS:
#if defined(_WIN32)
					fbfs_disk_lock(disk);
#endif
					err = fbfs_format_external(disk, opt.raw_parts[i].fat_type, 0, x_offset, opt.raw_parts[i].label);
					break;
				}

				if (err != FBFS_OK)
					return err;

				x_offset += sizes[i];
			}
		}

		return FBFS_OK;
	}

	if (!fbfs_is_supported_version(FBFS_VERSION_MAJOR, opt.version_minor))
		return fbfs_set_error(FBFS_ERR_BAD_VERSION, "invalid fbinst version");
	if (opt.primary_size < FBFS_MIN_PRI_SIZE || opt.primary_size > FBFS_MAX_PRI_SIZE)
		return fbfs_set_error(FBFS_ERR_SIZE, "primary data size invalid");
	if ((opt.nand_align < FBFS_MIN_NAND_ALIGN) ||
		(((opt.nand_align + 1u) & opt.nand_align) != 0))
		return fbfs_set_error(FBFS_ERR_SIZE, "invalid alignment value");

	list_sectors = (opt.list_size + FBFS_PRIMARY_SECTOR_DATA - 1u) /
		FBFS_PRIMARY_SECTOR_DATA;
	if (!list_sectors || list_sectors > FBFS_MAX_LIST_SECTORS)
		return fbfs_set_error(FBFS_ERR_SIZE, "file list too large");

	fbfs_get_boot_image(opt.debug_boot, &boot_image, &boot_image_size);
	if (boot_image_size <= FBFS_SECTOR_SIZE)
		return fbfs_set_error(FBFS_ERR_CORRUPT, "invalid boot image");
	boot_tail_size = boot_image_size - FBFS_SECTOR_SIZE;
	boot_sectors = (boot_tail_size + FBFS_PRIMARY_SECTOR_DATA - 1u) /
		FBFS_PRIMARY_SECTOR_DATA;

	disk_sectors = fbfs_disk_size(disk);
	if (disk_sectors == UINT64_MAX || disk_sectors > UINT32_MAX)
		return fbfs_set_error(FBFS_ERR_SIZE, "can't get disk size");
	max_size = (uint32_t)disk_sectors;

	if (opt.extended_max)
	{
		uint32_t aligned_max = max_size & ~opt.nand_align;
		if (aligned_max > opt.primary_size + opt.base + opt.nand_align)
		{
			opt.extended_size = aligned_max - opt.primary_size - opt.base - opt.nand_align - 1u;
		}
		else
		{
			opt.extended_size = 0;
		}
	}

	total_size = opt.raw ? opt.base :
		((opt.primary_size + opt.extended_size + opt.nand_align) &
			~opt.nand_align);
	if (total_size >= max_size)
		return fbfs_set_error(FBFS_ERR_SIZE, "the disk is too small");

	avail_space = max_size - total_size;
	main_part_size = 0;
	mid_part_size = opt.mid_size;
	tail_part_size = 0;

	if (opt.tail_max)
	{
		if (!opt.partition_size)
			return fbfs_set_error(FBFS_ERR_SIZE, "--tail max requires a fixed main partition size via --size");
		
		main_part_size = opt.partition_size;
		if (main_part_size + mid_part_size >= avail_space)
			return fbfs_set_error(FBFS_ERR_SIZE, "sum of main and middle partitions exceeds available disk space");
		
		tail_part_size = avail_space - main_part_size - mid_part_size;
	}
	else
	{
		uint32_t total_formatted = opt.partition_size;
		if (!total_formatted || total_formatted > avail_space)
			total_formatted = avail_space;

		tail_part_size = opt.tail_size;
		if (tail_part_size + mid_part_size >= total_formatted)
			return fbfs_set_error(FBFS_ERR_SIZE, "sum of tail and middle partitions exceeds specified formatted space");

		main_part_size = total_formatted - tail_part_size - mid_part_size;
	}

	main_start = total_size;
	mid_start = main_start + main_part_size;
	tail_start = mid_start + mid_part_size;

	fat32 = opt.fat_type == FBFS_FORMAT_FAT_AUTO ?
		(main_part_size >= FBFS_DEF_FAT32_SIZE) :
		(opt.fat_type == FBFS_FORMAT_FAT32);
	is_ext_fs = (opt.fat_type == FBFS_FORMAT_NTFS || opt.fat_type == FBFS_FORMAT_EXFAT || opt.fat_type == FBFS_FORMAT_REFS);
	if (opt.fat_type == FBFS_FORMAT_REFS)
		sys_id = 0x07; /* ReFS: use same type byte as NTFS */
	else if (is_ext_fs)
		sys_id = 0x07;
	else
		sys_id = fat32 ? 0x0c : 0x0e;
	partition_size = opt.partition_size;
	if (!partition_size || partition_size > max_size - total_size)
		partition_size = max_size - total_size;

	partition_size = opt.partition_size;
	if (!partition_size || partition_size > max_size - total_size)
		partition_size = max_size - total_size;

	if (opt.raw)
	{
		if (!opt.force)
			return fbfs_set_error(FBFS_ERR_UNSUPPORTED,
				"--raw requires --force");

		memset(sector, 0, sizeof(sector));
		if (opt.base)
		{
			sector[0x1be] = 0x80;
			fbfs_lba_to_chs(opt.base, sector + 0x1bf);
			sector[0x1c2] = sys_id;
			fbfs_lba_to_chs(opt.base + partition_size - 1u,
				sector + 0x1c3);
			*(uint32_t *)(sector + 0x1c6) = opt.base;
			*(uint32_t *)(sector + 0x1ca) = partition_size;
			*(uint16_t *)(sector + 0x1fe) = 0xaa55u;
			if (fbfs_disk_write(disk, 0, sector, 1) != FBFS_OK)
				return FBFS_ERR_WRITE;
			if (opt.base > 1 && fbfs_zero_range(disk, 1, opt.base - 1u) != FBFS_OK)
				return FBFS_ERR_WRITE;
		}

		return fat32 ?
			fbfs_format_fat32(disk, opt.base, partition_size, opt.unit_size, opt.align, opt.label, opt.vbr_type) :
			fbfs_format_fat16(disk, opt.base, partition_size, opt.unit_size, opt.align, opt.label, opt.vbr_type);
	}

	if (!opt.force)
	{
		uint32_t min_part;

		if (fbfs_disk_read(disk, 0, sector, 1) != FBFS_OK)
			return FBFS_ERR_READ;
		min_part = fbfs_get_part_offset(sector);
		if (min_part == UINT32_MAX)
			opt.force = 1;
		else
		{
			if (min_part < opt.primary_size + opt.extended_size)
				return fbfs_set_error(FBFS_ERR_SIZE,
					"offset of data partition too small; use --force");
			if (!opt.extended_size)
				opt.extended_size = min_part - opt.primary_size;
		}
	}

	if (opt.force)
	{
		fbfs_init_boot_mbr(sector, boot_image, boot_image_size, opt.base);
		if (!opt.extended_max)
		{
			sector[0x1be] = 0x80;
			fbfs_lba_to_chs(main_start, sector + 0x1bf);
			sector[0x1c2] = sys_id;
			fbfs_lba_to_chs(main_start + main_part_size - 1u, sector + 0x1c3);
			*(uint32_t *)(sector + 0x1c6) = main_start;
			*(uint32_t *)(sector + 0x1ca) = main_part_size;
		}

		uint32_t mid_slot_offset = 0;
		uint32_t tail_slot_offset = 0;

		if (mid_part_size > 0)
		{
			mid_slot_offset = 0x1ce;   // Slot 2
			tail_slot_offset = 0x1de;  // Slot 3
		}
		else if (tail_part_size > 0)
		{
			tail_slot_offset = 0x1ce;  // Slot 2
		}

		if (mid_part_size > 0)
		{
			uint8_t mid_sys_id;
			switch (opt.mid_fat_type) {
			case FBFS_FORMAT_NTFS:
			case FBFS_FORMAT_REFS:
				mid_sys_id = 0x07; break;
			case FBFS_FORMAT_FAT16:
				mid_sys_id = 0x0e; break;
			case FBFS_FORMAT_FAT32:
			default:
				mid_sys_id = 0x0c; break;
			}
			sector[mid_slot_offset] = 0x00;
			fbfs_lba_to_chs(mid_start, sector + mid_slot_offset + 1);
			sector[mid_slot_offset + 4] = mid_sys_id;
			fbfs_lba_to_chs(mid_start + mid_part_size - 1u, sector + mid_slot_offset + 5);
			*(uint32_t *)(sector + mid_slot_offset + 8) = mid_start;
			*(uint32_t *)(sector + mid_slot_offset + 12) = mid_part_size;
		}

		if (tail_part_size > 0)
		{
			uint8_t tail_sys_id;
			switch (opt.tail_fat_type) {
			case FBFS_FORMAT_NTFS:
			case FBFS_FORMAT_REFS:
				tail_sys_id = 0x07; break;
			case FBFS_FORMAT_FAT16:
				tail_sys_id = 0x0e; break;
			case FBFS_FORMAT_FAT32:
			default:
				tail_sys_id = 0x0c; break;
			}
			sector[tail_slot_offset] = 0x00;
			fbfs_lba_to_chs(tail_start, sector + tail_slot_offset + 1);
			sector[tail_slot_offset + 4] = tail_sys_id;
			fbfs_lba_to_chs(tail_start + tail_part_size - 1u, sector + tail_slot_offset + 5);
			*(uint32_t *)(sector + tail_slot_offset + 8) = tail_start;
			*(uint32_t *)(sector + tail_slot_offset + 12) = tail_part_size;
		}
	}
	else
	{
		uint8_t saved_table[0x46];

		if (fbfs_disk_read(disk, 0, sector, 1) != FBFS_OK)
			return FBFS_ERR_READ;
		memcpy(saved_table, sector + 0x1b8, sizeof(saved_table));
		fbfs_init_boot_mbr(sector, boot_image, boot_image_size, opt.base);
		memcpy(sector + 0x1b8, saved_table, sizeof(saved_table));
	}

	fbfs_config_mbr(sector, opt.max_sectors, opt.chs_mode, opt.zip);
	err = fbfs_sync_mbr_raw(disk, sector, opt.base, 0, opt.force);
	if (err != FBFS_OK)
		return err;

	{
		uint8_t *boot_buffer;
		struct fbfs_data *data;

		boot_buffer = calloc(boot_sectors, FBFS_SECTOR_SIZE);
		if (!boot_buffer)
			return fbfs_set_error(FBFS_ERR_NO_MEMORY, NULL);
		memcpy(boot_buffer, boot_image + FBFS_SECTOR_SIZE, boot_tail_size);
		fbfs_add_mark(boot_buffer, boot_sectors, opt.base + 1u,
			boot_tail_size);

		data = (struct fbfs_data *)boot_buffer;
		data->boot_size = (uint16_t)boot_sectors;
		data->ver_major = FBFS_VERSION_MAJOR;
		data->ver_minor = opt.version_minor;
		data->list_used = 1;
		data->list_size = (uint16_t)list_sectors;
		data->pri_size = (uint16_t)opt.primary_size;
		data->ext_size = opt.extended_size;
		err = fbfs_disk_write(disk, opt.base + 1u, boot_buffer,
			boot_sectors);
		free(boot_buffer);
		if (err != FBFS_OK)
			return err;
	}

	list = calloc(list_sectors, FBFS_SECTOR_SIZE);
	if (!list)
		return fbfs_set_error(FBFS_ERR_NO_MEMORY, NULL);
	fbfs_add_mark(list, list_sectors, opt.base + 1u + boot_sectors, 0);
	err = fbfs_disk_write(disk, opt.base + 1u + boot_sectors, list,
		list_sectors);
	free(list);
	if (err != FBFS_OK)
		return err;

	data_start = opt.base + 1u + boot_sectors + list_sectors;
	if (data_start > opt.primary_size)
		return fbfs_set_error(FBFS_ERR_SIZE,
			"primary data area is too small for boot and file list");
	while (data_start < opt.primary_size)
	{
		uint8_t zeros[FBFS_SECTOR_SIZE * FBFS_GLOB_SECTORS];
		uint32_t chunk;
		uint32_t i;

		chunk = opt.primary_size - data_start;
		if (chunk > FBFS_GLOB_SECTORS)
			chunk = FBFS_GLOB_SECTORS;

		memset(zeros, 0, sizeof(zeros));
		for (i = 0; i < chunk; i++)
			*(uint16_t *)(zeros + i * FBFS_SECTOR_SIZE +
				FBFS_PRIMARY_SECTOR_DATA) = (uint16_t)(data_start + i);
		if (fbfs_disk_write(disk, data_start, zeros, chunk) != FBFS_OK)
			return FBFS_ERR_WRITE;
		data_start += chunk;
	}

	if (opt.force && !opt.extended_max)
	{
		if (is_ext_fs)
		{
			err = fbfs_format_external(disk, opt.fat_type, opt.unit_size, main_start, opt.label);
		}
		else
		{
			err = fat32 ?
					fbfs_format_fat32(disk, main_start, main_part_size, opt.unit_size, opt.align, opt.label, opt.vbr_type) :
					fbfs_format_fat16(disk, main_start, main_part_size, opt.unit_size, opt.align, opt.label, opt.vbr_type);
		}
		if (err != FBFS_OK)
			return err;

		if (mid_part_size > 0)
		{
#if defined(_WIN32)
			fbfs_disk_lock(disk);
#endif
			switch (opt.mid_fat_type) {
			case FBFS_FORMAT_FAT16:
				err = fbfs_format_fat16(disk, mid_start, mid_part_size, 0, 0, opt.mid_label, opt.vbr_type);
				break;
			case FBFS_FORMAT_NTFS:
			case FBFS_FORMAT_EXFAT:
			case FBFS_FORMAT_REFS:
				err = fbfs_format_external(disk, opt.mid_fat_type, 0, mid_start, opt.mid_label);
				break;
			case FBFS_FORMAT_FAT32:
			default:
				err = fbfs_format_fat32(disk, mid_start, mid_part_size, 0, 0, opt.mid_label, opt.vbr_type);
				break;
			}
			if (err != FBFS_OK)
				return err;
		}

		if (tail_part_size > 0)
		{
#if defined(_WIN32)
			fbfs_disk_lock(disk);
#endif
			switch (opt.tail_fat_type) {
			case FBFS_FORMAT_FAT16:
				err = fbfs_format_fat16(disk, tail_start, tail_part_size, 0, 0, opt.tail_label, opt.vbr_type);
				break;
			case FBFS_FORMAT_NTFS:
			case FBFS_FORMAT_EXFAT:
			case FBFS_FORMAT_REFS:
				err = fbfs_format_external(disk, opt.tail_fat_type, 0, tail_start, opt.tail_label);
				break;
			case FBFS_FORMAT_FAT32:
			default:
				err = fbfs_format_fat32(disk, tail_start, tail_part_size, 0, 0, opt.tail_label, opt.vbr_type);
				break;
			}
			if (err != FBFS_OK)
				return err;
		}
	}

	if (opt.archive_path)
	{
		struct fbfs *fs;

		err = fbfs_mount(disk, 0, &fs);
		if (err != FBFS_OK)
			return err;
		err = fbfs_load_archive(fs, opt.archive_path);
		if (err == FBFS_OK)
			err = fbfs_flush(fs);
		fbfs_close(fs);
	}

	return err;
}

int
fbfs_restore(struct fbfs_disk *disk)
{
	uint8_t mbr_sector[FBFS_SECTOR_SIZE];
	uint8_t zero_sector[FBFS_SECTOR_SIZE];
	uint32_t i;

	if (!disk)
		return fbfs_set_error(FBFS_ERR_INVALID_ARGUMENT, NULL);

	for (i = 0; i <= FBFS_DEFAULT_BASE_SIZE; i++)
	{
		struct fbfs_mbr *mbr;

		if (fbfs_disk_read(disk, i, mbr_sector, 1) != FBFS_OK)
			return FBFS_ERR_READ;
		mbr = (struct fbfs_mbr *)mbr_sector;
		if (mbr->end_magic == 0xaa55u && mbr->fb_magic == FBFS_MAGIC &&
			mbr->lba == i)
			break;
	}

	if (i > FBFS_DEFAULT_BASE_SIZE)
		return fbfs_set_error(FBFS_ERR_NOT_FOUND, "can't find fb mbr");

	if (!i)
		return FBFS_OK;

	if (fbfs_disk_read(disk, 0, zero_sector, 1) != FBFS_OK)
		return FBFS_ERR_READ;
	memcpy(mbr_sector + 0x1b8, zero_sector + 0x1b8,
		FBFS_SECTOR_SIZE - 0x1b8);
	return fbfs_sync_mbr_raw(disk, mbr_sector, i - 1u, 0, 0);
}

int
fbfs_sync(struct fbfs *fs, const struct fbfs_sync_options *options)
{
	struct fbfs_sync_options opt;
	uint8_t mbr_sector[FBFS_SECTOR_SIZE];
	uint8_t part_sector[FBFS_SECTOR_SIZE];
	uint32_t part_start;
	int jmp_ofs;

	if (!fs)
		return fbfs_set_error(FBFS_ERR_INVALID_ARGUMENT, NULL);
	if (fs->archive_mode)
		return fbfs_set_error(FBFS_ERR_UNSUPPORTED,
			"sync is not supported for archive files");

	memset(&opt, 0, sizeof(opt));
	opt.copy_bpb = -1;
	if (options)
		opt = *options;

	if (fbfs_disk_read(fs->disk, 0, mbr_sector, 1) != FBFS_OK)
		return FBFS_ERR_READ;
	part_start = fbfs_get_part_offset(mbr_sector);
	if (part_start == UINT32_MAX)
		return fbfs_set_error(FBFS_ERR_NOT_FOUND, "data partition not found");
	if (fbfs_disk_read(fs->disk, part_start, part_sector, 1) != FBFS_OK)
		return FBFS_ERR_READ;

	jmp_ofs = mbr_sector[1];
	if (opt.copy_bpb == 2)
	{
		struct fbfs_fat_bs16 *bs;
		uint32_t total;

		bs = (struct fbfs_fat_bs16 *)mbr_sector;
		memcpy(mbr_sector + 2, part_sector + 2, jmp_ofs);
		bs->nrs = (uint16_t)(bs->nrs + part_start);
		bs->nhs = 0;
		total = ((struct fbfs_fat_bs16 *)part_sector)->ts16;
		if (!total)
			total = ((struct fbfs_fat_bs16 *)part_sector)->ts32;
		total += part_start;
		bs->ts16 = 0;
		bs->ts32 = 0;
		if (total < 65536u)
			bs->ts16 = (uint16_t)total;
		else
			bs->ts32 = total;
		opt.zip = 0;
	}
	else if (opt.copy_bpb >= 0)
	{
		memset(mbr_sector + 2, 0, jmp_ofs);
		if (opt.copy_bpb)
		{
			mbr_sector[0x10] = 2;
			mbr_sector[0x18] = 0x3f;
			mbr_sector[0x1a] = 0xff;
			mbr_sector[0x24] = 0x80;
		}
	}

	if (opt.bpb_size && opt.bpb_size < (uint32_t)jmp_ofs + 2u)
		memset(mbr_sector + opt.bpb_size, 0, (uint32_t)jmp_ofs + 2u -
			opt.bpb_size);

	fbfs_config_mbr(mbr_sector, opt.max_sectors, opt.chs_mode, opt.zip);
	return fbfs_sync_mbr_raw(fs->disk, mbr_sector, fs->boot_base,
		opt.copy_bpb == 1, 0);
}

int
fbfs_update(struct fbfs *fs, int debug_boot)
{
	const uint8_t *boot_image;
	uint32_t boot_image_size;
	uint32_t boot_tail_size;
	uint32_t boot_sectors;
	uint8_t mbr_sector[FBFS_SECTOR_SIZE];
	uint8_t *boot_buffer;
	uint32_t i;
	int err;

	if (!fs)
		return fbfs_set_error(FBFS_ERR_INVALID_ARGUMENT, NULL);
	if (fs->archive_mode)
		return fbfs_set_error(FBFS_ERR_UNSUPPORTED,
			"update is not supported for archive files");

	fbfs_get_boot_image(debug_boot, &boot_image, &boot_image_size);
	if (boot_image_size <= FBFS_SECTOR_SIZE)
		return fbfs_set_error(FBFS_ERR_CORRUPT, "invalid boot image");
	boot_tail_size = boot_image_size - FBFS_SECTOR_SIZE;
	boot_sectors = (boot_tail_size + FBFS_PRIMARY_SECTOR_DATA - 1u) /
		FBFS_PRIMARY_SECTOR_DATA;

	if (fs->boot_base + 1u + boot_sectors > fs->list_start)
		return fbfs_set_error(FBFS_ERR_NO_SPACE,
			"not enough space for boot code; use format command");

	for (i = 0; i <= fs->boot_base; i++)
	{
		uint32_t offset;

		if (fbfs_disk_read(fs->disk, i, mbr_sector, 1) != FBFS_OK)
			return FBFS_ERR_READ;

		offset = boot_image[1] + 2u;
		if (offset < FBFS_OFS_MAX_SEC)
			memcpy(mbr_sector + offset, boot_image + offset,
				FBFS_OFS_MAX_SEC - offset);
		mbr_sector[1] = boot_image[1];
		if (fbfs_disk_write(fs->disk, i, mbr_sector, 1) != FBFS_OK)
			return FBFS_ERR_WRITE;
	}

	boot_buffer = calloc(boot_sectors, FBFS_SECTOR_SIZE);
	if (!boot_buffer)
		return fbfs_set_error(FBFS_ERR_NO_MEMORY, NULL);
	if (fbfs_disk_read(fs->disk, fs->boot_base + 1u, boot_buffer, 1) != FBFS_OK)
	{
		free(boot_buffer);
		return FBFS_ERR_READ;
	}

	memcpy(boot_buffer + sizeof(struct fbfs_data),
		boot_image + FBFS_SECTOR_SIZE + sizeof(struct fbfs_data),
		boot_tail_size - sizeof(struct fbfs_data));
	fbfs_add_mark(boot_buffer, boot_sectors, fs->boot_base + 1u,
		boot_tail_size);

	err = fbfs_disk_write(fs->disk, fs->boot_base + 1u, boot_buffer,
		boot_sectors);
	free(boot_buffer);
	if (err == FBFS_OK)
		fs->boot_size = boot_sectors;

	return err;
}

int
fbfs_unmap_partition(struct fbfs *fs, uint32_t slot)
{
	uint8_t sector[FBFS_SECTOR_SIZE];
	uint32_t p;

	if (!fs || slot < 1 || slot > 4)
		return fbfs_set_error(FBFS_ERR_INVALID_ARGUMENT, NULL);

	if (fs->archive_mode)
		return fbfs_set_error(FBFS_ERR_UNSUPPORTED, "map/unmap not supported in archive mode");

	if (fbfs_disk_read(fs->disk, 0, sector, 1) != FBFS_OK)
		return FBFS_ERR_READ;

	p = 0x1beu + (slot - 1u) * 16u;
	memset(sector + p, 0, 16);

	if (fbfs_sync_mbr_raw(fs->disk, sector, fs->boot_base, 0, 0) != FBFS_OK)
		return FBFS_ERR_WRITE;

	return FBFS_OK;
}

int
fbfs_map_partition(struct fbfs *fs, const char *name, uint32_t slot, uint8_t type)
{
	uint8_t sector[FBFS_SECTOR_SIZE];
	struct fbfs_file_info file;
	uint32_t p;
	uint32_t file_start;
	uint32_t file_sectors;

	if (!fs || !name || slot < 1 || slot > 4)
		return fbfs_set_error(FBFS_ERR_INVALID_ARGUMENT, NULL);

	if (fs->archive_mode)
		return fbfs_set_error(FBFS_ERR_UNSUPPORTED, "map/unmap not supported in archive mode");

	// 1. 在 UD 隐藏文件系统中查找目标镜像文件
	if (fbfs_find_file(fs, name, &file) != FBFS_OK)
		return fbfs_set_error(FBFS_ERR_NOT_FOUND, "file %s not found in UD area", name);

	// 获取镜像文件的起始 LBA 扇区以及精确扇区大小
	file_start = file.data_start;
	if (file.extended_area)
	{
		file_sectors = (file.data_size + 511u) >> 9;
	}
	else
	{
		file_sectors = (file.data_size + 509u) / 510u;
	}

	if (!file_sectors)
		return fbfs_set_error(FBFS_ERR_SIZE, "mapped file cannot be empty");

	// 2. 读取 0 扇区 MBR
	if (fbfs_disk_read(fs->disk, 0, sector, 1) != FBFS_OK)
		return FBFS_ERR_READ;

	p = 0x1beu + (slot - 1u) * 16u;
	memset(sector + p, 0, 16);

	// 写入 16 字节的物理分区表表项
	sector[p + 0] = 0x00; // 非活动引导
	fbfs_lba_to_chs(file_start, sector + p + 1); // 起始 CHS 自动折算
	sector[p + 4] = type; // 分区文件系统系统 ID (如 0x0c 为 FAT32, 0x07 为 NTFS)
	fbfs_lba_to_chs(file_start + file_sectors - 1u, sector + p + 5); // 结束 CHS 自动折算
	*(uint32_t *)(sector + p + 8) = file_start; // 起始物理扇区 LBA 写入
	*(uint32_t *)(sector + p + 12) = file_sectors; // 扇区大小写入

	// 3. 将新 MBR 扇区同步并广播复制到 base 的备份引导块中，保证高兼容性
	if (fbfs_sync_mbr_raw(fs->disk, sector, fs->boot_base, 0, 0) != FBFS_OK)
		return FBFS_ERR_WRITE;

	return FBFS_OK;
}

int
fbfs_format_file(struct fbfs *fs, const char *name, const char *type_name, uint32_t unit_size, int align, const char *label)
{
	struct fbfs_file_info file;
	uint32_t file_start;
	uint32_t file_sectors;
	int fs_type;

	if (!fs || !name || !type_name)
		return fbfs_set_error(FBFS_ERR_INVALID_ARGUMENT, NULL);

	if (fs->archive_mode)
		return fbfs_set_error(FBFS_ERR_UNSUPPORTED, "format-file not supported in archive mode");

	// 1. 在 UD 隐藏区中检索文件
	if (fbfs_find_file(fs, name, &file) != FBFS_OK)
		return fbfs_set_error(FBFS_ERR_NOT_FOUND, "file %s not found in UD area", name);

	file_start = file.data_start;
	if (file.extended_area)
	{
		file_sectors = (file.data_size + 511u) >> 9;
	}
	else
	{
		file_sectors = (file.data_size + 509u) / 510u;
	}

	if (!file_sectors)
		return fbfs_set_error(FBFS_ERR_SIZE, "cannot format an empty file");

	// 解析目标文件系统类型
	if (!fbfs_strcasecmp(type_name, "fat16"))
		fs_type = FBFS_FORMAT_FAT16;
	else if (!fbfs_strcasecmp(type_name, "fat32"))
		fs_type = FBFS_FORMAT_FAT32;
	else if (!fbfs_strcasecmp(type_name, "ntfs"))
		fs_type = FBFS_FORMAT_NTFS;
	else if (!fbfs_strcasecmp(type_name, "exfat"))
		fs_type = FBFS_FORMAT_EXFAT;
	else if (!fbfs_strcasecmp(type_name, "refs"))
		fs_type = FBFS_FORMAT_REFS;
	else
		return fbfs_set_error(FBFS_ERR_INVALID_ARGUMENT, "unknown filesystem type %s (supported: fat16, fat32, ntfs, exfat, refs)", type_name);

	if (fs_type == FBFS_FORMAT_FAT16)
	{
#if defined(_WIN32)
		fbfs_disk_lock(fs->disk);
#endif
		return fbfs_format_fat16(fs->disk, file_start, file_sectors, unit_size, align, label, 1);
	}
	else if (fs_type == FBFS_FORMAT_FAT32)
	{
#if defined(_WIN32)
		fbfs_disk_lock(fs->disk);
#endif
		return fbfs_format_fat32(fs->disk, file_start, file_sectors, unit_size, align, label, 1);
	}
	else
	{
		// NTFS, exFAT, ReFS 等系统依赖 OS 驱动挂载，执行格式化时该镜像文件必须已被物理映射到 MBR 的四个槽位之一中
		uint8_t sector[FBFS_SECTOR_SIZE];
		uint32_t slot_found = 0;
		uint32_t i;

		if (fbfs_disk_read(fs->disk, 0, sector, 1) != FBFS_OK)
			return FBFS_ERR_READ;

		for (i = 0; i < 4; i++)
		{
			uint32_t p = 0x1beu + i * 16u;
			if (sector[p + 4] != 0 && *(uint32_t *)(sector + p + 8) == file_start)
			{
				slot_found = i + 1;
				break;
			}
		}

		if (!slot_found)
		{
			return fbfs_set_error(FBFS_ERR_UNSUPPORTED, 
				"NTFS/exFAT/ReFS format requires the image file to be mapped first.\n"
				"Please map the file to a partition slot (1-4) using 'map' command first.");
		}

		return fbfs_format_external(fs->disk, fs_type, unit_size, file_start, label);
	}
}

uint32_t
fbfs_get_max_free_size(struct fbfs *fs, int is_ext)
{
	uint32_t begin;
	uint32_t offset;
	uint32_t max_sectors = 0;
	uint32_t block_size;

	if (!fs)
		return 0;

	begin = fs->list_start + fs->list_sectors;
	offset = 0;

	while (offset < fs->list_tail)
	{
		struct fbfs_file_record *record;
		uint32_t end = fs->primary_size;

		record = fbfs_record_at(fs, offset);
		end = record->data_start;

		if (begin < fs->primary_size || end <= fs->primary_size)
		{
			if (begin >= fs->primary_size || !is_ext)
			{
				uint32_t diff = end - begin;
				if (diff > max_sectors)
					max_sectors = diff;
			}
		}
		else
		{
			if (!is_ext)
			{
				uint32_t diff = fs->primary_size - begin;
				if (diff > max_sectors)
					max_sectors = diff;
			}
			else
			{
				uint32_t diff = end - fs->primary_size;
				if (diff > max_sectors)
					max_sectors = diff;
			}
		}

		begin = record->data_start + fbfs_file_sector_count(fs, record);
		offset += record->size + 2u;
	}

	// 换算最后一个文件末尾到全盘边界 (fs->total_size) 之间的剩余大区间空闲
	if (begin < fs->primary_size || fs->total_size <= fs->primary_size)
	{
		if (begin >= fs->primary_size || !is_ext)
		{
			uint32_t diff = fs->total_size - begin;
			if (diff > max_sectors)
				max_sectors = diff;
		}
	}
	else
	{
		if (!is_ext)
		{
			uint32_t diff = fs->primary_size - begin;
			if (diff > max_sectors)
				max_sectors = diff;
		}
		else
		{
			uint32_t diff = fs->total_size - fs->primary_size;
			if (diff > max_sectors)
				max_sectors = diff;
		}
	}

	block_size = is_ext ? FBFS_SECTOR_SIZE : FBFS_PRIMARY_SECTOR_DATA;
	return max_sectors * block_size;
}


/* BOOTICE / Custom MBR & PBR Support Extension */

static const uint8_t nt60_mbr_code[446] = {
	0x33, 0xC0, 0x8E, 0xD0, 0xBC, 0x00, 0x7C, 0x8E, 0xC0, 0x8E, 0xD8, 0xBE, 0x00, 0x7C, 0xBF, 0x00,
	0x06, 0xB9, 0x00, 0x02, 0xFC, 0xF3, 0xA4, 0x50, 0x68, 0x1C, 0x06, 0xCB, 0xFB, 0xB9, 0x04, 0x00,
	0xBD, 0xBE, 0x07, 0x80, 0x7E, 0x00, 0x00, 0x7C, 0x0B, 0x0F, 0x85, 0x0E, 0x01, 0x83, 0xC5, 0x10,
	0xE2, 0xF1, 0xCD, 0x18, 0x88, 0x56, 0x00, 0x55, 0xC6, 0x46, 0x11, 0x05, 0xC6, 0x46, 0x10, 0x00,
	0xB4, 0x41, 0xBB, 0xAA, 0x55, 0xCD, 0x13, 0x5D, 0x72, 0x0F, 0x81, 0xFB, 0x55, 0xAA, 0x75, 0x09,
	0xF7, 0xC1, 0x01, 0x00, 0x74, 0x03, 0xFE, 0x46, 0x10, 0x66, 0x60, 0x80, 0x76, 0x10, 0x00, 0x74,
	0x26, 0x66, 0x68, 0x00, 0x00, 0x00, 0x00, 0x66, 0xFF, 0x76, 0x08, 0x68, 0x00, 0x00, 0x68, 0x00,
	0x7C, 0x68, 0x01, 0x00, 0x68, 0x10, 0x00, 0xB4, 0x42, 0x8A, 0x56, 0x00, 0x8B, 0xF4, 0xCD, 0x13,
	0x9F, 0x83, 0xC4, 0x10, 0x9E, 0xEB, 0x14, 0xB8, 0x01, 0x02, 0xBB, 0x00, 0x7C, 0x8A, 0x56, 0x00,
	0x8A, 0x76, 0x01, 0x8A, 0x4E, 0x02, 0x8A, 0x6E, 0x03, 0xCD, 0x13, 0x66, 0x61, 0x73, 0x1E, 0xFE,
	0x4E, 0x11, 0x75, 0x0C, 0x80, 0x7E, 0x00, 0x80, 0x0F, 0x84, 0x8A, 0x00, 0xB2, 0x80, 0xEB, 0x84,
	0x55, 0x32, 0xE4, 0x8A, 0x56, 0x00, 0xCD, 0x13, 0x5D, 0xEB, 0x9E, 0x81, 0x3E, 0xFE, 0x7D, 0x55,
	0xAA, 0x75, 0x6E, 0x8B, 0xF5, 0x83, 0xC6, 0x10, 0x49, 0x74, 0x19, 0x30, 0xE4, 0x80, 0x7C, 0x00,
	0x00, 0x74, 0xF1, 0x80, 0x7C, 0x00, 0x80, 0x75, 0x93, 0x30, 0xED, 0x8A, 0x56, 0x00, 0x8B, 0x44,
	0x08, 0x8B, 0x54, 0x0A, 0x03, 0x46, 0x08, 0x13, 0x56, 0x0A, 0xE8, 0x3A, 0x00, 0x72, 0x13, 0x8B,
	0xFC, 0x1E, 0x07, 0x8B, 0xF5, 0xCB, 0xBE, 0x05, 0x07, 0xBB, 0x07, 0x00, 0xB4, 0x0E, 0x2E, 0xAC,
	0xCD, 0x10, 0x3C, 0x00, 0x75, 0xF6, 0xF4, 0xEB, 0xFC, 0xBE, 0x10, 0x07, 0xEB, 0xEB, 0xBE, 0x21,
	0x07, 0xEB, 0xE6, 0x49, 0x6E, 0x76, 0x61, 0x6C, 0x69, 0x64, 0x20, 0x70, 0x61, 0x72, 0x74, 0x69,
	0x74, 0x69, 0x6F, 0x6E, 0x20, 0x74, 0x61, 0x62, 0x6C, 0x65, 0x00, 0x45, 0x72, 0x72, 0x6F, 0x72,
	0x20, 0x6C, 0x6F, 0x61, 0x64, 0x69, 0x6E, 0x67, 0x20, 0x6F, 0x70, 0x65, 0x72, 0x61, 0x74, 0x69,
	0x6E, 0x67, 0x20, 0x73, 0x79, 0x73, 0x74, 0x65, 0x6D, 0x00, 0x4D, 0x69, 0x73, 0x73, 0x69, 0x6E,
	0x67, 0x20, 0x6F, 0x70, 0x65, 0x72, 0x61, 0x74, 0x69, 0x6E, 0x67, 0x20, 0x73, 0x79, 0x73, 0x74,
	0x65, 0x6D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x01,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t nt52_mbr_code[446] = {
	0x33, 0xC0, 0x8E, 0xD0, 0xBC, 0x00, 0x7C, 0xFB, 0x50, 0x07, 0x50, 0x1F, 0xFC, 0xBE, 0x1B, 0x7C,
	0xBF, 0x1B, 0x06, 0x50, 0x57, 0xB9, 0xE5, 0x01, 0xF3, 0xA4, 0xCB, 0xBE, 0xBE, 0x07, 0xB1, 0x04,
	0x38, 0x2C, 0x7C, 0x09, 0x75, 0x15, 0x83, 0xC6, 0x10, 0xE2, 0xF5, 0xCD, 0x18, 0x8B, 0x14, 0x8B,
	0x4C, 0x02, 0x8B, 0xEE, 0x83, 0xC6, 0x10, 0x49, 0x74, 0x16, 0x38, 0x2C, 0x74, 0xF6, 0x85, 0x0D,
	0x75, 0x10, 0xBE, 0x85, 0x06, 0xAC, 0x3C, 0x00, 0x74, 0x0C, 0xBB, 0x07, 0x00, 0xB4, 0x0E, 0xCD,
	0x10, 0xEB, 0xF2, 0xF4, 0xEB, 0xFD, 0xBE, 0xA1, 0x06, 0xEB, 0xE8, 0xBE, 0xBA, 0x06, 0xEB, 0xE3,
	0xEB, 0x0A, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x8B, 0xFC, 0x1E, 0x57,
	0x8B, 0xF5, 0xCB, 0x49, 0x6E, 0x76, 0x61, 0x6C, 0x69, 0x64, 0x20, 0x70, 0x61, 0x72, 0x74, 0x69,
	0x74, 0x69, 0x6F, 0x6E, 0x20, 0x74, 0x61, 0x62, 0x6C, 0x65, 0x00, 0x45, 0x72, 0x72, 0x6F, 0x72,
	0x20, 0x6C, 0x6F, 0x61, 0x64, 0x69, 0x6E, 0x67, 0x20, 0x6F, 0x70, 0x65, 0x72, 0x61, 0x74, 0x69,
	0x6E, 0x67, 0x20, 0x73, 0x79, 0x73, 0x74, 0x65, 0x6D, 0x00, 0x4D, 0x69, 0x73, 0x73, 0x69, 0x6E,
	0x67, 0x20, 0x6F, 0x70, 0x65, 0x72, 0x61, 0x74, 0x69, 0x6E, 0x67, 0x20, 0x73, 0x79, 0x73, 0x74,
	0x65, 0x6D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x01,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static int get_partition_info(struct fbfs_disk *disk, uint32_t slot, uint32_t *start, uint32_t *size, uint8_t *sys_id)
{
	uint8_t sector0[FBFS_SECTOR_SIZE];
	if (fbfs_disk_read(disk, 0, sector0, 1) != FBFS_OK)
		return FBFS_ERR_READ;

	if (sector0[0x1fe] != 0x55 || sector0[0x1ff] != 0xaa)
		return fbfs_set_error(FBFS_ERR_BAD_FS, "invalid MBR partition table signature");

	uint32_t p = 0x1beu + (slot - 1u) * 16u;
	*sys_id = sector0[p + 4];
	if (*sys_id == 0)
		return fbfs_set_error(FBFS_ERR_NOT_FOUND, "partition slot %u is empty", slot);

	*start = *(uint32_t *)(sector0 + p + 8);
	*size = *(uint32_t *)(sector0 + p + 12);
	return FBFS_OK;
}

int fbfs_import_mbr(struct fbfs_disk *disk, const char *file_path, uint32_t sectors)
{
	FILE *f = fopen(file_path, "rb");
	if (!f)
		return fbfs_set_error(FBFS_ERR_OPEN, "cannot open file %s", file_path);

	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);

	if (size <= 0) {
		fclose(f);
		return fbfs_set_error(FBFS_ERR_SIZE, "file %s is empty", file_path);
	}

	uint32_t file_sectors = (uint32_t)((size + 511) / 512);
	if (sectors > 0 && sectors < file_sectors) {
		file_sectors = sectors;
	}

	uint8_t sector0[FBFS_SECTOR_SIZE];
	if (fbfs_disk_read(disk, 0, sector0, 1) != FBFS_OK) {
		fclose(f);
		return FBFS_ERR_READ;
	}

	uint8_t file_sector0[FBFS_SECTOR_SIZE];
	memset(file_sector0, 0, FBFS_SECTOR_SIZE);
	size_t read_bytes = fread(file_sector0, 1, FBFS_SECTOR_SIZE, f);

	memcpy(sector0, file_sector0, 446);
	sector0[0x1fe] = 0x55;
	sector0[0x1ff] = 0xaa;

	if (fbfs_disk_write(disk, 0, sector0, 1) != FBFS_OK) {
		fclose(f);
		return FBFS_ERR_WRITE;
	}

	if (file_sectors > 1) {
		uint8_t temp_buf[FBFS_SECTOR_SIZE];
		for (uint32_t i = 1; i < file_sectors; i++) {
			memset(temp_buf, 0, FBFS_SECTOR_SIZE);
			size_t rb = fread(temp_buf, 1, FBFS_SECTOR_SIZE, f);
			if (fbfs_disk_write(disk, i, temp_buf, 1) != FBFS_OK) {
				fclose(f);
				return FBFS_ERR_WRITE;
			}
		}
	}

	fclose(f);
	return FBFS_OK;
}

int fbfs_import_pbr(struct fbfs_disk *disk, uint32_t slot, const char *file_path, int keep_bpb)
{
	uint32_t part_start = 0, part_size = 0;
	uint8_t sys_id = 0;
	int err = get_partition_info(disk, slot, &part_start, &part_size, &sys_id);
	if (err != FBFS_OK)
		return err;

	FILE *f = fopen(file_path, "rb");
	if (!f)
		return fbfs_set_error(FBFS_ERR_OPEN, "cannot open file %s", file_path);

	uint8_t file_sector[FBFS_SECTOR_SIZE];
	memset(file_sector, 0, FBFS_SECTOR_SIZE);
	size_t read_bytes = fread(file_sector, 1, FBFS_SECTOR_SIZE, f);
	fclose(f);

	uint8_t part_sector[FBFS_SECTOR_SIZE];
	if (fbfs_disk_read(disk, part_start, part_sector, 1) != FBFS_OK)
		return FBFS_ERR_READ;

	if (keep_bpb) {
		part_sector[0] = file_sector[0];
		part_sector[1] = file_sector[1];
		part_sector[2] = file_sector[2];

		if (!strncmp((char *)(part_sector + 0x03), "NTFS", 4)) {
			memcpy(part_sector + 0x54, file_sector + 0x54, FBFS_SECTOR_SIZE - 0x54);
		} else if (!strncmp((char *)(part_sector + 0x03), "EXFAT", 5)) {
			memcpy(part_sector + 0x78, file_sector + 0x78, FBFS_SECTOR_SIZE - 0x78);
		} else if (!strncmp((char *)(part_sector + 0x52), "FAT32", 5)) {
			memcpy(part_sector + 0x5a, file_sector + 0x5a, FBFS_SECTOR_SIZE - 0x5a);
		} else if (!strncmp((char *)(part_sector + 0x36), "FAT16", 5) || !strncmp((char *)(part_sector + 0x36), "FAT12", 5)) {
			memcpy(part_sector + 0x3e, file_sector + 0x3e, FBFS_SECTOR_SIZE - 0x3e);
		} else {
			memcpy(part_sector + 0x5a, file_sector + 0x5a, FBFS_SECTOR_SIZE - 0x5a);
		}
	} else {
		memcpy(part_sector, file_sector, FBFS_SECTOR_SIZE - 2);
	}

	part_sector[0x1fe] = 0x55;
	part_sector[0x1ff] = 0xaa;

	if (fbfs_disk_write(disk, part_start, part_sector, 1) != FBFS_OK)
		return FBFS_ERR_WRITE;

	return FBFS_OK;
}

int fbfs_write_mbr(struct fbfs_disk *disk, const char *type)
{
	uint8_t sector0[FBFS_SECTOR_SIZE];
	if (fbfs_disk_read(disk, 0, sector0, 1) != FBFS_OK)
		return FBFS_ERR_READ;

	const uint8_t *code = NULL;
	if (!fbfs_strcasecmp(type, "nt60") || !fbfs_strcasecmp(type, "nt6.x") || !fbfs_strcasecmp(type, "nt6")) {
		code = nt60_mbr_code;
	} else if (!fbfs_strcasecmp(type, "nt52") || !fbfs_strcasecmp(type, "nt5.x") || !fbfs_strcasecmp(type, "nt5")) {
		code = nt52_mbr_code;
	} else {
		return fbfs_set_error(FBFS_ERR_INVALID_ARGUMENT, "unsupported MBR type %s (supported: nt60, nt52)", type);
	}

	memcpy(sector0, code, 446);
	sector0[0x1fe] = 0x55;
	sector0[0x1ff] = 0xaa;

	if (fbfs_disk_write(disk, 0, sector0, 1) != FBFS_OK)
		return FBFS_ERR_WRITE;

	return FBFS_OK;
}

int fbfs_write_pbr(struct fbfs_disk *disk, uint32_t slot, const char *type)
{
	uint32_t part_start = 0, part_size = 0;
	uint8_t sys_id = 0;
	int err = get_partition_info(disk, slot, &part_start, &part_size, &sys_id);
	if (err != FBFS_OK)
		return err;

	uint8_t part_sector[FBFS_SECTOR_SIZE];
	if (fbfs_disk_read(disk, part_start, part_sector, 1) != FBFS_OK)
		return FBFS_ERR_READ;

	const uint8_t *code = NULL;
	if (!fbfs_strcasecmp(type, "bootmgr") || !fbfs_strcasecmp(type, "nt60") || !fbfs_strcasecmp(type, "nt6")) {
		code = fat32_nt60_bc;
	} else if (!fbfs_strcasecmp(type, "ntldr") || !fbfs_strcasecmp(type, "nt52") || !fbfs_strcasecmp(type, "nt5")) {
		code = fat32_nt52_bc;
	} else {
		return fbfs_set_error(FBFS_ERR_INVALID_ARGUMENT, "unsupported PBR type %s (supported: bootmgr, ntldr)", type);
	}

	part_sector[0] = 0xeb;
	part_sector[1] = 0x58;
	part_sector[2] = 0x90;

	if (!strncmp((char *)(part_sector + 0x52), "FAT32", 5)) {
		memcpy(part_sector + 0x5a, code + 0x5a, FBFS_SECTOR_SIZE - 0x5a);
	} else if (!strncmp((char *)(part_sector + 0x03), "NTFS", 4)) {
		memcpy(part_sector + 0x54, code + 0x54, FBFS_SECTOR_SIZE - 0x54);
	} else {
		memcpy(part_sector + 0x5a, code + 0x5a, FBFS_SECTOR_SIZE - 0x5a);
	}

	part_sector[0x1fe] = 0x55;
	part_sector[0x1ff] = 0xaa;

	if (fbfs_disk_write(disk, part_start, part_sector, 1) != FBFS_OK)
		return FBFS_ERR_WRITE;

	return FBFS_OK;
}


/* Advanced BOOTICE Suite Implementation */

int fbfs_export_mbr(struct fbfs_disk *disk, const char *file_path, uint32_t sectors)
{
	if (sectors == 0) sectors = 1;
	FILE *f = fopen(file_path, "wb");
	if (!f) return fbfs_set_error(FBFS_ERR_OPEN, "cannot open file %s for write", file_path);

	uint8_t buf[FBFS_SECTOR_SIZE];
	for (uint32_t i = 0; i < sectors; i++) {
		if (fbfs_disk_read(disk, i, buf, 1) != FBFS_OK) {
			fclose(f);
			return FBFS_ERR_READ;
		}
		fwrite(buf, 1, FBFS_SECTOR_SIZE, f);
	}
	fclose(f);
	return FBFS_OK;
}

int fbfs_export_pbr(struct fbfs_disk *disk, uint32_t slot, const char *file_path, uint32_t sectors)
{
	if (sectors == 0) sectors = 1;
	uint32_t part_start = 0, part_size = 0;
	uint8_t sys_id = 0;
	int err = get_partition_info(disk, slot, &part_start, &part_size, &sys_id);
	if (err != FBFS_OK) return err;

	FILE *f = fopen(file_path, "wb");
	if (!f) return fbfs_set_error(FBFS_ERR_OPEN, "cannot open file %s for write", file_path);

	uint8_t buf[FBFS_SECTOR_SIZE];
	for (uint32_t i = 0; i < sectors; i++) {
		if (fbfs_disk_read(disk, part_start + i, buf, 1) != FBFS_OK) {
			fclose(f);
			return FBFS_ERR_READ;
		}
		fwrite(buf, 1, FBFS_SECTOR_SIZE, f);
	}
	fclose(f);
	return FBFS_OK;
}

int fbfs_backup_sectors(struct fbfs_disk *disk, const char *file_path, uint32_t lba, uint32_t sectors)
{
	if (sectors == 0) return fbfs_set_error(FBFS_ERR_INVALID_ARGUMENT, "sectors count cannot be zero");
	FILE *f = fopen(file_path, "wb");
	if (!f) return fbfs_set_error(FBFS_ERR_OPEN, "cannot open file %s for write", file_path);

	uint8_t buf[FBFS_SECTOR_SIZE];
	for (uint32_t i = 0; i < sectors; i++) {
		if (fbfs_disk_read(disk, lba + i, buf, 1) != FBFS_OK) {
			fclose(f);
			return FBFS_ERR_READ;
		}
		fwrite(buf, 1, FBFS_SECTOR_SIZE, f);
	}
	fclose(f);
	return FBFS_OK;
}

int fbfs_restore_sectors(struct fbfs_disk *disk, const char *file_path, uint32_t lba, uint32_t sectors, int keep_dpt, int keep_bpb)
{
	FILE *f = fopen(file_path, "rb");
	if (!f) return fbfs_set_error(FBFS_ERR_OPEN, "cannot open file %s for read", file_path);

	uint8_t file_buf[FBFS_SECTOR_SIZE];
	uint8_t disk_buf[FBFS_SECTOR_SIZE];

	for (uint32_t i = 0; i < sectors; i++) {
		memset(file_buf, 0, FBFS_SECTOR_SIZE);
		size_t rb = fread(file_buf, 1, FBFS_SECTOR_SIZE, f);
		if (rb == 0) break;

		uint32_t cur_lba = lba + i;
		if (fbfs_disk_read(disk, cur_lba, disk_buf, 1) != FBFS_OK) {
			fclose(f);
			return FBFS_ERR_READ;
		}

		if (cur_lba == 0 && keep_dpt) {
			memcpy(file_buf + 446, disk_buf + 446, 64);
		}

		if (keep_bpb) {
			if (disk_buf[510] == 0x55 && disk_buf[511] == 0xaa) {
				file_buf[0] = disk_buf[0];
				file_buf[1] = disk_buf[1];
				file_buf[2] = disk_buf[2];
				if (!strncmp((char *)(disk_buf + 3), "NTFS", 4)) {
					memcpy(file_buf + 3, disk_buf + 3, 81);
				} else if (!strncmp((char *)(disk_buf + 3), "EXFAT", 5)) {
					memcpy(file_buf + 3, disk_buf + 3, 117);
				} else if (!strncmp((char *)(disk_buf + 0x52), "FAT32", 5)) {
					memcpy(file_buf + 3, disk_buf + 3, 87);
				} else {
					memcpy(file_buf + 3, disk_buf + 3, 59);
				}
			}
		}

		file_buf[510] = 0x55;
		file_buf[511] = 0xaa;

		if (fbfs_disk_write(disk, cur_lba, file_buf, 1) != FBFS_OK) {
			fclose(f);
			return FBFS_ERR_WRITE;
		}
	}
	fclose(f);
	return FBFS_OK;
}

int fbfs_partition_op(struct fbfs_disk *disk, uint32_t slot, const char *op, const char *param)
{
	uint8_t sector0[FBFS_SECTOR_SIZE];
	if (fbfs_disk_read(disk, 0, sector0, 1) != FBFS_OK)
		return FBFS_ERR_READ;

	if (!fbfs_strcasecmp(op, "backup_dpt")) {
		FILE *f = fopen(param, "wb");
		if (!f) return fbfs_set_error(FBFS_ERR_OPEN, "cannot open file %s for write", param);
		fwrite(sector0 + 446, 1, 64, f);
		fclose(f);
		return FBFS_OK;
	}
	else if (!fbfs_strcasecmp(op, "restore_dpt")) {
		FILE *f = fopen(param, "rb");
		if (!f) return fbfs_set_error(FBFS_ERR_OPEN, "cannot open file %s for read", param);
		uint8_t dpt[64];
		size_t rb = fread(dpt, 1, 64, f);
		fclose(f);
		if (rb < 64) return fbfs_set_error(FBFS_ERR_SIZE, "invalid DPT file size");
		memcpy(sector0 + 446, dpt, 64);
		sector0[510] = 0x55;
		sector0[511] = 0xaa;
		return fbfs_disk_write(disk, 0, sector0, 1);
	}

	if (slot < 1 || slot > 4)
		return fbfs_set_error(FBFS_ERR_INVALID_ARGUMENT, "partition slot must be 1-4");

	uint32_t p = 0x1beu + (slot - 1u) * 16u;

	if (!fbfs_strcasecmp(op, "hide")) {
		uint8_t sys_id = sector0[p + 4];
		if (sys_id != 0 && sys_id < 0x10) {
			sector0[p + 4] = sys_id + 0x10;
		}
	}
	else if (!fbfs_strcasecmp(op, "unhide")) {
		uint8_t sys_id = sector0[p + 4];
		if (sys_id >= 0x11 && sys_id <= 0x1f) {
			sector0[p + 4] = sys_id - 0x10;
		}
	}
	else if (!fbfs_strcasecmp(op, "activate")) {
		for (int i = 0; i < 4; i++) {
			uint32_t cur_p = 0x1beu + i * 16u;
			if (i == (int)slot - 1) {
				sector0[cur_p] = 0x80;
			} else {
				sector0[cur_p] = 0x00;
			}
		}
	}
	else if (!fbfs_strcasecmp(op, "set_id")) {
		if (!param) return fbfs_set_error(FBFS_ERR_INVALID_ARGUMENT, "set_id requires param value");
		uint8_t new_id = (uint8_t)strtoul(param, NULL, 16);
		sector0[p + 4] = new_id;
	}
	else {
		return fbfs_set_error(FBFS_ERR_UNSUPPORTED, "unknown partition operation");
	}

	return fbfs_disk_write(disk, 0, sector0, 1);
}
