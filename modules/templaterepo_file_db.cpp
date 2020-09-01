#include <string>
#include <iostream>

#include "templaterepo_file_db.h"
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
#include <Poco/RegularExpression.h>


using Poco::Data::Statement;
using Poco::StringTokenizer;
using Poco::Data::RecordSet;
using namespace Poco::Data::Keywords;

FileDB::FileDB() {
    Poco::Data::SQLite::Connector::registerConnector();

    setDbPath();

    // 初始化 Database
    Poco::Data::Session session("SQLite", dbfile);
    Statement select(session);
    std::string init_sql = R"MULTILINE(
        CREATE TABLE IF NOT EXISTS repo_templates (
        rec_id int ,
        uid int ,
        cname text ,
        title TEXT,
        desc text ,
        endpt text UNIQUE,
        docname text ,
        extname text ,
        uptime text
        )
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
FileDB::~FileDB() {
    Poco::Data::SQLite::Connector::unregisterConnector();
}


void FileDB::setDbPath()
{
#if ENABLE_DEBUG
    dbfile = std::string(DEV_DIR) + "/runTimeData/templaterepo.sqlite";
#else
    auto previewConf = new Poco::Util::XMLConfiguration("/var/lib/ndcodfweb/templaterepo/templaterepo.xml");
    dbfile = previewConf->getString("template.db_path", "");
#endif
    std::cout<<"db: "<<dbfile<<std::endl;
}



/// @TODO: duplicate uuid?
std::string FileDB::newkey()
{
    Poco::UUIDGenerator& gen = Poco::UUIDGenerator::defaultGenerator();
    auto key = gen.create().toString();

    Poco::Data::Session session("SQLite", dbfile);
    Statement insert(session);
    insert << "INSERT INTO keylist (uuid, expires,file) VALUES (?, strftime('%s', 'now'), 'test')",
              use(key), now;
    while (!insert.done())
    {
        insert.execute();
    }
    insert.reset(session);
    session.close();

    return key ;
}

bool FileDB::setFile(Poco::Net::HTMLForm &form)
{
    Poco::Data::Session session("SQLite", dbfile);
    Statement select(session);
    std::string init_sql = "Insert into repo_templates values (?,?,?,?,?,?,?,?,?)";
    std::string rec_id  = form.get("rec_id", "");
    std::string uid     = form.get("uid", "");
    std::string cname   = form.get("cname", "");
    std::string title   = form.get("title", "");
    std::string desc    = form.get("desc", "");
    std::string endpt   = form.get("endpt", "");
    std::string docname = form.get("docname", "");
    std::string extname = form.get("extname", "");
    std::string uptime  = form.get("uptime", "");
    std::cout <<  "[db] endpt: " << endpt << std::endl;
    select << init_sql, use(rec_id),
                        use(uid),
                        use(cname),
                        use(title),
                        use(desc),
                        use(endpt),
                        use(docname),
                        use(extname),
                        use(uptime);
    try
    {
        while (!select.done())
        {
            select.execute();
            break;
        }
        select.reset(session);
        session.close();
        return true;
    }
    catch (std::exception &e)
    {
        std::cout << e.what() << std::endl;
        return false;
    }
}

void FileDB::delFile(std::string endpt)
{
    Poco::Data::Session session("SQLite", dbfile);
    Statement select(session);
    std::string del_sql = "Delete from repo_templates where endpt=?";
    select << del_sql, use(endpt);
    while (!select.done())
    {
        select.execute();
        break;
    }
    select.reset(session);
    session.close();
}

std::string FileDB::getFile(std::string endpt)
{
    Poco::Data::Session session("SQLite", dbfile);
    std::string extname = "";
    Statement select(session);
    std::string get_sql = "select extname from repo_templates where endpt=?";
    select << get_sql, use(endpt), into(extname);
    while (!select.done())
    {
        select.execute();
        break;
    }
    select.reset(session);
    session.close();
    return endpt + "." + extname;
}

std::string FileDB::getInfo()
{
    Poco::Data::Session session("SQLite", dbfile);
    std::string extname = "";
    Statement select(session);
    std::string get_sql = "select extname, cname, uptime, docname, endpt from repo_templates";
    select << get_sql;
    RecordSet rs(select);
    std::map<std::string, std::vector<std::string>> data;
	while (!select.done())
	{
		select.execute();
		bool more = rs.moveFirst();
		while (more)
		{
			std::string line = R"MULTILINE(
                    {
                        "extname"   : "%s",
                        "cname"     : "%s",
                        "uptime"    : "%s",
                        "docname"   : "%s",
                        "endpt"      : "%s"
                    }
                    )MULTILINE";
			std::string cname = "";
			for(unsigned int it = 0; it < rs.columnCount(); it++)
			{
				Poco::RegularExpression string_rule("\%s");
				string_rule.subst(line, rs[it].convert<std::string>());
				if(it==1)
				{
					cname = rs[it].convert<std::string>();
					std::cout << "cname =>" << cname <<"\n";
				}
			}
			more = rs.moveNext();
			if(data.find(cname) != data.end())
			{
				data[cname].push_back(line);
			}
			else
			{
				data[cname] = std::vector<std::string>();
				data[cname].push_back(line);

			}
		}
	}
	std::string result = "{";
	for(auto it=data.begin();it!=data.end();)
	{
		result += "\"" + it->first + "\":[";
		for(auto jt = it->second.begin(); jt!= it->second.end() ;)
		{
			result += *jt;
            jt++;
            if(jt != it->second.end())
                result+=",";
		}
		it++;
		if (it!=data.end())
			result += "],";
		else
			result += "]";
	}
	result += "}";
    select.reset(session);
    session.close();
    return result;
}
