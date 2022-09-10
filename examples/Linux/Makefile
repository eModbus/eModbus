all: SyncClient AsyncClient


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

BASESRC = ModbusMessage.cpp Logging.cpp ModbusClient.cpp ModbusClientTCP.cpp CoilData.cpp ModbusTypeDefs.cpp
BASEINC = ModbusMessage.h Logging.h ModbusClient.h ModbusClientTCP.h ModbusTypeDefs.h ModbusError.h options.h CoilData.h

$(info Be sure to copy files from the eModbus main directory into $(LIBDIR): )
$(info "      Headers: $(BASEINC)" )
$(info "      Sources: $(BASESRC)" )

AsyncClient: AsyncClient.o
	$(CXX) $^ -L $(LIBDIR) -leModbus -pthread -lexplain $(RPILIB) -o $@

SyncClient: SyncClient.o
	$(CXX) $^ -L $(LIBDIR) -leModbus -pthread -lexplain $(RPILIB) -o $@

%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -I $(LIBDIR) -MMD -c $<

.PHONY: clean all dist

clean:
	$(RM) core *.o *.d

dist:
	zip -u MBCLinux *.h *.cpp Makefile $(LIBDIR)/*.cpp $(LIBDIR)/*.h $(LIBDIR)/Makefile

