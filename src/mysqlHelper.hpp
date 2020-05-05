#ifndef __MYSQL_HELPER_H__
#define __MYSQL_HELPER_H__

#include <stdlib.h>
#include <map>
#include <vector>
#include <string>
#include <iostream>
#include <string.h>
#include <sstream>
#include <mysql/mysql.h>

using namespace std;

namespace mysqlhelper
{

  /*********************
   *@brief 数据库异常类
   **********************/
  struct MysqlHelper_Exception //: public TC_Exception
  {
    MysqlHelper_Exception(const string &sBuffer):errorInfo(sBuffer){}; //: TC_Exception(sBuffer){};
    ~MysqlHelper_Exception() throw(){}; 

    string errorInfo;
  };

  /***********************
   * @brief 数据库配置接口
   ***********************/
  struct DBConf
  {

    string _host;//主机地址
    string _user; //用户名 
    string _password;//密码
    string _database; //数据库
    string _charset; //字符集
    int _port;//端口
    int _flag; //客户端标识

    /*****************
     * @brief 构造函数
     *****************/
    DBConf():_port(0), _flag(0){}

    /**********************************
     * @brief 读取数据库配置. 
     * @param mpParam 存放数据库配置的map 
     * dbhost: 主机地址
     * dbuser:用户名
     * dbpass:密码
     * dbname:数据库名称
     * dbport:端口
     **********************************/
    void loadFromMap(const map<string, string> &mpParam)
    {
      map<string, string> mpTmp = mpParam;

      _host = mpTmp["dbhost"];
      _user = mpTmp["dbuser"];
      _password = mpTmp["dbpass"];
      _database = mpTmp["dbname"];
      _charset = mpTmp["charset"];
      _port = atoi(mpTmp["dbport"].c_str());
      _flag = 0;

      if(mpTmp["dbport"] == "")
      {
        _port = 3306;
      }
    }
  };

  /**************************************************************
   * @brief:MySQL数据库操作类 
   * @feature:非线程安全，通常一个线程一个MysqlHelper对象；
   * 对于insert/update可以有更好的函数封装，保证SQL注入；
   * MysqlHelper::DB_INT表示组装sql语句时，不加””和转义；
   * MysqlHelper::DB_STR表示组装sql语句时，加””并转义；
   **************************************************************/
  class MysqlHelper{

    public:
      /**
       * @brief 构造函数
       */
      MysqlHelper();

      /**
       * @brief 构造函数. 
       * @param: sHost:主机IP
       * @param sUser 用户
       * @param sPasswd 密码
       * @param sDatebase 数据库
       * @param port 端口
       * @param iUnixSocket socket
       * @param iFlag 客户端标识
       */
      MysqlHelper(const string& sHost, const string& sUser = "", const string& sPasswd = "", const string& sDatabase = "", const string &sCharSet = "", int port = 0, int iFlag = 0);

      /**
       * @brief 构造函数. 
       * @param tcDBConf 数据库配置
       */
      MysqlHelper(const DBConf& tcDBConf);

      /**
       * @brief 析构函数.
       */
      ~MysqlHelper();

      /**
       * @brief 初始化. 
       * 
       * @param sHost 主机IP
       * @param sUser 用户
       * @param sPasswd 密码
       * @param sDatebase 数据库
       * @param port 端口
       * @param iUnixSocket socket
       * @param iFlag 客户端标识
       * @return 无
       */
      void init(const string& sHost, const string& sUser = "", const string& sPasswd = "", const string& sDatabase = "", const string &sCharSet = "", int port = 0, int iFlag = 0);

      /**
       * @brief 初始化. 
       * 
       * @param tcDBConf 数据库配置
       */
      void init(const DBConf& tcDBConf);

      /**
       * @brief 连接数据库. 
       * 
       * @throws MysqlHelper_Exception
       * @return 无
       */
      void connect();

      /**
       * @brief 断开数据库连接. 
       * @return 无
       */
      void disconnect();

      /**
       * @brief 获取数据库变量. 
       * @return 数据库变量
       */
      string getVariables(const string &sName);

      /**
       * @brief 直接获取数据库指针. 
       * 
       * @return MYSQL* 数据库指针
       */
      MYSQL *getMysql();

      /**
       * @brief 字符转义. 
       * 
       * @param sFrom 源字符串
       * @param sTo 输出字符串
       * @return 输出字符串
       */
      string escapeString(const string& sFrom);

      /**
       * @brief 更新或者插入数据. 
       * 
       * @param sSql sql语句
       * @throws MysqlHelper_Exception
       * @return
       */
      void execute(const string& sSql);

      /**
       * @brief mysql的一条记录
       */
      class MysqlRecord
      {
        public:
          /**
           * @brief 构造函数.
           * 
           * @param record
           */
          MysqlRecord(const map<string, string> &record);

          /**
           * @brief 获取数据，s一般是指数据表的某个字段名 
           * @param s 要获取的字段
           * @return 符合查询条件的记录的s字段名
           */
          const string& operator[](const string &s);
        protected:
          const map<string, string> &_record;
      };

      /**
       * @brief 查询出来的mysql数据
       */
      class MysqlData
      {
        public:
          /**
           * @brief 所有数据.
           * 
           * @return vector<map<string,string>>&
           */
          vector<map<string, string> >& data();

          /**
           * 数据的记录条数
           * 
           * @return size_t
           */
          size_t size();

          /**
           * @brief 获取某一条记录. 
           * 
           * @param i 要获取第几条记录 
           * @return MysqlRecord类型的数据，可以根据字段获取相关信息，
           */
          MysqlRecord operator[](size_t i);

        protected:
          vector<map<string, string> > _data;
      };

      /**
       * @brief Query Record. 
       * 
       * @param sSql sql语句
       * @throws MysqlHelper_Exception
       * @return MysqlData类型的数据，可以根据字段获取相关信息
       */
      MysqlData queryRecord(const string& sSql);

      /**
       * @brief 定义字段类型， 
       * DB_INT:数字类型 
       * DB_STR:字符串类型
       */
      enum FT
      {
        DB_INT, 
        DB_STR, 
      };

      /**
       * 数据记录
       */
      typedef map<string, pair<FT, string> > RECORD_DATA;

      /**
       * @brief 更新记录. 
       * 
       * @param sTableName 表名
       * @param mpColumns 列名/值对
       * @param sCondition where子语句,例如:where A = B
       * @throws MysqlHelper_Exception
       * @return size_t 影响的行数
       */
      size_t updateRecord(const string &sTableName, const map<string, pair<FT, string> > &mpColumns, const string &sCondition);

      /**
       * @brief 插入记录. 
       * 
       * @param sTableName 表名
       * @param mpColumns 列名/值对
       * @throws MysqlHelper_Exception
       * @return size_t 影响的行数
       */
      size_t insertRecord(const string &sTableName, const map<string, pair<FT, string> > &mpColumns);

      /**
       * @brief 替换记录. 
       * 
       * @param sTableName 表名
       * @param mpColumns 列名/值对
       * @throws MysqlHelper_Exception
       * @return size_t 影响的行数
       */
      size_t replaceRecord(const string &sTableName, const map<string, pair<FT, string> > &mpColumns);

      /**
       * @brief 删除记录. 
       * 
       * @param sTableName 表名
       * @param sCondition where子语句,例如:where A = B
       * @throws MysqlHelper_Exception
       * @return size_t 影响的行数
       */
      size_t deleteRecord(const string &sTableName, const string &sCondition = "");

      /**
       * @brief 获取Table查询结果的数目. 
       * 
       * @param sTableName 用于查询的表名
       * @param sCondition where子语句,例如:where A = B
       * @throws MysqlHelper_Exception
       * @return size_t 查询的记录数目
       */
      size_t getRecordCount(const string& sTableName, const string &sCondition = "");

      /**
       * @brief 获取Sql返回结果集的个数. 
       * 
       * @param sCondition where子语句,例如:where A = B
       * @throws MysqlHelper_Exception
       * @return 查询的记录数目
       */
      size_t getSqlCount(const string &sCondition = "");

      /**
       * @brief 存在记录. 
       * 
       * @param sql sql语句
       * @throws MysqlHelper_Exception
       * @return 操作是否成功
       */
      bool existRecord(const string& sql);

      /**
       * @brief 获取字段最大值. 
       * 
       * @param sTableName 用于查询的表名
       * @param sFieldName 用于查询的字段
       * @param sCondition where子语句,例如:where A = B
       * @throws MysqlHelper_Exception
       * @return 查询的记录数目
       */
      int getMaxValue(const string& sTableName, const string& sFieldName, const string &sCondition = "");

      /**
       * @brief 获取auto_increment最后插入得ID. 
       * 
       * @return ID值
       */
      long lastInsertID();

      /**
       * @brief 构造Insert-SQL语句. 
       * 
       * @param sTableName 表名
       * @param mpColumns 列名/值对
       * @return string insert-SQL语句
       */
      string buildInsertSQL(const string &sTableName, const map<string, pair<FT, string> > &mpColumns);

      /**
       * @brief 构造Replace-SQL语句. 
       * 
       * @param sTableName 表名
       * @param mpColumns 列名/值对
       * @return string insert-SQL语句
       */
      string buildReplaceSQL(const string &sTableName, const map<string, pair<FT, string> > &mpColumns);

      /**
       * @brief 构造Update-SQL语句. 
       * 
       * @param sTableName 表名
       * @param mpColumns 列名/值对
       * @param sCondition where子语句
       * @return string Update-SQL语句
       */
      string buildUpdateSQL(const string &sTableName,const map<string, pair<FT, string> > &mpColumns, const string &sCondition);

      /**
       * @brief 获取最后执行的SQL语句.
       * 
       * @return SQL语句
       */
      string getLastSQL() { return _sLastSql; }

      /**
       * @brief 获取查询影响数
       * @return int
       */
      size_t getAffectedRows();
    protected:
      /**
       * @brief copy contructor，只申明,不定义,保证不被使用 
       */
      MysqlHelper(const MysqlHelper &tcMysql);

      /**
       * 
       * @brief 只申明,不定义,保证不被使用
       */
      MysqlHelper &operator=(const MysqlHelper &tcMysql);


    private:

      /**
       * 数据库指针
       */
      MYSQL *_pstMql;

      /**
       * 数据库配置
       */
      DBConf _dbConf;

      /**
       * 是否已经连接
       */
      bool _bConnected;

      /**
       * 最后执行的sql
       */
      string _sLastSql;

  };

  MysqlHelper::MysqlHelper():_bConnected(false)
  {
    _pstMql = mysql_init(NULL);
  }

  MysqlHelper::MysqlHelper(const string& sHost, const string& sUser, const string& sPasswd, const string& sDatabase, const string &sCharSet, int port, int iFlag)
    :_bConnected(false)
  {
    init(sHost, sUser, sPasswd, sDatabase, sCharSet, port, iFlag);

    _pstMql = mysql_init(NULL);
  }

  MysqlHelper::MysqlHelper(const DBConf& tcDBConf)
    :_bConnected(false)
  {
    _dbConf = tcDBConf;

    _pstMql = mysql_init(NULL); 
  }

  MysqlHelper::~MysqlHelper()
  {
    if (_pstMql != NULL)
    {
      mysql_close(_pstMql);
      _pstMql = NULL;
    }
  }

  void MysqlHelper::init(const string& sHost, const string& sUser, const string& sPasswd, const string& sDatabase, const string &sCharSet, int port, int iFlag)
  {
    _dbConf._host = sHost;
    _dbConf._user = sUser;
    _dbConf._password = sPasswd;
    _dbConf._database = sDatabase;
    _dbConf._charset = sCharSet;
    _dbConf._port = port;
    _dbConf._flag = iFlag;
  }

  void MysqlHelper::init(const DBConf& tcDBConf)
  {
    _dbConf = tcDBConf;
  }

  void MysqlHelper::connect()
  {
    disconnect();

    if( _pstMql == NULL) {
      _pstMql = mysql_init(NULL);
    }

    //建立连接后, 自动调用设置字符集语句
    if(!_dbConf._charset.empty()) {
      if (mysql_options(_pstMql, MYSQL_SET_CHARSET_NAME, _dbConf._charset.c_str())) {
        throw MysqlHelper_Exception(string("MysqlHelper::connect: mysql_options MYSQL_SET_CHARSET_NAME ") + _dbConf._charset + ":" + string(mysql_error(_pstMql)));
      }
    }

    if (mysql_real_connect(_pstMql, _dbConf._host.c_str(), _dbConf._user.c_str(), _dbConf._password.c_str(), _dbConf._database.c_str(), _dbConf._port, NULL, _dbConf._flag) == NULL) 
    {
      throw MysqlHelper_Exception("[MysqlHelper::connect]: mysql_real_connect: " + string(mysql_error(_pstMql)));
    }

    _bConnected = true;
  }

  void MysqlHelper::disconnect()
  {
    if (_pstMql != NULL)
    {
      mysql_close(_pstMql);
      _pstMql = mysql_init(NULL);
    }

    _bConnected = false; 
  }

  string MysqlHelper::escapeString(const string& sFrom)
  {
    if(!_bConnected)
    {
      connect();
    }

    string sTo;
    string::size_type iLen = sFrom.length() * 2 + 1;
    char *pTo = (char *)malloc(iLen);

    memset(pTo, 0x00, iLen);

    mysql_real_escape_string(_pstMql, pTo, sFrom.c_str(), sFrom.length());

    sTo = pTo;

    free(pTo);

    return sTo;
  }

  MYSQL *MysqlHelper::getMysql(void)
  {
    return _pstMql;
  }

  string MysqlHelper::buildInsertSQL(const string &sTableName, const RECORD_DATA &mpColumns)
  {
    ostringstream sColumnNames;
    ostringstream sColumnValues;

    map<string, pair<FT, string> >::const_iterator itEnd = mpColumns.end();

    for(map<string, pair<FT, string> >::const_iterator it = mpColumns.begin(); it != itEnd; ++it)
    {
      if (it != mpColumns.begin())
      {
        sColumnNames << ",";
        sColumnValues << ",";
      }
      sColumnNames << "`" << it->first << "`";
      if(it->second.first == DB_INT)
      {
        sColumnValues << it->second.second;
      }
      else
      {
        sColumnValues << "'" << escapeString(it->second.second) << "'";
      }
    }

    ostringstream os;
    os << "insert into " << sTableName << " (" << sColumnNames.str() << ") values (" << sColumnValues.str() << ")";
    return os.str();
  }

  string MysqlHelper::buildReplaceSQL(const string &sTableName, const RECORD_DATA &mpColumns)
  {
    ostringstream sColumnNames;
    ostringstream sColumnValues;

    map<string, pair<FT, string> >::const_iterator itEnd = mpColumns.end();
    for(map<string, pair<FT, string> >::const_iterator it = mpColumns.begin(); it != itEnd; ++it)
    {
      if (it != mpColumns.begin())
      {
        sColumnNames << ",";
        sColumnValues << ",";
      }
      sColumnNames << "`" << it->first << "`";
      if(it->second.first == DB_INT)
      {
        sColumnValues << it->second.second;
      }
      else
      {
        sColumnValues << "'" << escapeString(it->second.second) << "'";
      }
    }

    ostringstream os;
    os << "replace into " << sTableName << " (" << sColumnNames.str() << ") values (" << sColumnValues.str() << ")";
    return os.str();
  }

  string MysqlHelper::buildUpdateSQL(const string &sTableName,const RECORD_DATA &mpColumns, const string &sWhereFilter)
  {
    ostringstream sColumnNameValueSet;

    map<string, pair<FT, string> >::const_iterator itEnd = mpColumns.end();

    for(map<string, pair<FT, string> >::const_iterator it = mpColumns.begin(); it != itEnd; ++it)
    {
      if (it == mpColumns.begin())
      {
        sColumnNameValueSet << "`" << it->first << "`";
      }
      else
      {
        sColumnNameValueSet << ",`" << it->first << "`";
      }

      if(it->second.first == DB_INT)
      {
        sColumnNameValueSet << "= " << it->second.second;
      }
      else
      {
        sColumnNameValueSet << "= '" << escapeString(it->second.second) << "'";
      }
    }

    ostringstream os;
    os << "update " << sTableName << " set " << sColumnNameValueSet.str() << " " << sWhereFilter;

    return os.str();
  }

  string MysqlHelper::getVariables(const string &sName)
  {
    string sql = "SHOW VARIABLES LIKE '" + sName + "'";

    MysqlData data = queryRecord(sql);
    if(data.size() == 0)
    {
      return "";
    }

    if(sName == data[0]["Variable_name"])
    {
      return data[0]["Value"];
    }

    return "";
  }

  void MysqlHelper::execute(const string& sSql)
  {
    /**
      没有连上, 连接数据库
      */
    if(!_bConnected)
    {
      connect();
    }

    _sLastSql = sSql;

    printf("execute-->[%s]\n", sSql.c_str());
    int iRet = mysql_real_query(_pstMql, sSql.c_str(), sSql.length());
    if(iRet != 0)
    {
      /**
        自动重新连接
        */
      int iErrno = mysql_errno(_pstMql);
      if (iErrno == 2013 || iErrno == 2006)
      {
        connect();
        iRet = mysql_real_query(_pstMql, sSql.c_str(), sSql.length());
      }
    }

    if (iRet != 0)
    {
      throw MysqlHelper_Exception("[MysqlHelper::execute]: mysql_query: [ " + sSql+" ] :" + string(mysql_error(_pstMql))); 
    }
  }

  MysqlHelper::MysqlData MysqlHelper::queryRecord(const string& sSql)
  {
    MysqlData data;

    /**
      没有连上, 连接数据库
      */
    if(!_bConnected)
    {
      connect();
    }

    _sLastSql = sSql;

    printf("query-->[%s]\n", sSql.c_str());
    int iRet = mysql_real_query(_pstMql, sSql.c_str(), sSql.length());
    if(iRet != 0)
    {
      /**
        自动重新连接
        */
      int iErrno = mysql_errno(_pstMql);
      if (iErrno == 2013 || iErrno == 2006)
      {
        connect();
        iRet = mysql_real_query(_pstMql, sSql.c_str(), sSql.length());
      }
    }

    if (iRet != 0)
    {
      throw MysqlHelper_Exception("[MysqlHelper::execute]: mysql_query: [ " + sSql+" ] :" + string(mysql_error(_pstMql))); 
    }

    MYSQL_RES *pstRes = mysql_store_result(_pstMql);

    if(pstRes == NULL)
    {
      throw MysqlHelper_Exception("[MysqlHelper::queryRecord]: mysql_store_result: " + sSql + " : " + string(mysql_error(_pstMql)));
    }

    vector<string> vtFields;
    MYSQL_FIELD *field;
    while((field = mysql_fetch_field(pstRes)))
    {
      vtFields.push_back(field->name);
    }

    map<string, string> mpRow;
    MYSQL_ROW stRow;

    while((stRow = mysql_fetch_row(pstRes)) != (MYSQL_ROW)NULL)
    {
      mpRow.clear();
      unsigned long * lengths = mysql_fetch_lengths(pstRes);
      for(size_t i = 0; i < vtFields.size(); i++)
      {
        if(stRow[i])
        {
          mpRow[vtFields[i]] = string(stRow[i], lengths[i]);
        }
        else
        {
          mpRow[vtFields[i]] = "";
        }
      }

      data.data().push_back(mpRow);
    }

    mysql_free_result(pstRes);

    return data;
  }

  size_t MysqlHelper::updateRecord(const string &sTableName, const RECORD_DATA &mpColumns, const string &sCondition)
  {
    string sSql = buildUpdateSQL(sTableName, mpColumns, sCondition);
    execute(sSql);

    return mysql_affected_rows(_pstMql);
  }

  size_t MysqlHelper::insertRecord(const string &sTableName, const RECORD_DATA &mpColumns)
  {
    string sSql = buildInsertSQL(sTableName, mpColumns);
    execute(sSql);

    return mysql_affected_rows(_pstMql);
  }

  size_t MysqlHelper::replaceRecord(const string &sTableName, const RECORD_DATA &mpColumns)
  {
    string sSql = buildReplaceSQL(sTableName, mpColumns);
    execute(sSql);

    return mysql_affected_rows(_pstMql);
  }

  size_t MysqlHelper::deleteRecord(const string &sTableName, const string &sCondition)
  {
    ostringstream sSql;
    sSql << "delete from " << sTableName << " " << sCondition;

    execute(sSql.str());

    return mysql_affected_rows(_pstMql);
  }

  size_t MysqlHelper::getRecordCount(const string& sTableName, const string &sCondition)
  {
    ostringstream sSql;
    sSql << "select count(*) as num from " << sTableName << " " << sCondition;

    MysqlData data = queryRecord(sSql.str());

    long n = atol(data[0]["num"].c_str());

    return n;

  }

  size_t MysqlHelper::getSqlCount(const string &sCondition)
  {
    ostringstream sSql;
    sSql << "select count(*) as num " << sCondition;

    MysqlData data = queryRecord(sSql.str());

    long n = atol(data[0]["num"].c_str());

    return n;
  }

  int MysqlHelper::getMaxValue(const string& sTableName, const string& sFieldName,const string &sCondition)
  {
    ostringstream sSql;
    sSql << "select " << sFieldName << " as f from " << sTableName << " " << sCondition << " order by f desc limit 1";

    MysqlData data = queryRecord(sSql.str());

    int n = 0;

    if(data.size() == 0)
    {
      n = 0;
    }
    else
    {
      n = atol(data[0]["f"].c_str());
    }

    return n;
  }

  bool MysqlHelper::existRecord(const string& sql)
  {
    return queryRecord(sql).size() > 0;
  }

  long MysqlHelper::lastInsertID()
  {
    return mysql_insert_id(_pstMql);
  }

  size_t MysqlHelper::getAffectedRows()
  {
    return mysql_affected_rows(_pstMql);
  }

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////
  MysqlHelper::MysqlRecord::MysqlRecord(const map<string, string> &record):_record(record){}

  const string& MysqlHelper::MysqlRecord::operator[](const string &s)
  {
    map<string, string>::const_iterator it = _record.find(s);
    if(it == _record.end())
    {
      throw MysqlHelper_Exception("field '" + s + "' not exists.");
    }
    return it->second;
  }

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////

  vector<map<string, string> >& MysqlHelper::MysqlData::data()
  {
    return _data;
  }

  size_t MysqlHelper::MysqlData::size()
  {
    return _data.size();
  }

  MysqlHelper::MysqlRecord MysqlHelper::MysqlData::operator[](size_t i)
  {
    return MysqlRecord(_data[i]);
  }
}
#endif //__MYSQL_HELPER_H__
