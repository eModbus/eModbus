all: libeModbus.a

# Check if running on a Raspberry Pi
onRaspi := $(shell grep -c Raspberry < /proc/cpuinfo)
ifeq ($(onRaspi),1)
RPI = -DIS_RASPBERRY
endif

SRC = IPAddress.cpp Client.cpp parseTarget.cpp
INC = IPAddress.h Client.h parseTarget.h
BASESRC = ModbusMessage.cpp Logging.cpp ModbusClient.cpp ModbusClientTCP.cpp ModbusTypeDefs.cpp
BASEINC = ModbusMessage.h Logging.h ModbusClient.h ModbusClientTCP.h ModbusTypeDefs.h ModbusError.h options.h

OBJ = $(SRC:.cpp=.o) $(BASESRC:.cpp=.o)

CXXFLAGS = -Wextra -std=c++11
CPPFLAGS = -g -DLOG_LEVEL=6 -DLINUX $(RPI)

$(info Be sure to copy missing files from the eModbus main directory: )
$(info "      Headers: $(BASEINC)" )
$(info "      Sources: $(BASESRC)" )

DEPS := $(OBJ:.o=.d)
        -include $(DEPS)

libeModbus.a: $(OBJ)
        ar rcs $@ $(OBJ)

%.o: %.cpp
        $(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -c -pthread $<

.PHONY: clean all dist reallyclean

clean:
        $(RM) core *.o *.d

reallyclean:
        $(RM) core *.o *.d $(BASESRC) $(BASEINC)
