#include "CloudBackupClient.hpp"

int main()
{
	std::string path, dirData("./cli_dirpath.dat");
	std::vector<std::string> listenDirs;
	std::ifstream fin(dirData);
	if (!fin.is_open()) {
		std::cout << "open paths file error" << std::endl;
		return 1;
	}
	while (fin >> path) {
		listenDirs.push_back(path);
	}
	CloudBackup::HttpClientModule client(listenDirs, "39.102.34.164", 9000);
	client.start();

	return 0;
}
