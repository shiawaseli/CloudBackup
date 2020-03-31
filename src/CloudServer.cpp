#include "CloudBackupServer.hpp"
#include <thread>

void thr_hotfile()
{
  CloudBackup::FileManageModule fm;
  fm.start();
}
void thr_httpserv()
{
  CloudBackup::HttpServerModule srv;
  srv.start();
}

int main(int argc, char *argv[])
{
  std::thread hotfile(thr_hotfile);
  std::thread httpserv(thr_httpserv);
  hotfile.join();
  httpserv.join();

  return 0;
}
