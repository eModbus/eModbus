all: SyncClient AsyncClient

$(info "Assuming libeModbus.a was built and installed...")

# Check if running on a Raspberry Pi
onRaspi := $(shell grep -c Raspberry < /proc/cpuinfo)
ifeq ($(onRaspi),1)
RPI = -DIS_RASPBERRY
RPILIB = -lwiringPi
endif

CXXFLAGS = -Wextra 
CPPFLAGS = -DLOG_LEVEL=3 -DLINUX $(RPI)
LIBDIR = ./eModbus

DEPS := $(OBJ:.o=.d)
	-include $(DEPS)

AsyncClient: AsyncClient.o
	$(CXX) $^ -leModbus -pthread -lexplain $(RPILIB) -o $@

SyncClient: SyncClient.o
	$(CXX) $^ -leModbus -pthread -lexplain $(RPILIB) -o $@

%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -c $<

.PHONY: clean all dist reallyclean

clean:
	$(RM) core *.o *.d

reallyclean:
	$(RM) core *.o *.d SyncClient AsyncClient

dist:
	zip -u MBCLinux *.h *.cpp Makefile $(LIBDIR)/*.cpp $(LIBDIR)/*.h $(LIBDIR)/Makefile

