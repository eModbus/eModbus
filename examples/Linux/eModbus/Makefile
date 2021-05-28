all: libeModbus.a

SRC = IPAddress.cpp Client.cpp parseTarget.cpp
INC = IPAddress.h Client.h parseTarget.h
BASESRC = ModbusMessage.cpp Logging.cpp ModbusClient.cpp ModbusClientTCP.cpp CoilData.cpp
BASEINC = ModbusMessage.h Logging.h ModbusClient.h ModbusClientTCP.h ModbusTypeDefs.h ModbusError.h options.h CoilData.h

OBJ = $(SRC:.cpp=.o) $(BASESRC:.cpp=.o)

CXXFLAGS = -Wextra 
CPPFLAGS = -DLOG_LEVEL=3

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
