/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/*
 * The main entry point for the LibreOfficeKit process serving
 * a document editing session.
 */

#include <config.h>

#include <dlfcn.h>
#ifdef __linux
#include <ftw.h>
#include <sys/capability.h>
#include <sys/sysmacros.h>
#endif
#include <unistd.h>
#include <utime.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <atomic>
#include <cassert>
#include <climits>
#include <condition_variable>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <sstream>
#include <thread>

#define LOK_USE_UNSTABLE_API
#include <LibreOfficeKit/LibreOfficeKitInit.h>
#include <LibreOfficeKit/LibreOfficeKit.hxx>

#include <Poco/Exception.h>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Parser.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/NetException.h>
#include <Poco/Net/Socket.h>
#include <Poco/Process.h>
#include <Poco/Runnable.h>
#include <Poco/StringTokenizer.h>
#include <Poco/Thread.h>
#include <Poco/URI.h>
#include <Poco/Util/Application.h>

#include "ChildSession.hpp"
#include <Common.hpp>
#include <IoUtil.hpp>
#include "KitHelper.hpp"
#include "Kit.hpp"
#include <Protocol.hpp>
#include <Log.hpp>
#include <Png.hpp>
#include <Rectangle.hpp>
#include <TileDesc.hpp>
#include <Unit.hpp>
#include <UserMessages.hpp>
#include <Util.hpp>
#include "Delta.hpp"
#include "Watermark.hpp"

#ifndef MOBILEAPP
#include <common/SigUtil.hpp>
#include <common/Seccomp.hpp>
#endif

#ifdef FUZZER
#include <kit/DummyLibreOfficeKit.hpp>
#include <wsd/LOOLWSD.hpp>
#endif

#ifdef MOBILEAPP
#include "LOOLWSD.hpp"
#endif

#ifdef IOS
#include "ios.h"
#endif

#define LIB_SOFFICEAPP  "lib" "sofficeapp" ".so"
#define LIB_MERGED      "lib" "mergedlo" ".so"

using Poco::Exception;
using Poco::File;
using Poco::JSON::Array;
using Poco::JSON::Object;
using Poco::JSON::Parser;
using Poco::Runnable;
using Poco::StringTokenizer;
using Poco::Thread;
using Poco::Timestamp;
using Poco::URI;
using Poco::Util::Application;

#ifndef BUILDING_TESTS
using Poco::Path;
using Poco::Process;
#endif

using namespace LOOLProtocol;
using std::size_t;

// We only host a single document in our lifetime.
class Document;
static std::shared_ptr<Document> document;
#ifndef BUILDING_TESTS
static bool AnonymizeFilenames = false;
static bool AnonymizeUsernames = false;
static std::string ObfuscatedFileId;
#endif

#if ENABLE_DEBUG
#  define ADD_DEBUG_RENDERID(s) ((s)+ " renderid=" + Util::UniqueId())
#else
#  define ADD_DEBUG_RENDERID(s) (s)
#endif

#ifndef MOBILEAPP

static LokHookFunction2* initFunction = nullptr;

namespace
{
#ifndef BUILDING_TESTS
    enum class LinkOrCopyType { All, LO, NoUsr };
    LinkOrCopyType linkOrCopyType;
    std::string sourceForLinkOrCopy;
    Path destinationForLinkOrCopy;
    std::chrono::time_point<std::chrono::steady_clock> linkOrCopyStartTime;
    bool linkOrCopyVerboseLogging = false;
    unsigned slowLinkOrCopyLimitInSecs = 10; // after this much seconds, start spamming the logs

    bool shouldCopyDir(const char *path)
    {
        switch (linkOrCopyType)
        {
        case LinkOrCopyType::NoUsr:
            // bind mounted.
            return strcmp(path,"usr") != 0;
        case LinkOrCopyType::LO:
            return
                strcmp(path, "program/wizards") != 0 &&
                strcmp(path, "sdk") != 0 &&
                //strcmp(path, "share/basic") != 0 &&
                strcmp(path, "share/Scripts/java") != 0 &&
                strcmp(path, "share/Scripts/javascript") != 0 &&
                strcmp(path, "share/config/wizard") != 0;
        default: // LinkOrCopyType::All
            return true;
        }
    }

    bool shouldLinkFile(const char *path)
    {
        switch (linkOrCopyType)
        {
        case LinkOrCopyType::LO:
        {
            const char *dot = strrchr(path, '.');
            if (!dot)
                return true;

            if (!strcmp(dot, ".dbg") ||
                !strcmp(dot, ".so"))
            {
                // NSS is problematic ...
                if (strstr(path, "libnspr4") ||
                    strstr(path, "libplds4") ||
                    strstr(path, "libplc4") ||
                    strstr(path, "libnss3") ||
                    strstr(path, "libnssckbi") ||
                    strstr(path, "libnsutil3") ||
                    strstr(path, "libssl3") ||
                    strstr(path, "libsoftokn3") ||
                    strstr(path, "libsqlite3") ||
                    strstr(path, "libfreeblpriv3"))
                    return true;

                // As is Python ...
                if (strstr(path, "python-core"))
                    return true;

                // otherwise drop the rest of the code.
                return false;
            }
            const char *vers;
            if ((vers = strstr(path, ".so."))) // .so.[digit]+
            {
                for(int i = sizeof (".so."); vers[i] != '\0'; ++i)
                    if (!isdigit(vers[i]) && vers[i] != '.')
                        return true;
                return false;
            }
            return true;
        }
        case LinkOrCopyType::NoUsr:
        default: // LinkOrCopyType::All
            return true;
        }
    }

    void linkOrCopyFile(const char *fpath, const Path& newPath)
    {
        if (linkOrCopyVerboseLogging)
            LOG_INF("Linking file \"" << fpath << "\" to \"" << newPath.toString() << "\"");
        if (link(fpath, newPath.toString().c_str()) == -1)
        {
            LOG_INF("link(\"" << fpath << "\", \"" <<
                    newPath.toString() << "\") failed. Will copy.");
            try
            {
                File(fpath).copyTo(newPath.toString());
            }
            catch (const std::exception& exc)
            {
                LOG_FTL("Copying of '" << fpath << "' to " << newPath.toString() <<
                        " failed: " << exc.what() << ". Exiting.");
                Log::shutdown();
                std::_Exit(Application::EXIT_SOFTWARE);
            }
        }
    }

    int linkOrCopyFunction(const char *fpath,
                           const struct stat* /*sb*/,
                           int typeflag,
                           struct FTW* /*ftwbuf*/)
    {
        if (strcmp(fpath, sourceForLinkOrCopy.c_str()) == 0)
            return 0;

        if (!linkOrCopyVerboseLogging)
        {
            const auto durationInSecs = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - linkOrCopyStartTime);
            if (durationInSecs.count() > slowLinkOrCopyLimitInSecs)
            {
                LOG_WRN("Linking/copying files from " << sourceForLinkOrCopy << " to " << destinationForLinkOrCopy.toString() <<
                        " is taking too much time. Enabling verbose link/copy logging at information level.");
                linkOrCopyVerboseLogging = true;
            }
        }

        assert(fpath[strlen(sourceForLinkOrCopy.c_str())] == '/');
        const char *relativeOldPath = fpath + strlen(sourceForLinkOrCopy.c_str()) + 1;
        Path newPath(destinationForLinkOrCopy, Path(relativeOldPath));

        switch (typeflag)
        {
        case FTW_F:
        case FTW_SLN:
            File(newPath.parent()).createDirectories();

            if (shouldLinkFile(relativeOldPath))
                linkOrCopyFile(fpath, newPath);
            break;
        case FTW_D:
            {
                struct stat st;
                if (stat(fpath, &st) == -1)
                {
                    LOG_SYS("stat(\"" << std::string(fpath) << "\") failed.");
                    return 1;
                }
                if (!shouldCopyDir(relativeOldPath))
                {
                    LOG_TRC("skip redundant paths " << relativeOldPath);
                    return FTW_SKIP_SUBTREE;
                }
                File(newPath).createDirectories();
                struct utimbuf ut;
                ut.actime = st.st_atime;
                ut.modtime = st.st_mtime;
                if (utime(newPath.toString().c_str(), &ut) == -1)
                {
                    LOG_SYS("utime(\"" << newPath.toString() << "\") failed.");
                    return 1;
                }
            }
            break;
        case FTW_DNR:
            LOG_ERR("Cannot read directory '" << fpath << "'");
            return 1;
        case FTW_NS:
            LOG_ERR("nftw: stat failed for '" << fpath << "'");
            return 1;
        default:
            LOG_FTL("nftw: unexpected type: '" << typeflag);
            assert(false);
            break;
        }
        return 0;
    }

    void linkOrCopy(const std::string& source,
                    const Path& destination,
                    LinkOrCopyType type)
    {
        linkOrCopyType = type;
        sourceForLinkOrCopy = source;
        if (sourceForLinkOrCopy.back() == '/')
            sourceForLinkOrCopy.pop_back();
        destinationForLinkOrCopy = destination;
        linkOrCopyStartTime = std::chrono::steady_clock::now();
        if (nftw(source.c_str(), linkOrCopyFunction, 10, FTW_ACTIONRETVAL) == -1)
        {
            LOG_ERR("linkOrCopy: nftw() failed for '" << source << "'");
        }
        if (linkOrCopyVerboseLogging)
        {
            LOG_INF("Linking/Copying of files to " << destinationForLinkOrCopy.toString() << " finished.");
            linkOrCopyVerboseLogging = false;
        }
    }

    void dropCapability(cap_value_t capability)
    {
        cap_t caps;
        cap_value_t cap_list[] = { capability };

        caps = cap_get_proc();
        if (caps == nullptr)
        {
            LOG_SFL("cap_get_proc() failed.");
            Log::shutdown();
            std::_Exit(1);
        }

        char *capText = cap_to_text(caps, nullptr);
        LOG_TRC("Capabilities first: " << capText);
        cap_free(capText);

        if (cap_set_flag(caps, CAP_EFFECTIVE, sizeof(cap_list)/sizeof(cap_list[0]), cap_list, CAP_CLEAR) == -1 ||
            cap_set_flag(caps, CAP_PERMITTED, sizeof(cap_list)/sizeof(cap_list[0]), cap_list, CAP_CLEAR) == -1)
        {
            LOG_SFL("cap_set_flag() failed.");
            Log::shutdown();
            std::_Exit(1);
        }

        if (cap_set_proc(caps) == -1)
        {
            LOG_SFL("cap_set_proc() failed.");
            Log::shutdown();
            std::_Exit(1);
        }

        capText = cap_to_text(caps, nullptr);
        LOG_TRC("Capabilities now: " << capText);
        cap_free(capText);

        cap_free(caps);
    }

    void symlinkPathToJail(const Path& jailPath, const std::string &loTemplate,
                           const std::string &loSubPath)
    {
        Path symlinkSource(jailPath, Path(loTemplate.substr(1)));
        File(symlinkSource.parent()).createDirectories();

        std::string symlinkTarget;
        for (int i = 0; i < Path(loTemplate).depth(); i++)
            symlinkTarget += "../";
        symlinkTarget += loSubPath;

        LOG_DBG("symlink(\"" << symlinkTarget << "\",\"" << symlinkSource.toString() << "\")");
        if (symlink(symlinkTarget.c_str(), symlinkSource.toString().c_str()) == -1)
        {
            LOG_SYS("symlink(\"" << symlinkTarget << "\",\"" << symlinkSource.toString() << "\") failed");
            throw Exception("symlink() failed");
        }
    }
#endif
}

#endif

/// A quick & dirty cache of the last few PNGs
/// and their hashes to avoid re-compression
/// wherever possible.
class PngCache
{
    typedef std::shared_ptr< std::vector< char > > CacheData;

    struct CacheEntry {
    private:
        size_t    _hitCount;
        TileWireId _wireId;
        CacheData _data;
    public:
        CacheEntry(size_t defaultSize, TileWireId id) :
            _hitCount(1),   // Every entry is used at least once; prevent removal at birth.
            _wireId(id),
            _data( new std::vector< char >() )
        {
            _data->reserve( defaultSize );
        }

        size_t getHitCount() const
        {
            return _hitCount;
        }

        void incrementHitCount()
        {
            ++_hitCount;
        }

        void decrementHitCount()
        {
            --_hitCount;
        }

        const CacheData& getData() const
        {
            return _data;
        }

        TileWireId getWireId() const
        {
            return _wireId;
        }
    } ;
    size_t _cacheSize;
    static const size_t CacheSizeSoftLimit = (1024 * 4 * 32); // 128k of cache
    static const size_t CacheSizeHardLimit = CacheSizeSoftLimit * 2;
    static const size_t CacheWidHardLimit = 4096;
    size_t _cacheHits;
    size_t _cacheTests;
    TileWireId _nextId;
    DeltaGenerator _deltaGen;

    std::unordered_map< TileBinaryHash, CacheEntry > _cache;
    // This uses little storage so can be much larger
    std::unordered_map< TileBinaryHash, TileWireId > _hashToWireId;

    void clearCache(bool logStats = false)
    {
        if (logStats)
            LOG_DBG("cache clear " << _cache.size() << " items total size " <<
                    _cacheSize << " current hits " << _cacheHits);
        _cache.clear();
        _hashToWireId.clear();
        _cacheSize = 0;
        _cacheHits = 0;
        _cacheTests = 0;
        _nextId = 1;
    }

    // Keep these ids small and wrap them.
    TileWireId createNewWireId()
    {
        TileWireId id = ++_nextId;
        // FIXME: if we wrap - we should flush the clients too really ...
        if (id < 1)
            clearCache(true);
        return id;
    }

    void balanceCache()
    {
        // A normalish PNG image size for text in a writer document is
        // around 4k for a content tile, and sub 1k for a background one.
        if (_cacheSize > CacheSizeHardLimit)
        {
            size_t avgHits = 0;
            for (auto it = _cache.begin(); it != _cache.end(); ++it)
                avgHits += it->second.getHitCount();

            LOG_DBG("Cache " << _cache.size() << " items total size " <<
                    _cacheSize << " current hits " << avgHits << ", total hit rate " <<
                    (_cacheHits * 100. / _cacheTests) << "% at balance start.");
            avgHits /= _cache.size();

            for (auto it = _cache.begin(); it != _cache.end();)
            {
                if ((_cacheSize > CacheSizeSoftLimit && it->second.getHitCount() == 0) ||
                    (_cacheSize > CacheSizeHardLimit && it->second.getHitCount() > 0 && it->second.getHitCount() <= avgHits))
                {
                    // Shrink cache when we exceed the size to maximize
                    // the chance of hitting these entries in the future.
                    _cacheSize -= it->second.getData()->size();
                    it = _cache.erase(it);
                }
                else
                {
                    if (it->second.getHitCount() > 0)
                        it->second.decrementHitCount();
                    ++it;
                }
            }

            LOG_DBG("Cache " << _cache.size() << " items with total size of " <<
                    _cacheSize << " bytes after balance.");
        }

        if (_hashToWireId.size() > CacheWidHardLimit)
        {
            LOG_DBG("Clear half of wid cache of size " << _hashToWireId.size());
            TileWireId max = _nextId - CacheWidHardLimit/2;
            for (auto it = _hashToWireId.begin(); it != _hashToWireId.end();)
            {
                if (it->second < max)
                    it = _hashToWireId.erase(it);
                else
                    ++it;
            }
            LOG_DBG("Wid cache is now size " << _hashToWireId.size());
        }
    }

    /// Lookup an entry in the cache and store the data in output.
    /// Returns true on success, otherwise false.
    bool cacheTest(const uint64_t hash, std::vector<char>& output)
    {
        if (hash)
        {
            ++_cacheTests;
            auto it = _cache.find(hash);
            if (it != _cache.end())
            {
                ++_cacheHits;
                LOG_DBG("PNG cache with hash " << hash << " hit.");
                output.insert(output.end(),
                              it->second.getData()->begin(),
                              it->second.getData()->end());
                it->second.incrementHitCount();
                return true;
            }
        }

        LOG_DBG("PNG cache with hash " << hash << " missed.");
        return false;
    }

    bool cacheEncodeSubBufferToPNG(unsigned char* pixmap, size_t startX, size_t startY,
                                   int width, int height,
                                   int bufferWidth, int bufferHeight,
                                   std::vector<char>& output, LibreOfficeKitTileMode mode,
                                   TileBinaryHash hash, TileWireId wid, TileWireId /*oldWid*/)
    {
/*
 *Disable for now - pushed in error.
 *
        if (_deltaGen.createDelta(pixmap, startX, startY, width, height,
                                  bufferWidth, bufferHeight,
                                  output, wid, oldWid))
            return true;
*/

        LOG_DBG("Encode a new png for this tile (" << startX << ',' << startY << ").");
        CacheEntry newEntry(bufferWidth * bufferHeight * 1, wid);
        if (Png::encodeSubBufferToPNG(pixmap, startX, startY, width, height,
                                      bufferWidth, bufferHeight,
                                      *newEntry.getData(), mode))
        {
            if (hash)
            {
                newEntry.getData()->shrink_to_fit();
                _cache.emplace(hash, newEntry);
                _cacheSize += newEntry.getData()->size();
                LOG_TRC("Cached new tile (" << startX << ',' << startY << ") with hash " << hash
                                            << ". New size: " << _cacheSize);
            }

            output.insert(output.end(),
                          newEntry.getData()->begin(),
                          newEntry.getData()->end());
            balanceCache();
            return true;
        }
        else
            return false;
    }

public:
    PngCache()
    {
        clearCache();
    }

    TileWireId hashToWireId(TileBinaryHash hash)
    {
        TileWireId wid;
        if (hash == 0)
            return 0;
        auto it = _hashToWireId.find(hash);
        if (it != _hashToWireId.end())
            wid = it->second;
        else
        {
            wid = createNewWireId();
            _hashToWireId.emplace(hash, wid);
        }
        return wid;
    }

    bool encodeBufferToPNG(unsigned char* pixmap, int width, int height,
                           std::vector<char>& output, LibreOfficeKitTileMode mode,
                           TileBinaryHash hash, TileWireId wid, TileWireId oldWid)
    {
        if (cacheTest(hash, output))
            return true;

        return cacheEncodeSubBufferToPNG(pixmap, 0, 0, width, height,
                                         width, height, output, mode,
                                         hash, wid, oldWid);
    }

    bool encodeSubBufferToPNG(unsigned char* pixmap, size_t startX, size_t startY,
                              int width, int height,
                              int bufferWidth, int bufferHeight,
                              std::vector<char>& output, LibreOfficeKitTileMode mode,
                              TileBinaryHash hash, TileWireId wid, TileWireId oldWid)
    {
        if (cacheTest(hash, output))
            return true;

        return cacheEncodeSubBufferToPNG(pixmap, startX, startY, width, height,
                                         bufferWidth, bufferHeight, output, mode,
                                         hash, wid, oldWid);
    }
};

#ifndef MOBILEAPP
static FILE* ProcSMapsFile = nullptr;
#endif

/// A document container.
/// Owns LOKitDocument instance and connections.
/// Manages the lifetime of a document.
/// Technically, we can host multiple documents
/// per process. But for security reasons don't.
/// However, we could have a loolkit instance
/// per user or group of users (a trusted circle).
class Document : public Runnable, public DocumentManagerInterface
{
public:
    /// We have two types of password protected documents
    /// 1) Documents which require password to view
    /// 2) Document which require password to modify
    enum class PasswordType { ToView, ToModify };

public:
    Document(const std::shared_ptr<lok::Office>& loKit,
             const std::string& jailId,
             const std::string& docKey,
             const std::string& docId,
             const std::string& url,
             std::shared_ptr<TileQueue> tileQueue,
             SocketPoll& socketPoll,
             const std::shared_ptr<WebSocketHandler>& websocketHandler)
      : _loKit(loKit),
        _jailId(jailId),
        _docKey(docKey),
        _docId(docId),
        _url(url),
        _obfuscatedFileId(Util::getFilenameFromURL(docKey)),
        _tileQueue(std::move(tileQueue)),
        _socketPoll(socketPoll),
        _websocketHandler(websocketHandler),
        _docPassword(""),
        _haveDocPassword(false),
        _isDocPasswordProtected(false),
        _docPasswordType(PasswordType::ToView),
        _stop(false),
        _isLoading(0),
        _editorId(-1),
        _editorChangeWarning(false)
    {
        LOG_INF("Document ctor for [" << _docKey <<
                "] url [" << anonymizeUrl(_url) << "] on child [" << _jailId <<
                "] and id [" << _docId << "].");
        assert(_loKit);

        _callbackThread.start(*this);
    }

    ~Document()
    {
        LOG_INF("~Document dtor for [" << _docKey <<
                "] url [" << anonymizeUrl(_url) << "] on child [" << _jailId <<
                "] and id [" << _docId << "]. There are " <<
                _sessions.size() << " views.");

        // Wait for the callback worker to finish.
        _stop = true;

        _tileQueue->put("eof");
        _callbackThread.join();
    }

    const std::string& getUrl() const { return _url; }

    /// Post the message in the correct thread.
    bool postMessage(const std::shared_ptr<std::vector<char>>& message, const WSOpCode code) const
    {
        LOG_TRC("postMessage called with: " << getAbbreviatedMessage(message->data(), message->size()));
        if (!_websocketHandler)
        {
            LOG_ERR("Child Doc: Bad socket while sending [" << getAbbreviatedMessage(message->data(), message->size()) << "].");
            return false;
        }

        _socketPoll.addCallback([=]{
                _websocketHandler->sendMessage(message->data(), message->size(), code);
            });
        return true;
    }

    bool createSession(const std::string& sessionId)
    {
        std::unique_lock<std::mutex> lock(_mutex);

        try
        {
            if (_sessions.find(sessionId) != _sessions.end())
            {
                LOG_WRN("Session [" << sessionId << "] on url [" << anonymizeUrl(_url) << "] already exists.");
                return true;
            }

            LOG_INF("Creating " << (_sessions.empty() ? "first" : "new") <<
                    " session for url: " << anonymizeUrl(_url) << " for sessionId: " <<
                    sessionId << " on jailId: " << _jailId);

            auto session = std::make_shared<ChildSession>(sessionId, _jailId, *this);
            _sessions.emplace(sessionId, session);

            int viewId = session->getViewId();
            _lastUpdatedAt[viewId] = std::chrono::steady_clock::now();
            _speedCount[viewId] = 0;

            LOG_DBG("Sessions: " << _sessions.size());
            return true;
        }
        catch (const std::exception& ex)
        {
            LOG_ERR("Exception while creating session [" << sessionId <<
                    "] on url [" << anonymizeUrl(_url) << "] - '" << ex.what() << "'.");
            return false;
        }
    }

    /// Purges dead connections and returns
    /// the remaining number of clients.
    /// Returns -1 on failure.
    size_t purgeSessions()
    {
        std::vector<std::shared_ptr<ChildSession>> deadSessions;
        size_t num_sessions = 0;
        {
            std::unique_lock<std::mutex> lock(_mutex, std::defer_lock);
            if (!lock.try_lock())
            {
                // Not a good time, try later.
                return -1;
            }

            // If there are no live sessions, we don't need to do anything at all and can just
            // bluntly exit, no need to clean up our own data structures. Also, there is a bug that
            // causes the deadSessions.clear() call below to crash in some situations when the last
            // session is being removed.
            for (auto it = _sessions.cbegin(); it != _sessions.cend(); )
            {
                if (it->second->isCloseFrame())
                {
                    deadSessions.push_back(it->second);
                    it = _sessions.erase(it);
                }
                else
                {
                    ++it;
                }
            }

            num_sessions = _sessions.size();
#ifndef MOBILEAPP
            if (num_sessions == 0)
            {
                LOG_FTL("Document [" << anonymizeUrl(_url) << "] has no more views, exiting bluntly.");
                Log::shutdown();
                std::_Exit(Application::EXIT_OK);
            }
#endif
        }

        // Don't destroy sessions while holding our lock.
        // We may deadlock if a session is waiting on us
        // during callback initiated while handling a command
        // and the dtor tries to take its lock (which is taken).
        deadSessions.clear();

        return num_sessions;
    }

    /// Set Document password for given URL
    void setDocumentPassword(int passwordType)
    {
        LOG_INF("setDocumentPassword: passwordProtected=" << _isDocPasswordProtected <<
                " passwordProvided=" << _haveDocPassword <<
                " password='" << _docPassword << "'");

        Util::assertIsLocked(_documentMutex);

        if (_isDocPasswordProtected && _haveDocPassword)
        {
            // it means this is the second attempt with the wrong password; abort the load operation
            _loKit->setDocumentPassword(_jailedUrl.c_str(), nullptr);
            return;
        }

        // One thing for sure, this is a password protected document
        _isDocPasswordProtected = true;
        if (passwordType == LOK_CALLBACK_DOCUMENT_PASSWORD)
            _docPasswordType = PasswordType::ToView;
        else if (passwordType == LOK_CALLBACK_DOCUMENT_PASSWORD_TO_MODIFY)
            _docPasswordType = PasswordType::ToModify;

        LOG_INF("Calling _loKit->setDocumentPassword");
        if (_haveDocPassword)
            _loKit->setDocumentPassword(_jailedUrl.c_str(), _docPassword.c_str());
        else
            _loKit->setDocumentPassword(_jailedUrl.c_str(), nullptr);
        LOG_INF("setDocumentPassword returned");
    }

    void renderTile(const std::vector<std::string>& tokens)
    {
        TileDesc tile = TileDesc::parse(tokens);

        size_t pixmapDataSize = 4 * tile.getWidth() * tile.getHeight();
        std::vector<unsigned char> pixmap;
        pixmap.resize(pixmapDataSize);

        std::unique_lock<std::mutex> lock(_documentMutex);
        if (!_loKitDocument)
        {
            LOG_ERR("Tile rendering requested before loading document.");
            return;
        }

        if (_loKitDocument->getViewsCount() <= 0)
        {
            LOG_ERR("Tile rendering requested without views.");
            return;
        }

        // Find a session matching our view / render settings.
        const auto session = _sessions.findByCanonicalId(tile.getNormalizedViewId());
        if (!session)
        {
            LOG_ERR("Session is not found. Maybe exited after rendering request.");
            return;
        }

#ifdef FIXME_RENDER_SETTINGS
        // if necessary select a suitable rendering view eg. with 'show non-printing chars'
        if (tile.getNormalizedViewId())
            _loKitDocument->setView(session->getViewId());
#endif

        const double area = tile.getWidth() * tile.getHeight();
        Timestamp timestamp;
        _loKitDocument->paintPartTile(pixmap.data(), tile.getPart(),
                                      tile.getWidth(), tile.getHeight(),
                                      tile.getTilePosX(), tile.getTilePosY(),
                                      tile.getTileWidth(), tile.getTileHeight());
        const Poco::Timestamp::TimeDiff elapsed = timestamp.elapsed();
        LOG_TRC("paintTile at (" << tile.getPart() << ',' << tile.getTilePosX() << ',' << tile.getTilePosY() <<
                ") " << "ver: " << tile.getVersion() << " rendered in " << (elapsed/1000.) <<
                " ms (" << area / elapsed << " MP/s).");
        const auto mode = static_cast<LibreOfficeKitTileMode>(_loKitDocument->getTileMode());

        int pixelWidth = tile.getWidth();
        int pixelHeight = tile.getHeight();

        if (session->_docWatermark)
            session->_docWatermark->blending(pixmap.data(), 0, 0, pixelWidth, pixelHeight, pixelWidth, pixelHeight, mode);

        const TileBinaryHash hash = Png::hashBuffer(pixmap.data(), tile.getWidth(), tile.getHeight());
        TileWireId wid = _pngCache.hashToWireId(hash);
        TileWireId oldWireId = tile.getOldWireId();

        tile.setWireId(wid);

        if (hash != 0 && oldWireId == wid)
        {
            // The tile content is identical to what the client already has, so skip it
            LOG_TRC("Match oldWireId==wid (" << wid << " for hash " << hash << "); unchanged");
            return;
        }

        // Send back the request with all optional parameters given in the request.
        std::string response = ADD_DEBUG_RENDERID(tile.serialize("tile:")) + "\n";

        std::shared_ptr<std::vector<char>> output = std::make_shared<std::vector<char>>();
        output->reserve(response.size() + pixmapDataSize);
        output->resize(response.size());
        std::memcpy(output->data(), response.data(), response.size());

        if (!_pngCache.encodeBufferToPNG(pixmap.data(), tile.getWidth(), tile.getHeight(), *output, mode, hash, wid, oldWireId))
        {
            //FIXME: Return error.
            //sendTextFrame("error: cmd=tile kind=failure");

            LOG_ERR("Failed to encode tile into PNG.");
            return;
        }

        LOG_TRC("Sending render-tile response (" << output->size() << " bytes) for: " << response);
        postMessage(output, WSOpCode::Binary);
    }

    void renderCombinedTiles(const std::vector<std::string>& tokens)
    {
        TileCombined tileCombined = TileCombined::parse(tokens);
        auto& tiles = tileCombined.getTiles();

        Util::Rectangle renderArea;
        std::vector<Util::Rectangle> tileRecs;
        tileRecs.reserve(tiles.size());

        for (auto& tile : tiles)
        {
            Util::Rectangle rectangle(tile.getTilePosX(), tile.getTilePosY(),
                                      tileCombined.getTileWidth(), tileCombined.getTileHeight());

            if (tileRecs.empty())
            {
                renderArea = rectangle;
            }
            else
            {
                renderArea.extend(rectangle);
            }

            tileRecs.push_back(rectangle);
        }

        const size_t tilesByX = renderArea.getWidth() / tileCombined.getTileWidth();
        const size_t tilesByY = renderArea.getHeight() / tileCombined.getTileHeight();
        const size_t pixmapWidth = tilesByX * tileCombined.getWidth();
        const size_t pixmapHeight = tilesByY * tileCombined.getHeight();

        if (pixmapWidth > 4096 || pixmapHeight > 4096)
            LOG_WRN("Unusual extremely large tile combine of size " << pixmapWidth << "x" << pixmapHeight);

        const size_t pixmapSize = 4 * pixmapWidth * pixmapHeight;
        std::vector<unsigned char> pixmap(pixmapSize, 0);

        std::unique_lock<std::mutex> lock(_documentMutex);
        if (!_loKitDocument)
        {
            LOG_ERR("Tile rendering requested before loading document.");
            return;
        }

        if (_loKitDocument->getViewsCount() <= 0)
        {
            LOG_ERR("Tile rendering requested without views.");
            return;
        }

        // Find a session matching our view / render settings.
        const auto session = _sessions.findByCanonicalId(tileCombined.getNormalizedViewId());
        if (!session)
        {
            LOG_ERR("Session is not found. Maybe exited after rendering request.");
            return;
        }

#ifdef FIXME_RENDER_SETTINGS
        // if necessary select a suitable rendering view eg. with 'show non-printing chars'
        if (tileCombined.getNormalizedViewId())
            _loKitDocument->setView(session->getViewId());
#endif

        // Render the whole area
        const double area = pixmapWidth * pixmapHeight;
        Timestamp timestamp;
        LOG_TRC("Calling paintPartTile(" << (void*)pixmap.data() << ")");
        _loKitDocument->paintPartTile(pixmap.data(),
                                      tileCombined.getPart(),
                                      pixmapWidth, pixmapHeight,
                                      renderArea.getLeft(), renderArea.getTop(),
                                      renderArea.getWidth(), renderArea.getHeight());
        Timestamp::TimeDiff elapsed = timestamp.elapsed();
        LOG_DBG("paintTile (combined) at (" << renderArea.getLeft() << ", " << renderArea.getTop() << "), (" <<
                renderArea.getWidth() << ", " << renderArea.getHeight() << ") " <<
                " rendered in " << (elapsed/1000.) << " ms (" << area / elapsed << " MP/s).");
        const auto mode = static_cast<LibreOfficeKitTileMode>(_loKitDocument->getTileMode());

        std::vector<char> output;
        output.reserve(pixmapSize);

        size_t tileIndex = 0;
        for (Util::Rectangle& tileRect : tileRecs)
        {
            const size_t positionX = (tileRect.getLeft() - renderArea.getLeft()) / tileCombined.getTileWidth();
            const size_t positionY = (tileRect.getTop() - renderArea.getTop()) / tileCombined.getTileHeight();

            const size_t oldSize = output.size();
            const int pixelWidth = tileCombined.getWidth();
            const int pixelHeight = tileCombined.getHeight();

            const int offsetX = positionX * pixelWidth;
            const int offsetY = positionY * pixelHeight;

            if (session->_docWatermark)
                session->_docWatermark->blending(pixmap.data(), offsetX, offsetY,
                                        pixmapWidth, pixmapHeight,
                                        pixelWidth, pixelHeight,
                                        mode);

            const uint64_t hash = Png::hashSubBuffer(pixmap.data(), offsetX, offsetY,
                                                     pixelWidth, pixelHeight, pixmapWidth, pixmapHeight);

            TileWireId wireId = _pngCache.hashToWireId(hash);
            TileWireId oldWireId = tiles[tileIndex].getOldWireId();
            if (hash != 0 && oldWireId == wireId)
            {
                // The tile content is identical to what the client already has, so skip it
                LOG_TRC("Match for tile #" << tileIndex << " at (" << positionX << "," <<
                        positionY << ") oldhash==hash (" << hash << "), wireId: " << wireId << " skipping");
                tiles.erase(tiles.begin() + tileIndex);
                continue;
            }

            if (!_pngCache.encodeSubBufferToPNG(pixmap.data(), offsetX, offsetY,
                                                pixelWidth, pixelHeight, pixmapWidth, pixmapHeight, output, mode,
                                                hash, wireId, oldWireId))
            {
                //FIXME: Return error.
                //sendTextFrame("error: cmd=tile kind=failure");
                LOG_ERR("Failed to encode tile into PNG.");
                return;
            }

            const size_t imgSize = output.size() - oldSize;
            LOG_TRC("Encoded tile #" << tileIndex << " at (" << positionX << "," << positionY << ") with oldWireId=" <<
                    tiles[tileIndex].getOldWireId() << ", hash=" << hash << " wireId: " << wireId << " in " << imgSize << " bytes.");
            tiles[tileIndex].setWireId(wireId);
            tiles[tileIndex].setImgSize(imgSize);
            tileIndex++;
        }

        elapsed = timestamp.elapsed();
        LOG_DBG("renderCombinedTiles at (" << renderArea.getLeft() << ", " << renderArea.getTop() << "), (" <<
                renderArea.getWidth() << ", " << renderArea.getHeight() << ") " <<
                " took " << (elapsed/1000.) << " ms (including the paintTile).");

        if (tileIndex == 0)
        {
            LOG_DBG("All tiles skipped, not producing empty tilecombine: message");
            return;
        }

        const auto tileMsg = ADD_DEBUG_RENDERID(tileCombined.serialize("tilecombine:")) + "\n";
        LOG_TRC("Sending back painted tiles for " << tileMsg);

        std::shared_ptr<std::vector<char>> response = std::make_shared<std::vector<char>>();
        response->resize(tileMsg.size() + output.size());
        std::copy(tileMsg.begin(), tileMsg.end(), response->begin());
        std::copy(output.begin(), output.end(), response->begin() + tileMsg.size());

        postMessage(response, WSOpCode::Binary);
    }

    bool sendTextFrame(const std::string& message)
    {
        return sendFrame(message.data(), message.size());
    }

    bool sendFrame(const char* buffer, int length, WSOpCode opCode = WSOpCode::Text) override
    {
        try
        {
            std::shared_ptr<std::vector<char>> message = std::make_shared<std::vector<char>>();
            message->resize(length);
            std::memcpy(message->data(), buffer, length);

            return postMessage(message, opCode);
        }
        catch (const Exception& exc)
        {
            LOG_ERR("Document::sendFrame: Exception: " << exc.displayText() <<
                    (exc.nested() ? "( " + exc.nested()->displayText() + ")" : ""));
        }

        return false;
    }

    static void GlobalCallback(const int type, const char* p, void* data)
    {
        if (TerminationFlag)
        {
            return;
        }

        const std::string payload = p ? p : "(nil)";
        LOG_TRC("Document::GlobalCallback " << LOKitHelper::kitCallbackTypeToString(type) <<
                " [" << payload << "].");
        Document* self = static_cast<Document*>(data);
        if (type == LOK_CALLBACK_DOCUMENT_PASSWORD_TO_MODIFY ||
            type == LOK_CALLBACK_DOCUMENT_PASSWORD)
        {
            // Mark the document password type.
            self->setDocumentPassword(type);
            return;
        }
        else if (type == LOK_CALLBACK_STATUS_INDICATOR_SET_VALUE)
        {
            for (auto& it : self->_sessions)
            {
                std::shared_ptr<ChildSession> session = it.second;
                if (session && !session->isCloseFrame())
                {
                    session->loKitCallback(type, payload);
                }
            }
            return;
        }
        else if (type == LOK_CALLBACK_PROFILE_FRAME)
            return; // already trace dumped above.

        // Broadcast leftover status indicator callbacks to all clients
        self->broadcastCallbackToClients(type, payload);
    }

    static void ViewCallback(const int type, const char* p, void* data)
    {
        if (TerminationFlag)
        {
            return;
        }

        CallbackDescriptor* descriptor = static_cast<CallbackDescriptor*>(data);
        assert(descriptor && "Null callback data.");
        assert(descriptor->getDoc() && "Null Document instance.");

        std::shared_ptr<TileQueue> tileQueue = descriptor->getDoc()->getTileQueue();
        assert(tileQueue && "Null TileQueue.");

        const std::string payload = p ? p : "(nil)";
        LOG_TRC("Document::ViewCallback [" << descriptor->getViewId() <<
                "] [" << LOKitHelper::kitCallbackTypeToString(type) <<
                "] [" << payload << "].");

        // when we examine the content of the JSON
        std::string targetViewId;

        if (type == LOK_CALLBACK_CELL_CURSOR)
        {
            Poco::StringTokenizer tokens(payload, ",", Poco::StringTokenizer::TOK_IGNORE_EMPTY | Poco::StringTokenizer::TOK_TRIM);
            // Payload may be 'EMPTY'.
            if (tokens.count() == 4)
            {
                int cursorX = std::stoi(tokens[0]);
                int cursorY = std::stoi(tokens[1]);
                int cursorWidth = std::stoi(tokens[2]);
                int cursorHeight = std::stoi(tokens[3]);

                tileQueue->updateCursorPosition(0, 0, cursorX, cursorY, cursorWidth, cursorHeight);
            }
        }
        else if (type == LOK_CALLBACK_INVALIDATE_VISIBLE_CURSOR)
        {
            Poco::JSON::Parser parser;
            const Poco::Dynamic::Var result = parser.parse(payload);
            const auto& command = result.extract<Poco::JSON::Object::Ptr>();
            std::string rectangle = command->get("rectangle").toString();
            Poco::StringTokenizer tokens(rectangle, ",", Poco::StringTokenizer::TOK_IGNORE_EMPTY | Poco::StringTokenizer::TOK_TRIM);
            // Payload may be 'EMPTY'.
            if (tokens.count() == 4)
            {
                int cursorX = std::stoi(tokens[0]);
                int cursorY = std::stoi(tokens[1]);
                int cursorWidth = std::stoi(tokens[2]);
                int cursorHeight = std::stoi(tokens[3]);

                tileQueue->updateCursorPosition(0, 0, cursorX, cursorY, cursorWidth, cursorHeight);
            }
        }
        else if (type == LOK_CALLBACK_INVALIDATE_VIEW_CURSOR ||
                 type == LOK_CALLBACK_CELL_VIEW_CURSOR)
        {
            Poco::JSON::Parser parser;
            const Poco::Dynamic::Var result = parser.parse(payload);
            const auto& command = result.extract<Poco::JSON::Object::Ptr>();
            targetViewId = command->get("viewId").toString();
            std::string part = command->get("part").toString();
            std::string text = command->get("rectangle").toString();
            Poco::StringTokenizer tokens(text, ",", Poco::StringTokenizer::TOK_IGNORE_EMPTY | Poco::StringTokenizer::TOK_TRIM);
            // Payload may be 'EMPTY'.
            if (tokens.count() == 4)
            {
                int cursorX = std::stoi(tokens[0]);
                int cursorY = std::stoi(tokens[1]);
                int cursorWidth = std::stoi(tokens[2]);
                int cursorHeight = std::stoi(tokens[3]);

                tileQueue->updateCursorPosition(std::stoi(targetViewId), std::stoi(part), cursorX, cursorY, cursorWidth, cursorHeight);
            }
        }

        // merge various callback types together if possible
        if (type == LOK_CALLBACK_INVALIDATE_TILES ||
            type == LOK_CALLBACK_DOCUMENT_SIZE_CHANGED)
        {
            // no point in handling invalidations or page resizes per-view,
            // all views have to be in sync
            tileQueue->put("callback all " + std::to_string(type) + ' ' + payload);
        }
        else if (type == LOK_CALLBACK_INVALIDATE_VIEW_CURSOR ||
                 type == LOK_CALLBACK_CELL_VIEW_CURSOR)
        {
            // these should go to all views but the one that that triggered it
            tileQueue->put("callback except-" + targetViewId + ' ' + std::to_string(type) + ' ' + payload);
        }
        else
            tileQueue->put("callback " + std::to_string(descriptor->getViewId()) + ' ' + std::to_string(type) + ' ' + payload);
    }

private:

    /// Helper method to broadcast callback and its payload to all clients
    void broadcastCallbackToClients(const int type, const std::string& payload)
    {
        _tileQueue->put("callback all " + std::to_string(type) + ' ' + payload);
    }

    /// Load a document (or view) and register callbacks.
    bool onLoad(const std::string& sessionId,
                const std::string& uri,
                const std::string& uriAnonym,
                const std::string& userName,
                const std::string& userNameAnonym,
                const std::string& docPassword,
                const std::string& renderOpts,
                const bool haveDocPassword,
                const std::string& lang,
                const std::string& watermarkText,
                const std::string& docTemplate) override
    {
        std::unique_lock<std::mutex> lock(_mutex);

        LOG_INF("Loading url [" << uriAnonym << "] for session [" << sessionId <<
                "] which has " << (_sessions.size() - 1) <<
                " sessions. Another load in progress: " << _isLoading);

        while (_isLoading)
        {
            _cvLoading.wait(lock);
        }

        // This shouldn't happen, but for sanity.
        const auto it = _sessions.find(sessionId);
        if (it == _sessions.end() || !it->second)
        {
            LOG_ERR("Cannot find session [" << sessionId << "] to load view for.");
            return false;
        }

        std::shared_ptr<ChildSession> session = it->second;

        // Flag and release lock.
        ++_isLoading;
        lock.unlock();

        try
        {
            if (!load(session, uri, uriAnonym, userName, userNameAnonym, docPassword, renderOpts, haveDocPassword, lang, watermarkText, docTemplate))
            {
                return false;
            }
        }
        catch (const std::exception& exc)
        {
            LOG_ERR("Exception while loading url [" << uriAnonym <<
                    "] for session [" << sessionId << "]: " << exc.what());
            return false;
        }

        --_isLoading;
        _cvLoading.notify_one();

        return true;
    }

    void onUnload(const ChildSession& session) override
    {
        const auto& sessionId = session.getId();
        LOG_INF("Unloading session [" << sessionId << "] on url [" << anonymizeUrl(_url) << "].");

        const int viewId = session.getViewId();
        _tileQueue->removeCursorPosition(viewId);

        std::unique_lock<std::mutex> lockLokDoc(_documentMutex);
        if (_loKitDocument == nullptr)
        {
            LOG_ERR("Unloading session [" << sessionId << "] without loKitDocument.");
            return;
        }

        _loKitDocument->setView(viewId);
        _loKitDocument->registerCallback(nullptr, nullptr);

        int viewCount = _loKitDocument->getViewsCount();
        if (viewCount == 1)
        {
            std::unique_lock<std::mutex> lock(_mutex);
#ifndef MOBILEAPP
            if (_sessions.empty())
            {
                LOG_INF("Document [" << anonymizeUrl(_url) << "] has no more views, exiting bluntly.");
                Log::shutdown();
                std::_Exit(Application::EXIT_OK);
            }
#endif
            LOG_INF("Document [" << anonymizeUrl(_url) << "] has no more views, but has " <<
                    _sessions.size() << " sessions still. Destroying the document.");
            _loKitDocument.reset();
            LOG_INF("Document [" << anonymizeUrl(_url) << "] session [" << sessionId << "] unloaded Document.");
            return;
        }
        else
        {
            _loKitDocument->destroyView(viewId);
        }

        // Since callback messages are processed on idle-timer,
        // we could recieve callbacks after destroying a view.
        // Retain the CallbackDescriptor object, which is shared with Core.
        // _viewIdToCallbackDescr.erase(viewId);

        viewCount = _loKitDocument->getViewsCount();
        LOG_INF("Document [" << anonymizeUrl(_url) << "] session [" <<
                sessionId << "] unloaded view [" << viewId << "]. Have " <<
                viewCount << " view" << (viewCount != 1 ? "s." : "."));

        if (viewCount > 0)
        {
            // Broadcast updated view info
            notifyViewInfo();
        }
    }

    std::map<int, UserInfo> getViewInfo() override
    {
        std::unique_lock<std::mutex> lock(_mutex);

        return _sessionUserInfo;
    }

    std::mutex& getMutex() override
    {
        return _mutex;
    }

    std::shared_ptr<TileQueue>& getTileQueue() override
    {
        return _tileQueue;
    }

    int getEditorId() override
    {
        return _editorId;
    }

    /// Notify all views with the given message
    bool notifyAll(const std::string& msg) override
    {
        Util::assertIsLocked(_documentMutex);

        // Broadcast updated viewinfo to all clients.
        return sendTextFrame("client-all " + msg);
    }

    /// Notify all views of viewId and their associated usernames
    void notifyViewInfo() override
    {
        Util::assertIsLocked(_documentMutex);

        // Get the list of view ids from the core
        const int viewCount = getLOKitDocument()->getViewsCount();
        std::vector<int> viewIds(viewCount);
        getLOKitDocument()->getViewIds(viewIds.data(), viewCount);

        const std::map<int, UserInfo> viewInfoMap = _sessionUserInfo;

        const std::map<std::string, int> viewColorsMap = getViewColors();

        // Double check if list of viewids from core and our list matches,
        // and create an array of JSON objects containing id and username
        std::ostringstream oss;
        oss << "viewinfo: [";
        for (const auto& viewId : viewIds)
        {
            oss << "{\"id\":" << viewId << ",";
            int color = 0;
            const auto itView = viewInfoMap.find(viewId);
            if (itView == viewInfoMap.end())
            {
                LOG_ERR("No username found for viewId [" << viewId << "].");
                oss << "\"username\":\"Unknown\",";
            }
            else
            {
                oss << "\"userid\":\"" << itView->second.getUserId() << "\",";
                const std::string username = itView->second.getUserName();
                oss << "\"username\":\"" << username << "\",";
                if (!itView->second.getUserExtraInfo().empty())
                    oss << "\"userextrainfo\":" << itView->second.getUserExtraInfo() << ",";
                const bool readonly = itView->second.isReadOnly();
                oss << "\"readonly\":\"" << readonly << "\",";
                const auto it = viewColorsMap.find(username);
                if (it != viewColorsMap.end())
                {
                    color = it->second;
                }
            }

            oss << "\"color\":" << color << "},";
        }

        oss.seekp(-1, std::ios_base::cur); // Remove last comma.
        oss << "]";

        // Broadcast updated viewinfo to all clients.
        notifyAll(oss.str());
    }

    void updateEditorSpeeds(int id, int speed) override
    {
        int maxSpeed = -1, fastestUser = -1;

        auto now = std::chrono::steady_clock::now();
        _lastUpdatedAt[id] = now;
        _speedCount[id] = speed;

        for (const auto& it : _sessions)
        {
            const std::shared_ptr<ChildSession> session = it.second;
            int sessionId = session->getViewId();

            auto duration = (_lastUpdatedAt[id] - now);
            std::chrono::milliseconds::rep durationInMs = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
            if (_speedCount[sessionId] != 0 && durationInMs > 5000)
            {
                _speedCount[sessionId] = session->getSpeed();
                _lastUpdatedAt[sessionId] = now;
            }
            if (_speedCount[sessionId] > maxSpeed)
            {
                maxSpeed = _speedCount[sessionId];
                fastestUser = sessionId;
            }
        }
        // 0 for preventing selection of the first always
        // 1 for preventing the new users from directly beoming the editors
        if (_editorId != fastestUser && (maxSpeed != 0 && maxSpeed != 1)) {
            if (!_editorChangeWarning && _editorId != -1)
            {
                _editorChangeWarning = true;
            }
            else
            {
                _editorChangeWarning = false;
                _editorId = fastestUser;
                for (const auto& it : _sessions)
                    it.second->sendTextFrame("editor: " + std::to_string(_editorId));
            }
        }
        else
            _editorChangeWarning = false;
    }

private:

    // Get the color value for all author names from the core
    std::map<std::string, int> getViewColors()
    {
        Util::assertIsLocked(_documentMutex);

        char* values = _loKitDocument->getCommandValues(".uno:TrackedChangeAuthors");
        const std::string colorValues = std::string(values == nullptr ? "" : values);
        std::free(values);

        std::map<std::string, int> viewColors;
        try
        {
            if (!colorValues.empty())
            {
                Poco::JSON::Parser parser;
                Poco::JSON::Object::Ptr root = parser.parse(colorValues).extract<Poco::JSON::Object::Ptr>();
                if (root->get("authors").type() == typeid(Poco::JSON::Array::Ptr))
                {
                    Poco::JSON::Array::Ptr authorsArray = root->get("authors").extract<Poco::JSON::Array::Ptr>();
                    for (auto& authorVar: *authorsArray)
                    {
                        Poco::JSON::Object::Ptr authorObj = authorVar.extract<Poco::JSON::Object::Ptr>();
                        std::string authorName = authorObj->get("name").convert<std::string>();
                        int colorValue = authorObj->get("color").convert<int>();
                        viewColors[authorName] = colorValue;
                    }
                }
            }
        }
        catch(const Exception& exc)
        {
            LOG_ERR("Poco Exception: " << exc.displayText() <<
                    (exc.nested() ? " (" + exc.nested()->displayText() + ")" : ""));
        }

        return viewColors;
    }

    std::shared_ptr<lok::Document> load(const std::shared_ptr<ChildSession>& session,
                                        const std::string& uri,
                                        const std::string& uriAnonym,
                                        const std::string& userName,
                                        const std::string& userNameAnonym,
                                        const std::string& docPassword,
                                        const std::string& renderOpts,
                                        const bool haveDocPassword,
                                        const std::string& lang,
                                        const std::string& watermarkText,
                                        const std::string& docTemplate)
    {
        const std::string sessionId = session->getId();

        std::string options;
        if (!lang.empty())
            options = "Language=" + lang;

        std::unique_lock<std::mutex> lock(_documentMutex);

        if (!_loKitDocument)
        {
            // This is the first time we are loading the document
            LOG_INF("Loading new document from URI: [" << uriAnonym << "] for session [" << sessionId << "].");

            _loKit->registerCallback(GlobalCallback, this);

            const int flags = LOK_FEATURE_DOCUMENT_PASSWORD
                             | LOK_FEATURE_DOCUMENT_PASSWORD_TO_MODIFY
                             | LOK_FEATURE_PART_IN_INVALIDATION_CALLBACK
                             | LOK_FEATURE_NO_TILED_ANNOTATIONS
                             | LOK_FEATURE_RANGE_HEADERS
                             | LOK_FEATURE_VIEWID_IN_VISCURSOR_INVALIDATION_CALLBACK;
            _loKit->setOptionalFeatures(flags);

            // Save the provided password with us and the jailed url
            _haveDocPassword = haveDocPassword;
            _docPassword = docPassword;
            _jailedUrl = uri;
            _isDocPasswordProtected = false;

            LOG_DBG("Calling lokit::documentLoad(" << uriAnonym << ", \"" << options << "\").");
            Timestamp timestamp;
            _loKitDocument.reset(_loKit->documentLoad(docTemplate.empty() ? uri.c_str() : docTemplate.c_str(), options.c_str()));
            LOG_DBG("Returned lokit::documentLoad(" << uriAnonym << ") in " << (timestamp.elapsed() / 1000.) << "ms.");
#ifdef IOS
            // The iOS app (and the Android one) has max one document open at a time, so we can keep
            // a pointer to it in a global.
            lok_document = _loKitDocument.get();
#endif
            if (!_loKitDocument || !_loKitDocument->get())
            {
                LOG_ERR("Failed to load: " << uriAnonym << ", error: " << _loKit->getError());

                // Checking if wrong password or no password was reason for failure.
                if (_isDocPasswordProtected)
                {
                    LOG_INF("Document [" << uriAnonym << "] is password protected.");
                    if (!_haveDocPassword)
                    {
                        LOG_INF("No password provided for password-protected document [" << uriAnonym << "].");
                        std::string passwordFrame = "passwordrequired:";
                        if (_docPasswordType == PasswordType::ToView)
                            passwordFrame += "to-view";
                        else if (_docPasswordType == PasswordType::ToModify)
                            passwordFrame += "to-modify";
                        session->sendTextFrame("error: cmd=load kind=" + passwordFrame);
                    }
                    else
                    {
                        LOG_INF("Wrong password for password-protected document [" << uriAnonym << "].");
                        session->sendTextFrame("error: cmd=load kind=wrongpassword");
                    }
                }

                return nullptr;
            }

            // Only save the options on opening the document.
            // No support for changing them after opening a document.
            _renderOpts = renderOpts;
        }
        else
        {
            LOG_INF("Document with url [" << uriAnonym << "] already loaded. Need to create new view for session [" << sessionId << "].");

            // Check if this document requires password
            if (_isDocPasswordProtected)
            {
                if (!haveDocPassword)
                {
                    std::string passwordFrame = "passwordrequired:";
                    if (_docPasswordType == PasswordType::ToView)
                        passwordFrame += "to-view";
                    else if (_docPasswordType == PasswordType::ToModify)
                        passwordFrame += "to-modify";
                    session->sendTextFrame("error: cmd=load kind=" + passwordFrame);
                    return nullptr;
                }
                else if (docPassword != _docPassword)
                {
                    session->sendTextFrame("error: cmd=load kind=wrongpassword");
                    return nullptr;
                }
            }

            LOG_INF("Creating view to url [" << uriAnonym << "] for session [" << sessionId << "] with " << options << '.');
            _loKitDocument->createView(options.c_str());
            LOG_TRC("View to url [" << uriAnonym << "] created.");
        }

        LOG_INF("Initializing for rendering session [" << sessionId << "] on document url [" <<
                anonymizeUrl(_url) << "] with: [" << makeRenderParams(_renderOpts, userNameAnonym) << "].");

        // initializeForRendering() should be called before
        // registerCallback(), as the previous creates a new view in Impress.
        const std::string renderParams = makeRenderParams(_renderOpts, userName);
        _loKitDocument->initializeForRendering(renderParams.c_str());

        const int viewId = _loKitDocument->getView();
        session->setViewId(viewId);

        _sessionUserInfo[viewId] = UserInfo(session->getViewUserId(), session->getViewUserName(),
                                            session->getViewUserExtraInfo(), session->isReadOnly());

        _loKitDocument->setViewLanguage(viewId, lang.c_str());

        _viewIdToCallbackDescr.emplace(viewId,
                                       std::unique_ptr<CallbackDescriptor>(new CallbackDescriptor({ this, viewId })));
        _loKitDocument->registerCallback(ViewCallback, _viewIdToCallbackDescr[viewId].get());

        const int viewCount = _loKitDocument->getViewsCount();
        LOG_INF("Document url [" << anonymizeUrl(_url) << "] for session [" <<
                sessionId << "] loaded view [" << viewId << "]. Have " <<
                viewCount << " view" << (viewCount != 1 ? "s." : "."));

        if (session->hasWatermark())
            session->_docWatermark.reset(new Watermark(_loKitDocument, watermarkText, session));
        session->recalcCanonicalViewId(_sessions);

        return _loKitDocument;
    }

    bool forwardToChild(const std::string& prefix, const std::vector<char>& payload)
    {
        assert(payload.size() > prefix.size());

        // Remove the prefix and trim.
        size_t index = prefix.size();
        for ( ; index < payload.size(); ++index)
        {
            if (payload[index] != ' ')
            {
                break;
            }
        }

        const char* data = payload.data() + index;
        size_t size = payload.size() - index;

        std::string name;
        std::string sessionId;
        if (LOOLProtocol::parseNameValuePair(prefix, name, sessionId, '-') && name == "child")
        {
            std::unique_lock<std::mutex> lock(_mutex);

            const auto it = _sessions.find(sessionId);
            if (it != _sessions.end())
            {
                std::shared_ptr<ChildSession> session = it->second;

                static const std::string disconnect("disconnect");
                if (size == disconnect.size() &&
                    strncmp(data, disconnect.data(), disconnect.size()) == 0)
                {
                    if(session->getViewId() == _editorId) {
                        _editorId = -1;
                    }
                    LOG_DBG("Removing ChildSession [" << sessionId << "].");
                    _sessions.erase(it);
                    const size_t count = _sessions.size();
                    LOG_DBG("Have " << count << " child" << (count == 1 ? "" : "ren") <<
                            " after removing ChildSession [" << sessionId << "].");

                    // No longer needed, and allow session dtor to take it.
                    lock.unlock();
                    session.reset();
                    return true;
                }

                // No longer needed, and allow the handler to take it.
                lock.unlock();
                if (session)
                {
                    std::vector<char> vect(size);
                    vect.assign(data, data + size);

                    // TODO this is probably wrong...
                    session->handleMessage(/* fin = */ false, WSOpCode::Binary, vect);
                    return true;
                }
            }

            const std::string abbrMessage = getAbbreviatedMessage(data, size);
            LOG_WRN("Child session [" << sessionId << "] not found to forward message: " << abbrMessage);
        }
        else
        {
            LOG_ERR("Failed to parse prefix of forward-to-child message: " << prefix);
        }

        return false;
    }

    static std::string makeRenderParams(const std::string& renderOpts, const std::string& userName)
    {
        Object::Ptr renderOptsObj;

        // Fill the object with renderoptions, if any
        if (!renderOpts.empty())
        {
            Parser parser;
            Poco::Dynamic::Var var = parser.parse(renderOpts);
            renderOptsObj = var.extract<Object::Ptr>();
        }
        else if (!userName.empty())
        {
            renderOptsObj = new Object();
        }

        // Append name of the user, if any, who opened the document to rendering options
        if (!userName.empty())
        {
            Object::Ptr authorObj = new Object();
            authorObj->set("type", "string");
            authorObj->set("value", userName);
            renderOptsObj->set(".uno:Author", authorObj);
        }

        if (renderOptsObj)
        {
            std::ostringstream ossRenderOpts;
            renderOptsObj->stringify(ossRenderOpts);
            return ossRenderOpts.str();
        }

        return std::string();
    }

    void run() override
    {
        Util::setThreadName("kitbroker_" + _docId);

        LOG_DBG("Thread started.");
#ifndef MOBILEAPP
        // Update memory stats and editor every 5 seconds.
        const int memStatsPeriodMs = 5000;
        auto lastMemStatsTime = std::chrono::steady_clock::now();
        sendTextFrame(Util::getMemoryStats(ProcSMapsFile));
#endif
        try
        {
            while (!_stop && !TerminationFlag)
            {
                const TileQueue::Payload input = _tileQueue->get(POLL_TIMEOUT_MS * 2);
                if (input.empty())
                {
#ifndef MOBILEAPP
                    auto duration = (std::chrono::steady_clock::now() - lastMemStatsTime);
                    std::chrono::milliseconds::rep durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
                    if (durationMs > memStatsPeriodMs)
                    {
                        sendTextFrame(Util::getMemoryStats(ProcSMapsFile));
                        lastMemStatsTime = std::chrono::steady_clock::now();
                    }
#endif
                    continue;
                }

                LOG_TRC("Kit Recv " << LOOLProtocol::getAbbreviatedMessage(input));

                if (_stop || TerminationFlag)
                {
                    LOG_INF("_stop or TerminationFlag is set, breaking out of loop");
                    break;
                }

                const std::vector<std::string> tokens = LOOLProtocol::tokenize(input.data(), input.size());

                if (tokens[0] == "eof")
                {
                    LOG_INF("Received EOF. Finishing.");
                    break;
                }

                if (tokens[0] == "tile")
                {
                    renderTile(tokens);
                }
                else if (tokens[0] == "tilecombine")
                {
                    renderCombinedTiles(tokens);
                }
                else if (LOOLProtocol::getFirstToken(tokens[0], '-') == "child")
                {
                    forwardToChild(tokens[0], input);
                }
                else if (tokens[0] == "callback")
                {
                    if (tokens.size() >= 3)
                    {
                        bool broadcast = false;
                        int viewId = -1;
                        int exceptViewId = -1;

                        const std::string& target = tokens[1];
                        if (target == "all")
                        {
                            broadcast = true;
                        }
                        else if (LOOLProtocol::matchPrefix("except-", target))
                        {
                            exceptViewId = std::stoi(target.substr(7));
                            broadcast = true;
                        }
                        else
                        {
                            viewId = std::stoi(target);
                        }

                        const int type = std::stoi(tokens[2]);

                        // payload is the rest of the message
                        const size_t offset = tokens[0].length() + tokens[1].length() + tokens[2].length() + 3; // + delims
                        const std::string payload(input.data() + offset, input.size() - offset);

                        // Forward the callback to the same view, demultiplexing is done by the LibreOffice core.
                        // TODO: replace with a map to be faster.
                        bool isFound = false;
                        for (auto& it : _sessions)
                        {
                            std::shared_ptr<ChildSession> session = it.second;
                            if (session && ((broadcast && (session->getViewId() != exceptViewId)) || (!broadcast && (session->getViewId() == viewId))))
                            {
                                if (!it.second->isCloseFrame())
                                {
                                    isFound = true;
                                    session->loKitCallback(type, payload);
                                }
                                else
                                {
                                    LOG_ERR("Session-thread of session [" << session->getId() << "] for view [" <<
                                            viewId << "] is not running. Dropping [" << LOKitHelper::kitCallbackTypeToString(type) <<
                                            "] payload [" << payload << "].");
                                }

                                if (!broadcast)
                                {
                                    break;
                                }
                            }
                        }

                        if (!isFound)
                        {
                            LOG_WRN("Document::ViewCallback. Session [" << viewId <<
                                    "] is no longer active to process [" << LOKitHelper::kitCallbackTypeToString(type) <<
                                    "] [" << payload << "] message to Master Session.");
                        }
                    }
                    else
                    {
                        LOG_ERR("Invalid callback message: [" << LOOLProtocol::getAbbreviatedMessage(input) << "].");
                    }
                }
                else
                {
                    LOG_ERR("Unexpected request: [" << LOOLProtocol::getAbbreviatedMessage(input) << "].");
                }
            }
        }
        catch (const std::exception& exc)
        {
            LOG_ERR("QueueHandler::run: Exception: " << exc.what());
        }
        catch (...)
        {
            LOG_FTL("QueueHandler::run: Unknown exception");
        }

        LOG_DBG("Thread finished.");
    }

    /// Return access to the lok::Office instance.
    std::shared_ptr<lok::Office> getLOKit() override
    {
        return _loKit;
    }

    /// Return access to the lok::Document instance.
    std::shared_ptr<lok::Document> getLOKitDocument() override
    {
        if (!_loKitDocument)
        {
            LOG_ERR("Document [" << _docKey << "] is not loaded.");
            throw std::runtime_error("Document " + _docKey + " is not loaded.");
        }

        return _loKitDocument;
    }

    /// Return access to the lok::Document instance.
    std::mutex& getDocumentMutex() override
    {
        return _documentMutex;
    }

    std::string getObfuscatedFileId() override
    {
        return _obfuscatedFileId;
    }

private:
    std::shared_ptr<lok::Office> _loKit;
    const std::string _jailId;
    /// URL-based key. May be repeated during the lifetime of WSD.
    const std::string _docKey;
    /// Short numerical ID. Unique during the lifetime of WSD.
    const std::string _docId;
    const std::string _url;
    const std::string _obfuscatedFileId;
    std::string _jailedUrl;
    std::string _renderOpts;

    std::shared_ptr<lok::Document> _loKitDocument;
    std::shared_ptr<TileQueue> _tileQueue;
    SocketPoll& _socketPoll;
    std::shared_ptr<WebSocketHandler> _websocketHandler;
    PngCache _pngCache;

    // Document password provided
    std::string _docPassword;
    // Whether password was provided or not
    bool _haveDocPassword;
    // Whether document is password protected
    bool _isDocPasswordProtected;
    // Whether password is required to view the document, or modify it
    PasswordType _docPasswordType;

    std::atomic<bool> _stop;
    mutable std::mutex _mutex;

    /// Mutex guarding the lok::Document so that we can lock operations
    /// like setting a view followed by a tile render, etc.
    std::mutex _documentMutex;

    std::condition_variable _cvLoading;
    std::atomic_size_t _isLoading;
    int _editorId;
    bool _editorChangeWarning;
    std::map<int, std::unique_ptr<CallbackDescriptor>> _viewIdToCallbackDescr;
    SessionMap<ChildSession> _sessions;

    std::map<int, std::chrono::steady_clock::time_point> _lastUpdatedAt;
    std::map<int, int> _speedCount;
    /// For showing disconnected user info in the doc repair dialog.
    std::map<int, UserInfo> _sessionUserInfo;
    Poco::Thread _callbackThread;
};

class KitWebSocketHandler final : public WebSocketHandler, public std::enable_shared_from_this<KitWebSocketHandler>
{
    std::shared_ptr<TileQueue> _queue;
    std::string _socketName;
    std::shared_ptr<lok::Office> _loKit;
    std::string _jailId;
    SocketPoll& _socketPoll;

public:
    KitWebSocketHandler(const std::string& socketName, const std::shared_ptr<lok::Office>& loKit, const std::string& jailId, SocketPoll& socketPoll) :
        WebSocketHandler(/* isClient = */ true, /* isMasking */ false),
        _queue(std::make_shared<TileQueue>()),
        _socketName(socketName),
        _loKit(loKit),
        _jailId(jailId),
        _socketPoll(socketPoll)
    {
    }

protected:
    void handleMessage(bool /*fin*/, WSOpCode /*code*/, std::vector<char>& data) override
    {
        std::string message(data.data(), data.size());

#ifndef MOBILEAPP
        if (UnitKit::get().filterKitMessage(this, message))
            return;
#endif
        std::vector<std::string> tokens = LOOLProtocol::tokenize(message);
        Log::StreamLogger logger = Log::debug();
        if (logger.enabled())
        {
            logger << _socketName << ": recv [";
            for (const std::string& token : tokens)
            {
                // Don't log PII, there are anonymized versions that get logged instead.
                if (Util::startsWith(token, "jail") ||
                    Util::startsWith(token, "author") ||
                    Util::startsWith(token, "name") ||
                    Util::startsWith(token, "url"))
                    continue;

                logger << token << ' ';
            }

            LOG_END(logger, true);
        }

        // Note: Syntax or parsing errors here are unexpected and fatal.
        if (TerminationFlag)
        {
            LOG_DBG("Too late, TerminationFlag is set, we're going down");
        }
        else if (tokens[0] == "session")
        {
            const std::string& sessionId = tokens[1];
            const std::string& docKey = tokens[2];
            const std::string& docId = tokens[3];
            const std::string fileId = Util::getFilenameFromURL(docKey);
            Util::mapAnonymized(fileId, fileId); // Identity mapping, since fileId is already obfuscated

            std::string url;
            URI::decode(docKey, url);
            LOG_INF("New session [" << sessionId << "] request on url [" << url << "].");
            Util::setThreadName("kitbroker_" + docId);

            if (!document)
            {
                // Creating the Document object starts a thread running Document::run().
                document = std::make_shared<Document>(_loKit, _jailId, docKey, docId, url, _queue, _socketPoll, shared_from_this());
            }

            // Validate and create session.
            if (!(url == document->getUrl() && document->createSession(sessionId)))
            {
                LOG_DBG("CreateSession failed.");
            }
        }
        else if (tokens[0] == "exit")
        {
            LOG_WRN("Setting TerminationFlag due to 'exit' command from parent.");
            TerminationFlag = true;
            document.reset();
        }
        else if (tokens[0] == "tile" || tokens[0] == "tilecombine" || tokens[0] == "canceltiles" ||
                tokens[0] == "paintwindow" ||
                LOOLProtocol::getFirstToken(tokens[0], '-') == "child")
        {
            if (document)
            {
                _queue->put(message);
            }
            else
            {
                LOG_WRN("No document while processing " << tokens[0] << " request.");
            }
        }
        else if (tokens.size() == 3 && tokens[0] == "setconfig")
        {
#ifndef MOBILEAPP
            // Currently onlly rlimit entries are supported.
            if (!Rlimit::handleSetrlimitCommand(tokens))
            {
                LOG_ERR("Unknown setconfig command: " << message);
            }
#endif
        }
        else
        {
            LOG_ERR("Bad or unknown token [" << tokens[0] << "]");
        }
    }

    void onDisconnect() override
    {
#ifndef MOBILEAPP
        LOG_WRN("Kit connection lost without exit arriving from wsd. Setting TerminationFlag");
        TerminationFlag = true;
#endif
    }
};

void documentViewCallback(const int type, const char* payload, void* data)
{
    Document::ViewCallback(type, payload, data);
}

#ifndef BUILDING_TESTS

void lokit_main(
#ifndef MOBILEAPP
                const std::string& childRoot,
                const std::string& jailId,
                const std::string& sysTemplate,
                const std::string& loTemplate,
                const std::string& loSubPath,
                bool noCapabilities,
                bool noSeccomp,
                bool queryVersion,
                bool displayVersion,
#else
                const std::string& documentUri,
                int docBrokerSocket,
#endif
                size_t spareKitId
                )
{
#ifndef MOBILEAPP

#ifndef FUZZER
    SigUtil::setFatalSignals();
    SigUtil::setTerminationSignals();
#endif

    Util::setThreadName("kit_spare_" + Util::encodeId(spareKitId, 3));

    // Reinitialize logging when forked.
    const bool logToFile = std::getenv("LOOL_LOGFILE");
    const char* logFilename = std::getenv("LOOL_LOGFILENAME");
    const char* logLevel = std::getenv("LOOL_LOGLEVEL");
    const char* logColor = std::getenv("LOOL_LOGCOLOR");
    std::map<std::string, std::string> logProperties;
    if (logToFile && logFilename)
    {
        logProperties["path"] = std::string(logFilename);
    }

    Util::rng::reseed();

    const std::string LogLevel = logLevel ? logLevel : "trace";
    const bool bTraceStartup = (std::getenv("LOOL_TRACE_STARTUP") != nullptr);
    Log::initialize("kit", bTraceStartup ? "trace" : logLevel, logColor != nullptr, logToFile, logProperties);
    if (bTraceStartup && LogLevel != "trace")
    {
        LOG_INF("Setting log-level to [trace] and delaying setting to configured [" << LogLevel << "] until after Kit initialization.");
    }

    AnonymizeFilenames = std::getenv("LOOL_ANONYMIZE_FILENAMES") != nullptr;
    LOG_INF("Filename anonymization is " << (AnonymizeFilenames ? "enabled." : "disabled."));
    AnonymizeUsernames = std::getenv("LOOL_ANONYMIZE_USERNAMES") != nullptr;
    LOG_INF("Username anonymization is " << (AnonymizeUsernames ? "enabled." : "disabled."));

    assert(!childRoot.empty());
    assert(!sysTemplate.empty());
    assert(!loTemplate.empty());
    assert(!loSubPath.empty());

    LOG_DBG("Process started.");

    std::string userdir_url;
    std::string instdir_path;

    // lokit's destroy typically throws from
    // framework/source/services/modulemanager.cxx:198
    // So we insure it lives until std::_Exit is called.
    std::shared_ptr<lok::Office> loKit;
    Path jailPath;
    bool bRunInsideJail = !noCapabilities;
#else
    AnonymizeFilenames = false;
    AnonymizeUsernames = false;
#endif // MOBILEAPP

    try
    {
#ifndef MOBILEAPP
        jailPath = Path::forDirectory(childRoot + "/" + jailId);
        LOG_INF("Jail path: " << jailPath.toString());
        File(jailPath).createDirectories();
        chmod(jailPath.toString().c_str(), S_IXUSR | S_IWUSR | S_IRUSR);

        if (bRunInsideJail)
        {
            userdir_url = "file:///user";
            instdir_path = "/" + loSubPath + "/program";

            // Create a symlink inside the jailPath so that the absolute pathname loTemplate, when
            // interpreted inside a chroot at jailPath, points to loSubPath (relative to the chroot).
            symlinkPathToJail(jailPath, loTemplate, loSubPath);

            // Font paths can end up as realpaths so match that too.
            char *resolved = realpath(loTemplate.c_str(), nullptr);
            if (resolved)
            {
                if (strcmp(loTemplate.c_str(), resolved) != 0)
                    symlinkPathToJail(jailPath, std::string(resolved), loSubPath);
                free (resolved);
            }

            Path jailLOInstallation(jailPath, loSubPath);
            jailLOInstallation.makeDirectory();
            File(jailLOInstallation).createDirectory();

            // Copy (link) LO installation and other necessary files into it from the template.
            bool bLoopMounted = false;
            if (std::getenv("LOOL_BIND_MOUNT"))
            {
                Path usrSrcPath(sysTemplate, "usr");
                Path usrDestPath(jailPath, "usr");
                File(usrDestPath).createDirectory();
                std::string mountCommand =
                    std::string("ndcodfwebmount ") +
                    usrSrcPath.toString() +
                    std::string(" ") +
                    usrDestPath.toString();
                LOG_DBG("Initializing jail bind mount.");
                bLoopMounted = !system(mountCommand.c_str());
                LOG_DBG("Initialized jail bind mount.");
            }
            linkOrCopy(sysTemplate, jailPath,
                       bLoopMounted ? LinkOrCopyType::NoUsr : LinkOrCopyType::All);
            linkOrCopy(loTemplate, jailLOInstallation, LinkOrCopyType::LO);

            // Copy some needed files - makes the networking work in the
            // chroot
            const std::initializer_list<const char*> files = {"/etc/passwd", "/etc/group", "/etc/host.conf", "/etc/hosts", "/etc/nsswitch.conf", "/etc/resolv.conf", "/etc/locale.conf", "/etc/localtime"};
            for (const auto& filename : files)
            {
                const Poco::Path etcPath = Path(jailPath, filename);
                const std::string etcPathString = etcPath.toString();
                if (File(filename).exists() && !File(etcPathString).exists() )
                    linkOrCopyFile(filename, etcPath);
            }

            LOG_DBG("Initialized jail files.");

            // Create the urandom and random devices
            File(Path(jailPath, "/dev")).createDirectory();
            if (mknod((jailPath.toString() + "/dev/random").c_str(),
                      S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH,
                      makedev(1, 8)) != 0)
            {
                LOG_SYS("mknod(" << jailPath.toString() << "/dev/random) failed.");
            }

            if (mknod((jailPath.toString() + "/dev/urandom").c_str(),
                      S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH,
                      makedev(1, 9)) != 0)
            {
                LOG_SYS("mknod(" << jailPath.toString() << "/dev/urandom) failed.");
            }

            ProcSMapsFile = fopen("/proc/self/smaps", "r");
            if (ProcSMapsFile == nullptr)
            {
                LOG_SYS("Failed to symlink /proc/self/smaps. Memory stats will be missing.");
            }

            LOG_INF("chroot(\"" << jailPath.toString() << "\")");
            if (chroot(jailPath.toString().c_str()) == -1)
            {
                LOG_SFL("chroot(\"" << jailPath.toString() << "\") failed.");
                Log::shutdown();
                std::_Exit(Application::EXIT_SOFTWARE);
            }

            if (chdir("/") == -1)
            {
                LOG_SFL("chdir(\"/\") in jail failed.");
                Log::shutdown();
                std::_Exit(Application::EXIT_SOFTWARE);
            }

            dropCapability(CAP_SYS_CHROOT);
            dropCapability(CAP_MKNOD);
            dropCapability(CAP_FOWNER);

            LOG_DBG("Initialized jail nodes, dropped caps.");
        }
        else // noCapabilities set
        {
            LOG_ERR("Security warning - using template " << loTemplate << " as install subpath - skipping chroot jail setup");
            userdir_url = "file:///" + jailPath.toString() + "/user";
            instdir_path = "/" + loTemplate + "/program";
        }

        // hard-random tmpdir inside the jail / root
        std::string tmpSubdir = Util::createRandomTmpDir();
        ::setenv("TMPDIR", tmpSubdir.c_str(), 1);

        {
            const char *instdir = instdir_path.c_str();
            const char *userdir = userdir_url.c_str();
#ifndef KIT_IN_PROCESS
            LibreOfficeKit* kit = UnitKit::get().lok_init(instdir, userdir);
#else
            LibreOfficeKit* kit = nullptr;
#ifdef FUZZER
            if (LOOLWSD::DummyLOK)
                kit = dummy_lok_init_2(instdir, userdir);
#endif
#endif
            if (!kit)
            {
                kit = (initFunction ? initFunction(instdir, userdir) : lok_init_2(instdir, userdir));
            }

            loKit = std::make_shared<lok::Office>(kit);
            if (!loKit)
            {
                LOG_FTL("LibreOfficeKit initialization failed. Exiting.");
                Log::shutdown();
                std::_Exit(Application::EXIT_SOFTWARE);
            }
        }

        // Lock down the syscalls that can be used
        if (!Seccomp::lockdown(Seccomp::Type::KIT))
        {
            if (!noSeccomp)
            {
                LOG_FTL("LibreOfficeKit seccomp security lockdown failed. Exiting.");
                Log::shutdown();
                std::_Exit(Application::EXIT_SOFTWARE);
            }

            LOG_ERR("LibreOfficeKit seccomp security lockdown failed, but configured to continue. "
                    "You are running in a significantly less secure mode.");
        }

        rlimit rlim = { 0, 0 };
        if (getrlimit(RLIMIT_AS, &rlim) == 0)
            LOG_INF("RLIMIT_AS is " << Util::getHumanizedBytes(rlim.rlim_max) << " (" << rlim.rlim_max << " bytes)");
        else
            LOG_SYS("Failed to get RLIMIT_AS.");

        if (getrlimit(RLIMIT_STACK, &rlim) == 0)
            LOG_INF("RLIMIT_STACK is " << Util::getHumanizedBytes(rlim.rlim_max) << " (" << rlim.rlim_max << " bytes)");
        else
            LOG_SYS("Failed to get RLIMIT_STACK.");

        if (getrlimit(RLIMIT_FSIZE, &rlim) == 0)
            LOG_INF("RLIMIT_FSIZE is " << Util::getHumanizedBytes(rlim.rlim_max) << " (" << rlim.rlim_max << " bytes)");
        else
            LOG_SYS("Failed to get RLIMIT_FSIZE.");

        if (getrlimit(RLIMIT_NOFILE, &rlim) == 0)
            LOG_INF("RLIMIT_NOFILE is " << rlim.rlim_max << " files.");
        else
            LOG_SYS("Failed to get RLIMIT_NOFILE.");

        LOG_INF("Process is ready.");

        static const std::string pid = std::to_string(Process::id());

        Poco::URI uri("ws://127.0.0.1");
        uri.setPort(MasterPortNumber);
        uri.setPath(NEW_CHILD_URI);
        uri.addQueryParameter("pid", std::to_string(Process::id()));
        uri.addQueryParameter("jailid", jailId);

        if (queryVersion)
        {
            char* versionInfo = loKit->getVersionInfo();
            std::string versionString(versionInfo);
            if (displayVersion)
                std::cout << "office version details: " << versionString << std::endl;
            uri.addQueryParameter("version", versionString);
            free(versionInfo);
        }

#else // MOBILEAPP

#ifdef __linux
        Poco::URI userInstallationURI("file", LO_PATH);
        LibreOfficeKit *kit = lok_init_2(LO_PATH "/program", userInstallationURI.toString().c_str());
#else
        LibreOfficeKit *kit = lok_init_2(nullptr, nullptr);
#endif

        assert(kit);

        std::shared_ptr<lok::Office> loKit = std::make_shared<lok::Office>(kit);
        assert(loKit);

        LOOLWSD::LOKitVersion = loKit->getVersionInfo();

        // Dummies
        const std::string jailId = "jailid";
        const std::string pid = "101";

#endif // MOBILEAPP

        SocketPoll mainKit("kit");
        mainKit.runOnClientThread(); // We will do the polling on this thread.

#ifndef MOBILEAPP
        mainKit.insertNewWebSocketSync(uri, std::make_shared<KitWebSocketHandler>("child_ws_" + pid, loKit, jailId, mainKit));
#else
        mainKit.insertNewWebSocketSync(docBrokerSocket, std::make_shared<KitWebSocketHandler>("child_ws_" + pid, loKit, jailId, mainKit));
#endif

        LOG_INF("New kit client websocket inserted.");

#ifndef MOBILEAPP
        if (bTraceStartup && LogLevel != "trace")
        {
            LOG_INF("Kit initialization complete: setting log-level to [" << LogLevel << "] as configured.");
            Log::logger().setLevel(LogLevel);
        }
#endif

        while (!TerminationFlag)
        {
            mainKit.poll(SocketPoll::DefaultPollTimeoutMs);

#ifndef MOBILEAPP
            if (document && document->purgeSessions() == 0)
            {
                LOG_WRN("Last session discarded. Setting TerminationFlag");
                TerminationFlag = true;
            }
#endif
        }

        LOG_INF("Kit poll terminated.");

#ifdef MOBILEAPP
        SocketPoll::wakeupWorld();
#endif

        // Let forkit handle the jail cleanup.
    }
    catch (const Exception& exc)
    {
        LOG_ERR("Poco Exception: " << exc.displayText() <<
                (exc.nested() ? " (" + exc.nested()->displayText() + ")" : ""));
    }
    catch (const std::exception& exc)
    {
        LOG_ERR("Exception: " << exc.what());
    }

#ifndef MOBILEAPP

    // Trap the signal handler, if invoked,
    // to prevent exiting.
    LOG_INF("Process finished.");
    Log::shutdown();
    std::unique_lock<std::mutex> lock(SigHandlerTrap);
    std::_Exit(Application::EXIT_OK);

#endif
}
#endif

std::string anonymizeUrl(const std::string& url)
{
#ifndef BUILDING_TESTS
    return AnonymizeFilenames ? Util::anonymizeUrl(url) : url;
#else
    return url;
#endif
}

#ifndef MOBILEAPP

/// Initializes LibreOfficeKit for cross-fork re-use.
bool globalPreinit(const std::string &loTemplate)
{
#ifdef FUZZER
    if (LOOLWSD::DummyLOK)
        return true;
#endif
    const std::string libSofficeapp = loTemplate + "/program/" LIB_SOFFICEAPP;
    const std::string libMerged = loTemplate + "/program/" LIB_MERGED;

    std::string loadedLibrary;
    void *handle;
    if (File(libMerged).exists())
    {
        LOG_TRC("dlopen(" << libMerged << ", RTLD_GLOBAL|RTLD_NOW)");
        handle = dlopen(libMerged.c_str(), RTLD_GLOBAL|RTLD_NOW);
        if (!handle)
        {
            LOG_FTL("Failed to load " << libMerged << ": " << dlerror());
            return false;
        }
        loadedLibrary = libMerged;
    }
    else
    {
        if (File(libSofficeapp).exists())
        {
            LOG_TRC("dlopen(" << libSofficeapp << ", RTLD_GLOBAL|RTLD_NOW)");
            handle = dlopen(libSofficeapp.c_str(), RTLD_GLOBAL|RTLD_NOW);
            if (!handle)
            {
                LOG_FTL("Failed to load " << libSofficeapp << ": " << dlerror());
                return false;
            }
            loadedLibrary = libSofficeapp;
        }
        else
        {
            LOG_FTL("Neither " << libSofficeapp << " or " << libMerged << " exist.");
            return false;
        }
    }

    LokHookPreInit* preInit = reinterpret_cast<LokHookPreInit *>(dlsym(handle, "lok_preinit"));
    if (!preInit)
    {
        LOG_FTL("No lok_preinit symbol in " << loadedLibrary << ": " << dlerror());
        return false;
    }

    initFunction = reinterpret_cast<LokHookFunction2 *>(dlsym(handle, "libreofficekit_hook_2"));
    if (!initFunction)
    {
        LOG_FTL("No libreofficekit_hook_2 symbol in " << loadedLibrary << ": " << dlerror());
    }

    // Disable problematic components that may be present from a
    // desktop or developer's install if env. var not set.
    ::setenv("UNODISABLELIBRARY",
             "abp avmediagst avmediavlc cmdmail losessioninstall OGLTrans PresenterScreen "
             "syssh ucpftp1 ucpgio1 ucphier1 ucpimage updatecheckui updatefeed updchk"
             // Database
             "dbaxml dbmm dbp dbu deployment firebird_sdbc mork "
             "mysql mysqlc odbc postgresql-sdbc postgresql-sdbc-impl sdbc2 sdbt"
             // Java
             "javaloader javavm jdbc rpt rptui rptxml ",
             0 /* no overwrite */);

    LOG_TRC("Invoking lok_preinit(" << loTemplate << "/program\", \"file:///user\")");
    const auto start = std::chrono::steady_clock::now();
    if (preInit((loTemplate + "/program").c_str(), "file:///user") != 0)
    {
        LOG_FTL("lok_preinit() in " << loadedLibrary << " failed");
        return false;
    }

    LOG_TRC("Finished lok_preinit(" << loTemplate << "/program\", \"file:///user\") in " <<
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() <<
            " ms.");
    return true;
}

/// Anonymize usernames.
std::string anonymizeUsername(const std::string& username)
{
#ifndef BUILDING_TESTS
    return AnonymizeUsernames ? Util::anonymize(username) : username;
#else
    return username;
#endif
}

#if !defined(BUILDING_TESTS) && !defined(KIT_IN_PROCESS)
namespace Util
{

void alertAllUsers(const std::string& msg)
{
    document->sendTextFrame(msg);
}

void alertAllUsers(const std::string& cmd, const std::string& kind)
{
    alertAllUsers("errortoall: cmd=" + cmd + " kind=" + kind);
}

}
#endif

#endif // MOBILEAPP

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
