PROJECT_FILES=Proxy.cpp Plugins.cpp Utils.cpp Plugins.h Logger.h HttpRequestPacket.h HttpResponsePacket.h Utils.h
THREAD_POOL_FILES=./ThreadPool/threadpool.cpp
SIMPLE_INI_FILES=./SimpleIni/SimpleIni.h ./SimpleIni/ConvertUTF.c

proxy: $(PROJECT_FILES) $(THREAD_POOL_FILES) $(SIMPLE_INI_FILES)
	g++ -o proxy $(PROJECT_FILES) $(THREAD_POOL_FILES) $(SIMPLE_INI_FILES) -lpthread -ldl -Wall -Werror

clean:
	rm -f proxy