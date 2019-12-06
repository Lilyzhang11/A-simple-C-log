#ifndef ZRL_LOG_H
#define ZRL_LOG_H

#include<stdlib.h>
#include<mutex>
#include<string.h>
#include<iostream>
#include<string>
#include<time.h>

#define LOG_OFF      -1 
#define LOG_FATAL    0
#define LOG_ERROR    1
#define LOG_WARN     2 
#define LOG_INFO     3 
#define LOG_DEBUG    4 

#define LOG_ERR 1
#define LOG_NOERR 0

class Log
{
   public:
   	 Log() =delete;//禁止使用默认构造函数
     Log(const std::string& dir,const std::string&fname,int logvel,int newhour);
     ~Log();
    
     int logfatal(const char*fmt,...);
     int logerror(const char*fmt,...);
     int logwarn(const char*fmt,...);
     int loginfo(const char*fmt,...);
     int logdebug(const char*fmt,...);
     

   private:
   	  /*private variables*/
      std::string _dir;
      std::string _fname;
      std::string _fpath;
      std::string _lockfile;
      int _fd;
      FILE * _fh;
      std::mutex _mtx;
      int _loglevel;
      int _showpid;

      
      int _lockfd;
      int _newhour;
      time_t _expires_time;
     /*private functions*/
      time_t set_expires_time();
      int asinit();
      int daily_autosplit();
      
      int plockinit();
      int plock();
      int punlock();    
     
      int openlog();
      int reopen_log(const std::string& dir,const std::string& fname);          
      void error_std_out(const std::string&str);
      void log_quit_onerr(int errflag,const char* fmt,...);
      int logcore(int errflag,int loglevel,const char*fmt,va_list ap);
      int fwriten(FILE* fh,const std::string&str);
       inline std::string get_strtime_now()
     {  std:: string ret;
        time_t t =time(0);
        struct tm now;
        if(localtime_r(&t,&now)==NULL)
        {   
        	return ret; 
        }
        ret+=std::to_string(now.tm_year+1900);
        ret+="-";
        ret+=std::to_string(now.tm_mon+1);
        ret+="-";
        ret+=std::to_string(now.tm_mday);
        ret+=": ";
        ret+=std::to_string(now.tm_hour);
        ret+=":";
        ret+=std::to_string(now.tm_min);
        ret+=":";
        ret+=std::to_string(now.tm_sec);
        ret+=" ";      
        
     }


};
#endif