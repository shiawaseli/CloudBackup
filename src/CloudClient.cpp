#include "CloudBackupClient.hpp"

int main()
{
  std::map<std::string, std::string> config = CloudBackup::MyUtil::getConfig("./CBackup.cnf", "CloudClient");
	std::string dirpath, dirData(config["cliDirPath"]);
	std::vector<std::string> listenDirs;
	std::ifstream fin(dirData);
	if (!fin.is_open()) {
		std::cout << "open paths file error" << std::endl;
		return 1;
	}
	while (fin >> dirpath) {
		listenDirs.push_back(dirpath);
	}
	CloudBackup::HttpClientModule client(listenDirs, config["srvIP"], atoi(config["srvPort"].c_str()));
	client.start();

	return 0;
}
