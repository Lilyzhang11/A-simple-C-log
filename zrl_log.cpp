#include"zrl_log.h"
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include<iostream>
#include<stdarg.h>
#include<sstream>
#include<thread>


Log::Log(const std::string& dir,const std::string&fname,int loglevel,int newhour):_dir(dir),_fname(fname),_loglevel(loglevel),_newhour(newhour)
{
	int ret;
	int c,lasti;
	
	_showpid=1;
	_lockfile = "log.lock";
	if(newhour!=-1)
	{
	    _expires_time = set_expires_time();  //设置转储时间
	}
        else
        {
          _expires_time=-1;
        }
	plockinit();   //初始化文件锁
	openlog();

//初始化完成
	
}

void Log::error_std_out(const std::string&str)
{
  std::cout<<str<<std::endl;
}


void Log::log_quit_onerr(int errflag,const char* fmt,...)
{   
  char buff[1024]={'\0'};
  char errmsg[1024]={'\0'};

  std::string error_formation;
  assert(fmt);//判断参数列表不为空
  std:: string time_now =get_strtime_now();

  error_formation+=time_now;

  va_list ap;
  va_start(ap,fmt);
  int size =vsnprintf(buff,1024,fmt,ap);
  va_end(ap);
  
  error_formation+=std::string(buff);
  if(errflag)  //如果出现了错误
    {
      strerror_r(errno,errmsg,1024);//这里会存在线程不安全的问题，最终还是要使用strerror_r,因为strerro返回的是一个静态变量的指针，所有线程公用，存在线程不安全
      error_formation= error_formation+"("+std::to_string(errno)+":"+std::string(errmsg)+")\n";
  
      error_std_out(error_formation);
      abort();
    }

    error_formation= error_formation+"\n";
  
    error_std_out(error_formation);

}


int Log::openlog()
{
	if(_dir[_dir.size()-1]=='/'||_dir[_dir.size()-1]=='\\')
	{
		_dir = _dir.substr(0,_dir.size()-1);
	}
	  std:: string _full_path = _dir+"/"+_fname;
    _fh = fopen(_full_path.c_str(),"a");
    if(_fh==NULL)
    {
    	log_quit_onerr(1,"open log file [%s] failed.",_full_path.c_str());
    } 
    return 0;
}  

int Log::reopen_log(const std::string& dir,const std::string& fname)
{
	if(fclose(_fh))
	{
		log_quit_onerr(1,"close file [%s] failed",_fname.c_str());
	}
	_dir=dir;
	_fname=fname;
	openlog();
	return 0;

}


time_t Log::set_expires_time()
{
	time_t t =time(0);
	struct tm dt;
	if(localtime_r(&t,&dt)==NULL)
	{
		log_quit_onerr(1,"get_expires_time error");
		return -1;
	}
	dt.tm_hour=_newhour;
	dt.tm_min=0;
	dt.tm_sec=0;
	time_t t_tmp = mktime(&dt);
	if(t_tmp<0)
	{
		log_quit_onerr(1,"get_expires_time error");
		return -2;
	}
	if(difftime(t_tmp,time(0))<=0)
	{
		t_tmp+=24*60*60;
	} 
	return t_tmp;

}


int Log::daily_autosplit()
{

  if(difftime(time(0),_expires_time)<0)  //如果还没有到转储时间就返回
  {
  	return 0;
  }
  _expires_time = set_expires_time();
  std::string dir =_dir;
  std::string fname=_fname.substr(0,_fname.find_first_of('.'))+"."+get_strtime_now();
  reopen_log(dir,fname);
  return 0;
 
}


int Log::plockinit()  //创建或者打开 log.lock文件
{
   std::string path = _dir+_lockfile;
   int ret = open(path.c_str(), O_CREAT | O_RDWR,S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
   if(ret < 0)
   {
   	log_quit_onerr(LOG_ERR,"open lock file(%s) failed",path.c_str());
   }
   _lockfd = ret;
   return 0;
}


int Log::plock()   //对文件上锁
{
   struct flock fl;
   fl.l_type = F_WRLCK;
   fl.l_whence = SEEK_SET;
   fl.l_start = 0;
   fl.l_len = 0;
   return fcntl(_lockfd, F_SETLKW, &fl);
}


int Log::punlock()  //对文件解锁
{
   struct flock fl;
   fl.l_type = F_WRLCK;
   fl.l_whence = SEEK_SET;
   fl.l_start = 0;
   fl.l_len = 0;
   return fcntl(_lockfd, F_UNLCK, &fl);
}


int Log::fwriten(FILE* fh,const std::string&str)
{ size_t nwritten;
 const char * ptr = str.c_str();
 int nleft = str.size();
 while(nleft>0)
 {
 	if((nwritten=fwrite(ptr,1,nleft,fh))<=0)
 	{
 		return -1;
 	}
 	nleft-=nwritten;
 	ptr+=nwritten;
 }
 return str.size()-nleft;
}


int Log::logcore(int errflag,int loglevel,const char*fmt,va_list ap)
{ 
  
  std::ostringstream oss;

  std::string info;
	assert(fmt);
	char buff[1024]={'\0'};
	char errmsg[1024]={'\0'};
	if(loglevel>_loglevel)  //例：如果设置为debug模式，那么其他loglevel都不会记录
	{
		return 0;
	}
	info+=get_strtime_now()+" log_level="+std::to_string(loglevel);
	if(_showpid) 
	{    oss << std::this_thread::get_id();
       std::string stid = oss.str();
       unsigned long long tid = std::stoull(stid);
       info+="  thread_id="+std::to_string(tid);
	}

	vsnprintf(buff,1024,fmt,ap); 
	info+="  "+std::string(buff);
	if(errflag)
	{    
       strerror_r(errno,errmsg,1024);
       info+="("+std::to_string(errno)+":"+std::string(errmsg)+")";
	}
	info+="\n";
	plock();  //上文件锁
	_mtx.lock(); //上互斥锁
    daily_autosplit();
    int ret = fwriten(_fh,info);
    if(ret<info.size())
    {
    	log_quit_onerr(LOG_ERR,"write to log [%s,%s] error",_dir.c_str(),_fname.c_str());
    }
    fflush(_fh);
    _mtx.unlock();
    punlock();
    return 0;
}


int Log::logfatal(const char*fmt,...)
{
    va_list ap;
    va_start(ap,fmt);
    int ret = logcore(LOG_ERR,LOG_FATAL,fmt,ap);
    va_end(ap);
    abort();
    return ret;
}


int Log::logerror(const char*fmt,...)
{
    va_list ap;
    va_start(ap,fmt);
    int ret = logcore(LOG_ERR,LOG_ERROR,fmt,ap);
    va_end(ap);
    return ret;
} //一旦发生了error程序需要终止


 int Log::logwarn(const char*fmt,...)
 {
    va_list ap;
    va_start(ap,fmt);
    int ret = logcore(LOG_NOERR,LOG_WARN,fmt,ap);
    va_end(ap);
    return ret;
 }


 int Log::loginfo(const char*fmt,...)
 {
    va_list ap;
    va_start(ap,fmt);
    int ret = logcore(LOG_NOERR,LOG_INFO,fmt,ap);
    va_end(ap);
    return ret;
 }


 int Log::logdebug(const char*fmt,...)
 {
 	  va_list ap;
    va_start(ap,fmt);
    int ret = logcore(LOG_NOERR,LOG_DEBUG,fmt,ap);
    va_end(ap);
    return ret;
 }

 Log::~Log()
 { if(_fh!=NULL)
  {if(fclose(_fh))
   {
     log_quit_onerr(LOG_ERR,"close log file [%s/%s] failed!",_dir.c_str(),_fname.c_str());
   }
  }

  if(close(_lockfd))
  {
    log_quit_onerr(LOG_ERR,"close lock file [%s/%s] failed!",_dir.c_str(),_lockfile.c_str());
  }

 }
