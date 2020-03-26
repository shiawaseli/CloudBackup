#include "CloudBackup.hpp"

void testData() {
  using namespace std;
  using namespace CloudBackup;

  FileDataManager mgr("");
  vector<string> List;
  mgr.getList(List);
  cout << "show init data: " << endl;
  for (auto &t : List) {
    cout << t << endl;
  }

  int input;
  string filename, pathname;
  
  cout << ios::boolalpha  << "input your choice(1-insert,2-delete,3-showdata,4-checkcompress,5-checkexist,6-changedata): ";
  while (cin >> input) {
    if (input == 1) {
      cin >> filename >> pathname;
      mgr.insertData(filename, pathname);
    } else if (input == 2) {
      cin >> filename >> pathname;
      mgr.deleteData(filename, pathname);
    } else if (input == 3) {
      mgr.getList(List);
      cout << "show data list: " << endl;
      for (auto &t : List) {
        cout << t << endl;
      }
    } else if (input == 4) {
      cin >> filename >> pathname;
      cout << mgr.isCompressedFile(pathname + filename) << endl;;
    } else if (input == 5) {
      cin >> filename >> pathname;
      cout << mgr.isExistFile(pathname + filename) << endl;;
    } else if (input == 6) {
      cin >> filename >> pathname;
      mgr.changeData(filename, pathname);
    } else if (input == 0) {
      break;
    }
    cout << "input your choice(1-insert,2-delete,3-showdata,4-checkcompress,5-checkexist,6-changedata): ";
  }

}

void testCompress(int argc, char *argv[])
{  
  if (argc != 3) {
    std::cout << "argument is not equal 3" << std::endl;
    return;
  }
  CloudBackup::CompressUtil::compress(argv[1], argv[2]);
  CloudBackup::CompressUtil::decompress(argv[2], argv[1] + std::string(".txt"));
}

int main(int argc, char *argv[])
{
  //testCompress(argc, argv);
  testData();


  return 0;
}
