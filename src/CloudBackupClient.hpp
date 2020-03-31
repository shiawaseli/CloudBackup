#ifndef _CLOUDBACKUPCLIENT_HPP_
#define _CLOUDBACKUPCLIENT_HPP_

#include <vector>
#include <string>
#include <iostream>
#include <sys/stat.h>
#include <unordered_map>
#include <boost/filesystem.hpp>
#include "httplib.h"
#include "MyUtil.hpp"

namespace CloudBackup {
	// 本地文件信息管理类
	class LocalFileManager
	{
	public:
		LocalFileManager(const std::string &name = "cli_log.dat") : m_filename(name) {
			_loadData();
		}
		~LocalFileManager() {
			_storageData();
		}
		bool isExistFile(const std::string &filepath) {
			return m_map.find(filepath) != m_map.end();
		}
		bool isNewFile(const std::string &filepath) {
			struct stat buf;
			int ret = stat(filepath.c_str(), &buf);
			if (ret < 0) {
				std::cout << "get " << filepath << " stat error" << std::endl;
				return false;
			}
			return !isExistFile(filepath) || (getMtime(filepath) < buf.st_mtime);
		}
		bool insertData(const std::string &filepath) {
			struct stat buf;
			int ret = stat(filepath.c_str(), &buf);
			if (ret < 0) {
				std::cout << "get " << filepath << " stat error" << std::endl;
				return false;
			}
			m_map[filepath] = buf.st_mtime;
			_storageData();
			return true;
		}
		bool deleteData(const std::string &filepath) {
			if (!isExistFile(filepath)) {
				return false;
			}
			m_map.erase(m_map.find(filepath));
			_storageData();
			return true;
		}
		bool getAllList(std::vector<std::string> &fileList) {
			fileList.clear();
			for (auto &it : m_map) {
				fileList.push_back(it.first);
			}
			return false;
		}
		std::vector<std::string> getUpdateFileList(std::string Dirname) {
			std::vector<std::string> files;
			boost::filesystem::recursive_directory_iterator iter_begin(Dirname), iter_end;
			for (; iter_begin != iter_end; ++iter_begin) {
				std::string filepath = iter_begin->path().string();
				MyUtil::DealPath(filepath);
				if (!boost::filesystem::is_directory(filepath) && isNewFile(filepath)) {
					files.push_back(filepath);
				}
			}
			return files;
		}
		time_t getMtime(std::string filepath) {
			if (!isExistFile(filepath))
				return 0;
			return m_map[filepath];
		}
	private:
		void _loadData() {
			std::fstream fin;
			fin.open(m_filename);
			if (!fin.is_open()) {
				throw std::runtime_error("open file error!");
			}
			time_t mtime;
			std::string filepath;
			while (fin >> filepath >> mtime) {
				m_map[filepath] = mtime;
			}
		}
		void _storageData() {
			std::ofstream fout;
			fout.open(m_filename);
			if (!fout.is_open()) {
				throw std::runtime_error("open file error!");
			}
			for (auto &it : m_map) {
				fout << it.first << ' ' << it.second << std::endl;
			}
		}
	private:
		std::string m_filename;
		std::unordered_map<std::string, time_t> m_map;
	};
	// http客户端
	class HttpClientModule
	{
	public:
		HttpClientModule(std::vector<std::string> listenDirs, const std::string &host, int port = 80)
			: m_cli(host, port) {
			if (listenDirs.empty()) {
				m_listenDirs.push_back("C:/Program Files/CloudBackup");
			}
			for (auto &path : listenDirs) {
				MyUtil::DealPath(path);
				if (!boost::filesystem::exists(path)) {
					boost::filesystem::create_directories(path);
				}
				m_listenDirs.push_back(path);
			}
		}
		HttpClientModule(const std::string &listenDir, const std::string &host, int port = 80)
			: HttpClientModule(std::vector<std::string>(1, listenDir), host, port) {}
		void start() {
			std::string path, body;
			while (true) {
				for (const auto &dirpath : m_listenDirs) {
					std::vector<std::string> fileList = m_lfm.getUpdateFileList(dirpath);
					for (auto &file : fileList) {
						MyUtil::readFile(file, body);
						path = "/upload" + file.substr(dirpath.size());
						auto res = m_cli.Put(path.c_str(), body, "application/octet-stream");
						if (res && res->status == 200 && m_lfm.insertData(file)) {
							std::cout << "upload file " << file << " success!" << std::endl;
						} else {
							std::cout << "upload file " << file << " failed!" << std::endl;
						}
					}
				}
				MyUtil::MySleep(IntervalTime);
			}
		}

	private:
		LocalFileManager m_lfm;
		httplib::Client m_cli;
		std::vector<std::string> m_listenDirs;
		static const time_t IntervalTime = 3;
	};
	const time_t HttpClientModule::IntervalTime;
}

#endif /* _CLOUDBACKUPCLIENT_HPP_ */ 
