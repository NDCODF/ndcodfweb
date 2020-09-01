#include "templaterepo_db.h"
#include "templaterepo.h"
#include "LOOLWSD.hpp"
#include "Log.hpp"
#include <string>
#include <iostream>

#include <Poco/Data/Session.h>
#include <Poco/Data/RecordSet.h>
#include <Poco/Data/SQLite/Connector.h>
#include <Poco/Data/Session.h>
#include <Poco/Util/XMLConfiguration.h>
#include <Poco/Timestamp.h>
#include <Poco/Data/Statement.h>
#include <Poco/Exception.h>
#include <Poco/UUIDGenerator.h>
#include <Poco/StringTokenizer.h>
#include <Poco/Net/NetworkInterface.h>
#include <Poco/Net/IPAddress.h>
#include <Poco/Net/DNS.h>

using Poco::Data::Statement;
using Poco::StringTokenizer;
using Poco::Data::RecordSet;
using namespace Poco::Data::Keywords;

TemplateRepoDB::TemplateRepoDB() {
	max_mac = 10;
	max_ip = 10;
    Poco::Data::SQLite::Connector::registerConnector();
    setDbPath();
    // 初始化 Database
    Poco::Data::Session session("SQLite", dbfile);
    Statement select(session);
    std::string init_sql = R"MULTILINE(
    CREATE TABLE IF NOT EXISTS keylist (uuid text not null, expires text not null, file text);
    CREATE UNIQUE INDEX IF NOT EXISTS uuid on keylist(uuid);
    CREATE TABLE IF NOT EXISTS maciplist (
        rec_id INTEGER PRIMARY KEY AUTOINCREMENT,
        macip text not null,
        ftype text not null, description text
    );
    CREATE UNIQUE INDEX IF NOT EXISTS macip on maciplist(macip);
        )MULTILINE";
    select << init_sql;
    while (!select.done())
    {
        select.execute();
        break;
    }
    select.reset(session);
    session.close();
}
TemplateRepoDB::~TemplateRepoDB() {
    Poco::Data::SQLite::Connector::unregisterConnector();
}

/// 從設定檔取得資料庫檔案位置名稱 & timeout
void TemplateRepoDB::setDbPath()
{
#if ENABLE_DEBUG
    dbfile = std::string(DEV_DIR) + "/runTimeData/templaterepo.sqlite";
#else
    auto convertConf = new Poco::Util::XMLConfiguration("/var/lib/ndcodfweb/templaterepo/templaterepo.xml");
    dbfile = convertConf->getString("template.db_path", "");
#endif
    std::cout<<"db: "<<dbfile<<std::endl;
}

/// found the key?
bool TemplateRepoDB::hasKey(const std::string key)
{
    return !getKeyExpire(key).empty();
}

/// key expired?
bool TemplateRepoDB::hasKeyExpired(const std::string key)
{
    std::string t0 = getKeyExpire(key);
    if (t0.empty())
        return true;

    Poco::Timestamp now;
    std::time_t t1 = now.epochTime();
    //std::cout<<"t1:"<<t1<<std::endl;

    return (t1 - std::atoi(t0.c_str())) > keyTimeout;
}

/// set filename for convert
void TemplateRepoDB::setFile(std::string key, std::string file)
{
    Poco::Data::Session session("SQLite", dbfile);
    Statement insert(session);
    insert << "UPDATE keylist SET file=? WHERE uuid=?",
                    use(file), use(key), now;
    insert.reset(session);
    session.close();
}

/// get convert filename
std::string TemplateRepoDB::getFile(std::string key)
{
    if (hasKeyExpired(key))
    {
        std::cout<<"expire"<<std::endl;
        return "";
    }
    Poco::Data::Session session("SQLite", dbfile);

    Statement select(session);
    std::string file;
    select << "SELECT file FROM keylist WHERE uuid=?",
                    into(file), use(key);
    while (!select.done())
    {
        select.execute();
    }
    select.reset(session);
    session.close();
    return file;
}

/// get expires for key
std::string TemplateRepoDB::getKeyExpire(std::string key)
{
    Poco::Data::Session session("SQLite", dbfile);

    Statement select(session);
    std::string expires;
    select << "SELECT expires FROM keylist WHERE uuid=?",
                    into(expires), use(key);
    while (!select.done())
    {
        select.execute();
    }
    select.reset(session);
    session.close();
    return expires;
}

// 列表：Mac address or IP address
std::string TemplateRepoDB::getMacIpList(std::string ftype)
{
    Poco::Data::Session session("SQLite", dbfile);

    int max = "mac" == ftype ? max_mac : max_ip;

    Statement select(session);
    std::string line;

    select << "SELECT rec_id, macip, description FROM maciplist " <<
              "WHERE ftype=? LIMIT ?",
                use(ftype), use(max);
    RecordSet rs(select);

    try
    {
        line = "{\"max\": \"" + std::to_string(max) + "\", \"list\": [";

        while (!select.done())
        {
            select.execute();
            bool more = rs.moveFirst();
            while (more)
            {
                std::string rec_id, macip, desc;

                try { rec_id = rs[0].convert<std::string>(); }
                catch (Poco::Exception& e) {
                    LOG_WRN("sql var convert failed: rec_id");
                }
                try { desc = rs[2].convert<std::string>(); }
                catch (Poco::Exception& e) {
                    LOG_WRN("sql var convert failed: desc");
                }
                try { macip = rs[1].convert<std::string>(); }
                catch (Poco::Exception& e) {
                    LOG_WRN("sql var convert failed: macip");
                }

                line += "{\"rec_id\":\"" + rec_id + "\",";
                line += "\"desc\":\"" + desc + "\",";
                line += "\"macip\":\"" + macip + "\"}";
                more = rs.moveNext();
                if (more)
                    line += ",";
            }
        }
        line += "]}";
    }
    catch (Poco::Exception& e)
    {
        LOG_ERR("sql executed failed: " << std::string(e.displayText()) <<
                ", SQL: " << select.toString());
        select.reset(session);
        session.close();
        return "[]";
    }
    select.reset(session);
    rs.reset(session);
    session.close();
    return line;
}

// 更新 mac/ip address
bool TemplateRepoDB::editMacIp(std::string rec_id,
                          std::string macip,
                          std::string desc)
{
    Poco::Data::Session session("SQLite", dbfile);
    Statement insert(session);
    try
    {
        insert <<
            "UPDATE maciplist SET macip=?, description=? WHERE rec_id=?",
            use(macip), use(desc), use(rec_id), now;
    }
    catch (Poco::Exception& e)
    {
		std::cout << "Error update mac/ip data: " << std::string(e.displayText()) << std::endl;
        insert.reset(session);
        session.close();
        return false;
    }
    insert.reset(session);
    session.close();
    return true;
}

/// 刪除 mac/ip address
bool TemplateRepoDB::delMacIp(std::string rec_id)
{
    Poco::Data::Session session("SQLite", dbfile);
    Statement insert(session);
    try
    {
        insert << "DELETE FROM maciplist WHERE rec_id=?",
                  use(rec_id), now;
    }
    catch (Poco::Exception& e)
    {
		std::cout << "sql executed failed: " << std::string(e.displayText()) <<
                ", SQL: " << insert.toString() << std::endl;
        insert.reset(session);
        session.close();
        return false;
    }
    insert.reset(session);
    session.close();
    return true;
}

/// 新增 mac/ip address
bool TemplateRepoDB::newMacIp(std::string macip,
                       std::string desc,
                       std::string ftype)
{
    Poco::Data::Session session("SQLite", dbfile);
    Statement insert(session);
    try
    {
        insert << "INSERT INTO maciplist " <<
                  "(macip, description, ftype) VALUES (?, ?, ?)",
                    use(macip), use(desc), use(ftype), now;
    }
    catch (Poco::Exception& e)
    {
		std::cout << "sql executed failed: " << std::string(e.displayText()) <<
                ", SQL: " << insert.toString();
        insert.reset(session);
        session.close();
        return false;
    }
    insert.reset(session);
    session.close();
    return true;
}

/// @TODO: duplicate uuid?
std::string TemplateRepoDB::newkey()
{
    //std::cout<<"newkey()"<<std::endl;
    Poco::UUIDGenerator& gen = Poco::UUIDGenerator::defaultGenerator();
    auto key = gen.create().toString();

    Poco::Data::Session session("SQLite", dbfile);
    Statement insert(session);
    insert << "INSERT INTO keylist (uuid, expires) VALUES (?, strftime('%s', 'now'))",
              use(key), now;
    insert.reset(session);
    session.close();

    return key;
}

/// mac/ip 總比數是否超過系統預設上限？
bool TemplateRepoDB::macIpReachLimit(std::string ftype)
{
    Poco::Data::Session session("SQLite", dbfile);

    int max = "mac" == ftype ? max_mac : max_ip;

    Statement select(session);
    int curMax;
    select << "SELECT COUNT(*) FROM maciplist WHERE ftype=?",
                    into(curMax), use(ftype);
    try
    {
        while (!select.done())
        {
            select.execute();
        }
    }
    catch (Poco::Exception& e)
    {
		std::cout << "log file read error: " << std::string(e.displayText());
        select.reset(session);
        session.close();
        return true;
    }

    select.reset(session);
    session.close();
    return max < curMax;
}

/*
 * 驗證 Mac
 * mac_addr=aaa
 * mac_addr=aaa,bbb,ccc
 */
bool TemplateRepoDB::validateMac(std::string macStr)
{
    if (macIpReachLimit("mac"))
    {
		std::cout << "mac 列表數量超過範圍!!!\n";
        return false;
    }

    const int tokenOpts = StringTokenizer::TOK_IGNORE_EMPTY |
                          StringTokenizer::TOK_TRIM;

    Poco::Data::Session session("SQLite", dbfile);

    Statement select(session);
    StringTokenizer tokens(macStr, ",", tokenOpts);
    for(size_t idx = 0; idx < tokens.count(); idx ++)
    {
        int count;
        auto mac = tokens[idx];
        std::cout<<"mac:"<<mac<<std::endl;
        select << "SELECT COUNT(*) FROM maciplist \
                    WHERE macip=? AND ftype='mac'",
                        into(count), use(mac);
        try
        {
            while (!select.done())
            {
                select.execute();
            }
        }
        catch (Poco::Exception& e)
        {
            std::cerr << e.displayText() << std::endl;
            select.reset(session);
            session.close();
            return false;
        }
        if (count > 0)
            return true;
    }
    select.reset(session);
    session.close();
    return false;
}

/*
 * 驗證 IP
 */
bool TemplateRepoDB::validateIP(std::string clientAddress)
{
    const int tokenOpts = StringTokenizer::TOK_IGNORE_EMPTY |
                          StringTokenizer::TOK_TRIM;

    if (macIpReachLimit("ip"))
    {
		std::cout << ("ip 列表數量超過範圍!!!") << std::endl;
        return false;
    }

    if (isLocalhost(clientAddress))
        return true;

    /*
     * Oiginal format = :ffff:127.0.0.1
     */
    StringTokenizer tokens(clientAddress, ":", tokenOpts);
    clientAddress = tokens[1];
    std::cout << clientAddress << std::endl;
    Poco::Data::Session session("SQLite", dbfile);
    Statement select(session);
    int count;
    select << "SELECT COUNT(*) FROM maciplist WHERE macip=? AND ftype='ip'",
                    into(count), use(clientAddress);
    try
    {
        while (!select.done())
        {
            select.execute();
        }
    }
    catch (Poco::Exception& e)
    {
        std::cerr << e.displayText() << std::endl;
        select.reset(session);
        session.close();
        return false;
    }
    std::cout << count << std::endl;
    select.reset(session);
    session.close();
    return count > 0;
}

/// @TODO: DUPLICATE with Util.cpp
bool TemplateRepoDB::isLocalhost(const std::string& targetHost)
{
    std::string targetAddress;
    try
    {
        targetAddress = Poco::Net::DNS::resolveOne(targetHost).toString();
    }
    catch (const Poco::Exception& exc)
    {
        //Log::warn("Poco::Net::DNS::resolveOne(\"" + targetHost + "\") failed: " + exc.displayText());
        try
        {
            targetAddress = Poco::Net::IPAddress(targetHost).toString();
        }
        catch (const Poco::Exception& exc1)
        {
            //Log::warn("Poco::Net::IPAddress(\"" + targetHost + "\") failed: " + exc1.displayText());
        }
    }

    Poco::Net::NetworkInterface::NetworkInterfaceList list = Poco::Net::NetworkInterface::list(true,true);
    for (auto& netif : list)
    {
        std::string address = netif.address().toString();
        address = address.substr(0, address.find('%', 0));
        if (address == targetAddress)
        {
            return true;
        }
    }

    return false;
}

/// @TODO: maybe no need
/// 定期清除資料
void TemplateRepoDB::cleanup()
{
    Poco::Data::Session session("SQLite", dbfile);
    Statement insert(session);
    insert << "delete from keylist where (strftime('%s', 'now')-expires) > 60*60",
              now;
    insert << "VACUUM FULL", now;
    insert.reset(session);
    session.close();
    std::cout<<"TemplateRepoDB::cleanup"<<std::endl;
}
