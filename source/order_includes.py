import enum
import functools
import os
import os.path
import re
import sys

third_party_prefixes = [
	"gtest/",
	"RtMidi",
	"portaudio"
]

class Command(enum.Enum):
	ANALYZE = 0
	APPLY = 1

command = Command.ANALYZE
if len(sys.argv) >= 2:
	command_arg = sys.argv[1]
	if command_arg == "-w":
		command = Command.APPLY

source_files = []
for dirpath, dirnames, filenames in os.walk("."):
	for filename in filenames:
		if filename.endswith(".h") or filename.endswith(".cpp"):
			source_files.append(os.path.join(dirpath, filename))

class IncludeType(enum.Enum):
	LOCAL = 0
	THIRD_PARTY = 1
	STANDARD_LIBRARY = 2

def get_ordering_group(c):
	if c == ord("."):
		return 0
	elif c == ord("/") or c == ord("\\"):
		return 1
	else:
		return 2

def compare_includes(a, b):
	if a[0].value != b[0].value:
		return a[0].value - b[0].value

	min_length = min(len(a[1]), len(b[1]))
	for index in range(min_length):
		ch_a = ord(a[1][index])
		ch_b = ord(b[1][index])
		ch_a_lower = ord(a[1][index].lower())
		ch_b_lower = ord(b[1][index].lower())
		ordering_group_a = get_ordering_group(ch_a)
		ordering_group_b = get_ordering_group(ch_b)
		if ordering_group_a != ordering_group_b:
			return ordering_group_a - ordering_group_b
		else:
			if ch_a_lower != ch_b_lower:
				return ch_a_lower - ch_b_lower
			elif ch_a != ch_b:
				return ch_a - ch_b

	return len(a[1]) - len(b[1])

for source_file in source_files:
	file = open(source_file, "r")

	# Make sure this source file starts with the expected preamble
	preamble = [
		"#pragma once\n",
		"\n"
	]

	has_preamble = True
	for preamble_line in preamble:
		line = file.readline()
		if line != preamble_line:
			has_preamble = False
			break

	if not has_preamble:
		file.seek(0)

	# Read all #include lines, skipping over empty lines
	includes = []
	original_include_lines = []
	source_lines = []
	fixup = True
	parsing_source = False
	has_any_includes = False
	while True:
		line = file.readline()
		if line == "\n":
			if parsing_source:
				source_lines.append(line)
			else:
				original_include_lines.append(line)
		elif line.startswith("#include "):
			if parsing_source:
				fixup = False
				print("{}: #include found later in the file, skipping this file".format(source_file))
				break

			# Parse the #include
			result = None
			if line.startswith("#include \""):
				result = re.fullmatch("#include \\\"([^\"]*)\\\"[ \t]*(//[^\n]*)?\n", line)
				include_type = IncludeType.LOCAL
			elif line.startswith("#include <"):
				result = re.fullmatch("#include <([^>]*)>[ \t]*(//[^\n]*)?\n", line)
				include_type = IncludeType.STANDARD_LIBRARY

			if result is None:
				fixup = False
				print("{}: failed to parse '{}', skipping this file".format(source_file, line.strip()))
				break

			path = result.group(1)
			if include_type is IncludeType.STANDARD_LIBRARY and any(path.startswith(x) for x in third_party_prefixes):
				include_type = IncludeType.THIRD_PARTY

			includes.append((include_type, line))
			original_include_lines.append(line)
			has_any_includes = True
		elif line.startswith("#"):
			parsing_source = True
			source_lines.append(line)
		elif line.startswith("//"):
			parsing_source = True
			source_lines.append(line)
		else:
			parsing_source = True
			if len(line) > 0:
				source_lines.append(line)
			else:
				break

	if not has_any_includes:
		fixup = False

	if not fixup:
		continue

	# Re-order the includes and check if they've changed
	sorted_includes = sorted(includes, key = functools.cmp_to_key(compare_includes))
	new_include_lines = []
	previous_include = None
	for include_type, include_line in sorted_includes:
		if previous_include is not None:
			if include_type != previous_include[0]:
				new_include_lines.append("\n")
			elif include_type == IncludeType.LOCAL:
				# Add a newline between root-level local directories
				local_directory = include_line[:include_line.find("/")]
				previous_local_directory = previous_include[1][:previous_include[1].find("/")]
				if local_directory != previous_local_directory:
					new_include_lines.append("\n")

		new_include_lines.append(include_line)
		previous_include = (include_type, include_line)

	if len(source_lines) > 0:
		new_include_lines.append("\n")

	if original_include_lines != new_include_lines:
		if command is Command.ANALYZE:
			print(source_file)
			print("\tOLD:")
			print("".join("\t\t" + x for x in original_include_lines))
			print("\tNEW:")
			print("".join("\t\t" + x for x in new_include_lines))
		elif command is Command.APPLY:
			file.close()
			file = open(source_file, "w")

			if has_preamble:
				for line in preamble:
					file.write(line)

			for line in new_include_lines:
				file.write(line)

			for line in source_lines:
				file.write(line)

			print("Applied changes to {}".format(source_file))
