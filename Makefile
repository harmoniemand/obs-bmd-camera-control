
CC=g++
SDK_PATH=decklink-sdk/Linux/include
CFLAGS=-Wno-multichar -I $(SDK_PATH) -fno-rtti -I decklink-sdk
LDFLAGS=-lm -ldl -lpthread

ccontrol: src/ccontrol.cpp $(SDK_PATH)/DeckLinkAPIDispatch.cpp
	$(CC) -o ccontrol src/ccontrol.cpp $(SDK_PATH)/DeckLinkAPIDispatch.cpp $(CFLAGS) $(LDFLAGS)

clean:
	rm -f ccontrol
