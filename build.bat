if not exist bin\ (
  echo "bin" folder not found, creating...
  mkdir bin
) 

echo Building c8080_shell.cpp ...
g++ -std=c++17 src/c8080_shell.cpp -o bin/c8080_shell.exe
echo Building disassem.cpp ...
g++ -std=c++17 src/disassem.cpp -o bin/disassem.exe

echo Build complete.
