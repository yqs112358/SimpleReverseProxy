cd PluginDemo
make
cd ..
mkdir plugins 2>/dev/null
cp PluginDemo/plugindemo.so plugins/ -f
make
./proxy
