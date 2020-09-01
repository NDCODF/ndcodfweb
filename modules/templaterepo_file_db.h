#ifndef __FILE_DB_H__
#define __FILE_DB_H__
#include "config.h"
#include <Poco/Net/HTMLForm.h>

class FileDB
{
public:
    FileDB();
    virtual ~FileDB();

    virtual void setDbPath();

    std::string newkey();
    bool setFile(Poco::Net::HTMLForm &);
    void delFile(std::string);
    void updateFile(Poco::Net::HTMLForm &);
	std::string getInfo();
    std::string getFile(std::string);



private:
    std::string dbfile;
};

#endif
