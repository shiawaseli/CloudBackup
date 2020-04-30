#ifndef _MYUTIL_HPP_
#define _MYUTIL_HPP_

#include <string>
#include <iostream>
#include <fstream>
#include <boost/filesystem.hpp>
#include <boost/thread/thread.hpp>

namespace CloudBackup {
	// ������
	class MyUtil
	{
	public:
		// ��ȡָ���ļ��е��������ݲ����浽dst��
		static bool readFile(const std::string &name, std::string &dst) {
			std::ifstream fin(name.c_str(), std::ios::binary);
			if (!fin.is_open()) {
				std::cout << "open file " + name + " failed!" << std::endl;
				return false;
			}

			fin.seekg(0, fin.end);
			int length = fin.tellg(); // ����ļ����ݵĳ���
			fin.seekg(0, fin.beg);

			dst.resize(length);
			fin.read(&dst[0], length);
			if (!fin.good()) {
				std::cout << "read file " + name + " failed!" << std::endl;
				fin.close();
				return false;
			}
			fin.close();
			return true;
		}
		// ��src�е���������д�뵽ָ���ļ���
		static bool writeFile(const std::string &name, const std::string &src) {
			std::ofstream fout(name.c_str(), std::ios::binary);
			if (!fout.is_open()) {
				std::cout << "open file " + name + " failed!" << std::endl;
				return false;
			}

			fout.write(src.c_str(), src.length());
			if (!fout.good()) {
				std::cout << "write file " + name + " failed!" << std::endl;
				fout.close();
				return false;
			}
			fout.close();
			return true;
		}
    static std::map<std::string, std::string> getConfig(const std::string &cnfFile, const std::string &item)
    {
      std::ifstream fin(cnfFile.c_str());
      if (!fin.is_open()) {
        std::cout << "open file " + cnfFile + " failed!" << std::endl;
        throw std::runtime_error("open file " + cnfFile + " failed!");
      }

      std::string tmp;
      std::map<std::string, std::string> config;
      while (getline(fin, tmp)) {
        if (tmp.size() > 2 && tmp.substr(1, tmp.size() - 2) == item) {
          break;
        }
      }
      while (getline(fin, tmp)) {
        if (tmp[0] == '#' || tmp.find("=") == std::string::npos) {
          continue;
        } else if (tmp[0] == '[') {
          break;
        }
        auto pos = tmp.find("=");
        config[tmp.substr(0, pos)] = tmp.substr(pos + 1, tmp.find("#") - pos - 1);
      }
      return config;
    }
		// ����·��
		static std::string& DealPath(std::string &path) {
			while (path.find("\\") != std::string::npos) {
				size_t pos = path.find("\\");
				path.erase(path.begin() + pos, path.begin() + pos + 1);
				path.insert(path.begin() + pos, '/');
			}
			if (path[path.size() - 1] == '/') {
				path.pop_back();
			}
			return path;
		}
		// ���ȵ��ļ��ж�
		static bool isNonHotFile(const std::string &filepath, time_t intervalTime) {
			time_t cur = time(nullptr);
			struct stat buf;
			int ret = stat(filepath.c_str(), &buf);
			if (ret < 0) {
				std::cout << "get " << filepath << " stat error" << std::endl;
				return false;
			}
			return (cur - buf.st_atime) > intervalTime;
		}
		// sleep����
		static void MySleep(int secs) {
			boost::xtime xt;
			boost::xtime_get(&xt, boost::xtime_clock_types::TIME_UTC_); //�õ���ǰʱ���xt.
			xt.sec += secs; //�ڵ�ǰʱ����ϼ���secs��,�õ�һ���µ�ʱ���,
			//Ȼ��ʹ�߳����ߵ�ָ����ʱ���xt.
			boost::thread::sleep(xt); // sleep for secs second;
		}
	};
}
#endif /* _MYUTIL_HPP_ */
