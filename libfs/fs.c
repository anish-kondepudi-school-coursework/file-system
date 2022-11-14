#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "disk.h"
#include "fs.h"

#define SIGNATURE_LENGTH 8

/* TODO: Phase 1 */
struct __attribute__ ((__packed__)) superblock {
	uint64_t signature;
	uint16_t num_blocks_of_virtual_disk;
	uint16_t root_directory_block_index;
	uint16_t data_block_start_index;
	uint16_t num_data_blocks;
	uint8_t num_fat_blocks;
	uint8_t padding[4079]; //such a waste of space...
};

struct __attribute__ ((__packed__)) fat {
	uint16_t num_entries; //equal to the num_data_blocks
	uint16_t fat_free;
	uint16_t *entries;
};

struct __attribute__ ((__packed__)) file_entry {
	uint8_t filename[16];
	uint32_t file_size;
	uint16_t index_first_data_block;
	uint8_t padding[10]; //such a waste of space...
};

typedef struct superblock *superblock_t;
typedef struct fat *fat_t;
typedef struct file_entry *file_entry_t;

superblock_t superblock;
fat_t fat;
file_entry_t root_directory; //will be an array of size 128 though, each of size 32

uint8_t num_files_open;
bool disk_open;

int fs_mount(const char *diskname)
{
	/* TODO: Phase 1 */
	if (disk_open) {
		return -1;
	}
	if (block_disk_open(diskname) == -1) {
		return -1;
	}
	disk_open = true;
	superblock = malloc(sizeof(struct superblock));

	if (superblock == NULL) {
		return -1;
	}

	block_read(0, superblock);

	uint8_t signature_array[] = {'E','C','S','1','5','0','F','S'};
	
	//signature_array[0] = 'E';
	//signature_array[1] = 'C';
	//signature_array[2] = 'S';
	//signature_array[3] = '1';
	//signature_array[4] = '5';
	//signature_array[5] = '0';
	//signature_array[6] = 'F';
	//signature_array[7] = 'S';
	if (memcmp(&(superblock->signature), signature_array, SIGNATURE_LENGTH) != 0) {
		printf("mismatched signatures!\n"); //for testing only, like all the following print statements
		return -1;
	}

	if (superblock->num_data_blocks <= 0) {
		return -1;
	}

	int expected_block_count = superblock->num_data_blocks + superblock->num_fat_blocks + 2;
	if (block_disk_count() != expected_block_count) {
		printf("total block count is not adding up, block_disk_count() says %d while manual counting says %d\n", block_disk_count(), expected_block_count);
		return -1;
	}
	fat = malloc(2 * sizeof(uint16_t) + sizeof(uint16_t*));
	if (fat == NULL) {
		return -1;
	}
	fat->num_entries = superblock->num_data_blocks;
	fat->fat_free = superblock->num_data_blocks - 1;
	fat->entries = malloc((superblock->num_data_blocks) * sizeof(uint16_t));
	int fat_block_counter = 1;	
	while (fat_block_counter <= superblock->num_fat_blocks) {
		//printf("fat_block_counter at %d\n", fat_block_counter);
		block_read(fat_block_counter, &(fat->entries[(fat_block_counter - 1) * 2048]));//each entry of fat is 2 bytes, while each block is 4096. So 2048 entries per block
		fat_block_counter++;
	}

	if (fat->entries[0] != 0xFFFF) {
		printf("uh the first fat entry should be 0xFFFF, instead its %x\n", fat->entries[0]);
		return -1;
	}

	root_directory = malloc(128 * sizeof(struct file_entry));
	if (root_directory == NULL) {
		return -1;
	}
	//since fat_block_counter is incremented to 1 past the fat, then that index is the root directory
	block_read(fat_block_counter, root_directory);
	num_files_open = 0;
	return 0;
}

int fs_umount(void)
{
	/* TODO: Phase 1 */
	if (!disk_open || num_files_open) {
		return -1;
	}
	if (block_disk_close() < 0) {
		return -1;
	}
	disk_open = false;
	free(superblock);
	free(fat);
	free(root_directory);
	return 0;
}

int fs_info(void)
{
	/* TODO: Phase 1 */
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

