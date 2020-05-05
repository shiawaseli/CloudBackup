#ifndef _CLOUDBACKUP_HPP_
#define _CLOUDBACKUP_HPP_ 

#define CPPHTTPLIB_OPENSSL_SUPPORT

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
#include "mysqlHelper.hpp"

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
        if (m_map.find(filepath) != m_map.end() 
            && find(tmpList.begin(), tmpList.end(), filepath) == tmpList.end()) {
          tmpList.push_back(filepath);
        } else if (filepath.rfind(".gz") == filepath.size() - 3) {
          filepath.erase(filepath.end() - 3, filepath.end());
          if (m_map.find(filepath) != m_map.end() 
              && find(tmpList.begin(), tmpList.end(), filepath) == tmpList.end()) {
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

  using namespace mysqlhelper;
  class MysqlModule
  {
    public:
      void init(const std::string &sHost = "", const std::string &sUser = "", 
          const std::string &sPasswd = "", const std::string &sDataBase = "", 
          const std::string &sCharSet = "", int sPort = 0, int sFlag = 0) {
        m_mysql.init(sHost, sUser, sPasswd, sDataBase, sCharSet, sPort, sFlag);
      }
      void connect() {
        try {
          m_mysql.connect();
          std::cout << "数据库连接成功..." << std::endl;
        }
        catch (mysqlhelper::MysqlHelper_Exception &excep) {
          std::cout << excep.errorInfo << std::endl;
          return; // 未处理数据库不存在的情况
        }
      }
      void disconnect() {
        m_mysql.disconnect();
      }
      bool createUser(const std::string &nickname, MysqlHelper::RECORD_DATA &record)
      {
        try {
          MysqlHelper::RECORD_DATA user;
          user.insert(std::make_pair("nickname", std::make_pair(MysqlHelper::DB_STR, nickname)));
          int res = m_mysql.insertRecord("user_info", user);
          std::cout << " |--->用户创建成功..." << std::endl;

          std::string uid = std::to_string(m_mysql.lastInsertID());
          record.insert(std::make_pair("uid", std::make_pair(MysqlHelper::DB_INT, uid)));
          res = m_mysql.insertRecord("user_auths", record);
          std::cout << "   |--->用户授权成功..." << std::endl;
        }
        catch (mysqlhelper::MysqlHelper_Exception &excep) {
          std::cout << excep.errorInfo << std::endl;
          return false;
        }
        return true;
      }
      bool openidExist(const std::string &openid) {
        try {
          std::stringstream sql;
          sql << "select openid from user_auths"
            << " where openid='" << openid << "'";
          return m_mysql.existRecord(sql.str());
        }
        catch (MysqlHelper_Exception &excep) {
          std::cout << excep.errorInfo << std::endl;
          return false;
        }
      }
      bool userCheck(const std::string &openid, const std::string &login_token) {
        try {
          std::stringstream sql;
          sql << "select openid, login_token from user_auths"
            << " where openid='" << openid << "' and login_token='" << login_token << "'";
          return m_mysql.existRecord(sql.str());
        }
        catch (MysqlHelper_Exception &excep) {
          std::cout << excep.errorInfo << std::endl;
          return false;
        }
      }
      std::string getNickname(const std::string &uid) {
        try {
          std::string sql = "select nickname from user_info where user_id='" + uid + "'";
          MysqlHelper::MysqlData data = m_mysql.queryRecord(sql);
          if (data.size() != 1) {
            return "???getNickname ERROR???";
          }
          return data[0]["nickname"];
        }
        catch (MysqlHelper_Exception &excep) {
          std::cout << excep.errorInfo << std::endl;
          return "???getNickname ERROR???";
        }
      }
      std::string getUserId(const std::string &openid) {
        try {
          std::string sql = "select uid from user_auths where openid='" + openid + "'";
          MysqlHelper::MysqlData data = m_mysql.queryRecord(sql);
          if (data.size() != 1) {
            return "???getUserId ERROR???";
          }
          return data[0]["uid"];
        }
        catch (MysqlHelper_Exception &excep) {
          std::cout << excep.errorInfo << std::endl;
          return "???getUserId ERROR???";
        }
      }
      std::string getCookie(const std::string &uid) {
        try {
          std::string sql = "select login_token from user_auths where uid='" + uid + "'";
          MysqlHelper::MysqlData data = m_mysql.queryRecord(sql);
          if (data.size() != 1) {
            return "???getCookie ERROR???";
          }
          std::string password = data[0]["login_token"];
          return "sid=" + md5(password) + "-" + uid;
        }
        catch (MysqlHelper_Exception &excep) {
          std::cout << excep.errorInfo << std::endl;
          return "???getCookie ERROR???";
        }
      }
      bool cookieCheck(const std::string &cookie, const std::string &uid) {
        return cookie.find(getCookie(uid)) != std::string::npos;
      }
      std::string getUTC() {
        try {
          std::string sql = "select utc_timestamp()";
          return m_mysql.queryRecord(sql)[0]["utc_timestamp()"];
        }
        catch (MysqlHelper_Exception &excep) {
          std::cout << excep.errorInfo << std::endl;
          return "???getUTC ERROR???";
        }
      }
      std::string md5(const std::string &code) {
        try {
          std::string sql = "select md5('" + code + "')";
          return m_mysql.queryRecord(sql)[0][sql.substr(7)];
        }
        catch (MysqlHelper_Exception &excep) {
          std::cout << excep.errorInfo << std::endl;
          return "???MD5 ERROR???";
        }
      }
      std::string password(const std::string &pwd) {
        try {
          std::string sql = "select password('" + pwd + "')";
          return m_mysql.queryRecord(sql)[0][sql.substr(7)];
        }
        catch (MysqlHelper_Exception &excep) {
          std::cout << excep.errorInfo << std::endl;
          return "???password ERROR???";
        }
      }
    private:
      MysqlHelper m_mysql;
  };

  // http服务器
  class HttpServerModule
  {
    public:
      HttpServerModule() {
        std::map<std::string, std::string> config = MyUtil::getConfig("./CBackup.cnf", "CloudServer");
        m_db.init(config["sHost"], config["sUser"], config["sPasswd"], config["sDataBase"], config["sCharSet"], atoi(config["sPort"].c_str()), atoi(config["sFlag"].c_str()));
        m_db.connect();
      }
      ~HttpServerModule() {
        m_db.disconnect();
      }
      void start(const std::string &host = "0.0.0.0", int port = 9000) {
        int ret = m_srv.set_mount_point("/", "./www");
        if (!ret) {
          boost::filesystem::create_directories("./www");
          m_srv.set_mount_point("/", "./www");
        }
        m_srv.Post("/register", _register);
        m_srv.Post("/login", _login);
        m_srv.Get("/logout", _logout);
        m_srv.Get("/profile", _profile);
        m_srv.Get("/clist/(.*)", _cfileList);
        m_srv.Get("/list/([0-9]*)/(.*)", _fileList);
        m_srv.Get("/download/(.*)", _fileDownload);
        m_srv.Put("/upload/(.*)", _fileUpload);
        m_srv.listen(host.c_str(), port);
      }
    private:
      static void _register(const httplib::Request &req, httplib::Response &res) {
        printf("register:> name[%s] password[%s]\n", 
            req.get_param_value("username").c_str(), req.get_param_value("password").c_str());

        std::string openid = req.get_param_value("username");
        std::string password = m_db.password(req.get_param_value("password"));
        MysqlHelper::RECORD_DATA record;
        record.insert(std::make_pair("typeid", std::make_pair(MysqlHelper::DB_INT, "1")));
        record.insert(std::make_pair("openid", std::make_pair(MysqlHelper::DB_STR, openid)));
        record.insert(std::make_pair("login_token", std::make_pair(MysqlHelper::DB_STR, password)));

        res.set_header("Content-Type", "text/html;charset=utf8");
        if (m_db.openidExist(openid)) {
          res.status = 400;
          res.body = "该用户已存在";
          res.set_redirect("/register.html");
        } else if (m_db.createUser(openid, record)) {
          res.status = 200;
          res.body = "用户创建成功";
          res.set_redirect("/index.html");
        } else {
          res.status = 500;
          res.body = "用户创建失败";
          res.set_redirect("/register.html");
        }
      }
      static void _login(const httplib::Request &req, httplib::Response &res) {
        printf("login:> name[%s] password[%s]\n", 
            req.get_param_value("username").c_str(), req.get_param_value("password").c_str());

        std::string openid = req.get_param_value("username");
        std::string password = m_db.password(req.get_param_value("password"));
        if (m_db.userCheck(openid, password)) {
          res.status = 301;
          res.body = "登录成功";
          res.set_redirect(("/list/" + m_db.getUserId(openid) +"/").c_str());
          std::string uid = m_db.getUserId(openid);
          std::string cookie = m_db.getCookie(uid); 
          res.set_header("Set-Cookie", cookie);
          res.set_header("Content-Type", "text/plain;charset=utf8");
        } else {
          res.status = 301;
          res.body = "登录失败";
          res.set_redirect("/index.html");
          res.set_header("Content-Type", "text/html;charset=utf8");
        }
      }
      static void _logout(const httplib::Request &req, httplib::Response &res) {
        printf("logout:> [%s]\n", req.matches[0].str().c_str());

        res.status = 200;
        res.body = "退出成功";
        res.set_redirect("/");
        // 设定一个过期的expires
        res.set_header("Set-Cookie", "sid=0;expires=Sat, 02 May 2009 23:38:25 GMT");
        res.set_header("Content-Type", "text/plain;charset=utf8");
      }
      static void _fileUpload(const httplib::Request &req, httplib::Response &res) {
        printf("upload:> [%s]\n", req.matches[0].str().c_str());
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
      static void _profile(const httplib::Request &req, httplib::Response &res) {
        printf("profile:> [%s]\n", req.matches[0].str().c_str());

        res.status = 301;
        res.body = "个人页面未完成，先跳转到暂定的暂时的界面...";
        res.set_redirect("/profile.html");
        res.set_header("Content-Type", "text/plain;charset=utf8");
      }
      static void _fileDownload(const httplib::Request &req, httplib::Response &res) {
        printf("download:> [%s]\n", req.matches[0].str().c_str());
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
        printf("list:> [%s]\n", req.matches[0].str().c_str());

        std::string uid = req.matches[1].str();
        std::string cookie = req.get_header_value("Cookie");
        std::cout << "Cookie:> " << cookie << std::endl;
        if (!m_db.cookieCheck(cookie, uid)) {
          res.status = 301;
          res.set_redirect("/index.html");
          res.set_header("Content-Type", "text/html;charset=utf8");
          return;
        }

        std::string dirpath, pathtmp, filename, parentDirpath;
        std::stringstream buf;
        std::vector<std::string> fileList;
        if (req.matches[0].str() == "/list/" + uid) {
          dirpath = "/data/CloudBackup/" + uid;
          parentDirpath = "/data/CloudBackup/" + uid;
        } else {
          dirpath = "/data/CloudBackup/" + uid + "/" + req.matches[2].str();
          parentDirpath = dirpath.substr(0, dirpath.rfind("/", dirpath.size() - 2) + 1);
        }
        fdManager.getDirList(dirpath, fileList);
        std::string indexPath = "(" + m_db.getNickname(uid) + "):/"
          + dirpath.substr(17 + uid.size() + 2);
        buf << "<html>\r\n"
          << "\t<head><title>Index of " << indexPath << "</title></head>\r\n\r\n"
          << "\t<body>\r\n"
          << "\t\t<a href=\"/profile\">个人中心</a>\t<a href=\"/logout\">退出</a>" 
          << "\t<a href=\"/list/" << uid << "/\">私有文件</a>\r\n"
          << "\t<a href=\"/clist/\">公共文件</a>\r\n"
          << "\t\t<h1>Index of " << indexPath << "</h1><hr><pre>\r\n"
          << "<a href='/list/" << parentDirpath.substr(17) << "'>../</a>\r\n";
        for (std::vector<std::string>::size_type i = 0; i < fileList.size(); ++i) {
          pathtmp = fileList[i].substr(18);
          filename = pathtmp.substr(pathtmp.rfind("/") + 1);
          if (boost::filesystem::is_directory(fileList[i])) {
            buf << "<a href='/list/" << pathtmp << "/'>" << filename << "/</a>"
              << std::string(99, ' ') << "\t\t\t  -" << std::string(17, ' ')
              << "\t  -<br>";
          } else {
            std::string filesize = fdManager.getFileSize(fileList[i]);
            buf << "<a href='/download/" << uid << "/" << pathtmp << "'>" << filename << "</a>"
              << std::string((filename.size() < 100) ? 100 - filename.size() : 0, ' ') 
              << "\t\t\t大小: " << filesize 
              << std::string((filesize.size() < 20) ? 20 - filesize.size() : 0, ' ') 
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
      static void _cfileList(const httplib::Request &req, httplib::Response &res) {
        printf("clist:> [%s]\n", req.matches[0].str().c_str());

        auto cookie = req.get_header_value("Cookie");
        std::cout << "Cookie:> " << cookie << std::endl;

        std::string dirpath, pathtmp, filename, parentDirpath;
        std::stringstream buf;
        std::vector<std::string> fileList;

        if (req.matches[0].str() == "/clist/") {
          dirpath = "/data/CloudBackup/";
          parentDirpath = "/data/CloudBackup/";
        } else {
          dirpath = "/data/CloudBackup/" + req.matches[1].str();
          parentDirpath = dirpath.substr(0, dirpath.rfind("/", dirpath.size() - 2) + 1);
        }
        fdManager.getDirList(dirpath, fileList);
        buf << "<html>\r\n"
          << "\t<head><title>Index of "<< dirpath.substr(17) << "</title></head>\r\n\r\n"
          << "\t<body>\r\n";
        if (cookie.find("sid=") == std::string::npos) {
          buf << "\t\t<a href='/index.html'>登录</a>\t<a href='/register.html'>注册</a>" 
          << "\t<a href='http://connect.qq.com/toc/auth_manager?from=auth'>QQ授权管理</a>";
        } else {
          std::string uid = cookie.substr(cookie.find("-", cookie.find("sid=") + 4) + 1);
          buf << "\t\t<a href=\"/profile\">个人中心</a>\t<a href=\"/logout\">退出</a>" 
          << "\t<a href=\"/list/" << uid.substr(0, uid.find_first_not_of("0123456789")) << "/\">私有文件</a>\r\n";
        }
        buf << "\t<a href='/clist/'>公共文件</a>\r\n"
          << "\t\t<h1>Index of " << dirpath.substr(17)  << "</h1><hr><pre>\r\n"
          << "<a href='/clist" << parentDirpath.substr(17) << "'>../</a>\r\n";
        for (std::vector<std::string>::size_type i = 0; i < fileList.size(); ++i) {
          pathtmp = fileList[i].substr(18);
          filename = pathtmp.substr(pathtmp.rfind("/") + 1);
          if (boost::filesystem::is_directory(fileList[i])) {
            buf << "<a href='/clist/" << pathtmp << "/'>" << filename << "/</a>"
              << std::string(99, ' ') << "\t\t\t  -" << std::string(17, ' ')
              << "\t  -<br>";
          } else {
            std::string filesize = fdManager.getFileSize(fileList[i]);
            buf << "<a href='/download/" << pathtmp << "'>" << filename << "</a>"
              << std::string((filename.size() < 100) ? 100 - filename.size() : 0, ' ') 
              << "\t\t\t大小: " << filesize 
              << std::string((filesize.size() < 20) ? 20 - filesize.size() : 0, ' ') 
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
      static MysqlModule m_db;
  };
  MysqlModule HttpServerModule::m_db;
  }

#endif /* _CLOUDBACKUP_HPP_ */ 
