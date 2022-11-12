#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

/* TODO: Phase 1 */
struct __attribute__ ((__packed__)) superblock_t {
	uint64_t signature;
	uint16_t num_blocks_of_virtual_disk;
	uint16_t root_directory_block_index;
	uint16_t data_block_start_index;
	uint16_t num_data_blocks;
	uint8_t num_fat_blocks;
};

//block_disk_count(void);
struct fat_t { //should this have the packed attribute?
	uint16_t num_entries; //equal to the num_data_blocks
	uint16_t fat_free;
	uint16_t *entries;
};

struct __attribute__ ((__packed__)) file_entry_t {
	uint8_t filename[16];
	uint32_t file_size;
	uint16_t index_first_data_block;
	//does padding have to be part of this?
};

struct superblock_t *superblock;
struct fat_t *fat;
struct file_entry_t *root_directory;
uint8_t num_files_open;
int disk_open;

int fs_mount(const char *diskname)
{
	/* TODO: Phase 1 */
	if (disk_open) {
		return -1;
	}
	if (block_disk_open(diskname) < 0) {
		return -1;
	}
	disk_open = 1;
	superblock = malloc(sizeof(struct superblock_t));

	block_read(0, superblock);

	uint8_t signature_array[8];// = ['E','C','S','1','5','0','F','S'];
	
	uint64_t true_signature;
	memcpy(&true_signature, signature_array, 8); //probably a better way to do this lol
	signature_array[0] = 'E';
	signature_array[1] = 'C';
	signature_array[2] = 'S';
	signature_array[3] = '1';
	signature_array[4] = '5';
	signature_array[5] = '0';
	signature_array[6] = 'F';
	signature_array[7] = 'S';
	if (memcmp(&(superblock->signature), signature_array, 8) != 0) {
		printf("mismatched signatures!\n"); //for testing only, like all the following print statements
		return -1;
	}

	if (superblock->num_data_blocks > 0) {
		printf("nice, %d data blocks\n", superblock->num_data_blocks);
	}
	else {
		return -1;
	}
	if (block_disk_count() != superblock->num_data_blocks + superblock->num_fat_blocks + 2) {
		printf("total block count is not adding up, block_disk_count() says %d while manual counting says %d\n", block_disk_count(), superblock->num_data_blocks + superblock->num_fat_blocks + 2);
		return -1;
	}
	fat = malloc(2*sizeof(uint16_t) + superblock->num_data_blocks*sizeof(uint16_t));
	fat->num_entries = superblock->num_data_blocks;
	fat->fat_free = superblock->num_data_blocks-1;
	if (fat->entries[0] != 0xFFFF) {
		printf("uh the first fat entry should be 0xFFFF, instead its %x\n", fat->entries[0]);
		return -1;
	}

	root_directory = malloc(128 * sizeof(struct file_entry_t)); //starts off empty anyways
	num_files_open = 0;
	return 0;
}

int fs_umount(void)
{
	/* TODO: Phase 1 */
	if (!disk_open || num_files_open) return -1;
	if (block_disk_close() < 0) return -1;
	disk_open = 0;
	free(superblock);
	free(fat);
	free(root_directory);
	return 0;
}

int fs_info(void)
{
	/* TODO: Phase 1 */
	if (!disk_open) return -1;
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

