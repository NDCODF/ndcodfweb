#include "templaterepo.h"
#include "templaterepo_file_db.h"
#include "templaterepo_db.h"
#include <memory>
#include <fstream>
#include "JsonUtil.hpp"

#include <sys/wait.h>

#define LOK_USE_UNSTABLE_API
#include <LibreOfficeKit/LibreOfficeKitEnums.h>
#include <LibreOfficeKit/LibreOfficeKit.hxx>

#include <Poco/RegularExpression.h>
#include <Poco/Net/HTMLForm.h>
#include <Poco/Net/PartHandler.h>
#include <Poco/Delegate.h>
#include <Poco/Zip/Compress.h>
#include <Poco/Zip/Decompress.h>
#include <Poco/Glob.h>
#include <Poco/TemporaryFile.h>
#include <Poco/Format.h>
#include <Poco/StringTokenizer.h>
#include <Poco/StreamCopier.h>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Parser.h>
#include <Poco/Util/Application.h>
#include <Poco/FileChannel.h>
#include <Poco/PatternFormatter.h>

using Poco::Net::HTMLForm;
using Poco::Net::MessageHeader;
using Poco::Net::PartHandler;
using Poco::Net::HTTPResponse;
using Poco::RegularExpression;
using Poco::Zip::Compress;
using Poco::Zip::Decompress;
using Poco::Path;
using Poco::File;
using Poco::TemporaryFile;
using Poco::StreamCopier;
using Poco::StringTokenizer;
using Poco::Dynamic::Var;
using Poco::JSON::Object;
using Poco::DynamicStruct;
using Poco::JSON::Array;
using Poco::Util::Application;

using Poco::FileChannel;
using Poco::PatternFormatter;

static Poco::Logger& logger()
{
    return Application::instance().logger();
}

class ManageFilePartHandler : public PartHandler
{
    public:
        NameValueCollection vars;  /// post filenames
    private:
        std::string _filename;  /// current post filename

    public:
        ManageFilePartHandler(std::string filename)
        :_filename(filename)
        {
        }

        virtual void handlePart(const MessageHeader& header,
                std::istream& stream) override
        {
            // Extract filename and put it to a temporary directory.
            std::string disp;
            NameValueCollection params;
            if (header.has("Content-Disposition"))
            {
                std::string cd = header.get("Content-Disposition");
                MessageHeader::splitParameters(cd, disp, params);
            }

            if (!params.has("filename"))
                return;
            if (params.get("filename").empty())
                return;

            auto tempPath = Path::forDirectory(
                    TemporaryFile::tempName() + "/");
            File(tempPath).createDirectories();
            // Prevent user inputting anything funny here.
            // A "filename" should always be a filename, not a path
            const Path filenameParam(params.get("filename"));
            tempPath.setFileName(filenameParam.getFileName());
            _filename = tempPath.toString();

            // Copy the stream to _filename.
            std::ofstream fileStream;
            fileStream.open(_filename);
            StreamCopier::copyStream(stream, fileStream);
            fileStream.close();

            vars.add("name", _filename);
            fprintf(stderr, "handle part, %s\n", _filename.c_str());
        }
};

TemplateRepo::TemplateRepo()
{
#if ENABLE_DEBUG
    ConfigFile = std::string(DEV_DIR) + "/templaterepo.xml";
#else
    ConfigFile = "/var/lib/ndcodfweb/templaterepo/templaterepo.xml";
#endif
    xml_config = new Poco::Util::XMLConfiguration(ConfigFile);
    startStamp = std::chrono::steady_clock::now();
    setLogPath();
    Application::instance().logger().setChannel(channel);
}

TemplateRepo::~TemplateRepo()
{}

/// init. logger
/// 設定 log 檔路徑後直接 init.
void TemplateRepo::setLogPath()
{
    Poco::AutoPtr<FileChannel> fileChannel(new FileChannel);
    std::string logFile;
#if ENABLE_DEBUG
    logFile = "/tmp/templaterepo.log";
#else
    logFile = xml_config->getString("logging.log_file", "/var/log/templaterepo.log");
#endif
    fileChannel->setProperty("path", logFile);
    fileChannel->setProperty("archive", "timestamp");
    fileChannel->setProperty("rotation", "weekly");
    fileChannel->setProperty("compress", "true");
    Poco::AutoPtr<PatternFormatter> patternFormatter(new PatternFormatter());
    patternFormatter->setProperty("pattern","%Y/%m/%d %L%H:%M:%S: %t");
    channel = new Poco::FormattingChannel(patternFormatter, fileChannel);
}

bool TemplateRepo::checkIP(std::string clientAddress)
{
    TemplateRepoDB *tr_db = new TemplateRepoDB();
    if (!tr_db->validateIP(clientAddress))
    {
		return false;
    }
    return true;
}

bool TemplateRepo::checkMac(std::string macAddr)
{

    TemplateRepoDB *tr_db = new TemplateRepoDB();
	if(macAddr.empty() || !tr_db->validateMac(macAddr))
	{
		return false;
	}
	return true;
}
/*
 * FileServer Implement
 */

void TemplateRepo::uploadFile(std::weak_ptr<StreamSocket> _socket, 
        MemoryInputStream& message, 
        HTTPRequest& request)
{
    std::cout << "upload file\n" ;
    ManageFilePartHandler handler(std::string(""));
    HTMLForm form(request, message, handler);
    auto socket = _socket.lock();
    if(!checkIP(socket->clientAddress()))
    {
        quickHttpRes(_socket, HTTPResponse::HTTP_SERVICE_UNAVAILABLE,"Not Auth");
        return;
    }
    if (handler.vars.has("name"))
    {
        auto filedb = FileDB();
        if (filedb.setFile(form))
        {
            std::string tempPath = handler.vars.get("name");
            Poco::File tempFile(tempPath);
            std::string filePath = template_dir + form.get("endpt") + "." + form.get("extname");
            tempFile.copyTo(filePath);
            tempFile.remove();
            quickHttpRes(_socket, HTTPResponse::HTTP_OK, "Upload Success");
            logger().notice("Upload Success as: " + filePath);
        }
        else
        {
            quickHttpRes(_socket, HTTPResponse::HTTP_SERVICE_UNAVAILABLE, "endpt already exists");
        }
    }
    else
    {
        quickHttpRes(_socket, HTTPResponse::HTTP_SERVICE_UNAVAILABLE, "fail to save file to server");
    }
}

void TemplateRepo::downloadFile(std::weak_ptr<StreamSocket> _socket, 
        MemoryInputStream& message, 
        HTTPRequest& request)
{
    HTMLForm form(request, message);
	std::string macAddr="";
    if (form.has("mac_addr") && !form.get("mac_addr").empty())
    {
        macAddr = form.get("mac_addr");
    }
    auto socket = _socket.lock();
    if(!checkMac(macAddr))
    {
        quickHttpRes(_socket, HTTPResponse::HTTP_SERVICE_UNAVAILABLE,"Not Auth");
        return;
    }
    std::string endpt = form.get("endpt", "");
    std::string extname = form.get("extname", "");
    if (endpt.empty())
    {
        quickHttpRes(_socket, HTTPResponse::HTTP_SERVICE_UNAVAILABLE, "No file specify for download");
        return;
    }
    FileDB filedb;
    std::string filePath = template_dir + filedb.getFile(endpt);
    Poco::File targetFile(filePath);
    if (targetFile.exists())
    {
        HTTPResponse response;

        response.set("Access-Control-Allow-Origin", "*");
        response.set("Access-Control-Allow-Methods", "POST, OPTIONS");
        response.set("Access-Control-Allow-Headers",
                "Origin, X-Requested-With, Content-Type, Accept");
        response.set("Content-Disposition",
                "attachment; filename=\"" + endpt + "."+ extname+"\"");
        std::string mimeType = "application/octet-stream";
        HttpHelper::sendFile(socket, filePath, mimeType, response);
        logger().notice("File Send successfully to: " + request.getHost());
    }
    else
        quickHttpRes(_socket, HTTPResponse::HTTP_SERVICE_UNAVAILABLE, "No such file, please confirm");
}

void TemplateRepo::deleteFile(std::weak_ptr<StreamSocket> _socket, 
        MemoryInputStream& message, 
        HTTPRequest& request)
{
    HTMLForm form(request, message);
    auto socket = _socket.lock();
    if(!checkIP(socket->clientAddress()))
    {
        quickHttpRes(_socket, HTTPResponse::HTTP_SERVICE_UNAVAILABLE,"Not Auth");
        return;
    }
    std::string endpt = form.get("endpt", "");
    std::string extname = form.get("extname", "");
    if (endpt == "")
    {
        quickHttpRes(_socket, HTTPResponse::HTTP_SERVICE_UNAVAILABLE, "No endpt provide");
        return;
    }
    Poco::File targetFile(template_dir + endpt + "." + extname);
    if (targetFile.exists())
    {
        targetFile.remove();
        auto filedb = FileDB();
        filedb.delFile(endpt);
        quickHttpRes(_socket, HTTPResponse::HTTP_OK, "delete success");
        logger().notice("File delete. From: " + request.getHost());
    }
    else
    {
        quickHttpRes(_socket, HTTPResponse::HTTP_SERVICE_UNAVAILABLE, "File not exist");
    }
}

void TemplateRepo::updateFile(std::weak_ptr<StreamSocket> _socket, 
        MemoryInputStream& message, 
        HTTPRequest& request)
{
    ManageFilePartHandler handler(std::string(""));
    HTMLForm form(request, message, handler);
    auto socket = _socket.lock();
    if(!checkIP(socket->clientAddress()))
    {
        quickHttpRes(_socket, HTTPResponse::HTTP_SERVICE_UNAVAILABLE,"Not Auth");
        return;
    }
    if (handler.vars.has("name"))
    {
        auto filedb = FileDB();
        std::string endpt = form.get("endpt", "");
        std::string extname = form.get("extname", "");
        std::string oldFile = filedb.getFile(endpt);
        std::cout << "oldFile: " << oldFile << std::endl;
        Poco::File targetFile(template_dir + oldFile);
        if (targetFile.exists())
        {
            targetFile.remove();
            filedb.delFile(endpt);
            filedb.setFile(form);
            Poco::File tempFile(handler.vars.get("name"));
            tempFile.copyTo(template_dir + endpt + "." + extname);
            tempFile.remove();
            quickHttpRes(_socket, HTTPResponse::HTTP_OK, "Update success");
            logger().notice("File" + endpt +" Update, from " + request.getHost());
        }
        else
        {
            quickHttpRes(_socket, HTTPResponse::HTTP_SERVICE_UNAVAILABLE, "No such file, please contact admin");
        }
    }
    else
    {
        quickHttpRes(_socket, HTTPResponse::HTTP_SERVICE_UNAVAILABLE,"No file provide");
    }
}

void TemplateRepo::saveInfo(std::weak_ptr<StreamSocket> _socket, 
        MemoryInputStream& message, 
        HTTPRequest& request)
{
    HTMLForm form(request, message);
    auto socket = _socket.lock();
    if(!checkIP(socket->clientAddress()))
    {
        quickHttpRes(_socket, HTTPResponse::HTTP_SERVICE_UNAVAILABLE,"Not Auth");
        return;
    }
    std::string data = form.get("data", "");
    if (! data.empty())
    {
        std::cout << "data: " << data << std::endl;
        std::ofstream myfile;
        myfile.open (template_dir + "myfile.json");
        myfile << data;
        myfile.close();
        quickHttpRes(_socket, HTTPResponse::HTTP_OK, "Save data success");
        logger().notice("Save info file as: " + template_dir + "myfile.json");
    }
    else
    {
        quickHttpRes(_socket, HTTPResponse::HTTP_SERVICE_UNAVAILABLE, "No Data Provide");
    }
}
// END OF FileServer Function


std::string TemplateRepo::makeApiJson(std::string URL,
        bool anotherJson,
        bool yaml,
        bool showHead)
{
    if(anotherJson && yaml && showHead)
        std::cout << URL << std::endl;
    return URL;
}

void TemplateRepo::handleAPIHelp(const Poco::Net::HTTPRequest& request,
        std::weak_ptr<StreamSocket> _socket, Poco::MemoryInputStream& message)
{
    HTMLForm form(request, message);
    auto socket = _socket.lock();
    if(!checkIP(socket->clientAddress()))
    {
        quickHttpRes(_socket, HTTPResponse::HTTP_SERVICE_UNAVAILABLE,"Not Auth");
        return;
    }

    const auto& app = Poco::Util::Application::instance();
    const auto ServerName = app.config().getString("server_name");
#if ENABLE_DEBUG
    std::cout << "Skip checking the server_name...";
#else
    // 檢查是否有填 server_name << restful client 依據此作為呼叫之 url
    // url 帶入 TEMPL 之 "host"
    if (app.config().getString("server_name").empty())
    {
        HTTPResponse response;
        response.setStatusAndReason(
                HTTPResponse::HTTP_SERVICE_UNAVAILABLE,
                "config server_name not set");
        response.setContentLength(0);
        socket->send(response);
        return;
    }
#endif

    auto mediaType = "text/plain";

    std::string result = Poco::format(YAMLTEMPL, ServerName);
    // TODO: Refactor this to some common handler.
    std::ostringstream oss;
    oss << "HTTP/1.1 200 OK\r\n"
        << "Last-Modified: " << Poco::DateTimeFormatter::format(Poco::Timestamp(), Poco::DateTimeFormat::HTTP_FORMAT) << "\r\n"
        << "Access-Control-Allow-Origin: *" << "\r\n"
        << "User-Agent: " << WOPI_AGENT_STRING << "\r\n"
        << "Content-Length: " << result.size() << "\r\n"
        << "Content-Type: " << mediaType << "\r\n"
        << "X-Content-Type-Options: nosniff\r\n"
        << "\r\n"
        << result;

    socket->send(oss.str());
}

bool TemplateRepo::isTemplateRepoHelpUri(std::string URL)
{
    if(URL == "/lool/templaterepo/api")
        return true;
    else
        return false;
}


void TemplateRepo::getInfo(std::weak_ptr<StreamSocket> _socket)
{
	auto filedb = FileDB();
	std::string result = filedb.getInfo();
	quickHttpRes(_socket, HTTPResponse::HTTP_OK,result);
}

void TemplateRepo::downloadAllTemplates(std::weak_ptr<StreamSocket> _socket)
{
    std::string zipFilePath = zip_ALL_FILE();
    HTTPResponse response;
    auto socket = _socket.lock();
    std::string mimeType = "application/zip";

    response.set("Access-Control-Allow-Origin", "*");
    response.set("Access-Control-Allow-Methods", "GET, OPTIONS");
    response.set("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept");
    HttpHelper::sendFile(socket, zipFilePath, mimeType, response);
}

void TemplateRepo::syncTemplates(std::weak_ptr<StreamSocket> _socket,
        const Poco::Net::HTTPRequest& request,
        Poco::MemoryInputStream& message)
{
    Object::Ptr object;

    /*
     * Swagger's CORS would send OPTIONS first to check if the server allow CROS, So Check First OPTIONS and allow 
     */
    if (request.getMethod() == HTTPRequest::HTTP_OPTIONS)
    {
        std::ostringstream oss;
        oss << "HTTP/1.1 200 OK\r\n"
            << "Last-Modified: " << Poco::DateTimeFormatter::format(Poco::Timestamp(), Poco::DateTimeFormat::HTTP_FORMAT) << "\r\n"
            << "Access-Control-Allow-Origin: *" << "\r\n"
            << "Access-Control-Allow-Headers: Origin, X-Requested-With, Content-Type, Accept" << "\r\n"
            << "User-Agent: " << WOPI_AGENT_STRING << "\r\n"
            << "Content-Type: application/json; charset=utf-8\r\n"
            << "X-Content-Type-Options: nosniff\r\n"
            << "\r\n";
        auto socket = _socket.lock();
        socket->send(oss.str());
        return;
    }
    HTMLForm form(request, message);
	std::string macAddr = "";
    if (form.has("mac_addr") && !form.get("mac_addr").empty())
    {
        macAddr = form.get("mac_addr");
    }
    if(!checkMac(macAddr))
    {
        quickHttpRes(_socket, HTTPResponse::HTTP_SERVICE_UNAVAILABLE,"Not Auth");
        return;
    }
	std::string jstr = "";
    if (form.has("data") && !form.get("data").empty())
    {
        jstr = form.get("data");
    }
	else
	{
        quickHttpRes(_socket, HTTPResponse::HTTP_SERVICE_UNAVAILABLE,"No data provide");
        return;
	}
	Poco::JSON::Parser jparser;
	Var result;

	// Parse data to PocoJSON
	try{
		result = jparser.parse(jstr);
		object = result.extract<Object::Ptr>();
	}
	catch (Poco::Exception& e)
	{
		std::cerr << e.displayText() << std::endl;
		std::string rrr = "JSON Error\n";
		std::ostringstream oss;
		oss << "HTTP/1.1 401 JSON ERROR\r\n"
			<< "Last-Modified: " << Poco::DateTimeFormatter::format(Poco::Timestamp(), Poco::DateTimeFormat::HTTP_FORMAT) << "\r\n"
			<< "Access-Control-Allow-Origin: *" << "\r\n"
			<< "User-Agent: " << WOPI_AGENT_STRING << "\r\n"
			<< "Content-Length: " << rrr.size() << "\r\n"
			<< "Content-Type: application/json; charset=utf-8\r\n"
			<< "X-Content-Type-Options: nosniff\r\n"
			<< "\r\n"
			<< rrr;
		auto socket = _socket.lock();
		socket->send(oss.str());
		return;
	}
    auto socket = _socket.lock();

    std::string zipFilePath = zip_DIFF_FILE(object);
    std::cout << zipFilePath << std::endl;
    HTTPResponse response;
    std::string mimeType = "application/octet-stream";

    response.set("Access-Control-Allow-Origin", "*");
    response.set("Access-Control-Allow-Methods", "POST, OPTIONS");
    response.set("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept");
    response.set("Content-Disposition", "attachment; filename=\"sync.zip\"");
    HttpHelper::sendFile(socket, zipFilePath, mimeType, response);
    logger().notice("範本檔案傳送完成, 請求來自 " + request.getHost());
}

void TemplateRepo::createDirectory(std::string filePath)
{
    Poco::File dir(Poco::Path(filePath, Poco::Path::PATH_UNIX));
    dir.createDirectories();
}

Object::Ptr TemplateRepo::JSON_FROM_FILE()
{
    std::ifstream inFile;
    std::string line;
    inFile.open(template_dir + "myfile.json");
    while(std::getline(inFile, line))
    {
        std::cout << line << "\n";
    }
    Poco::JSON::Parser parser;
    Var result = parser.parse(line);
    Object::Ptr object;
    object = result.extract<Object::Ptr>();
    return object;
}

std::string TemplateRepo::zip_ALL_FILE()
{
    //Create temp dir
    //
    //
    std::string templatePath = template_dir;

    std::string extra2;
    extra2 = TemporaryFile::tempName();
    createDirectory(extra2);
    Object::Ptr object;
    object = JSON_FROM_FILE();
    for(auto it = object->begin();it!=object->end();it++){
        createDirectory(extra2 + "/" + it->first);
        Array::Ptr templArray = object->getArray(it->first);
        std::string extraPath = extra2 + "/" + it->first + "/";
        for(auto tp = templArray->begin();tp!=templArray->end();tp++)
        {
            Object::Ptr oData = tp->extract<Object::Ptr>();
            std::string endpt = oData->getValue<std::string>("endpt");
            std::string docname = oData->getValue<std::string>("docname");
            std::string extname = oData->getValue<std::string>("extname");
            std::string oFilePath = templatePath + endpt + "." + extname;
            std::string nFilePath = extra2 + "/" + it->first + "/" + docname + "." + extname;
            std::string oFileName = endpt + "." + extname;
            std::string nFileName = docname + "." + extname;
            Poco::File oFile(templatePath+endpt + "." + extname);
            oFile.copyTo(extraPath);
            Poco::File nFile(extraPath + oFileName);
            nFile.renameTo(extraPath + nFileName);
        }

    }

    //zip the dir
    const auto zip2 = extra2 + ".zip";

    std::ofstream out(zip2, std::ios::binary);
    Compress c(out, true);

    c.addRecursive(extra2);
    c.close();
    return zip2;
}


std::string TemplateRepo::zip_DIFF_FILE(Object::Ptr object)
{
    //Create temp dir
    std::string templatePath = template_dir ;

    std::string extra2;
    extra2 = TemporaryFile::tempName();
    createDirectory(extra2);
    for(auto it = object->begin();it!=object->end();it++){
        createDirectory(extra2 + "/" + it->first);
        Array::Ptr templArray = object->getArray(it->first);
        std::string extraPath = extra2 + "/" + it->first + "/";
        for(auto tp = templArray->begin();tp!=templArray->end();tp++)
        {
            Object::Ptr oData = tp->extract<Object::Ptr>();
            std::string endpt = oData->getValue<std::string>("endpt");
            std::string docname = oData->getValue<std::string>("docname");
            std::string extname = oData->getValue<std::string>("extname");
            std::string oFilePath = templatePath + endpt + "." + extname;
            std::string nFilePath = extra2 + "/" + it->first + "/" + docname + "." + extname;
            std::string oFileName = endpt + "." + extname;
            std::string nFileName = docname + "." + extname;
            Poco::File oFile(templatePath+endpt + "." + extname);
            oFile.copyTo(extraPath);
            Poco::File nFile(extraPath + oFileName);
            nFile.renameTo(extraPath + nFileName);
        }

    }

    //zip the dir
    const auto zip2 = extra2 + ".zip";

    std::ofstream out(zip2, std::ios::binary);
    Compress c(out, true);

    c.addRecursive(extra2);
    c.close();
    return zip2;
}


void TemplateRepo::handleRequest(std::weak_ptr<StreamSocket> _socket,
        MemoryInputStream& message,
        HTTPRequest& request,
        SocketDisposition& disposition)
{
	exit_application = false;
    Process::PID pid = fork();
    if (pid < 0)
    {
        quickHttpRes(_socket, HTTPResponse::HTTP_SERVICE_UNAVAILABLE,
                "error loading templaterepo");
    }
    else if (pid == 0)
    {
        try{
            if (false)
                disposition.setClosed();
            doTemplateRepo(_socket, message, request);
        }
        catch(const std::exception &e){
            quickHttpRes(_socket, HTTPResponse::HTTP_SERVICE_UNAVAILABLE, "Please Contanct Adminmanager");
            std::cout << e.what() << std::endl;
            logger().notice("[Exception]" + std::string(e.what()));
        }
        try
        {
            auto socket = _socket.lock();
            socket->shutdown();
        }
        catch(const std::exception &e)
        {
            std::cout<< "Force shutdown socket in module\n";
        }
        exit_application = true;
    }
    else
    {
        std::cout << "call from parent" << std::endl;
    }
}


void TemplateRepo::doTemplateRepo(std::weak_ptr<StreamSocket> _socket,
        MemoryInputStream& message,
        HTTPRequest& request)
{
    std::string URI = request.getURI();
    std::string method = request.getMethod();
    auto socket = _socket.lock();

#if ENABLE_DEBUG
    template_dir = std::string(DEV_DIR) + "/runTimeData/templaterepo/templates/";
#else
    template_dir = xml_config->getString("template.dir_path", "");
#endif
    auto checkTemplateDir = Poco::File(template_dir);
    if(!checkTemplateDir.exists())
    {
        checkTemplateDir.createDirectories();
    }
    std::cout << "template_dir: " << template_dir << std::endl;
    HTTPResponse response;

    response.set("Access-Control-Allow-Origin", "*");
    response.set("Access-Control-Allow-Methods", "POST, OPTIONS");
    response.set("Access-Control-Allow-Headers",
            "Origin, X-Requested-With, Content-Type, Accept");

    if ( method == HTTPRequest::HTTP_GET &&
            URI == "/lool/templaterepo/list")
    {
        getInfo(socket);
        logger().notice("用戶端 " + request.getHost() + " 同步範本資訊");
    }
    else if ((method == HTTPRequest::HTTP_POST ||
                method == HTTPRequest::HTTP_OPTIONS) &&
            URI == "/lool/templaterepo/sync")
    {
        syncTemplates(socket, request, message);
    }
    else if (method == HTTPRequest::HTTP_GET &&
            isTemplateRepoHelpUri(URI))
    {  // /lool/tempalterepo/api
        handleAPIHelp(request, socket, message);
    }
    else if (method == HTTPRequest::HTTP_POST &&  URI == "/lool/templaterepo/saveinfo")
    {
        saveInfo(_socket, message, request);
    }
    else if (method == HTTPRequest::HTTP_POST &&  URI == "/lool/templaterepo/upload")
    {
        uploadFile(_socket, message, request);
    }
    else if (method == HTTPRequest::HTTP_POST &&  URI == "/lool/templaterepo/delete")
    {
        deleteFile(_socket, message, request);
    }
    else if (method == HTTPRequest::HTTP_POST &&  URI == "/lool/templaterepo/update")
    {
        updateFile(_socket, message, request);
    }
    else if (method == HTTPRequest::HTTP_POST &&  URI == "/lool/templaterepo/download")
    {
        downloadFile(_socket, message, request);
    }
    else
    {
        quickHttpRes(_socket, HTTPResponse::HTTP_SERVICE_UNAVAILABLE,"No such API");
        std::cout << "Else Api route\n";
    }
}

std::string TemplateRepo::handleAdmin(std::string command)
{
    /*
     *command format: module <modulename> <action> <data in this format: x,y,z ....>
     */
    auto tokenOpts = StringTokenizer::TOK_IGNORE_EMPTY | StringTokenizer::TOK_TRIM;
    StringTokenizer tokens(command, " ", tokenOpts);
    std::string result = "Module Loss return";
    std::string action = tokens[2];
    std::string dataString = tokens.count() >= 4 ? tokens[3] : "";

    if(action == "getLog")
    {
        std::cout << "getLog\n";
        result = "getLog OK";
    }
    else if(action == "mac_list")
    {
        TemplateRepoDB *cdb = new TemplateRepoDB();
        result = action + " "  + cdb->getMacIpList("mac");
    }
    else if(action == "ip_list")
    {
        TemplateRepoDB *cdb = new TemplateRepoDB();
        result = action + " " + cdb->getMacIpList("ip");
    }
    else if(action == "add_mac_data" || action == "add_ip_data")
    {
        StringTokenizer data(dataString, ",", tokenOpts);
        result = action + " false";
        if(data[0] != "")
        {
            std::string desc = "";
            std::string sourceType = action == "add_mac_data" ? "mac" : "ip";
            if (data.count() >= 2)
                desc = data[1];
            TemplateRepoDB *cdb = new TemplateRepoDB();
			if(!cdb->macIpReachLimit(sourceType))
			{
				if(cdb->newMacIp(data[0], desc, sourceType))
					result = action + " true";
			}
        }
    }
    else if(action == "set_ip" || action == "set_mac")
    {
        StringTokenizer data(dataString, ",", tokenOpts);
        std::string desc = "";
        result = action + " false";
        if (data.count() >=2 && data[0] != "" && data[1] !="")
        {
            TemplateRepoDB *cdb = new TemplateRepoDB();
            if (data.count()>=3)
                desc = data[2];
            if(cdb->editMacIp(data[0], data[1], desc))
                result = action + " true";
        }
    }
    else if(action == "rm_mac_data" || action == "rm_ip_data")
    {
        TemplateRepoDB *cdb = new TemplateRepoDB();
        result = action + " false";
        if(cdb->delMacIp(dataString))
            result = action + " true";
    }
    else
        result = "No such command for module " + tokens[1];

    //std::cout << tokens[2] << " : " << result << std::endl;
    return result;
}

extern "C" TemplateRepo* create_object()
{
    return new TemplateRepo;
}

extern "C" void destroy_object(TemplateRepo* templateRepo)
{
    delete templateRepo;
}
