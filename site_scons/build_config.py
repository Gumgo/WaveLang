from enum import Enum
import json

class Configuration(Enum):
	DEBUG = "debug"
	DEVELOPMENT = "development"
	RELEASE = "release"

class SIMDSupport(Enum):
	NONE = "none"
	AVX = "avx"
	AVX2 = "avx2"
	# $TODO add NEON options

# As this grows, subdivide it into categories (e.g. Windows, Linux)
class BuildOptions:
	def __init__(self):
		self.llvm_config_dir = "C:\\Program Files (x86)\\LLVM\\bin"
		self.llvm_lib_dir_debug = ""
		self.llvm_lib_dir_release = ""

		self.portaudio_include_dir = ""
		self.portaudio_lib_dir_debug = ""
		self.portaudio_lib_dir_release = ""

		self.rtmidi_include_dir = ""
		self.rtmidi_lib_dir_debug = ""
		self.rtmidi_lib_dir_release = ""

		self.rapidxml_include_dir = ""

		self.googletest_include_dir = ""
		self.googletest_lib_dir_debug = ""
		self.googletest_lib_dir_release = ""

		self.simd_support = SIMDSupport.AVX2

	def read(self, filename):
		try:
			with open(filename, "r") as file:
				data = json.load(file)
				for key, value in self.__dict__.items():
					expected_type = type(getattr(self, key))
					setattr(self, key, expected_type(data[key]))
			return True
		except IOError:
			return False

	def write(self, filename):
		# Note: I'm using JSON because it is a platform-agnostic standard, is there a better format? configparser would
		# work but it seems fairly Windows-specific
		data = {}
		for key, value in self.__dict__.items():
			if isinstance(value, Enum):
				value = value.value
			data[key] = value
		with open(filename, "w") as file:
			json.dump(data, file, indent = 4)
