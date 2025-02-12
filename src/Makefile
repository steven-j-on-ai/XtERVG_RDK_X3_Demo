CXX = g++
CXX_FLAGS = -g -O3
CMP_FLAGS = $(CXX_FLAGS) -c
EXE_FLAGS = $(CXX_FLAGS) -o

XFTP = xftp
MONITOR = monitor
EXECS = $(XFTP) $(MONITOR)
XFTP_OBJS = xftp.o string_split.o sqlite.o http.o thread.o sps_parser.o frame_cir_buff.o annotation_info.o fcos_post_process.o
MONITOR_OBJS = monitor.o config_file.o string_split.o sqlite.o http.o

LIB_PATH := ../libs /usr/local/xt/lib
LIB_PATH += /usr/lib /usr/lib/hbmedia
LIBS := $(addprefix -L,$(LIB_PATH))
LIBS += -lxftp_live -lxttp_rtc -lrtsp_utils -lcurl -lcjson -lsqlite3 -pthread -lm -lstdc++
LIBS += -lspcdev -lhbmem -lion -lhbmedia -lvio

INC_PATH := ../include /usr/local/xt/include
INC_PATH += /usr/include /usr/include/vio
INCS := $(addprefix -I,$(INC_PATH))

INSTALL_DIR = /usr/local/xt/bin


.PHONY: all clean

all: $(EXECS)

$(XFTP): $(XFTP_OBJS)
	$(CXX) $(EXE_FLAGS) $@ $^ $(LIBS)

$(MONITOR): $(MONITOR_OBJS)
	$(CXX) $(EXE_FLAGS) $@ $^ $(LIBS)

%.o: %.cpp
	$(CXX) $(CMP_FLAGS) $(INCS) -o $@ $<

clean:
	rm -f $(EXECS) *.o

install:
	cp -f $(EXECS) $(INSTALL_DIR)
