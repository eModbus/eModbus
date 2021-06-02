all: SyncClient AsyncClient

CXXFLAGS = -Wextra 
CPPFLAGS = -DLOG_LEVEL=3
LIBDIR = ./eModbus

DEPS := $(OBJ:.o=.d)
	-include $(DEPS)

BASESRC = ModbusMessage.cpp Logging.cpp ModbusClient.cpp ModbusClientTCP.cpp CoilData.cpp
BASEINC = ModbusMessage.h Logging.h ModbusClient.h ModbusClientTCP.h ModbusTypeDefs.h ModbusError.h options.h CoilData.h

$(info Be sure to copy files from the eModbus main directory into $(LIBDIR): )
$(info "      Headers: $(BASEINC)" )
$(info "      Sources: $(BASESRC)" )

AsyncClient: AsyncClient.o
	$(CXX) $^ -L $(LIBDIR) -leModbus -pthread -lexplain -o $@

SyncClient: SyncClient.o
	$(CXX) $^ -L $(LIBDIR) -leModbus -pthread -lexplain -o $@

%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -I $(LIBDIR) -MMD -c $<

.PHONY: clean all dist

clean:
	$(RM) core *.o *.d

dist:
	zip -u MBCLinux *.h *.cpp Makefile $(LIBDIR)/*.cpp $(LIBDIR)/*.h $(LIBDIR)/Makefile

