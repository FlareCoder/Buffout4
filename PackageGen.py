import os
import zipfile
import zlib

def make_rel_archive(a_parent, a_name):
	archive = zipfile.ZipFile("release/" + a_name + ".zip", "w", zipfile.ZIP_DEFLATED)
	def do_write(a_relative):
		archive.write(a_parent + a_relative, a_relative)

	do_write("F4SE/Plugins/" + a_name + ".dll")
	do_write("F4SE/Plugins/" + a_name + ".toml")
	do_write("F4SE/Plugins/" + a_name + "_preload.txt")

def make_dbg_archive(a_parent, a_name):
	archive = zipfile.ZipFile("release/" + a_name + "_pdb" + ".zip", "w", zipfile.ZIP_DEFLATED)
	archive.write(a_parent + "F4SE/Plugins/" + a_name + ".pdb", a_name + ".pdb")

def main():
	os.chdir(os.path.dirname(os.path.realpath(__file__)))
	try:
		os.mkdir("release")
	except FileExistsError:
		pass

	parent = os.environ["Fallout4Path"] + "/Data/"
	project = os.path.split(os.getcwd())[1].strip(os.sep)
	make_rel_archive(parent, project)
	make_dbg_archive(parent, project)
	
if __name__ == "__main__":
	main()
