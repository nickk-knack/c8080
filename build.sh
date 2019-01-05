if [ ! -d "bin" ]; then
  echo "bin" directory not found, creating...
  mkdir bin
fi

echo Building c8080_shell.cpp ...
g++ -std=c++17 src/c8080_shell.cpp -o bin/c8080_shell
echo Building disassem.cpp ...
g++ -std=c++17 src/disassem.cpp -o bin/disassem

echo Build complete.
