#include <QCoreApplication>
#include <ctime>
#include <string>
#include <sstream>
#include <map>
#include <iostream>
#include <fstream>
#include <streambuf>
#include <sys/types.h>
#include <stdio.h>
#include <limits.h>     /* PATH_MAX */
#include <sys/stat.h>   /* mkdir(2) */
#include <unistd.h>
#include <chrono>
#include <qstring.h>
#include <qcache.h>
#include "oncache2.h"

#include "stlcache.h"
#include <folly/container/EvictingCacheMap.h>

#define TEST_MAXSIZE 1000000
#define START_LOGPOINT 1000
#define TEST_CASE_FILE
#define LOAD_DATA_FROM_TEST_CASE_FILE false
#define MAKE_HASH_NOT_UNIQUE true

template <typename T>
std::string to_string(T value)
{
    std::ostringstream os;
    os << value;
    return os.str();
}

struct  Elem  {
  Elem(unsigned long long  p0,  unsigned long long  p1,  unsigned long long  p2,  std::string  &&_data)
        : key(p0,  p1,  p2),  data(std::move(_data)) { }
  Elem(const Elem  &other)  :  data(other.data),  key(other.key)  {  }
  Elem(const Elem  *other)  :  data(other->data),  key(other->key)  {  }

  TKey key;
  const  std::string  data;
};


void  writeDuration(auto  &start,  std::string  &str)  {
  auto  end  =  std::chrono::high_resolution_clock::now();
  long long  ms  =  std::chrono::duration_cast<std::chrono::microseconds>(
                  end - start).count();    
  str.append(",").append(to_string(ms));
  start  =  end;
}

/*
 * This is Facebook cache based on Boost containers
 * https://github.com/facebook/folly/blob/master/folly/container/EvictingCacheMap.h
*/
void  testBoostCache(const std::map<TKey, Elem *>  &mData,
    std::string  &strInsert,  std::string  &strFind,
    unsigned int  testSize,  unsigned int  capacity)  {
  folly::EvictingCacheMap<TKey, Elem *, THash, TEqual> boostCache(capacity);

  auto  start  =  std::chrono::high_resolution_clock::now();
  unsigned int  i  =  0;
  for (auto&&  it  :  mData)  {
    boostCache.insert(it.second->key,  it.second);
    ++i;
    if  (i  >=  testSize)  {
      break;
    }
  }  //for
  writeDuration(start,  strInsert);

  start  =  std::chrono::high_resolution_clock::now();
  i  =  0;
  unsigned int  notFound  =  0;
  unsigned int  found  =  0;
  for (auto&&  it  :  mData)  {
    //For best speed, I only check the existence:
    if (boostCache.exists(it.second->key))  {
      ++found;
    }  else  {
      ++notFound;
    }
    ++i;
    if  (i  >=  testSize)  {
      break;
    }
  }  //for
  writeDuration(start, strFind);
  std::cout  <<  "found:"  <<  found  <<  " ,ERROR(NotFound):"  <<notFound  <<  '\n';

}  //  testBoostCache

void  testSTLCache(const std::map<TKey, Elem *>  &mData,
    std::string  &strInsert,  std::string  &strFind,
    unsigned int  testSize,  unsigned int  capacity)  {
  LRU<TKey, Elem *>  stlCache(capacity);
  auto  start  =  std::chrono::high_resolution_clock::now();
  unsigned int  i  =  0;
  for (auto&&  it  :  mData)  {
    stlCache.put(it.second->key,  it.second);
    ++i;
    if  (i  >=  testSize)  {
      break;
    }
  }  //for
  writeDuration(start,  strInsert);

  start  =  std::chrono::high_resolution_clock::now();
  i  =  0;
  unsigned int  notFound  =  0;
  unsigned int  found  =  0;
  for (auto&&  it  :  mData)  {
    //For best speed, I only check the existence:
    if (stlCache.exist(it.second->key))  {
      ++found;        
    }  else  {
      ++notFound;
    }
    ++i;
    if  (i  >=  testSize)  {
      break;
    }
  }  //for
  writeDuration(start, strFind);
  std::cout  <<  "found:"  <<  found  <<  " ,ERROR(NotFound):"  <<notFound  <<  '\n';

}  //  testSTLCache


void  testQCache(const std::map<TKey, Elem *>  &mData,
    std::string  &strInsert,  std::string  &strFind,
    unsigned int  testSize,  unsigned int  capacity)  {
  QCache<TKey, Elem> qCache;
  qCache.setMaxCost(capacity);
  auto  start  =  std::chrono::high_resolution_clock::now();
  unsigned int  i  =  0;
  for  (auto&&  it  :  mData)  {
    qCache.insert(TKey(it.second->key),  new Elem(it.second));
    ++i;
    if  (i  >=  testSize)  {
      break;
    }
  }  //  for
  writeDuration(start, strInsert);

  start  =  std::chrono::high_resolution_clock::now();
  i  =  0;
  unsigned int  notFound  =  0;
  unsigned int  found  =  0;
  for  (auto&&  it  :  mData)  {
    if  (qCache.take(it.second->key))  {
      ++found;
    }  else  {
      // std::cout<<"ERROR(NotFound):"<<it.second->key<<'\n';
      ++notFound;
    }
    ++i;
    if  (i  >=  testSize)  {
      break;
    }
  }  //  for
  writeDuration(start,  strFind);
  std::cout  <<  "found:"  <<  found  <<  " ,ERROR(NotFound):"  <<  notFound  <<  '\n';

}  //  testQCache


void  testOnCache(const std::map<TKey, Elem *>  &mData,
    std::string  &strInsert,  std::string  &strFind,
    unsigned int  testSize,  unsigned int  capacity)  {
  OnCache oCache(capacity, [](const char * data)  {
        //std::cout<<"deleted:"<<data<<',';
    });    
  auto  start  =  std::chrono::high_resolution_clock::now();
  unsigned int  i  =  0;
  for (auto&&  it  :  mData)  {
    Elem  *ptr  =  it.second;

//        if (ptr->key.keyArray[0]==538286
//            ||ptr->key.keyArray[0]==705025
//                ||ptr->key.keyArray[0]==1505242) {
//            std::cout<<"\n";
//        }
//    if (ptr->key.keyArray[0]==18531564
//            ||ptr->key.keyArray[0]==17501564) {
//        std::cout<<"\n";
//    }
//        if (ptr->key.keyArray[0] == 45581756
//                ||ptr->key.keyArray[0] == 44571824) {
//            std::cout<<"\n";
//        }
//    if (ptr->key.keyArray[0] == 284605469) {
//       std::cout<<"\n";
//    }
    oCache.insertNode(&(ptr->key),  ptr->data.c_str());
  // oCache.find_bug();
    ++i;
    if  (i  >=  testSize)  {
      break;
    }
  }  //  for
  writeDuration(start, strInsert);

  start  =  std::chrono::high_resolution_clock::now();
  i  =  0;
  unsigned int  notFound  =  0;
  unsigned int  found  =  0;
  for (auto&&  it  :  mData)  {
    Elem  *ptr  =  it.second;
    const char  *data  =  oCache.getData(&(ptr->key));
    if  (data)  {
      if  (data  ==  ptr->data.c_str())  {
        ++found;
      } else {
        std::cout<<"ERROR(data):"<<ptr->data<<"<>"<<data<<'\n';
      }
    }  else  {
             //std::cout<<"ERROR(NotFound):"<<ptr->key.keyArray[0]<<'\n';             
      ++notFound;
    }
    ++i;
    if  (i  >=  testSize)  {
      break;
    }
  }  //  for
  writeDuration(start, strFind);
  std::cout  <<  "found:"  <<  found  <<  " ,ERROR(NotFound):"  <<  notFound  <<  '\n';
}  //  OnChache


void  testOnCache2(const std::map<TKey, Elem *>  &mData,
    std::string  &strInsert,  std::string  &strFind,
    unsigned int  testSize,  unsigned int  capacity)  {
  OnCache2 oCache(capacity, 10, [](const char * data)  {
        //std::cout<<"deleted:"<<data<<',';
    });
  auto  start  =  std::chrono::high_resolution_clock::now();
  unsigned int  i  =  0;
  for (auto&&  it  :  mData)  {
    Elem  *ptr  =  it.second;

//        if (ptr->key.keyArray[0]==538286
//            ||ptr->key.keyArray[0]==705025
//                ||ptr->key.keyArray[0]==1505242) {
//            std::cout<<"\n";
//        }
//    if (ptr->key.keyArray[0]==18531564
//            ||ptr->key.keyArray[0]==17501564) {
//        std::cout<<"\n";
//    }
//        if (ptr->key.keyArray[0] == 47635
//                ||  ptr->key.keyArray[0] == 6690060) {
//            std::cout<<"\n";
//        }
//    if (ptr->key.keyArray[0] == 215343422) {
//       std::cout<<"\n";
//    }
    oCache.insertNode(&(ptr->key),  ptr->data.c_str());
  // oCache.find_bug();
    ++i;
    if  (i  >=  testSize)  {
      break;
    }
  }  //  for
  writeDuration(start, strInsert);

  start  =  std::chrono::high_resolution_clock::now();
  i  =  0;
  unsigned int  notFound  =  0;
  unsigned int  found  =  0;
  for (auto&&  it  :  mData)  {
    Elem  *ptr  =  it.second;
    const char  *data  =  oCache.getData(&(ptr->key));
    if  (data)  {
      if  (data  ==  ptr->data.c_str())  {
        ++found;
      } else {
        std::cout<<"ERROR(data):"<<ptr->data<<"<>"<<data<<'\n';
      }
    }  else  {
             //std::cout<<"ERROR(NotFound):"<<ptr->key.keyArray[0]<<'\n';
      ++notFound;
    }
    ++i;
    if  (i  >=  testSize)  {
      break;
    }
  }  //  for
  writeDuration(start, strFind);
  std::cout  <<  "found:"  <<  found  <<  " ,ERROR(NotFound):"  <<  notFound  <<  '\n';


  //debug not found:
//    i  =  0;
//    for (auto&&  it  :  mData)  {
//      Elem  *ptr  =  it.second;
////          if (ptr->key.keyArray[0] == 86635) {
////             std::cout << oCache.getRateCount() <<"\n";
////          }
////          if (ptr->key.keyArray[0] == 47635
////                  ||  ptr->key.keyArray[0] == 6690060) {
////            std::cout<<"\n";
////          }
////      if (ptr->key.keyArray[0] == 1010060
////              ||  ptr->key.keyArray[0] == 1007461) {
////        std::cout<<"\n";
////      }
//      oCache.insertNode(&(ptr->key),  ptr->data.c_str());
//    // oCache.find_bug();
//      ++i;
//      if  (i  >=  capacity)  {
//        break;
//      }
//    }  //  for
//    i  =  0;
//    for (auto&&  it  :  mData)  {
//      Elem  *ptr  =  it.second;
//      const char  *data  =  oCache.getData(&(ptr->key));
//      if  (data)  {
//        if  (data  ==  ptr->data.c_str())  {
//          ++found;
//        } else {
//          std::cout<<"ERROR(data):"<<ptr->data<<"<>"<<data<<'\n';
//        }
//      }  else  {
//               //std::cout<<"ERROR(NotFound):"<<ptr->key.keyArray[0]<<'\n';
//        ++notFound;
//        assert(false);
//      }
//      ++i;
//      if  (i  >=  capacity)  {
//        break;
//      }
//    }  //  for
}  //  OnChache2

bool  mkdir_p(const char  *filePath)  {
  if  (!(filePath  &&  *filePath))  {  return false;  }
  char  _path[PATH_MAX];
  char  *p  =  _path;
  char *end  =  _path  +  PATH_MAX;
  *p  =  *filePath;
  ++p;
  ++filePath;
  while  (*filePath  &&  p < end)  {
    if  ('/' == *filePath  ||  '\\' == *filePath)  {
      *p  =  0;
#if defined(Windows)            
      if (_mkdir(_path)  !=  0)  {
#else            
      if  (mkdir(_path, S_IRWXU)  !=  0)  {
#endif                
        if (errno  !=  EEXIST)  {
          return  false;
        }
      }
    }
    *p  =  *filePath;
    ++p;
    ++filePath;
  }
  return  true;
}

bool  saveTFileS(const char  *filePath,  const char  *data,  unsigned long  len)  {
  bool re  =  false;
  if (mkdir_p(filePath))  {
    std::ofstream outfile  (filePath);        
    outfile.write (data,  len);
    outfile.close();
    re  =  true;
  }
  return re;
}

std::string  loadFileS(const char  *filePath)  {
  std::string str;
  std::ifstream t(filePath);
  t.seekg(0,  std::ios::end);
  str.reserve(t.tellg());
  t.seekg(0, std::ios::beg);
  str.assign((std::istreambuf_iterator<char>(t)),
                   std::istreambuf_iterator<char>());    
  return str;
}

long long  stoll(const char * &p,  const char  *end)  {
  unsigned long long re  =  0ULL; //18 446 744 073 709 551 615 = 20
  while (*p != ','  &&  *p  &&  p < end  &&  re < LLONG_MAX)  {
    re  =  10 * re  +  *p  -  '0';
    ++p;
  }
  ++p;
  return re;
}

std::string  getExePathS()  {
  std::string  _exePath;
  char  buf[PATH_MAX];
#if defined(Windows)
    DWORD len = GetModuleFileNameA(GetModuleHandleA(0x0), buf, MAX_PATH);

    if (len > 0) {
        for (--len; len > 0; --len) {
            if ('/' == buf[len] || '\\' == buf[len]) {
                _exePath = std::string(buf, len);
                break;
            }
        }
    }
#else    
  ssize_t  len  =  0;
  len  =  readlink("/proc/self/exe",  buf,  PATH_MAX);
  if (len  >  0)  {
    for (--len;  len  >  0;  --len)  {
      if ('/'  ==  buf[len])  {
        _exePath  =  std::string(buf, len);
        break;
      }
    }
  }
#endif    
  return _exePath;
}

int main(int  argc,  char  *argv[])  {
  /* params: */
  //  if you need same test with same data from previos run:
  bool  fromFile  =  LOAD_DATA_FROM_TEST_CASE_FILE;
  //  change hash function for best/worst case:
  TKey::do_worst_case  =  MAKE_HASH_NOT_UNIQUE;


  QCoreApplication a(argc, argv);
  srand(std::time(nullptr));
  std::map<TKey, Elem *>  mData;
  unsigned long  nextLogPoint  =  START_LOGPOINT;
  const std::string  &exePath  =  getExePathS();
  std::string csvPath(exePath);
  auto  start  =  std::chrono::system_clock::now();
  long long  s  =  std::chrono::duration_cast<std::chrono::seconds>(
    start.time_since_epoch()).count();

  csvPath.append("/cache.compare.")
            .append(to_string(s))
            .append(".csv");  
  std::cout<<"Test results will be written to:\n"
            <<  csvPath  <<  '\n';
  std::ofstream  csvfile (csvPath);
  csvPath.clear();
  csvPath.append("Test name");


#ifdef TEST_CASE_FILE
    if (fromFile)  {
      std::string  testCaseFile(exePath);
      testCaseFile.append("/testCase.txt");
      const std::string  &testCase  =  loadFileS(testCaseFile.c_str());
      const char  *begin  =  testCase.c_str();
      const char  *end  =  begin  +  testCase.length();
      unsigned long  i  =  0;
      while (begin  <  end)  {
        int  r  =  stoll(begin,  end);
        Elem  *e  =  new Elem(r,  r,  r,  to_string(r));
        mData.insert(std::make_pair(e->key,  e));
        ++i;
        if (i  >=  nextLogPoint)  {
          nextLogPoint  *=  10;
          csvPath.append(",")
                        .append(to_string(i));            
        }
      }
    }  else  {
      std::string testCase;
#endif        
      for (unsigned long  i=0;  i  <=  TEST_MAXSIZE;  ++i)  {
        int  r  =  rand();
#ifdef TEST_CASE_FILE            
        testCase.append(to_string(r)).push_back(',');
#endif            
        Elem  *e  =  new Elem(r,  r,  r,  to_string(r));
        mData.insert(std::make_pair(e->key,  e));
        if  (i  >=  nextLogPoint)  {
          nextLogPoint  *=  10;
          csvPath.append(",")
              .append(to_string(i));
        }
      }
#ifdef TEST_CASE_FILE        
      std::string testCaseFile(exePath);
      testCaseFile.append("/testCase.txt");
      saveTFileS(testCaseFile.c_str(),
        testCase.c_str(),  testCase.length());
    }
#endif

//    csvPath.append(",")
//            .append(to_string(mData.size())).push_back('\n');
    csvPath.push_back('\n');
    csvfile.write (csvPath.c_str(), csvPath.length());

    std::cout  <<  "mData.size()="  <<  mData.size()  <<'\n';

//    for (auto it : mData) {
//        std::cout <<it.first<<"\t:\t"<<it.second.data<<'\n';
//    }

    std::string  strInsert;
    std::string  strFind;
    typedef  void(*TFunc)(const std::map<TKey, Elem *>  &mData,
      std::string  &strInsert,  std::string  &strFind,
      unsigned int  testSize,  unsigned int  capacity);
    std::map<std::string, TFunc>  mFun;

    //Used alhorithms:
    mFun.insert(std::make_pair(std::string("QCache"),  testQCache));
    mFun.insert(std::make_pair(std::string("STLCache"),  testSTLCache));
    mFun.insert(std::make_pair(std::string("OnCache"),  testOnCache));
    mFun.insert(std::make_pair(std::string("OnCache2"),  testOnCache2));
    mFun.insert(std::make_pair(std::string("BoostCache"),  testBoostCache));
    for (auto&&  p:  mFun)  {
        //*p(mData, strInsert, strFind, testSize, capacity);        
      strInsert.clear();
      strInsert.append(p.first)
                .append(".insert (micros)");        
      strFind.clear();
      strFind.append(p.first)
                .append(".find (micros)");        
      unsigned int  testSize  =  START_LOGPOINT;
      unsigned int  capacity  =  testSize / 10;

        //p.second(mData, strInsert, strFind, 1000, 100);
        //p.second(mData, strInsert, strFind, 10000, 1000);
        //p.second(mData, strInsert, strFind, 100000, 10000);
        //p.second(mData, strInsert, strFind, 1000000, 100000);


      while (testSize  <=  TEST_MAXSIZE)  {
        p.second(mData,  strInsert,  strFind,  testSize,  capacity);
        capacity  =  testSize;
        testSize  *= 10;
      }
      strInsert.push_back('\n');
      strFind.push_back('\n');
      csvfile.write (strInsert.c_str(), strInsert.length());
      csvfile.write (strFind.c_str(), strFind.length());
    }  // main




    //testQCache

    csvfile.close();
    std::cout << "\nThe end of tests.\n";

    for (auto it : mData) {         
         delete it.second;
    }
    return a.exec();
}
