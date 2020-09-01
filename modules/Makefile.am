lib_LTLIBRARIES = libtemplaterepo.la

lib_path = $(abs_top_srcdir)
AM_CPPFLAGS = -I$(lib_path) \
	-I$(lib_path)/src/include \
	-I${lib_path}/common \
	-I${lib_path}/net \
	-I${lib_path}/wsd \
	-I${lib_path}/kit \
	-std=c++11 \
	-DDEV_DIR=\"$(abs_top_srcdir)/modules\"


libtemplaterepo_la_SOURCES = templaterepo.cpp \
							 templaterepo_file_db.cpp \
							 templaterepo_db.cpp
