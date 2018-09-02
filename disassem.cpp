#include <iostream>
#include <stdio.h>
#include <cstdlib>

#include "disassem.h"

using namespace std;

// TODO: Convert this stuff to use c++ features

int main(int argc, char** argv) {
	FILE *f = fopen(argv[1], "rb");
	if (f == NULL) {
		cout << "error: Could not open file " << argv[1] << endl;
		return 1;
	}

	fseek(f, 0L, SEEK_END);
	long fsize = ftell(f);
	fseek(f, 0L, SEEK_SET);

	unsigned char* buffer = (unsigned char*)malloc(fsize);

	fread(buffer, fsize, 1, f);
	fclose(f);

	int pc = 0;

	while (pc < fsize) {
		pc += disassem8080Op(buffer, pc);
	}

	return 0;
}

