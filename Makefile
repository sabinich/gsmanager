VERSION = 5.1.0
MGR = gsmgr
LANGS = ru
LIB += gsmgr
SUBDIR = main teamfortress minecraft cs16 gta installer mini sbin ostemplate-lib

gsmgr_SOURCES = gsmgr.cpp dbobject.cpp user.cpp nodes.cpp gsutils.cpp \
				install.cpp gamedistr.cpp nodesync.cpp nodestat.cpp stat_raw.cpp stat_utils.cpp gsstatutils.cpp teamspeak.cpp screen.cpp stat.cpp
gsmgr_LDADD = -lbase -lmgrdb

PKG+=pkggsmgr
pkggsmgr_SOURCES=mp_gsmgr.cpp
pkggsmgr_DLIBS=mgr parser ispapi mgrpkg mgrdb scheduler
pkggsmgr_GROUP=mpmgr

WRAPPER += gsinstalldist
gsinstalldist_SOURCES = gsinstalldist.cpp cmdarg.cpp
gsinstalldist_LDADD = -lmgr -lbase

WRAPPER += gscopydist
gscopydist_SOURCES = gscopydistr.cpp cmdarg.cpp
gscopydist_LDADD = -lmgr -lbase

WRAPPER += migratesrv
migratesrv_SOURCES = migratesrv.cpp cmdarg.cpp
migratesrv_LDADD = -lmgr -lbase
include ../isp.mk
