import os.path
import string
import random

LARGEST_DISK_SIZE_BYTES = 100_000_000

filename = "file_that_is_larger_than_disk"
if not os.path.exists(filename):
    with open(filename, "w") as file:
        for i in range(LARGEST_DISK_SIZE_BYTES):
            random_character = random.choice(string.ascii_letters)
            file.write(random_character)

