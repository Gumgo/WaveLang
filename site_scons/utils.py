import os
import os.path

SOURCE_EXTENSIONS = ["c", "cpp"]

def enumerate_files(env, directory, extensions):
	results = []

	build_node = env.Dir(directory)
	source_node = build_node.srcnode()
	for source_dirpath, dirnames, filenames in os.walk(source_node.abspath):
		build_dirpath = os.path.join(source_dirpath.replace(source_node.abspath, build_node.abspath))
		for extension in extensions:
			results += env.Glob("{}/*.{}".format(build_dirpath, extension))

	return results
