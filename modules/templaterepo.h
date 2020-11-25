#ifndef __templaterepo_H__
#define __templaterepo_H__
#include "config.h"
#include "Socket.hpp"
#include <memory>

#include <Poco/Tuple.h>
#include <Poco/FileStream.h>
#include <Poco/Net/HTMLForm.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/MemoryStream.h>
#include <Poco/URI.h>
#include <Poco/Process.h>
#include <Poco/AutoPtr.h>
#include <Poco/Util/LayeredConfiguration.h>
#include <Poco/Util/XMLConfiguration.h>
#include <Poco/JSON/Object.h>
#include <Poco/FormattingChannel.h>

using Poco::Path;
using Poco::URI;
using Poco::Net::NameValueCollection;
using Poco::Net::HTTPRequest;
using Poco::Net::HTMLForm;
using Poco::MemoryInputStream;
using Poco::Process;
using Poco::JSON::Object;

class TemplateRepo
{
public:
    TemplateRepo();
    virtual ~TemplateRepo();
    virtual void handleRequest(std::weak_ptr<StreamSocket>, MemoryInputStream&, HTTPRequest&, SocketDisposition&);
    virtual std::string handleAdmin(std::string);
    virtual std::string getHTMLFile(std::string);
    virtual void doTemplateRepo(std::weak_ptr<StreamSocket>, MemoryInputStream&, HTTPRequest&);

    virtual void getInfo(std::weak_ptr<StreamSocket>);
    virtual void syncTemplates(std::weak_ptr<StreamSocket>, const Poco::Net::HTTPRequest& , Poco::MemoryInputStream&);
    virtual void downloadAllTemplates(std::weak_ptr<StreamSocket>);
    virtual std::string makeApiJson(std::string,
            bool anotherJson=false,
            bool yaml=false,
            bool showHead=true);
    virtual bool isTemplateRepoHelpUri(std::string);
    virtual void handleAPIHelp(const Poco::Net::HTTPRequest&,std::weak_ptr<StreamSocket>,Poco::MemoryInputStream&);
    virtual std::string zip_DIFF_FILE(Object::Ptr object);
    virtual std::string zip_ALL_FILE();
    virtual Object::Ptr JSON_FROM_FILE();
    virtual void createDirectory(std::string filePath);
	virtual void quickHttpRes(std::weak_ptr<StreamSocket> _socket,
			Poco::Net::HTTPResponse::HTTPStatus errorCode,
			const std::string msg="")
	{
		auto socket = _socket.lock();
		Poco::Net::HTTPResponse response;
		response.setContentType("charset=UTF-8");
		response.setContentLength(msg.size());
		response.setStatusAndReason(
				errorCode,
				msg.c_str());
		socket->send(response);
		socket->send(msg);
	}
	// USE in LOOLWSD's ClientDispatcher onDisconnect
	bool exit_application;

private:
    bool checkIP(std::string);
    bool checkMac(std::string);

    // Add for logging database
    std::string reqClientAddress;

    std::chrono::steady_clock::time_point startStamp;
    Poco::Util::XMLConfiguration *xml_config;
    std::string ConfigFile;
    // logger
    Poco::AutoPtr<Poco::FormattingChannel> channel;
    virtual void setLogPath();

    // fileserver function
    virtual void uploadFile(std::weak_ptr<StreamSocket>, MemoryInputStream&, HTTPRequest&);
    virtual void downloadFile(std::weak_ptr<StreamSocket>, MemoryInputStream&, HTTPRequest&);
    virtual void deleteFile(std::weak_ptr<StreamSocket>, MemoryInputStream&, HTTPRequest&);
    virtual void updateFile(std::weak_ptr<StreamSocket>, MemoryInputStream&, HTTPRequest&);
    virtual void saveInfo(std::weak_ptr<StreamSocket>, MemoryInputStream&, HTTPRequest&);
    std::string template_dir;

private:
    const std::string YAMLTEMPL = R"MULTILINE(
        swagger: '2.0'
        info:
          version: v1
          title: ODF Template Center API
          description: ''
        host: '%s'
        paths:
          /lool/templaterepo/list:
            get:
              responses:
                '200':
                  description: Success
                  schema:
                    type: object
                    properties:
                      Category:
                        type: array
                        items:
                          $ref: '#/definitions/Category'
          /lool/templaterepo/sync:
            post:
              consumes:
                - application/json
              parameters:
                - $ref : '#/parameters/Sync'
              responses:
                '200':
                  description: Success
                '401':
                  description: Json data error
        schemes:
          - http
          - https
        definitions:
          Category:
            type: object
            required:
              - uptime
              - endpt
              - cid
              - hide
              - extname
              - docname
            properties:
              uptime:
                type: string
                format: date-time
              endpt:
                type: string
              cid:
                type: string
              hide:
                type: string
              extname:
                type: string
              docname:
                type: string

        parameters:
          Sync:
            in: body
            name: body
            description: ''
            required: true
            schema:
              type: object
              properties:
                Category:
                  type: array
                  items:
                    $ref: '#/definitions/Category'

        )MULTILINE";
};

#endif
