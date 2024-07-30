all: libeModbus libeModbusdebug

# Use different compiler flags for regular and debug versions
libeModbus: 
	$(MAKE) TFLAGS="-DLOG_LEVEL=3" libeModbus.a
libeModbusdebug: 
	$(MAKE) TFLAGS="-DLOG_LEVEL=6 -g" libeModbusdebug.a

# Check if running on a Raspberry Pi
onRaspi := $(shell grep -c Raspberry < /proc/cpuinfo)
ifeq ($(onRaspi),1)
RPI = -DIS_RASPBERRY
endif

# Local sources
SRC = IPAddress.cpp Client.cpp parseTarget.cpp
INC = IPAddress.h Client.h parseTarget.h
# eModbus library sources
BASESRC = ModbusMessage.cpp Logging.cpp ModbusClient.cpp ModbusClientTCP.cpp ModbusTypeDefs.cpp CoilData.cpp
BASEINC = ModbusMessage.h Logging.h ModbusClient.h ModbusClientTCP.h ModbusTypeDefs.h ModbusError.h options.h CoilData.h

# Get library sources, if necessary
$(BASEINC) : % : ../../../src/%
	cp $< $@
# Same for header files
$(BASESRC) : % : ../../../src/%
	cp $< $@

# Header dependencies
ModbusMessage.o: ModbusMessage.h ModbusTypeDefs.h ModbusError.h
Logging.o: Logging.h options.h
ModbusClient.o: ModbusClient.h options.h ModbusMessage.h
ModbusClientTCP.o: ModbusClientTCP.h ModbusClient.h options.h Client.h ModbusMessage.h
ModbusTypeDefs.o: ModbusTypeDefs.h
IPAddress.o: IPAddress.h Logging.h options.h
Client.o: Client.h Logging.h options.h
parseTarget.o: IPAddress.h Client.h Logging.h options.h
CoilData.o: CoilData.h options.h Logging.h

OBJ = $(SRC:.cpp=.o) $(BASESRC:.cpp=.o)

CXXFLAGS = -Wextra -std=c++11
CPPFLAGS = -DLINUX $(TFLAGS) $(RPI)

DEPS := $(OBJ:.o=.d)
        -include $(DEPS)

libeModbus.a: $(OBJ) 
	ar rcs $@ $(OBJ)

libeModbusdebug.a: $(OBJ) 
	ar rcs $@ $(OBJ)

%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -c -pthread $<

.PHONY: clean all dist reallyclean

clean:
	$(RM) core *.o *.d

reallyclean:
	$(RM) core *.o *.d $(BASESRC) $(BASEINC)

install:
	chmod 0755 *.a
	sudo cp *.a /usr/local/lib
	sudo cp *.h /usr/local/include
