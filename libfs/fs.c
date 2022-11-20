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

#define FILE_NUM 32

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

struct file_descriptor_entry {
	struct file_entry *file_entry;
	int offset;
	int fd;
	bool is_open;
};

typedef struct superblock *superblock_t;
typedef struct fat *fat_t;
typedef struct file_entry *file_entry_t;
typedef struct file_descriptor_entry *file_descriptor_entry_t;

superblock_t superblock;
fat_t fat;
file_entry_t root_directory; //will be an array of size 128 though, each of size 32
file_descriptor_entry_t *file_descriptor_table;

uint8_t num_files_open;
uint8_t num_files_total;
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

int initialize_file_descriptor_table() {
	for (int fd = 0; fd < FILE_NUM; fd++) {
		file_descriptor_entry_t file_descriptor_entry = malloc(sizeof(struct file_descriptor_entry));
		if (file_descriptor_entry == NULL) {
			return -1;
		}

		file_descriptor_entry->file_entry = NULL;
		file_descriptor_entry->offset = 0;
		file_descriptor_entry->fd = fd;
		file_descriptor_entry->is_open = false;

		file_descriptor_table[fd] = file_descriptor_entry;
	}

	return 0;
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
	file_descriptor_table = malloc(FILE_NUM * sizeof(struct file_descriptor_entry));
	if (superblock == NULL
		|| fat == NULL
		|| root_directory == NULL
		|| file_descriptor_table == NULL) {
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

	// Determine the number of files in the root directory
	num_files_total = 0;
	for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if (root_directory[i].filename[0] != 0) {
			num_files_total++;
		}
	}

	// Initialize file_descriptors_table
	if (initialize_file_descriptor_table() == -1) {
		return -1;
	}

	return 0;
}

int fs_umount(void)
{
	// Ensure disk is open
	if (!disk_open || num_files_open) {
		return -1;
	}

	// Write all FAT entries to disk
	for (int i = 0; i < superblock->num_fat_blocks; i++) {
		if (block_write(i + 1, (u_int8_t*) fat->entries + (i * BLOCK_SIZE)) == -1) {
			return -1;
		}
	}

	// Write Root Directory to disk
	if (block_write(superblock->root_directory_block_index, root_directory) == -1) {
		return -1;
	}

	// Close disk
	if (block_disk_close() == -1) {
		return -1;
	}

	// Free local disk data members
	free(superblock);
	free(fat);
	free(root_directory);
	for (int i = 0; i < FILE_NUM; i++) {
		free(file_descriptor_table[i]);
	}
	free(file_descriptor_table);

	// Mark disc as closed
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
	printf("rdir_free_ratio=%d/%d\n", 128-num_files_total, 128);

	return 0;
}

bool validate_filename(const char *filename) {
	// Filename can't be null
	if (filename == NULL) {
		return false;
	}

	// Filename can't be empty
	if (filename[0] == '\0') {
		return false;
	}

	// Filename must contain null terminator in first 16 bytes
	bool filename_contains_null_terminator = false;
	for (int i = 0; i < FS_FILENAME_LEN; i++) {
		if (filename[i] == '\0') {
			filename_contains_null_terminator = true;
		}
	}

	if (!filename_contains_null_terminator) {
		return false;
	}

	// Filename cannot be longer than 16 bytes
	if (strlen(filename) > FS_FILENAME_LEN) {
		return false;
	}

	return true;
}

bool file_exists_in_root_directory(const char *filename) {
	for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		char *current_file = (char *) root_directory[i].filename;
		if(strcmp(current_file, filename) == 0) {
			return true;
		}
	}
	return false;
}

bool validate_file_creation(const char *filename)
{
	// Validate filename
	if (!validate_filename(filename)) {
		return false;
	}

	// Root directory must have enough space for new file
	int num_files_in_root_directory = 0;
	for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if (root_directory[i].filename[0] != '\0') {
			num_files_in_root_directory++;
		}
	}

	if (num_files_in_root_directory == FS_FILE_MAX_COUNT) {
		return false;
	}

	// File should not already exists
	if (file_exists_in_root_directory(filename)) {
		return false;
	}

    return true;
}

bool validate_file_deletion(const char *filename) {
	// Validate filename
	if (!validate_filename(filename)) {
		return false;
	}

	// File must exists in root directory
	if (!file_exists_in_root_directory(filename)) {
		return false;
	}

	return true;
}

bool validate_file_opening(const char *filename) {
	// Validate filename
	if (!validate_filename(filename)) {
		return false;
	}

	// File must exist in root directory
	if (!file_exists_in_root_directory(filename)) {
		return false;
	}

	// File descriptor must be available
	file_descriptor_entry_t free_file_descriptor_entry = NULL;
	for (int i = 0; i < FILE_NUM; i++) {
		if (!file_descriptor_table[i]->is_open) {
			free_file_descriptor_entry = file_descriptor_table[i];
			break;
		}
	}

	if (free_file_descriptor_entry == NULL) {
		return false;
	}

	return true;
}

int fs_create(const char *filename)
{
	// Check if disk is open
	if (!disk_open) {
		return -1;
	}

	// Check if filename is valid for creation
	if (!validate_file_creation(filename)) {
		return -1;
	}

	// Create empty file entry in root_directory
	for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if (root_directory[i].filename[0] != 0) {
			continue;
		}

		strcpy(root_directory[i].filename, filename);
		root_directory[i].file_size = 0;
		root_directory[i].index_first_data_block = FAT_EOC;
		num_files_total++;
		return 0;
	}

	// If no file created, return error status
	return -1;
}

int fs_delete(const char *filename)
{
	// Check if disk is open
	if (!disk_open) {
		return -1;
	}

	// Check if filename is valid for deletion
	if (!validate_file_deletion(filename)) {
		return -1;
	}

	// Find file from root directory
	file_entry_t file_entry = NULL;
	for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if (strcmp(root_directory[i].filename, filename) == 0) {
			file_entry = &root_directory[i];
			break;

		}
	}

	// Return error if no file was found in directory
	if (file_entry == NULL) {
		return -1;
	}

	// Clear FAT chain
	int fat_idx = file_entry->index_first_data_block;
	while (fat_idx != FAT_EOC) {
		uint16_t next_fat_idx = fat->entries[fat_idx];
		fat->entries[fat_idx] = 0;
		fat_idx = next_fat_idx;
		fat->fat_free++;
	}

	// Clear Root Entry
	file_entry->filename[0] = 0;
	file_entry->file_size = 0;
	file_entry->index_first_data_block = 0;
	num_files_total--;
	return 0;
}

int fs_ls(void)
{
	// Check if disk is open
	if (!disk_open) {
		return -1;
	}

	// Print files in specific format
	printf("FS Ls:\n");

	for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if (root_directory[i].filename[0] == 0) {
			continue;
		}

		printf("file: %s, size: %d, data_blk: %d\n",
			root_directory[i].filename,
			root_directory[i].file_size,
			root_directory[i].index_first_data_block);
	}

	return 0;
}

int fs_open(const char *filename)
{
	if (!disk_open
		|| block_disk_count() == -1
		|| !validate_file_opening(filename)) {
		return -1;
	}

	// Find free file descriptor
	file_descriptor_entry_t free_file_descriptor_entry = NULL;
	for (int fd = 0; fd < FILE_NUM; fd++) {
		if (!file_descriptor_table[fd]->is_open) {
			free_file_descriptor_entry = file_descriptor_table[fd];
			break;
		}
	}

	if (free_file_descriptor_entry == NULL) {
		return -1;
	}

	// Find file entry for filename
	file_entry_t file_entry = NULL;
	for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
		if (strcmp(root_directory[i].filename, filename) == 0) {
			file_entry = &root_directory[i];
			break;
		}
	}

	if (file_entry == NULL) {
		return -1;
	}

	// Initialize file descriptor
	free_file_descriptor_entry->file_entry = file_entry;
	free_file_descriptor_entry->is_open = true;
	free_file_descriptor_entry->offset = 0;
	num_files_open++;
	return free_file_descriptor_entry->fd;
}

bool validate_fd(int fd) {
	// Validate that fd is in range and that fd is open
	return fd >= 0 && fd < FILE_NUM && file_descriptor_table[fd]->is_open;
}

int fs_close(int fd)
{
	// Check if disk is open
	if (!disk_open) {
		return -1;
	}

	// Validate fd
	if (!validate_fd(fd)) {
		return -1;
	}

	// Close fd
	file_descriptor_table[fd]->is_open = false;
	num_files_open--;
	return 0;
}

int fs_stat(int fd)
{
	// Check if disk is open
	if (!disk_open) {
		return -1;
	}

	// Validate fd
	if (!validate_fd(fd)) {
		return -1;
	}

	return file_descriptor_table[fd]->file_entry->file_size;
}

int fs_lseek(int fd, size_t offset)
{
	// Check if disk is open
	if (!disk_open) {
		return -1;
	}

	// Validate fd
	if (!validate_fd(fd)) {
		return -1;
	}

	// Validate offset
	if (offset > file_descriptor_table[fd]->file_entry->file_size) {
		return -1;
	}

	file_descriptor_table[fd]->offset = offset;
	return 0;
}

int fs_write(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
}

int fs_read(int fd, void *buf, size_t count)
{
	// Error checking
	if (!disk_open || !validate_fd(fd) || buf == NULL) {
		return -1;
	}
	uint8_t *output_buffer = (uint8_t*)buf;
	// If the count is 0, then no point doing anything: just return 0 bytes read
	if (count == 0) {
		return 0;
	}

	file_descriptor_entry_t file = file_descriptor_table[fd];
	int offset_in_block = file->offset%BLOCK_SIZE;
	//what block is the offset in
	//previously file_offset_block_location
	int start_block_location = file->offset/BLOCK_SIZE; //divided 2 ints will return only the quotient
	size_t bytes_read = 0;
	size_t bytes_left_to_read = count;
	if (count > file->file_entry->file_size - file->offset) {
		//count is more than there are bytes to read
		bytes_left_to_read = file->file_entry->file_size - file->offset;
	}
	size_t output_buffer_index = 0;

	//first get the data block index of the offset
	int fat_idx = file->file_entry->index_first_data_block;
	int current_block_in_file = 0;
	while (current_block_in_file < start_block_location) {
		fat_idx = fat->entries[fat_idx];
		current_block_in_file++;
	}
	uint8_t *buffer = malloc(BLOCK_SIZE);

	int block_read_start = offset_in_block;
	int block_read_finish = offset_in_block + count;
	if (block_read_finish > BLOCK_SIZE) {
		//going to have to read across multiple blocks. for now, set it to the max
		block_read_finish = BLOCK_SIZE;
	}

	while (bytes_left_to_read > 0) {
		int block_read_amount = block_read_finish - block_read_start;
		block_read(fat_idx + superblock->data_block_start_index, buffer);
		memcpy(&output_buffer[output_buffer_index], &buffer[offset_in_block], block_read_amount);
		file->offset += block_read_amount;
		output_buffer_index += block_read_amount;
		bytes_left_to_read -= block_read_amount;
		bytes_read += block_read_amount;
		fat_idx = fat->entries[fat_idx];
		block_read_start = 0;
		block_read_finish = bytes_left_to_read;
		if (block_read_finish >= BLOCK_SIZE) {
			block_read_finish = BLOCK_SIZE;
		}
	}

	return bytes_read;
	//3 cases:
	//read from offset for count bytes. all within 1 block
	//read from offset until end of block
	//read entire block
	//wait all of these are just read from X to Y in block Z
	//so set it up to generalize such that Z is incremented one at a time as necessary
	//when reading an entire block, X is 0 and Y is the block size
	//when reading offset for count bytes, X is offset and Y is offset + count
	//when reading from offset until end, X is offset and Y is block size
	//the issue is chaining them together

}

