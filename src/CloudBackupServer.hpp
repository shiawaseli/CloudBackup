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
#include <algorithm>
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
      FileDataManager() {
        m_filename = MyUtil::getConfig("./CBackup.cnf", "CloudServer")["srcLog"];
        // 初始化读写锁
        _loadData();
      }
      ~FileDataManager() {
        // 销毁读写锁
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
        m_mutex.lock();
        if (!getFileData(filepath, data)) {
          m_mutex.unlock();
          std::cout << filepath << " insert error" << std::endl;
          return false;
        }
        m_map[filepath] = data;
        m_mutex.unlock();
        _storageData();
        return true;
      }
      bool deleteData(const std::string &filepath) {
        if (!isExistFile(filepath)) {
          return false;
        }
        m_mutex.lock();
        m_map.erase(m_map.find(filepath));
        m_mutex.unlock();
        _storageData();
        return true;
      }
      bool changeData(const std::string &filepath) {
        if (!isExistFile(filepath)) {
          return false;
        }
        if (isCompressedFile(filepath)) {
          m_mutex.lock();
          m_map[filepath].m_fileStatus = NORMAL;
          m_mutex.unlock();
        } else {
          m_mutex.lock();
          m_map[filepath].m_fileStatus = COMPRESSED;
          m_mutex.unlock();
        }
        _storageData();
        return true;
      }
      bool getAllList(std::vector<std::string> &fileList) {
        fileList.clear();
        m_mutex.lock();
        for (auto &it : m_map) {
          fileList.push_back(it.first);
        }
        m_mutex.unlock();
        return true;
      }
      bool getDirList(const std::string &dirpath, std::vector<std::string> &fileList) {
        fileList.clear();
        if (!boost::filesystem::is_directory(dirpath)) {
          return false;
        }
        std::string filepath;
        std::vector<std::string> tmpList;
        boost::filesystem::directory_iterator iter_dir(dirpath), iter_file(dirpath), iter_end;
        m_mutex.lock();
        for (; iter_dir != iter_end; ++iter_dir) {
          filepath = iter_dir->path().string();
          if (boost::filesystem::is_directory(filepath)) {
            fileList.push_back(filepath);
          }
        }
        sort(fileList.begin(), fileList.end());
        for (; iter_file != iter_end; ++iter_file) {
          filepath = iter_file->path().string();
          if (m_map.find(filepath) != m_map.end()) {
            tmpList.push_back(filepath);
          } else if (filepath.rfind(".gz") == filepath.size() - 3) {
            filepath.erase(filepath.end() - 3, filepath.end());
            if (m_map.find(filepath) != m_map.end()) {
              tmpList.push_back(filepath);
            }
          }
        }
        sort(tmpList.begin(), tmpList.end());
        fileList.insert(fileList.end(), tmpList.begin(), tmpList.end());
        m_mutex.unlock();
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
        m_mutex.lock();
        while (fin >> filepath >> flag >> tmpdata.m_fileATime >> tmpdata.m_fileSize) {
          tmpdata.m_fileStatus = (flag == 0) ? NORMAL : COMPRESSED;
          m_map[filepath] = tmpdata;
        }
        m_mutex.unlock();
      }
      void _storageData() {
        std::ofstream fout;
        fout.open(m_filename);
        if (!fout.is_open()) {
          std::cout << "open file " << m_filename << " error!" << std::endl;
          abort();
        }
        m_mutex.lock();
        for (auto &it : m_map) {
          fout << it.first << ' ' << ((it.second.m_fileStatus == NORMAL) ? 0 : 1) 
            << ' ' << it.second.m_fileATime << ' ' << it.second.m_fileSize << std::endl;
        }
        m_mutex.unlock();
      }
    private:
      std::string m_filename;
      std::unordered_map<std::string, FileData> m_map;
      std::mutex m_mutex;
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
      void start(const std::string &host = "0.0.0.0", int port = 9000) {
        int ret = m_srv.set_mount_point("/", "./www");
        if (!ret) {
          boost::filesystem::create_directories("./www");
          m_srv.set_mount_point("/", "./www");
        }
        m_srv.Post("/register", _register);
        m_srv.Post("/login", _login);
        m_srv.Get("/list", _fileList);
        m_srv.Get("/list/(.*)", _fileList);
        m_srv.Get("/download/(.*)", _fileDownload);
		    m_srv.Put("/upload/(.*)", _fileUpload);
        m_srv.listen(host.c_str(), port);
      }
    private:
      static void _register(const httplib::Request &req, httplib::Response &res) {
        std::cout << "register:> " << "name[" << req.get_param_value("username") 
          << "] password1[" << req.get_param_value("password") 
          << "] password2[" << req.get_param_value("password2") << std::endl;
        res.status = 200;
        res.body = "----";
      }
      static void _login(const httplib::Request &req, httplib::Response &res) {
        std::cout << "login:> " << "name[" << req.get_param_value("username") 
          << "] password[" << req.get_param_value("password") << std::endl;
        res.status = 200;
        res.body = "----";
      }
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
          << "\t<head><title>Index of "<< dirpath.substr(17) << "</title></head>\r\n\r\n"
          << "\t<body>\r\n"
          << "\t\t<a href=\"/index.html\">登录</a>\t<a href=\"/register.html\">注册</a>" 
          << "\t<a href=\"http://connect.qq.com/toc/auth_manager?from=auth\">QQ授权管理</a>"
          << "\t<a href=\"/list\">公共文件</a>\r\n"
          << "\t\t<h1>Index of " << dirpath.substr(17)  << "</h1><hr><pre>\r\n"
          << "<a href='/list" << parentDirpath.substr(17) << "'>../</a>\r\n";
        for (std::vector<std::string>::size_type i = 0; i < fileList.size(); ++i) {
          pathtmp = fileList[i].substr(18);
          filename = pathtmp.substr(pathtmp.rfind("/") + 1);
          if (boost::filesystem::is_directory(fileList[i])) {
            buf << "<a href='/list/" << pathtmp << "'>" << filename << "/</a><br>";
          } else {
            std::string filesize = fdManager.getFileSize(fileList[i]);
            buf << "<a href='/download/" << pathtmp << "'>" << filename << "</a>"
              << std::string((filename.size() < 70) ? 70 - filename.size() : 0, ' ') 
              << "\t\t\t大小: " << filesize 
              << std::string((filesize.size() < 10) ? 10 - filesize.size() : 0, ' ') 
              << "\t最近访问: " << fdManager.getAtime(fileList[i]);
          }
        }
        buf << "</pre>\r\n\t\t</hr>\r\n\t</body>\r\n"
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
