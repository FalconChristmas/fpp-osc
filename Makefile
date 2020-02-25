include /opt/fpp/src/makefiles/common/setup.mk
include /opt/fpp/src/makefiles/platform/*.mk

all: libfpp-osc.so
debug: all

CFLAGS+=-I.
OBJECTS_fpp_osc_so += src/FPPOSC.o src/tinyexpr.o
LIBS_fpp_osc_so += -L/opt/fpp/src -lfpp
CXXFLAGS_src/FPPOSC.o += -I/opt/fpp/src


%.o: %.cpp Makefile
	$(CCACHE) $(CC) $(CFLAGS) $(CXXFLAGS) $(CXXFLAGS_$@) -c $< -o $@

%.o: %.c Makefile
	$(CCACHE) gcc $(CFLAGS)  -c $< -o $@

libfpp-osc.so: $(OBJECTS_fpp_osc_so) /opt/fpp/src/libfpp.so
	$(CCACHE) $(CC) -shared $(CFLAGS_$@) $(OBJECTS_fpp_osc_so) $(LIBS_fpp_osc_so) $(LDFLAGS) -o $@

clean:
	rm -f libfpp-osc.so $(OBJECTS_fpp_osc_so)

