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
others = ["./libs/json_reader.o","./libs/json_value.o","./libs/json_writer.o"]
allllllllll = ["test", "instance", "accounts", "java"]
if len(sys.argv) < 2:
    pass
os.system(f"g++ -std=c++20 {' '.join([f'{name}.cpp' for name in allllllllll])} {" ".join(others)} -lminizip -lssl -lcrypto -lz -g -o build/LauncherQiu-Test")
os.system("./build/LauncherQiu-Test")
