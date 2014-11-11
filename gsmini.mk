VERSION = 5.1.0
MGR = gsmini
LANGS = ru
LIB += gsmini
XMLPATH = minixml
SUBDIR = main teamfortress minecraft cs16 gta installer mini csgo left4dead2 arma3 rust
XMLLIST += teamfortress/teamfortress.xml minecraft/minecraft.xml cs16/cs16.xml gta/gta.xml csgo/csgo.xml left4dead2/left4dead2.xml arma3/arma3.xml rust/rust.xml

gsmini_SOURCES = gsmini.cpp gamemodule.cpp instance.cpp distr.cpp minecraft_mini.cpp teamfortress_mini.cpp cs16_mini.cpp gta_mini.cpp utils.cpp \
				 screen.cpp portmgr.cpp steammodule.cpp nodestat_mini.cpp serverstat.cpp ftp.cpp csgo_mini.cpp left4dead2_mini.cpp arma3_mini.cpp rust_mini.cpp
gsmini_LDADD = -lbase

include ../isp.mk
