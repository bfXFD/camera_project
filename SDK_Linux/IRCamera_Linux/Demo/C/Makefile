APP_TARGETSO = IRCUSBSDKDemo

CXXFLAGS += -I../../include
CXXFLAGS += -L../../lib/linux/x86_64 \
			-lIRCUSBSDK -lPocoCrypto -lPocoFoundation -lPocoJSON -luvc -ludev
CXXFLAGS += ../../lib/linux/x86_64/libIRCUSBSDK.so

# 根据DEBUG变量设置编译选项
ifeq ($(DEBUG),1)
CXXFLAGS += -g
else
CXXFLAGS += -O2
endif

CXX = g++
RM = rm -f

.PHONY: all,clean

all: 
#ifeq ($(SO),1)
	$(CXX) -o $(APP_TARGETSO) $(CXXFLAGS) main.cpp ../../lib/linux/x86_64/libIRCUSBSDK.so
#else
#	$(CXX) $(CXXFLAGS) main.cpp LinuxDef.cpp -o $(APP_TARGETA)  ./libIRNetClient.a -ldl -lrt
#endif

clean:
	rm -f $(APP_TARGETA)
	rm -f $(APP_TARGETSO)
