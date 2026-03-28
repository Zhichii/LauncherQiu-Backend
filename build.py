import os
import sys
if os.path.exists("build"):
    if not os.path.isdir("build"):
        os.remove("build")
    if False: # 我现在不想清除，所以我只是ban了，但没删掉。
        for i in os.listdir("build"):
            os.remove(f"build/{i}")
        os.removedirs("build")
if not os.path.exists("build"):
    os.mkdir("build")
def compile(name):
    os.system(f"g++ -c {name}.cpp -std=c++20 -o build/{name}.o")
others = ["./libs/json_reader.o","./libs/json_value.o","./libs/json_writer.o"]
allllllllll = ["test", "instance", "accounts", "java"]
if len(sys.argv) < 2:
    pass
elif sys.argv[1] == 'all':
    for i in allllllllll:
        compile(i)
else:
    for i in sys.argv[1:]:
        compile(i)
#../json/build.sh
os.system(f"g++ {' '.join([f'build/{name}.o' for name in allllllllll]+others)} -lminizip -lssl -lcrypto -lz -o build/LauncherQiu-Core")
os.system("./build/LauncherQiu-Core")
