
CC=g++
PLATFORM_PATH=Linux
SDK_PATH=submodules/decklink-sdk
CFLAGS=-Wno-multichar -I $(SDK_PATH)/Linux/include -fno-rtti -I ${SDK_PATH}/Examples
LDFLAGS=-lm -ldl -lpthread

COMMON_SOURCES= \
	$(SDK_PATH)/Linux/include/DeckLinkAPIDispatch.cpp \
	${SDK_PATH}/Examples/Linux/platform.cpp


ccontrol: src/ccontrol.cpp  $(COMMON_SOURCES)
	$(CC) -o ccontrol src/ccontrol.cpp $(COMMON_SOURCES) $(CFLAGS) $(LDFLAGS)

clean:
	rm -f ccontrol
