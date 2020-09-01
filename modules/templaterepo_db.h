#ifndef __TEMPLATEREPO_DB_H__
#define __TEMPLATEREPO_DB_H__
#include "config.h"
#include <string>

class TemplateRepoDB
{
public:
    TemplateRepoDB();
    virtual ~TemplateRepoDB();

    bool hasKey(const std::string);
    bool hasKeyExpired(const std::string);
    virtual void setDbPath();

    void setFile(std::string, std::string);
    virtual std::string getFile(std::string);
    std::string newkey();

    bool validateIP(std::string);
    bool validateMac(std::string);
    bool macIpReachLimit(std::string);

    virtual std::string getMacIpList(std::string);
    virtual bool editMacIp(std::string, std::string, std::string);
    virtual bool delMacIp(std::string);
    virtual bool newMacIp(std::string, std::string, std::string);
    bool isLocalhost(const std::string& targetHost);

    virtual void cleanup();

private:
    std::string getKeyExpire(std::string);
    std::string dbfile;
    int keyTimeout;
	int max_mac;
	int max_ip;
};
#endif
