all: testMB

OBJ = main.o ModbusMessage.o Logging.o ModbusClient.o ModbusClientTCP.o IPAddress.o Client.o
CXXFLAGS = -Wextra 
CPPFLAGS = -DLOG_LEVEL=3

DEPS := $(OBJ:.o=.d)
	-include $(DEPS)

testMB: $(OBJ)
	$(CXX) $^ -pthread -lexplain -o $@

%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -c $<

.PHONY: clean all dist

clean:
	$(RM) core *.o *.d

dist:
	zip -f MB *.h *.cpp Makefile

