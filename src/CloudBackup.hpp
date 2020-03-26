#ifndef _CLOUDBACKUP_HPP_
#define _CLOUDBACKUP_HPP_ 

#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cstdio>
#include <zlib.h>
#include <memory>
#include <iostream>
#include <pthread.h>
#include <unordered_map>
//#include "httplib.h"

namespace CloudBackup {
  // 读写文件的工具类
  class MyFileUtil
  {
    public:
      // 读取指定文件中的所有内容并保存到dst中
      static bool readFile(const std::string &name, std::string &dst) {
        std::ifstream fin(name.c_str(), std::ios::binary);
        if (!fin.is_open()) {
          std::cout << "open file " + name + " failed!" << std::endl;;
          return false;
        }

        fin.seekg(0, fin.end);
        int length = fin.tellg(); // 获得文件内容的长度
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
      // 将src中的所有数据写入到指定文件中
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
  };
  // 压缩解压文件的工具类
  class CompressUtil
  {
    static const int BufSize = 8192;
    public:
      // 将src文件中的内容压缩存储到dst文件中
      static bool compress(const std::string &src, const std::string &dst) {
        gzFile file = gzopen(dst.c_str(), "wb");
        if (file == NULL) {
          std::cout << "open file " + dst + " failed!" << std::endl;
          return false;
        }
        std::string buf;
        if (!MyFileUtil::readFile(src, buf)) {
          std::cout << "data error!" << std::endl;
          gzclose(file);
          return false;
        }

        int ret = gzwrite(file, buf.c_str(), buf.length());
        if (ret == 0) {
          std::cout << "compress " + dst + " failed!" << std::cout;
          gzclose(file);
          return false;
        }
        gzclose(file);
        return true;
      }
      // 将src文件中的内容解压存储到dst文件中
      static bool decompress(const std::string &src, const std::string &dst) {
        gzFile file = gzopen(src.c_str(), "rb");
        if (file == NULL) {
          std::cout << "open file " + src + " failed!" << std::endl;
          return false;
        }
        std::ofstream fout(dst.c_str(), std::ios::binary);
        if (!fout.is_open()) {
          std::cout << "open file " + dst + " failed!" << std::endl;
          gzclose(file);
          return false;
        }

        char buf[8192] = { 0 };
        int ret = 8192;
        while (ret == 8192) {
          ret = gzread(file, buf, 8192);
          if (ret < 0) {
            std::cout << "decompress " + src + " failed!" << std::cout;
            gzclose(file);
            return false;
          }
          fout.write(buf, ret);
        }
        gzclose(file);
        return true;
      }
  };
  const int CompressUtil::BufSize;
  // 文件信息管理类
  class FileDataManager 
  {
    public:
      FileDataManager(const std::string &name = "log.dat") : m_filename(name) {
        // 初始化读写锁
        pthread_rwlock_init(&m_rwlock, NULL);
        _loadData();
      }
      ~FileDataManager() {
        // 销毁读写锁
        pthread_rwlock_destroy(&m_rwlock);
        _storageData();
      }
      bool isCompressedFile(const std::string &filepath) {
        if (!isExistFile(filepath)) {
          return false;
        }
        return m_map[filepath] == COMPRESSED;
      }
      bool isExistFile(const std::string &filepath) {
        return m_map.find(filepath) != m_map.end();
      }
      bool insertData(const std::string &name, const std::string &Path = "") {
        std::string filepath = Path + name;
        if (isExistFile(filepath)) {
          return false;
        }
        pthread_rwlock_wrlock(&m_rwlock);
        m_map[filepath] = NORMAL;
        pthread_rwlock_unlock(&m_rwlock);
        return true;
      }
      bool deleteData(const std::string &name, const std::string &Path = "") {
        std::string filepath = Path + name;
        if (!isExistFile(filepath)) {
          return false;
        }
        pthread_rwlock_wrlock(&m_rwlock);
        m_map.erase(m_map.find(filepath));
        pthread_rwlock_unlock(&m_rwlock);
        return true;
      }
      bool changeData(const std::string &name, const std::string &Path = "") {
        std::string filepath = Path + name;
        if (!isExistFile(filepath)) {
          return false;
        }
        if (isCompressedFile(filepath)) {
          pthread_rwlock_wrlock(&m_rwlock);
          m_map[filepath] = NORMAL;
          pthread_rwlock_unlock(&m_rwlock);
        } else {
          pthread_rwlock_wrlock(&m_rwlock);
          m_map[filepath] = COMPRESSED;
          pthread_rwlock_unlock(&m_rwlock);
        }
        return true;
      }
      bool getList(std::vector<std::string> &fileList) {
        fileList.clear();
        pthread_rwlock_rdlock(&m_rwlock);
        for (auto &it : m_map) {
          fileList.push_back(it.first);
        }
        pthread_rwlock_unlock(&m_rwlock);
        return false;
      }
    private:
      void _loadData() {
        std::ifstream fin;
        try {
          fin.open(m_filename);
          if (!fin.is_open()) {
            throw std::runtime_error("load file data error!!!");
          }
        } catch(std::runtime_error &e) {
          fin.open("./.temp");
          if (!fin.is_open()) {
            throw e;
          }
        }
        int flag;
        std::string filepath;
        while (fin >> filepath >> flag) {
          m_map[filepath] = (flag == 0) ? NORMAL : COMPRESSED;
        }
      }
      void _storageData() {
        std::ofstream fout;
        try {
          fout.open(m_filename);
          if (!fout.is_open()) {
            throw std::runtime_error("save file data error!!!");
          }
        } catch(std::runtime_error &e) {
          fout.open("./.temp");
          if (!fout.is_open()) {
            throw e;
          }
        }
        std::string filepath;
        for (auto &it : m_map) {
          fout << it.first << ' ' << ((it.second == NORMAL) ? 0 : 1) << std::endl;
        }
      }
      enum status {
        NORMAL, COMPRESSED
      };
    private:
      std::string m_filename;
      std::unordered_map<std::string, status> m_map;
      pthread_rwlock_t m_rwlock;
  };
  class FileManageModule
  {
    public:
      FileManageModule(const std::string &filename) : m_fdm(filename) {}
      void start();
    private:
      FileDataManager m_fdm;
  };
  /*
  // http服务器
  class HttpServerModule
  {
    public:
      HttpServerModule(const std::string &path) : m_path(path) {}
      void start();
    private:
      static void _fileUpload(const httplib::Request &req, httplib::Response &res);
      static void _fileDownload(const httplib::Request &req, httplib::Response &res);
      static void _fileList(const httplib::Request &req, httplib::Response &res);
    private:
      std::string m_path; // 上传文件的根目录
      httplib::Server m_srv;
  }; */
}

#endif /* _CLOUDBACKUP_HPP_ */ 
