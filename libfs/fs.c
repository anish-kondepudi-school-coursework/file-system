#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "disk.h"
#include "fs.h"

#define BLOCK_SIZE 4096

#define SIGNATURE_LENGTH 8
#define FILENAME_LENGTH 16

#define SUPERBLOCK_PADDING 4079
#define FILE_ENTRY_PADDING 10

#define FAT_EOC 0xFFFF
#define FIRST_FAT_BLOCK_INDEX 1

struct __attribute__ ((__packed__)) superblock {
	uint64_t signature;
	uint16_t num_blocks_of_virtual_disk;
	uint16_t root_directory_block_index;
	uint16_t data_block_start_index;
	uint16_t num_data_blocks;
	uint8_t num_fat_blocks;
	uint8_t padding[SUPERBLOCK_PADDING];
};

struct __attribute__ ((__packed__)) fat {
	uint16_t num_entries; //equal to the num_data_blocks
	uint16_t fat_free;
	uint16_t *entries;
};

struct __attribute__ ((__packed__)) file_entry {
	uint8_t filename[FILENAME_LENGTH];
	uint32_t file_size;
	uint16_t index_first_data_block;
	uint8_t padding[FILE_ENTRY_PADDING];
};

typedef struct superblock *superblock_t;
typedef struct fat *fat_t;
typedef struct file_entry *file_entry_t;

superblock_t superblock;
fat_t fat;
file_entry_t root_directory; //will be an array of size 128 though, each of size 32

uint8_t num_files_open;
bool disk_open;

bool validate_superblock() {
	// Validate signature of superblock
	uint8_t signature_array[] = {'E','C','S','1','5','0','F','S'};
	if (memcmp(&(superblock->signature), signature_array, SIGNATURE_LENGTH) != 0) {
		printf("mismatched signatures!\n");
		return false;
	}

	// Validate number of data blocks for superblock
	if (superblock->num_data_blocks <= 0) {
		return false;
	}

	// Validate block count for superblock
	int expected_block_count_from_manual_calculation = superblock->num_data_blocks + superblock->num_fat_blocks + 2;
	int disk_block_count = block_disk_count();
	if (disk_block_count != expected_block_count_from_manual_calculation) {
		printf("total block count is not adding up from block_disk_count.\n Found: %d\nExpected %d\n",
			disk_block_count,
			expected_block_count_from_manual_calculation);
		return false;
	}

	if (superblock->num_blocks_of_virtual_disk != expected_block_count_from_manual_calculation) {
		printf("total block count is not adding up from superblock->num_blocks_of_virtual_disk.\n Found: %d\nExpected %d\n",
			disk_block_count,
			expected_block_count_from_manual_calculation);
		return false;
	}

	return true;
}

int initialize_fat() {
	// Initialize fat data members
	fat->num_entries = superblock->num_data_blocks;
	fat->fat_free = superblock->num_data_blocks - 1;
	fat->entries = malloc(superblock->num_data_blocks * sizeof(uint16_t));

	// Handle case where malloc fails
	if (fat->entries == NULL) {
		return -1;
	}

	// Populate fat entries
	for (int fat_block_idx = FIRST_FAT_BLOCK_INDEX; fat_block_idx <= superblock->num_fat_blocks; fat_block_idx++) {
		// each entry of fat is 2 bytes, while each block is 4096 bytes. So 2048 entries per block
		block_read(fat_block_idx, &(fat->entries[(fat_block_idx - 1) * (BLOCK_SIZE / 2)]));
	}

	return 0;
}

bool validate_fat() {
	// Validate that first fat entry is FAT_EOC
	if (fat->entries[0] != FAT_EOC) {
		printf("the first fat entry should be 0xFFFF, instead its %x\n", fat->entries[0]);
		return false;
	}

	return true;
}

int fs_mount(const char *diskname)
{
	// Handle case where disk is already open
	if (disk_open) {
		return -1;
	}

	// Handle case where opening disk fails
	if (block_disk_open(diskname) == -1) {
		return -1;
	}

	// Allocate memory for the superblock, fat, and root directory
	superblock = malloc(sizeof(struct superblock));
	fat = malloc(sizeof(struct fat));
	root_directory = malloc(FS_FILE_MAX_COUNT * sizeof(struct file_entry));
	if (superblock == NULL || fat == NULL || root_directory == NULL) {
		return -1;
	}

	// Mark disk as open and set number of open files to 0
	disk_open = true;
	num_files_open = 0;

	// Read superblock from disk
	block_read(0, superblock);

	// Validate data members for superblock
	if (!validate_superblock()) {
		return -1;
	}

	// Initialize data members for fat
	if (initialize_fat() == -1) {
		return -1;
	}

	// Validate data members for fat
	if (!validate_fat()) {
		return -1;
	}

	// Read root directory from disk
	block_read(superblock->root_directory_block_index, root_directory);

	return 0;
}

int fs_umount(void)
{
	if (!disk_open || num_files_open) {
		return -1;
	}

	if (block_disk_close() == -1) {
		return -1;
	}

	free(superblock);
	free(fat);
	free(root_directory);

	disk_open = false;
	return 0;
}

int fs_info(void)
{
	if (!disk_open) {
		return -1;
	}

	printf("FS Info:\n");
	printf("total_blk_count=%d\n", block_disk_count());
	printf("fat_blk_count=%d\n", superblock->num_fat_blocks);
	printf("rdir_blk=%d\n", superblock->num_fat_blocks+1);
	printf("data_blk=%d\n", superblock->num_fat_blocks+2);
	printf("data_blk_count=%d\n", superblock->num_data_blocks);
	printf("fat_free_ratio=%d/%d\n", fat->fat_free, superblock->num_data_blocks);
	printf("rdir_free_ratio=%d/%d\n", 128-num_files_open, 128);

	return 0;
}

int fs_create(const char *filename)
{
	/* TODO: Phase 2 */
}

int fs_delete(const char *filename)
{
	/* TODO: Phase 2 */
}

int fs_ls(void)
{
	/* TODO: Phase 2 */
}

int fs_open(const char *filename)
{
	/* TODO: Phase 3 */
}

int fs_close(int fd)
{
	/* TODO: Phase 3 */
}

int fs_stat(int fd)
{
	/* TODO: Phase 3 */
}

int fs_lseek(int fd, size_t offset)
{
	/* TODO: Phase 3 */
}

int fs_write(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
}

int fs_read(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
}

