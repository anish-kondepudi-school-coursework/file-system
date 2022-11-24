import os.path
import string
import random

LARGEST_DISK_SIZE_BYTES = 100_000_000
FS_FILE_MAX_COUNT = 128

# filename = "file_that_is_larger_than_disk"
# if not os.path.exists(filename):
#     with open(filename, "w") as file:
#         for i in range(LARGEST_DISK_SIZE_BYTES):
#             random_character = random.choice(string.ascii_letters)
#             file.write(random_character)

script_name = "large_empty_file1.script"
with open(script_name, "w") as file:
	file.write("MOUNT\n")
	for i in range(FS_FILE_MAX_COUNT + 1):
		file.write("CREATE	file_fs" + str(i + 1) + "\n")
	for i in range(FS_FILE_MAX_COUNT + 1):
		file.write("DELETE	file_fs" + str(i + 1) + "\n")
	file.write("UMOUNT\n")

script_name = "large_empty_file2.script"
with open(script_name, "w") as file:
	file.write("MOUNT\n")
	for i in range(FS_FILE_MAX_COUNT):
		file.write("CREATE	file_fs" + str(i + 1) + "\n")
	file.write("DELETE	file_fs10\n")
	file.write("CREATE	file_fs" + str(FS_FILE_MAX_COUNT + 1) + "\n")
	for i in range(FS_FILE_MAX_COUNT + 1):
		if i + 1 == 10:
			continue
		file.write("DELETE	file_fs" + str(i + 1) + "\n")
	file.write("UMOUNT\n")

script_name = "large_small_file.script"
with open(script_name, "w") as file:
	file.write("MOUNT\n")
	file_numbers = [x for x in range(FS_FILE_MAX_COUNT)]
	for file_number in sorted(file_numbers, key = lambda _ : random.random()):
		file.write("CREATE	file_fs" + str(file_number + 1) + "\n")
	file_number_to_data = {}
	for file_number in sorted(file_numbers, key = lambda _ : random.random()):
		random_string = ''.join(random.choice(string.ascii_lowercase) for _ in range(random.randint(1, 100))) # can be 4096
		file.write("OPEN	file_fs" + str(file_number + 1) + "\n")
		file.write("WRITE	DATA	" + random_string + "\n")
		file.write("CLOSE\n")
		file_number_to_data[file_number] = random_string
	for file_number in sorted(file_numbers, key = lambda _ : random.random()):
		file.write("OPEN	file_fs" + str(file_number + 1) + "\n")
		file.write("READ	" + str(len(file_number_to_data[file_number])) + "	DATA	" + file_number_to_data[file_number] + "\n")
		file.write("CLOSE\n")
	for file_number in file_numbers:
		file.write("DELETE	file_fs" + str(file_number + 1) + "\n")
	file.write("UMOUNT\n")
