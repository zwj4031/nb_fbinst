#ifndef FBFS_H
#define FBFS_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FBFS_SECTOR_SIZE			512u
#define FBFS_PRIMARY_SECTOR_DATA		510u
#define FBFS_MAGIC				0x46424246u
#define FBFS_AR_MAGIC				0x52414246u
#define FBFS_VERSION_MAJOR			1u
#define FBFS_VERSION_MINOR_16			6u
#define FBFS_VERSION_MINOR_17			7u
#define FBFS_DEFAULT_BASE_SIZE			63u
#define FBFS_MAX_LIST_SECTORS			((0x80000u - 0x10000u) >> 9)
#define FBFS_DEFAULT_LIST_SIZE			(FBFS_MAX_LIST_SECTORS * FBFS_PRIMARY_SECTOR_DATA)
#define FBFS_MIN_PRI_SIZE			(63u * 256u)
#define FBFS_MAX_PRI_SIZE			65535u
#define FBFS_MIN_NAND_ALIGN			255u
#define FBFS_AR_MAX_SIZE			0x7fffffffu

#define FBFS_OPEN_ALLOW_ARCHIVE			0x0001u
#define FBFS_DISK_WRITABLE			0x0001u
#define FBFS_DISK_CREATE			0x0002u
#define FBFS_DISK_TRUNCATE			0x0004u

#define FBFS_FILE_EXTENDED			0x01u
#define FBFS_FILE_SYSLINUX			0x02u

#define FBFS_FORMAT_FAT_AUTO			-1
#define FBFS_FORMAT_FAT16			0
#define FBFS_FORMAT_FAT32			1
#define FBFS_FORMAT_NTFS			2
#define FBFS_FORMAT_EXFAT			3
#define FBFS_FORMAT_REFS			4

enum fbfs_error
{
	FBFS_OK = 0,
	FBFS_ERR_INVALID_ARGUMENT,
	FBFS_ERR_NO_MEMORY,
	FBFS_ERR_OPEN,
	FBFS_ERR_READ,
	FBFS_ERR_WRITE,
	FBFS_ERR_SEEK,
	FBFS_ERR_SIZE,
	FBFS_ERR_LOCK,
	FBFS_ERR_BAD_FS,
	FBFS_ERR_BAD_VERSION,
	FBFS_ERR_BAD_LIST,
	FBFS_ERR_NOT_FOUND,
	FBFS_ERR_ALREADY_EXISTS,
	FBFS_ERR_NO_SPACE,
	FBFS_ERR_TOO_LARGE,
	FBFS_ERR_UNSUPPORTED,
	FBFS_ERR_CORRUPT,
	FBFS_ERR_IO
};

struct fbfs_disk;
struct fbfs;

struct fbfs_disk_entry
{
	char name[32];
	uint64_t sectors;
	int has_fbinst;
};

struct fbfs_info
{
	int archive_mode;
	uint8_t ver_major;
	uint8_t ver_minor;
	uint32_t boot_base;
	uint32_t boot_size;
	uint32_t primary_size;
	uint32_t extended_size;
	uint32_t original_primary_size;
	uint32_t original_extended_size;
	uint32_t total_size;
	uint32_t list_start;
	uint32_t list_sectors;
	uint32_t list_size;
	uint32_t list_used;
	uint32_t list_tail;
	uint32_t archive_size;
};

struct fbfs_file_info
{
	char name[260];
	uint8_t flag;
	uint32_t data_start;
	uint32_t data_size;
	uint32_t data_time;
	int extended_area;
};

struct fbfs_raw_part
{
	uint32_t size;
	int fat_type;
	const char *label;
};

struct fbfs_format_options
{
	uint32_t partition_size;
	uint32_t primary_size;
	uint32_t extended_size;
	uint32_t list_size;
	uint32_t base;
	uint32_t unit_size;
	uint32_t max_sectors;
	uint32_t nand_align;
	int force;
	int zip;
	int raw;
	int align;
	int fat_type;
	int chs_mode;
	int debug_boot;
	uint8_t version_minor;
	const char *archive_path;
	/* tail partition: placed at the very end of the disk */
	uint32_t tail_size;     /* size in sectors; 0 = no tail partition */
	int tail_fat_type;      /* FBFS_FORMAT_FAT32 / NTFS / EXFAT / REFS */
	int extended_max;      /* 自动计算全盘占满扩展区 */
	const char *label;      /* volume label for main partition */
	const char *tail_label; /* volume label for tail partition */
	uint32_t mid_size;      /* size in sectors; 0 = no mid partition */
	int mid_fat_type;       /* FBFS_FORMAT_FAT32 / NTFS / EXFAT / REFS */
	const char *mid_label;  /* volume label for mid partition */
	int tail_max;           /* 1 = tail partition takes up all remaining space */
	/* Raw MBR formatting mode (No UD area, custom 1-4 partition layout) */
	int raw_mbr_mode;
	struct fbfs_raw_part raw_parts[4];
	int active_slot;
	int vbr_type;          /* 1 = NT6.x (BOOTMGR), 2 = NT5.x (NTLDR) */
};

struct fbfs_sync_options
{
	int copy_bpb;
	uint32_t bpb_size;
	int zip;
	uint32_t max_sectors;
	int chs_mode;
};

struct fbfs_archive_options
{
	uint32_t primary_size;
	uint32_t extended_size;
	uint32_t list_size;
	uint8_t version_minor;
};

const char *fbfs_strerror(int err);
const char *fbfs_last_error(void);

int fbfs_disk_open(const char *path, uint32_t flags, struct fbfs_disk **out);
void fbfs_disk_close(struct fbfs_disk *disk);
int fbfs_disk_read(struct fbfs_disk *disk, uint32_t sector, void *buffer, uint32_t count);
int fbfs_disk_write(struct fbfs_disk *disk, uint32_t sector, const void *buffer, uint32_t count);
uint64_t fbfs_disk_size(struct fbfs_disk *disk);
int fbfs_disk_truncate(struct fbfs_disk *disk, uint32_t sectors);
int fbfs_disk_lock(struct fbfs_disk *disk);
void fbfs_disk_unlock_all(void);
int fbfs_disk_list(struct fbfs_disk_entry *entries, size_t capacity, size_t *count);

int fbfs_mount(struct fbfs_disk *disk, uint32_t flags, struct fbfs **out);
void fbfs_close(struct fbfs *fs);
int fbfs_flush(struct fbfs *fs);
int fbfs_get_info(struct fbfs *fs, struct fbfs_info *info);
size_t fbfs_file_count(struct fbfs *fs);
int fbfs_get_file(struct fbfs *fs, size_t index, struct fbfs_file_info *info);
int fbfs_find_file(struct fbfs *fs, const char *name, struct fbfs_file_info *info);

int fbfs_clear(struct fbfs *fs);
int fbfs_add_file(struct fbfs *fs, const char *name, const char *path, uint32_t flags);
int fbfs_add_stream(struct fbfs *fs, const char *name, FILE *stream, uint32_t size,
	uint32_t timestamp, uint32_t flags);
int fbfs_add_buffer(struct fbfs *fs, const char *name, const void *buffer,
	uint32_t size, uint32_t flags);
int fbfs_export_file(struct fbfs *fs, const char *name, const char *path);
int fbfs_read_file(struct fbfs *fs, const char *name, void *buffer,
	uint32_t size, uint32_t *written);
int fbfs_remove_file(struct fbfs *fs, const char *name);
int fbfs_resize_file(struct fbfs *fs, const char *name, uint32_t size,
	uint8_t fill, uint32_t flags);
int fbfs_copy_file(struct fbfs *fs, const char *old_name, const char *new_name);
int fbfs_move_file(struct fbfs *fs, const char *old_name, const char *new_name);
int fbfs_pack(struct fbfs *fs);
int fbfs_check(struct fbfs *fs);
int fbfs_syslinux_patch(struct fbfs *fs, const char *name);

int fbfs_save_archive(struct fbfs *fs, const char *path, uint32_t list_size);
int fbfs_load_archive(struct fbfs *fs, const char *path);
int fbfs_create_archive(struct fbfs_disk *disk, const struct fbfs_archive_options *options);

int fbfs_format(struct fbfs_disk *disk, const struct fbfs_format_options *options);
int fbfs_restore(struct fbfs_disk *disk);
int fbfs_sync(struct fbfs *fs, const struct fbfs_sync_options *options);
int fbfs_update(struct fbfs *fs, int debug_boot);

int fbfs_map_partition(struct fbfs *fs, const char *name, uint32_t slot, uint8_t type);
int fbfs_unmap_partition(struct fbfs *fs, uint32_t slot);
int fbfs_format_file(struct fbfs *fs, const char *name, const char *type_name, uint32_t unit_size, int align, const char *label);
uint32_t fbfs_get_max_free_size(struct fbfs *fs, int is_ext);

int fbfs_import_mbr(struct fbfs_disk *disk, const char *file_path, uint32_t sectors);
int fbfs_import_pbr(struct fbfs_disk *disk, uint32_t slot, const char *file_path, int keep_bpb);
int fbfs_write_mbr(struct fbfs_disk *disk, const char *type);
int fbfs_write_pbr(struct fbfs_disk *disk, uint32_t slot, const char *type);
int fbfs_export_mbr(struct fbfs_disk *disk, const char *file_path, uint32_t sectors);
int fbfs_export_pbr(struct fbfs_disk *disk, uint32_t slot, const char *file_path, uint32_t sectors);
int fbfs_backup_sectors(struct fbfs_disk *disk, const char *file_path, uint32_t lba, uint32_t sectors);
int fbfs_restore_sectors(struct fbfs_disk *disk, const char *file_path, uint32_t lba, uint32_t sectors, int keep_dpt, int keep_bpb);
int fbfs_partition_op(struct fbfs_disk *disk, uint32_t slot, const char *op, const char *param);
int fbfs_query_mbr(struct fbfs_disk *disk, char *type_name, size_t max_len);
int fbfs_query_pbr(struct fbfs_disk *disk, uint32_t slot, char *type_name, size_t max_len);

uint32_t fbfs_parse_size(const char *text, int *ok);
uint32_t fbfs_file_time(const char *path);

#ifdef __cplusplus
}
#endif

#endif
