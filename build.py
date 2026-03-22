import os
import sys
def compile(name):
    os.system(f"g++ -c {name}.cpp -std=c++20 -o Build/{name}.o")
others = ["./libs/json_reader.o","./libs/json_value.o","./libs/json_writer.o"]
allllllllll = ["test"]
if len(sys.argv) < 2:
    pass
elif sys.argv[1] == 'all':
    for i in allllllllll:
        compile(i)
else:
    for i in sys.argv[1:]:
        compile(i)
#../json/build.sh
os.system(f"g++ {' '.join([f'Build/{name}.o' for name in allllllllll]+others)} -lminizip -lssl -lcrypto -lz -o Build/LauncherQiu-Core")
os.system("./Build/LauncherQiu-Core")
