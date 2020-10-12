import math
import random
import sys

count = int(sys.argv[1])
for i in range(0, count):
	print("0x{:08x}".format(random.getrandbits(32)))
