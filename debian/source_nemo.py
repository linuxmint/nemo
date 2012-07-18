import os, apport.packaging
from apport.hookutils import *

def add_info(report):
	# the crash is not in nemo code so reassign
	if report.has_key("Stacktrace") and "/usr/lib/nemo" in report["Stacktrace"]:
		for words in report["Stacktrace"].split():
			if words.startswith("/usr/lib/nemo"):
				report.add_package_info(apport.packaging.get_file_package(words))
				return

	# collect informations on the /usr/lib/nemo components 
	plugin_packages = set()
	for dirpath, dirnames, filenames in os.walk("/usr/lib/nemo"):
		for filename in filenames:
			path = os.path.join(dirpath, filename)
			package = apport.packaging.get_file_package(path)
			if package == 'nemo':
				continue

			plugin_packages.add(package)

	report["usr_lib_nemo"] = package_versions(*sorted(plugin_packages))
	attach_gsettings_package(report, 'nemo-data')

