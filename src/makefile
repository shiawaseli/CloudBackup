# makefile CloudBackup

bin = CloudServer CloudClient
all: $(bin)

CloudServer: CloudServer.cpp httplib.h MyUtil.hpp CloudBackupServer.hpp
	g++ -std=c++11 -pthread -lz -lboost_filesystem -lboost_system -lboost_thread $< -o $@
CloudClient: CloudClient.cpp httplib.h MyUtil.hpp CloudBackupClient.hpp
	g++ -std=c++11 -pthread -lz -lboost_filesystem -lboost_system -lboost_thread $< -o $@
clean:
	rm -rf $(bin)
