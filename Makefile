SRCDIR ?= /opt/fpp/src
include $(SRCDIR)/makefiles/common/setup.mk
include $(SRCDIR)/makefiles/platform/*.mk

all: libfpp-osc.$(SHLIB_EXT)
debug: all

CFLAGS+=-I.
OBJECTS_fpp_osc_so += src/FPPOSC.o
LIBS_fpp_osc_so += -L$(SRCDIR) -lfpp -ljsoncpp -lhttpserver
CXXFLAGS_src/FPPOSC.o += -I$(SRCDIR)


%.o: %.cpp Makefile
	$(CCACHE) $(CC) $(CFLAGS) $(CXXFLAGS) $(CXXFLAGS_$@) -c $< -o $@

libfpp-osc.$(SHLIB_EXT): $(OBJECTS_fpp_osc_so) $(SRCDIR)/libfpp.$(SHLIB_EXT)
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_osc_so) $(LIBS_fpp_osc_so) $(LDFLAGS) -o $@

clean:
	rm -f libfpp-osc.so $(OBJECTS_fpp_osc_so)

