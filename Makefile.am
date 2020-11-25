ETAGS=ctags -e

if ENABLE_IOSAPP

SUBDIRS = loleaflet

else

if ENABLE_GTKAPP

SUBDIRS = gtk loleaflet

else

SUBDIRS = . loleaflet extensions fonts modules

export ENABLE_DEBUG

bin_PROGRAMS = \
	ndcodfweb ndcodfwebforkit \
	ndcodfwebconvert ndcodfwebconfig

dist_bin_SCRIPTS = ndcodfweb-systemplate-setup

man_MANS = man/ndcodfweb.1 \
           man/ndcodfwebforkit.1 \
           man/ndcodfwebconvert.1 \
           man/ndcodfwebconfig.1 \
           man/ndcodfweb-systemplate-setup.1

dist_doc_DATA = wsd/README \
                wsd/README.vars \
                wsd/protocol.txt \
                wsd/reference.md

oxoolwsddatadir = @LOOLWSD_DATADIR@

oxoolwsddata_DATA = discovery.xml \
                   favicon.ico

sudoersdir = ${sysconfdir}/sudoers.d
sudoers_DATA = etc/sudoers.d/lool

crondir = ${sysconfdir}/cron.d
cron_DATA = etc/cron.d/ndcodfweb.cron

pamdir = ${sysconfdir}/pam.d
pam_DATA = etc/pam.d/ndcodfweb

servicesysconfigdir = ${sysconfdir}/sysconfig
servicesysconfig_DATA = etc/sysconfig/ndcodfweb

oxoolwsdconfigdir = @LOOLWSD_CONFIGDIR@

oxoolwsdconfig_DATA = ndcodfweb.xml \
		      ndcodfwebkitconfig.xcu \
		      etc/ca-chain.cert.pem \
		      etc/cert.pem \
		      etc/key.pem

#apache2configdir = ${sysconfdir}/apache2/conf-available

#apache2config_DATA = etc/apache2/oxoolwsd.conf

#nginxconfigdir = ${sysconfdir}/nginx/snippets

#nginxconfig_DATA = etc/nginx/oxoolwsd.conf

ACLOCAL_AMFLAGS = -I m4

# quick and easy for now.
include_paths = -I${top_srcdir}/common -I${top_srcdir}/net -I${top_srcdir}/wsd -I${top_srcdir}/kit ${ZLIB_CFLAGS} ${FONTCONFIG_CFLAGS}

AM_CPPFLAGS = -pthread -DLOOLWSD_DATADIR='"@LOOLWSD_DATADIR@"' \
	      -DLOOLWSD_CONFIGDIR='"@LOOLWSD_CONFIGDIR@"' \
	      -DDEBUG_ABSSRCDIR='"@abs_srcdir@"' \
	      ${include_paths}

if !ENABLE_DEBUG
AM_CPPFLAGS += -DNDEBUG
endif

AM_LDFLAGS = -pthread -Wl,-E,-rpath,/snap/ndcodfweb/current/usr/lib -lpam $(ZLIB_LIBS) ${FONTCONFIG_LIBS}

if ENABLE_SSL
AM_LDFLAGS += -lssl -lcrypto
else
ndcodfweb_fuzzer_LDADD = -lssl -lcrypto
ndcodfweb_LDADD = -lssl -lcrypto
ndcodfwebconfig_LDADD = -lssl -lcrypto
endif

ndcodfweb_fuzzer_CPPFLAGS = -DKIT_IN_PROCESS=1 -DFUZZER=1 -DTDOC=\"$(abs_top_srcdir)/test/data\" $(AM_CPPFLAGS)

AM_ETAGSFLAGS = --c++-kinds=+p --fields=+iaS --extra=+q -R --totals=yes --exclude=loleaflet *
AM_CTAGSFLAGS = $(AM_ETAGSFLAGS)

shared_sources = common/FileUtil.cpp \
                 common/IoUtil.cpp \
                 common/Log.cpp \
                 common/Protocol.cpp \
                 common/Session.cpp \
                 common/Seccomp.cpp \
                 common/MessageQueue.cpp \
                 common/SigUtil.cpp \
                 common/SpookyV2.cpp \
                 common/Unit.cpp \
                 common/Util.cpp \
                 common/Authorization.cpp \
                 net/DelaySocket.cpp \
                 net/Socket.cpp
if ENABLE_SSL
shared_sources += net/Ssl.cpp
endif

ndcodfweb_sources = common/Crypto.cpp \
                  wsd/Admin.cpp \
                  wsd/AdminModel.cpp \
                  wsd/Auth.cpp \
                  wsd/DocumentBroker.cpp \
                  wsd/LOOLWSD.cpp \
                  wsd/ClientSession.cpp \
                  wsd/FileServer.cpp \
                  wsd/Storage.cpp \
                  wsd/TileCache.cpp

ndcodfweb_SOURCES = $(ndcodfweb_sources) \
                  $(shared_sources)

noinst_PROGRAMS = clientnb \
                  connect \
                  lokitclient \
                  ndcodfweb_fuzzer \
                  ndcodfwebmap \
                  ndcodfwebstress \
                  ndcodfwebmount \
                  ndcodfwebsocketdump

connect_SOURCES = tools/Connect.cpp \
                  common/Log.cpp \
                  common/Protocol.cpp \
                  common/Util.cpp

lokitclient_SOURCES = common/IoUtil.cpp \
                      common/Log.cpp \
                      tools/KitClient.cpp \
                      common/Protocol.cpp \
                      common/Util.cpp

ndcodfwebforkit_sources = kit/ChildSession.cpp \
                     kit/ForKit.cpp \
                     kit/Kit.cpp

ndcodfwebforkit_SOURCES = $(ndcodfwebforkit_sources) \
                     $(shared_sources)

ndcodfweb_fuzzer_SOURCES = $(ndcodfweb_sources) \
                         $(ndcodfwebforkit_sources) \
                         $(shared_sources) \
                         kit/DummyLibreOfficeKit.cpp

clientnb_SOURCES = net/clientnb.cpp \
                   common/Log.cpp \
                   common/Util.cpp


ndcodfwebmount_SOURCES = tools/mount.cpp

ndcodfwebmap_SOURCES = tools/map.cpp

ndcodfwebconvert_SOURCES = tools/Tool.cpp

ndcodfwebstress_CPPFLAGS = -DTDOC=\"$(abs_top_srcdir)/test/data\" ${include_paths}
ndcodfwebstress_SOURCES = tools/Stress.cpp \
                     common/Protocol.cpp \
                     common/Log.cpp \
		     common/Util.cpp

ndcodfwebconfig_SOURCES = tools/Config.cpp \
		     common/Crypto.cpp \
		     common/Log.cpp \
		     common/Util.cpp

ndcodfwebsocketdump_SOURCES = tools/WebSocketDump.cpp \
			 $(shared_sources)

wsd_headers = wsd/Admin.hpp \
              wsd/AdminModel.hpp \
              wsd/Auth.hpp \
              wsd/ClientSession.hpp \
              wsd/DocumentBroker.hpp \
              wsd/Exceptions.hpp \
              wsd/FileServer.hpp \
              wsd/LOOLWSD.hpp \
              wsd/QueueHandler.hpp \
              wsd/SenderQueue.hpp \
              wsd/Storage.hpp \
              wsd/TileCache.hpp \
              wsd/TileDesc.hpp \
              wsd/TraceFile.hpp \
              wsd/UserMessages.hpp

shared_headers = common/Common.hpp \
                 common/Crypto.hpp \
                 common/JsonUtil.hpp \
                 common/IoUtil.hpp \
                 common/FileUtil.hpp \
                 common/Log.hpp \
                 common/LOOLWebSocket.hpp \
                 common/Protocol.hpp \
                 common/Seccomp.hpp \
                 common/Session.hpp \
                 common/Unit.hpp \
                 common/UnitHTTP.hpp \
                 common/Util.hpp \
                 common/Authorization.hpp \
                 common/MessageQueue.hpp \
                 common/Message.hpp \
                 common/Png.hpp \
                 common/Rectangle.hpp \
                 common/SigUtil.hpp \
                 common/security.h \
                 common/SpookyV2.h \
                 net/DelaySocket.hpp \
                 net/FakeSocket.hpp \
                 net/ServerSocket.hpp \
                 net/Socket.hpp \
                 net/WebSocketHandler.hpp \
                 tools/Replay.hpp
if ENABLE_SSL
shared_headers += net/Ssl.hpp \
                  net/SslSocket.hpp
endif

kit_headers = kit/ChildSession.hpp \
              kit/Delta.hpp \
              kit/DummyLibreOfficeKit.hpp \
              kit/Kit.hpp \
              kit/KitHelper.hpp \
              kit/Watermark.hpp

noinst_HEADERS = $(wsd_headers) $(shared_headers) $(kit_headers) \
                 bundled/include/LibreOfficeKit/LibreOfficeKit.h \
                 bundled/include/LibreOfficeKit/LibreOfficeKit.hxx \
                 bundled/include/LibreOfficeKit/LibreOfficeKitEnums.h \
                 bundled/include/LibreOfficeKit/LibreOfficeKitInit.h \
                 bundled/include/LibreOfficeKit/LibreOfficeKitTypes.h \
                 test/WopiTestServer.hpp \
                 test/countloolkits.hpp \
                 test/test.hpp \
                 test/helpers.hpp

dist-hook:
	git log -1 --format=%h > $(distdir)/dist_git_hash 2> /dev/null || rm $(distdir)/dist_git_hash

EXTRA_DIST = discovery.xml \
             favicon.ico \
             ndcodfwebkitconfig.xcu \
             ndcodfweb.spec \
             ndcodfweb.xml.in \
             ndcodfweb.service \
             debian/compat \
             debian/copyright \
             debian/ndcodfweb.preinst \
             debian/ndcodfweb.service \
             debian/rules \
             debian/source/format \
             debian/changelog \
             debian/control \
             debian/ndcodfweb.postinst \
             debian/ndcodfweb.postrm \
             etc/key.pem \
             etc/cert.pem \
             etc/ca-chain.cert.pem \
             etc/sudoers.d/lool \
             etc/cron.d/ndcodfweb.cron \
             etc/pam.d/ndcodfweb \
             etc/sysconfig/ndcodfweb \
             scripts/unocommands.py \
             $(man_MANS) \
			 modules


if HAVE_LO_PATH

SYSTEM_STAMP = @SYSTEMPLATE_PATH@/system_stamp

$(SYSTEM_STAMP) : ${top_srcdir}/ndcodfweb-systemplate-setup
	if test "z@SYSTEMPLATE_PATH@" != "z"; then rm -rf "@SYSTEMPLATE_PATH@"; fi
	${top_srcdir}/ndcodfweb-systemplate-setup "@SYSTEMPLATE_PATH@" "@LO_PATH@" "@EXTENSIONS_PATH@" && touch $@

@JAILS_PATH@ :
	mkdir -p $@

@TILECACHE_PATH@ :
	mkdir -p $@

clean-local:
	if test "z@JAILS_PATH@" != "z"; then rm -rf "@JAILS_PATH@"; fi
	if test "z@SYSTEMPLATE_PATH@" != "z"; then rm -rf "@SYSTEMPLATE_PATH@"; fi
	if test "z@TILECACHE_PATH@" != "z"; then rm -rf "@TILECACHE_PATH@"; fi

run: all @TILECACHE_PATH@ @JAILS_PATH@
	@echo "Launching ndcodfweb"
	@fc-cache "@LO_PATH@"/share/fonts/truetype
	@cp $(abs_top_srcdir)/test/data/hello.odt $(abs_top_srcdir)/test/data/hello-world.odt
	@cp $(abs_top_srcdir)/test/data/hello.ods $(abs_top_srcdir)/test/data/hello-world.ods
	@cp $(abs_top_srcdir)/test/data/hello.odp $(abs_top_srcdir)/test/data/hello-world.odp
	@cp $(abs_top_srcdir)/modules/.libs/libtemplaterepo.so.0.0.0 $(abs_top_srcdir)/libtemplaterepo.so
	@echo
	LOOL_SERVE_FROM_FS=1 ./ndcodfweb --o:sys_template_path="@SYSTEMPLATE_PATH@" --o:lo_template_path="@LO_PATH@" \
			  --o:file_server_root_path="$(abs_top_srcdir)" \
			  --o:child_root_path="@JAILS_PATH@" --o:storage.filesystem[@allow]=true \
			  --o:tile_cache_path="@TILECACHE_PATH@" \
			  --o:ssl.cert_file_path="$(abs_top_srcdir)/etc/cert.pem" \
			  --o:ssl.key_file_path="$(abs_top_srcdir)/etc/key.pem" \
			  --o:ssl.ca_file_path="$(abs_top_srcdir)/etc/ca-chain.cert.pem" \
			  --o:admin_console.username=admin --o:admin_console.password=admin \
			  --o:logging.file[@enable]=true --o:logging.level=trace

run-valgrind: all @TILECACHE_PATH@ @JAILS_PATH@
	@echo "Launching ndcodfweb under valgrind (but not forkit/ndcodfwebkit, yet)"
	@fc-cache "@LO_PATH@"/share/fonts/truetype
	@cp $(abs_top_srcdir)/test/data/hello.odt $(abs_top_srcdir)/test/data/hello-world.odt
	@cp $(abs_top_srcdir)/test/data/hello.ods $(abs_top_srcdir)/test/data/hello-world.ods
	@cp $(abs_top_srcdir)/test/data/hello.odp $(abs_top_srcdir)/test/data/hello-world.odp
	valgrind --tool=memcheck --trace-children=no -v --read-var-info=yes \
		./ndcodfweb --o:sys_template_path="@SYSTEMPLATE_PATH@" --o:lo_template_path="@LO_PATH@" \
			  --o:child_root_path="@JAILS_PATH@" --o:storage.filesystem[@allow]=true \
			  --o:tile_cache_path="@TILECACHE_PATH@" \
			  --o:ssl.cert_file_path="$(abs_top_srcdir)/etc/cert.pem" \
			  --o:ssl.key_file_path="$(abs_top_srcdir)/etc/key.pem" \
			  --o:ssl.ca_file_path="$(abs_top_srcdir)/etc/ca-chain.cert.pem" \
			  --o:admin_console.username=admin --o:admin_console.password=admin \
			  --o:logging.file[@enable]=false --o:logging.level=trace

run-gdb: all @TILECACHE_PATH@ @JAILS_PATH@
	@echo "Launching ndcodfweb under valgrind's callgrind"
	@fc-cache "@LO_PATH@"/share/fonts/truetype
	@cp $(abs_top_srcdir)/test/data/hello.odt $(abs_top_srcdir)/test/data/hello-world.odt
	@cp $(abs_top_srcdir)/test/data/hello.ods $(abs_top_srcdir)/test/data/hello-world.ods
	@cp $(abs_top_srcdir)/test/data/hello.odp $(abs_top_srcdir)/test/data/hello-world.odp
	gdb --tui --args \
		./ndcodfweb --o:security.capabilities="false" \
			  --o:sys_template_path="@SYSTEMPLATE_PATH@" --o:lo_template_path="@LO_PATH@" \
			  --o:child_root_path="@JAILS_PATH@" --o:storage.filesystem[@allow]=true \
			  --o:tile_cache_path="@TILECACHE_PATH@" \
			  --o:ssl.cert_file_path="$(abs_top_srcdir)/etc/cert.pem" \
			  --o:ssl.key_file_path="$(abs_top_srcdir)/etc/key.pem" \
			  --o:ssl.ca_file_path="$(abs_top_srcdir)/etc/ca-chain.cert.pem" \
			  --o:admin_console.username=admin --o:admin_console.password=admin \
			  --o:logging.file[@enable]=false --o:logging.level=error

run-callgrind: all @TILECACHE_PATH@ @JAILS_PATH@
	@echo "Launching ndcodfweb under valgrind's callgrind"
	@fc-cache "@LO_PATH@"/share/fonts/truetype
	@cp $(abs_top_srcdir)/test/data/hello.odt $(abs_top_srcdir)/test/data/hello-world.odt
	@cp $(abs_top_srcdir)/test/data/hello.ods $(abs_top_srcdir)/test/data/hello-world.ods
	@cp $(abs_top_srcdir)/test/data/hello.odp $(abs_top_srcdir)/test/data/hello-world.odp
	valgrind --tool=callgrind --simulate-cache=yes --dump-instr=yes --num-callers=50 --error-limit=no --trace-children=yes \
		./ndcodfweb --o:security.capabilities="false" \
			  --o:sys_template_path="@SYSTEMPLATE_PATH@" --o:lo_template_path="@LO_PATH@" \
			  --o:child_root_path="@JAILS_PATH@" --o:storage.filesystem[@allow]=true \
			  --o:tile_cache_path="@TILECACHE_PATH@" \
			  --o:ssl.cert_file_path="$(abs_top_srcdir)/etc/cert.pem" \
			  --o:ssl.key_file_path="$(abs_top_srcdir)/etc/key.pem" \
			  --o:ssl.ca_file_path="$(abs_top_srcdir)/etc/ca-chain.cert.pem" \
			  --o:admin_console.username=admin --o:admin_console.password=admin \
			  --o:logging.file[@enable]=false --o:logging.level=error

run-strace: all @TILECACHE_PATH@ @JAILS_PATH@
	@echo "Launching ndcodfweb under strace"
	@fc-cache "@LO_PATH@"/share/fonts/truetype
	@cp $(abs_top_srcdir)/test/data/hello.odt $(abs_top_srcdir)/test/data/hello-world.odt
	@cp $(abs_top_srcdir)/test/data/hello.ods $(abs_top_srcdir)/test/data/hello-world.ods
	@cp $(abs_top_srcdir)/test/data/hello.odp $(abs_top_srcdir)/test/data/hello-world.odp
	strace -o strace.log -f -tt -s 256 \
		./ndcodfweb --o:security.capabilities="false" \
			  --o:sys_template_path="@SYSTEMPLATE_PATH@" --o:lo_template_path="@LO_PATH@" \
			  --o:child_root_path="@JAILS_PATH@" --o:storage.filesystem[@allow]=true \
			  --o:tile_cache_path="@TILECACHE_PATH@" \
			  --o:ssl.cert_file_path="$(abs_top_srcdir)/etc/cert.pem" \
			  --o:ssl.key_file_path="$(abs_top_srcdir)/etc/key.pem" \
			  --o:ssl.ca_file_path="$(abs_top_srcdir)/etc/ca-chain.cert.pem" \
			  --o:admin_console.username=admin --o:admin_console.password=admin \
			  --o:logging.file[@enable]=false --o:logging.level=error

else

SYSTEM_STAMP =

endif

clean-cache cache-clean:
# Intentionally don't use "*" below... Avoid risk of accidentally running rm -rf /*
	test -n "@LOOLWSD_CACHEDIR@" && rm -rf "@LOOLWSD_CACHEDIR@"/[0-9a-f]
	test -n "@TILECACHE_PATH@" && rm -rf "@TILECACHE_PATH@"/[0-9a-f]

clang-tidy:
	parallel clang-tidy -header-filter=^$(PWD).* -- $(shell git ls-files|grep /.*cpp$$|grep -v test)

# After building ndcodfwebforkit, set its capabilities as required. Do it
# already after a plain 'make' to allow for testing without
# installing. When building for packaging, no need for this, as the
# capabilities won't survive packaging anyway. Instead, handle it when
# installing the RPM or Debian package.

all-local: ndcodfweb ndcodfwebforkit ndcodfwebmount @TILECACHE_PATH@ @JAILS_PATH@ $(SYSTEM_STAMP)
if ENABLE_SETCAP
	    sudo @SETCAP@ cap_dac_override,cap_net_bind_service=+ep ndcodfweb
	    sudo @SETCAP@ cap_fowner,cap_mknod,cap_sys_chroot=ep ndcodfwebforkit
	    sudo @SETCAP@ cap_sys_admin=ep ndcodfwebmount
else
	    echo "Skipping capability setting"
endif

# just run the build without any tests
build-nocheck: all-am

endif
endif
