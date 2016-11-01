import math
import random
import sys

hex_digits = ["0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "a", "b", "c", "d", "e", "f"]
count = int(sys.argv[1])
for i in range(0, count):
	id = "0x"
	for j in range(0, 8):
		digit = math.floor(random.random() * 16)
		id += hex_digits[digit]
	print(id)