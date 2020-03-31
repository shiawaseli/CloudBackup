#ifndef _CLOUDBACKUP_HPP_
#define _CLOUDBACKUP_HPP_ 

#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <zlib.h>
#include <iostream>
#include <sys/stat.h>
#include <pthread.h>
#include <unordered_map>
#include <boost/filesystem.hpp>
#include "httplib.h"
#include "MyUtil.hpp"

namespace CloudBackup {
  // 压缩解压文件的工具类
  class CompressUtil
  {
    public:
      // 将src文件中的内容压缩存储到dst文件中
      static bool compress(const std::string &src, const std::string &dst) {
        gzFile file = gzopen(dst.c_str(), "wb");
        if (file == NULL) {
          std::cout << "open file " + dst + " failed!" << std::endl;
          return false;
        }
        std::string buf;
        if (!MyUtil::readFile(src, buf)) {
          std::cout << "data error!" << std::endl;
          gzclose(file);
          return false;
        }

        int ret = gzwrite(file, buf.c_str(), buf.length());
        if (ret == 0) {
          std::cout << "compress " + dst + " failed!" << std::endl;
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

        char buf[m_s_BufSize] = { 0 };
        int ret = m_s_BufSize;
        while (ret == m_s_BufSize) {
          ret = gzread(file, buf, m_s_BufSize);
          if (ret < 0) {
            std::cout << "decompress " + src + " failed!" << std::endl;
            gzclose(file);
            return false;
          }
          fout.write(buf, ret);
        }
        gzclose(file);
        return true;
      }
  private:
	  static const int m_s_BufSize = 8192;
  };
  const int CompressUtil::m_s_BufSize;
  // 文件信息管理类
  class FileDataManager 
  {
      enum status {
        NORMAL, COMPRESSED
      };
      struct FileData
      {
          status m_fileStatus;
          time_t m_fileATime;
          size_t m_fileSize;
          FileData(status sta = NORMAL, time_t atime = 0, size_t size = 0)
            : m_fileStatus(sta), m_fileATime(atime), m_fileSize(size) {}
      };
    public:
      FileDataManager(const std::string &name = "/root/workspace/CloudBackup/src/srv_log.dat") : m_filename(name) {
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
        return m_map[filepath].m_fileStatus == COMPRESSED;
      }
      bool isExistFile(const std::string &filepath) {
        return m_map.find(filepath) != m_map.end();
      }
      bool insertData(const std::string &filepath) {
        FileData data;
        pthread_rwlock_wrlock(&m_rwlock);
        if (!getFileData(filepath, data)) {
          pthread_rwlock_unlock(&m_rwlock);
          std::cout << filepath << " insert error" << std::endl;
          return false;
        }
        m_map[filepath] = data;
        pthread_rwlock_unlock(&m_rwlock);
        _storageData();
        return true;
      }
      bool deleteData(const std::string &filepath) {
        if (!isExistFile(filepath)) {
          return false;
        }
        pthread_rwlock_wrlock(&m_rwlock);
        m_map.erase(m_map.find(filepath));
        pthread_rwlock_unlock(&m_rwlock);
        _storageData();
        return true;
      }
      bool changeData(const std::string &filepath) {
        if (!isExistFile(filepath)) {
          return false;
        }
        if (isCompressedFile(filepath)) {
          pthread_rwlock_wrlock(&m_rwlock);
          m_map[filepath].m_fileStatus = NORMAL;
          pthread_rwlock_unlock(&m_rwlock);
        } else {
          pthread_rwlock_wrlock(&m_rwlock);
          m_map[filepath].m_fileStatus = COMPRESSED;
          pthread_rwlock_unlock(&m_rwlock);
        }
        _storageData();
        return true;
      }
      bool getAllList(std::vector<std::string> &fileList) {
        fileList.clear();
        pthread_rwlock_rdlock(&m_rwlock);
        for (auto &it : m_map) {
          fileList.push_back(it.first);
        }
        pthread_rwlock_unlock(&m_rwlock);
        return true;
      }
      bool getDirList(const std::string &dirpath, std::vector<std::string> &fileList) {
        fileList.clear();
        if (!boost::filesystem::is_directory(dirpath)) {
          return false;
        }
        std::string filepath;
        boost::filesystem::directory_iterator iter_begin(dirpath), iter_end;
        pthread_rwlock_rdlock(&m_rwlock);
        for (; iter_begin != iter_end; ++iter_begin) {
          filepath = iter_begin->path().string();
          if (filepath.rfind(".gz") == filepath.size() - 3) {
            filepath.erase(filepath.end() - 3, filepath.end());
          }
          if (boost::filesystem::is_directory(filepath)) {
            fileList.push_back(filepath);
          } else if (m_map.find(filepath) != m_map.end()) {
            fileList.push_back(filepath);
          }
        }
        pthread_rwlock_unlock(&m_rwlock);
        return true;
      }
      bool getFileData(const std::string &filepath, FileData &res) {
        struct stat buf;
        int ret = stat(filepath.c_str(), &buf);
        if (ret < 0) {
          std::cout << "get " << filepath << " stat error" << std::endl;
          return false;
        }
        res.m_fileStatus = NORMAL;
        res.m_fileATime = buf.st_atime;
        res.m_fileSize = buf.st_size;
        return true;
      }
      std::string getAtime(std::string filepath) {
        if (!isExistFile(filepath)) {
          return "-error data-";
        }
        return ctime(&m_map[filepath].m_fileATime);
      }
      std::string getFileSize(std::string filepath) {
        if (!isExistFile(filepath)) {
          return "-error data-";
        }
        return std::to_string(m_map[filepath].m_fileSize);
      }
    private:
      void _loadData() {
        if (!boost::filesystem::exists(m_filename)) {
          boost::filesystem::create_directories(boost::filesystem::path(m_filename));
        }
        std::ifstream fin;
		fin.open(m_filename);
        if (!fin.is_open()) {
          std::cout << "open file " << m_filename << " error!" << std::endl;
          abort();
        }
        int flag;
        std::string filepath;
        FileData tmpdata;
        pthread_rwlock_wrlock(&m_rwlock);
        while (fin >> filepath >> flag >> tmpdata.m_fileATime >> tmpdata.m_fileSize) {
          tmpdata.m_fileStatus = (flag == 0) ? NORMAL : COMPRESSED;
          m_map[filepath] = tmpdata;
        }
        pthread_rwlock_unlock(&m_rwlock);
      }
      void _storageData() {
        std::ofstream fout;
        fout.open(m_filename);
        if (!fout.is_open()) {
          std::cout << "open file " << m_filename << " error!" << std::endl;
          abort();
        }
        pthread_rwlock_rdlock(&m_rwlock);
        for (auto &it : m_map) {
          fout << it.first << ' ' << ((it.second.m_fileStatus == NORMAL) ? 0 : 1) 
            << ' ' << it.second.m_fileATime << ' ' << it.second.m_fileSize << std::endl;
        }
        pthread_rwlock_unlock(&m_rwlock);
      }
    private:
      std::string m_filename;
      std::unordered_map<std::string, FileData> m_map;
      pthread_rwlock_t m_rwlock;
  };
  
  FileDataManager fdManager;
  class FileManageModule
  {
    public:
      FileManageModule(FileDataManager &fdm = fdManager)
        : m_fdm(fdm) {}
      void start()
      {
        std::vector<std::string> fileList;
        while (true) {
          m_fdm.getAllList(fileList);
          for (std::vector<int>::size_type i = 0; i < fileList.size(); ++i) {
            if (!m_fdm.isCompressedFile(fileList[i]) && MyUtil::isNonHotFile(fileList[i], m_s_IntervalTime)) {
              CompressUtil::compress(fileList[i], fileList[i] + ".gz");
              unlink(fileList[i].c_str());
              m_fdm.changeData(fileList[i]);
            }
          }
		  MyUtil::MySleep(m_s_IntervalTime);
        }
      }
    private:
      FileDataManager &m_fdm;
	  static const time_t m_s_IntervalTime = 30;
  };
  const time_t FileManageModule::m_s_IntervalTime;
  // http服务器
  class HttpServerModule
  {
    public:
      void start() {
        m_srv.Get("/", _fileList);
        m_srv.Get("/list", _fileList);
        m_srv.Get("/list/(.*)", _fileList);
        m_srv.Get("/download/(.*)", _fileDownload);
		m_srv.Put("/upload/(.*)", _fileUpload);
        m_srv.listen("0.0.0.0", 9000);
      }
    private:
      static void _fileUpload(const httplib::Request &req, httplib::Response &res) {
        std::cout << "upload:> " << req.matches[0].str() << std::endl;
        std::string filepath = "/data/CloudBackup/" + req.matches[1].str();
        std::string dirpath = filepath.substr(0, filepath.find_last_of("/"));
        if (!boost::filesystem::exists(dirpath)) {
          boost::filesystem::create_directories(dirpath);
        }
        if (!MyUtil::writeFile(filepath, req.body) || !fdManager.insertData(filepath)) {
          res.status = 500;
          return;
        }
        res.status = 200;
      }
      static void _fileDownload(const httplib::Request &req, httplib::Response &res) {
        std::cout << "download:> " << req.matches[0].str() << std::endl;
        std::string filepath = "/data/CloudBackup/" + req.matches[1].str();
        if (!fdManager.isExistFile(filepath)) {
          res.status = 404;
          return;
        }
        if (fdManager.isCompressedFile(filepath)) {
          fdManager.changeData(filepath);
          CompressUtil::decompress(filepath + ".gz", filepath);
          unlink((filepath + ".gz").c_str());
        }
        MyUtil::readFile(filepath, res.body);
        res.status = 200;
        res.set_header("Content-Type", "application/octet-stream");
        res.set_header("Content-Length", std::to_string(res.body.size()));
      }
      static void _fileList(const httplib::Request &req, httplib::Response &res) {
        std::cout << "list:> " << req.matches[0].str() << std::endl;
        std::string dirpath, pathtmp, filename, parentDirpath;
        std::stringstream buf;
        std::vector<std::string> fileList;
        if (req.matches[0].str() == "/") {
          dirpath = "/data/CloudBackup/";
          parentDirpath = "/data/CloudBackup/";
        } else {
          dirpath = "/data/CloudBackup/" + req.matches[1].str();
          parentDirpath = dirpath.substr(0, dirpath.rfind("/"));
        }
        fdManager.getDirList(dirpath, fileList);
        buf << "<html>\r\n"
          << "<head><title>Index of "<< dirpath.substr(17) << "</title></head>\r\n"
          << "<body>\r\n"
          << "<h1>Index of " << dirpath.substr(17)  << "</h1><hr><pre>\r\n"
          << "<a href='/list" << parentDirpath.substr(17) << "'>../</a>\r\n";
        for (std::vector<std::string>::size_type i = 0; i < fileList.size(); ++i) {
          pathtmp = fileList[i].substr(18);
          filename = pathtmp.substr(pathtmp.rfind("/") + 1);
          if (boost::filesystem::is_directory(fileList[i])) {
            buf << "<a href='/list/" << pathtmp << "'>" << filename << "/</a><br>";
          } else {
            buf << "<a href='/download/" << pathtmp << "'>" << filename << "</a>"
              << std::string((filename.size() < 70) ? 70 - filename.size() : 0, ' ') 
              << "\t\t\t大小: " << fdManager.getFileSize(fileList[i]) 
              << "\t最新访问时间: " << fdManager.getAtime(fileList[i]);
          }
        }
        buf << "</pre><hr></body>\r\n"
          << "</html>";
        res.body = buf.str();
        res.status = 200;
        res.set_header("Content-Type", "text/html;charset=utf8");
        res.set_header("Content-Length", std::to_string(res.body.size()));
      }
    private:
      httplib::Server m_srv;
  };
}

#endif /* _CLOUDBACKUP_HPP_ */ 
