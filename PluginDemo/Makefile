PROJECT_FILES=RenameNginx.cpp ../HttpRequestPacket.h ../HttpResponsePacket.h

plugindemo: $(PROJECT_FILES)
	g++ -o plugindemo.so $(PROJECT_FILES) -shared -fPIC -Wall -Werror

clean:
	rm -f plugindemo.so