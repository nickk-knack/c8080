/*
	(8/1/18, 7:30 PM CDT)
	Currently implements 91.40% of instructions, or 234/256.
	The 22 (20 if you don't count OUT/IN) remaining unimplemented 
	instructions consist of ACI, SBI, and rotate instructions,
	as well as a few "weird" instructions. 

	Two instructions have partial implementations (OUT D8/IN D8) 
	but still contain "UnimplementedInstruction" as a comment to 
	show that they are partial implementations.

	One instruction, DAA, is weird to me. No idea how it works.
	Need to figure it out.

	Todo: 
	* add command line arguments to allow for debugging/stepping
	through program
	* live code input
	* hiding disassembly/state output
	* remove any strictly c code and reimplement as c++
	* ^ goes for any other files in this project
	* FIX SUB INSTRUCTIONS
	* ALSO SBB B, and then implement rest of SBB
	* BECAUSE THAT SHIT AINT RIGHT AND I KNOW IT
*/

#include <iostream>
#include <iomanip>
#include <stdio.h>
#include <cstdlib>

#include "disassem.h"

using namespace std;

typedef struct ConditionCodes {
	uint8_t	z:1;
	uint8_t s:1;
	uint8_t p:1;
	uint8_t cy:1;
	uint8_t ac:1;
	uint8_t pad:3;
} ConditionCodes;

typedef struct State8080 {
	uint8_t a;
	uint8_t b;
	uint8_t c;
	uint8_t d;
	uint8_t e;
	uint8_t h;
	uint8_t l;
	uint16_t sp;
	uint16_t pc;
	uint8_t *memory;
	struct ConditionCodes cc;
	uint8_t int_enable;
} State8080;

// Function headers

void UnimplementedInstruction(State8080* state);
int Emulate8080Op(State8080* state);
int parity(int x);
void ReadFileIntoMemoryAt(State8080* state, char* filename, uint32_t offset);
State8080* Init8080();

// Main

int main(int argc, char** argv) {
	if (argc < 2) {
		cout << "No filename given!" << endl;
		exit(1);
	}

	int done = 0;
	State8080* state = Init8080();

	ReadFileIntoMemoryAt(state, argv[1], 0);

	while (done == 0) {
		done = Emulate8080Op(state);
	}

	return 0;
}

// Function defs

void ReadFileIntoMemoryAt(State8080* state, char* filename, uint32_t offset) {
	FILE* f = fopen(filename, "rb");
	if (f == NULL) {
		cout << "error: Could not open " << filename << endl;
		exit(1);
	}

	fseek(f, 0L, SEEK_END);
	long fsize = ftell(f);
	fseek(f, 0L, SEEK_SET);

	uint8_t *buffer = &state->memory[offset];
	fread(buffer, fsize, 1, f);
	fclose(f);
}

State8080* Init8080() {
	State8080* state = (State8080*)calloc(1, sizeof(State8080));
	state->memory = (uint8_t*)malloc(0x10000); // 16K
	return state;
}

void UnimplementedInstruction(State8080* state) {
	cout << "Error: Unimplemented instruction" << endl;
	state->pc--;
	disassem8080Op(state->memory, state->pc);
	cout << endl;
	exit(1);
}

int parity(int x) {
	// opt. size param, im defaulting it to 8
	int p = 0;
	x = (x & ((1 << 8) - 1));

	for (int i = 0; i < 8; i++) {
		if (x & 0x01) p++;
		x = x >> 1;
	}

	return ((p & 0x01) == 0) ? 1 : 0;
}

int Emulate8080Op(State8080* state) {
	unsigned char* opcode = &state->memory[state->pc];
	disassem8080Op(state->memory, state->pc);
	state->pc += 1;

	switch (*opcode) {
		case 0x00: break; 			// NOP
		case 0x01: 					// LXI B,D16
			state->c = opcode[1];
			state->b = opcode[2];
			state->pc += 2;
			break;
		case 0x02: 					// STAX B
			{
				uint16_t offset = (state->b << 8) | state->c;
				state->memory[offset] = state->a;
			}
			break;
		case 0x03: 					// INX B
			state->c++;
			if (state->c == 0) 
				state->b++;
			break;
		case 0x04:					// INR B 
			{
				uint8_t res = state->b + 1;
				state->cc.z = (res == 0) ? 1 : 0;
				state->cc.s = ((res & 0x80) == 0x80) ? 1 : 0;
				state->cc.p = parity(res);
				state->b = res;
			}
			break;
		case 0x05: 					// DCR B
			{
				uint8_t res = state->b - 1;
				state->cc.z = (res == 0) ? 1 : 0;
				state->cc.s = ((res & 0x80) == 0x80) ? 1 : 0;
				state->cc.p = parity(res);
				state->b = res;
			}
			break;
		case 0x06: 					// MVI B, D8
			state->b = opcode[1];
			state->pc++;
			break;
		case 0x07: UnimplementedInstruction(state); break;
		case 0x08: break;			// NOP
		case 0x09: 					// DAD B
			{
				uint32_t hl = (state->h << 8) | state->l;
				uint32_t bc = (state->b << 8) | state->c;
				uint32_t res = hl + bc;
				state->h = (res & 0xff00) >> 8;
				state->l = res & 0xff;
				state->cc.cy = ((res & 0xffff0000) > 0) ? 1 : 0;
			}
			break;
		case 0x0a: 					// LDAX B
			{
				uint16_t offset = (state->b << 8) | state->c;
				state->a = state->memory[offset];
			}
			break;
		case 0x0b: 					// DCX B
			state->c--;
			if (state->c == 0xff)	// POTENTIALLY INCORRECT
				state->b--;
			break;
		case 0x0c: 					// INR C
			{
				uint8_t res = state->c + 1;
				state->cc.z = (res == 0) ? 1 : 0;
				state->cc.s = ((res & 0x80) == 0x80) ? 1 : 0;
				state->cc.p = parity(res);
				state->c = res;
			}
			break;
		case 0x0d: 					// DCR C
			{
				uint8_t res = state->c - 1;
				state->cc.z = (res == 0) ? 1 : 0;
				state->cc.s = ((res & 0x80) == 0x80) ? 1 : 0;
				state->cc.p = parity(res);
				state->c = res;
			}
			break;
		case 0x0e: 					// MVI C, D8
			state->c = opcode[1];
			state->pc++;
			break;
		case 0x0f: 					// RRC
			{
				uint8_t x = state->a;
				state->a = ((x & 1) << 7) | (x >> 1);
				state->cc.cy = ((x & 1) == 1) ? 1 : 0;
			}
			break;

		case 0x10: break;			// NOP
		case 0x11: 					// LXI D, D16
			state->e = opcode[1];
			state->d = opcode[2];
			state->pc += 2;
			break;
		case 0x12:					// STAX D
			{
				uint16_t offset = (state->d << 8) | state->e;
				state->memory[offset] = state->a;
			}
			break;
		case 0x13: 					// INX D
			state->e++;
			if (state->e == 0) 
				state->d++;
			break;
		case 0x14: 					// INR D
			{
				uint8_t res = state->d + 1;
				state->cc.z = (res == 0) ? 1 : 0;
				state->cc.s = ((res & 0x80) == 0x80) ? 1 : 0;
				state->cc.p = parity(res);
				state->d = res;
			}
			break;
		case 0x15: 					// DCR D
			{
				uint8_t res = state->d - 1;
				state->cc.z = (res == 0) ? 1 : 0;
				state->cc.s = ((res & 0x80) == 0x80) ? 1 : 0;
				state->cc.p = parity(res);
				state->d = res;
			}
			break;
		case 0x16: 					// MVI D, D8
			state->d = opcode[1];
			state->pc++;
			break;
		case 0x17: UnimplementedInstruction(state); break;
		case 0x18: break;			// NOP
		case 0x19: 					// DAD D
			{
				uint32_t hl = (state->h << 8) | state->l;
				uint32_t de = (state->d << 8) | state->e;
				uint32_t res = hl + de;
				state->h = (res & 0xff00) >> 8;
				state->l = res & 0xff;
				state->cc.cy = ((res & 0xffff0000) != 0) ? 1 : 0;
			}
			break;
		case 0x1a: 					// LDAX D
			{
				uint16_t offset = (state->d << 8) | state->e;
				state->a = state->memory[offset];
			}
			break;
		case 0x1b: 					// DCX D
			state->e--;
			if (state->e == 0xff)	// POTENTIALLY INCORRECT
				state->d--;
			break;
		case 0x1c: 					// INR E
			{
				uint8_t res = state->e + 1;
				state->cc.z = (res == 0) ? 1 : 0;
				state->cc.s = ((res & 0x80) == 0x80) ? 1 : 0;
				state->cc.p = parity(res);
				state->e = res;
			}
			break;
		case 0x1d: 					// DCR E
			{
				uint8_t res = state->e - 1;
				state->cc.z = (res == 0) ? 1 : 0;
				state->cc.s = ((res & 0x80) == 0x80) ? 1 : 0;
				state->cc.p = parity(res);
				state->e = res;
			}
			break;
		case 0x1e: 					// MVI E, D8
			state->e = opcode[1];
			state->pc++;
			break;
		case 0x1f: 					// RAR
			{
				uint8_t x = state->a;
				state->a = (state->cc.cy << 7) | (x >> 1);
				state->cc.cy = ((x & 1) == 1) ? 1 : 0;
			}
			break;

		case 0x20: break;			// NOP
		case 0x21:					// LXI H, D16
			state->l = opcode[1];
			state->h = opcode[2];
			state->pc += 2;
			break;
		case 0x22: 					// SHLD
			{
				uint16_t offset = (opcode[2] << 8) | opcode[1];
				state->memory[offset] = state->l;
				state->memory[offset + 1] = state->h;
				state->pc += 2;
			}
			break;
		case 0x23: 					// INX H
			state->l++;
			if (state->l == 0)
				state->h++;
			break;
		case 0x24: 					// INR H
			{
				uint8_t res = state->h + 1;
				state->cc.z = (res == 0) ? 1 : 0;
				state->cc.s = ((res & 0x80) == 0x80) ? 1 : 0;
				state->cc.p = parity(res);
				state->h = res;
			}
			break;
		case 0x25: 					// DCR H
			{
				uint8_t res = state->h - 1;
				state->cc.z = (res == 0) ? 1 : 0;
				state->cc.s = ((res & 0x80) == 0x80) ? 1 : 0;
				state->cc.p = parity(res);
				state->d = res;
			}
			break;
		case 0x26: 					// MVI H, D8
			state->h = opcode[1];
			state->pc++;
			break;
		case 0x27: UnimplementedInstruction(state); break; //DAA (complicated)
		case 0x28: break;			// NOP
		case 0x29: 					// DAD H
			{
				uint32_t hl = (state->h << 8) | state->l;
				uint32_t res = hl + hl;
				state->h = (res & 0xff00) >> 8;
				state->l == res & 0xff;
				state->cc.cy = ((res & 0xffff0000) != 0) ? 1 : 0;
			}
			break;
		case 0x2a: 					// LHLD
			{
				uint16_t offset = (opcode[2] << 8) | opcode[1];
				state->l = state->memory[offset];
				state->h = state->memory[offset + 1];
				state->pc += 2;
			}
			break;
		case 0x2b: 					// DCX H
			state->l--;
			if (state->l == 0xff)	// POTENTIALLY INCORRECT
				state->h--;
			break;
		case 0x2c: 					// INR L
			{
				uint8_t res = state->l + 1;
				state->cc.z = (res == 0) ? 1 : 0;
				state->cc.s = ((res & 0x80) == 0x80) ? 1 : 0;
				state->cc.p = parity(res);
				state->l = res;
			}
			break;
		case 0x2d: 					// DCR L
			{
				uint8_t res = state->l - 1;
				state->cc.z = (res == 0) ? 1 : 0;
				state->cc.s = ((res & 0x80) == 0x80) ? 1 : 0;
				state->cc.p = parity(res);
				state->l = res;
			}
			break;
		case 0x2e: 					// MVI L, D8
			state->l = opcode[1];
			state->pc++;
			break;
		case 0x2f: 					// CMA (not)
			state->a = ~state->a;
			break;

		case 0x30: break;			// NOP
		case 0x31:					// LXI SP, D16
			state->sp = (opcode[2] << 8) | opcode[1];
			state->pc += 2;
			break;
		case 0x32:					// STA D16
			{
				uint16_t offset = (opcode[2] << 8) | opcode[1];
				state->memory[offset] = state->a;
				state->pc += 2;
			}
			break;
		case 0x33: 					// INX SP
			state->sp++;
			break;
		case 0x34: 					// INR M
			{
				uint16_t offset = (state->h << 8) | state->l;
				uint8_t res = state->memory[offset] + 1;
				state->cc.z = (res == 0) ? 1 : 0;
				state->cc.s = ((res & 0x80) == 0x80) ? 1 : 0;
				state->cc.p = parity(res);
				state->memory[offset] = res;

			}
			break;
		case 0x35: 					// DCR M
			{
				uint16_t offset = (state->h << 8) | state->l;
				uint8_t res = state->memory[offset] - 1;
				state->cc.z = (res == 0) ? 1 : 0;
				state->cc.s = ((res & 0x80) == 0x80) ? 1 : 0;
				state->cc.p = parity(res);
				state->memory[offset] = res;

			}
			break;
		case 0x36: 					// MVI M, D8
			{
				uint16_t offset = (state->h << 8) | state->l;
				state->memory[offset] = opcode[1];
				state->pc++;
			}
			break;
		case 0x37: 					// STC
			state->cc.cy = 0x01;
			break;
		case 0x38: break;			// NOP
		case 0x39: 					// DAD SP
			{
				uint32_t hl = (state->h << 8) | state->l;
				uint32_t res = hl + state->sp;	// May be incorrect?
				state->h = (res & 0xff00) >> 8;
				state->l == res & 0xff;
				state->cc.cy = ((res & 0xffff0000) != 0) ? 1 : 0;
			}
			break;
		case 0x3a: 					// LDA D16
			{
				uint16_t offset = (opcode[2] << 8) | opcode[1];
				state->a = state->memory[offset];
				state->pc += 2;
			}
			break;
		case 0x3b: 					// DCX SP
			state->sp--;
			break;
		case 0x3c: 					// INR A
			{
				uint8_t res = state->a + 1;
				state->cc.z = (res == 0) ? 1 : 0;
				state->cc.s = ((res & 0x80) == 0x80) ? 1 : 0;
				state->cc.p = parity(res);
				state->a = res;
			}
			break;
		case 0x3d:					// DCR A
			{
				uint8_t res = state->a - 1;
				state->cc.z = (res == 0) ? 1 : 0;
				state->cc.s = ((res & 0x80) == 0x80) ? 1 : 0;
				state->cc.p = parity(res);
				state->a = res;
			}
			break;
		case 0x3e: 					// MVI A, D8
			state->a = opcode[1];
			state->pc++;
			break;
		case 0x3f: 					// CMC
			state->cc.cy = !state->cc.cy;
			break;
		case 0x40: 					// MOV B, B
			state->b = state->b;
			break;
		case 0x41: 					// MOV B, C
			state->b = state->c;
			break;
		case 0x42: 					// MOV B, D
			state->b = state->d;
			break;
		case 0x43: 					// MOV B, E
			state->b = state->e;
			break;
		case 0x44: 					// MOV B, H
			state->b = state->h;
			break;
		case 0x45: 					// MOV B, L
			state->b = state->l;
			break;
		case 0x46: 					// MOV B, M
			{
				uint16_t offset = (state->h << 8) | state->l;
				state->b = state->memory[offset];
			}
			break;
		case 0x47:					// MOV B, A
			state->b = state->a;
			break;
		case 0x48: 					// MOV C, B
			state->c = state->b;
			break;
		case 0x49: 					// MOV C, C
			state->c = state->c;
			break;
		case 0x4a: 					// MOV C, D
			state->c = state->d;
			break;
		case 0x4b: 					// MOV C, E
			state->c = state->e;
			break;
		case 0x4c: 					// MOV C, H
			state->c = state->h;
			break;
		case 0x4d: 					// MOV C, L
			state->c = state->l;
			break;
		case 0x4e: 					// MOV C, M
			{
				uint16_t offset = (state->h << 8) | state->l;
				state->c = state->memory[offset];
			}
			break;
		case 0x4f: 					// MOV C, A
			state->c = state->a;
			break;
		case 0x50: 					// MOV D, B
			state->d = state->b;
			break;
		case 0x51: 					// MOV D, C
			state->d = state->c;
			break;
		case 0x52: 					// MOV D, D
			state->d = state->d;
			break;
		case 0x53: 					// MOV D, E
			state->d = state->e;
			break;
		case 0x54: 					// MOV D, H
			state->d = state->h;
			break;
		case 0x55: 					// MOV D, L
			state->d = state->l;
			break;
		case 0x56: 					// MOV D, M
			{
				uint16_t offset = (state->h << 8) | state->l;
				state->d = state->memory[offset];
			}
			break;
		case 0x57: 					// MOV D, A
			state->d = state->a;
			break;
		case 0x58: 					// MOV E, B
			state->e = state->b;
			break;
		case 0x59: 					// MOV E, C
			state->e = state->c;
			break;
		case 0x5a: 					// MOV E, D
			state->e = state->d;
			break;
		case 0x5b: 					// MOV E, E
			state->e = state->e;
			break;
		case 0x5c: 					// MOV E, H
			state->e = state->h;
			break;
		case 0x5d: 					// MOV E, L
			state->e = state->l;
			break;
		case 0x5e: 					// MOV E, M
			{
				uint16_t offset = (state->h << 8) | state->l;
				state->e = state->memory[offset];
			}
			break;
		case 0x5f: 					// MOV E, A
			state->e = state->a;
			break;

		case 0x60: 					// MOV H, B
			state->h = state->b;
			break;
		case 0x61: 					// MOV H, C
			state->h = state->c;
			break;
		case 0x62: 					// MOV H, D
			state->h = state->d;
			break;
		case 0x63: 					// MOV H, E
			state->h = state->e;
			break;
		case 0x64: 					// MOV H, H
			state->h = state->h;
			break;
		case 0x65: 					// MOV H, L
			state->h = state->l;
			break;
		case 0x66: 					// MOV H, M
			{
				uint16_t offset = (state->h << 8) | state->l;
				state->h = state->memory[offset];
			}
			break;
		case 0x67: 					// MOV H, A
			state->h = state->a;
			break;
		case 0x68: 					// MOV L, B
			state->l = state->b;
			break;
		case 0x69: 					// MOV L, C
			state->l = state->c;
			break;
		case 0x6a: 					// MOV L, D
			state->l = state->d;
			break;
		case 0x6b: 					// MOV L, E
			state->l = state->e;
			break;
		case 0x6c: 					// MOV L, H
			state->l = state->h;
			break;
		case 0x6d: 					// MOV L, L
			state->l = state->l;
			break;
		case 0x6e: 					// MOV L, M
			{
				uint16_t offset = (state->h << 8) | state->l;
				state->l = state->memory[offset];
			}
			break;
		case 0x6f: 					// MOV L, A
			state->l = state->a;
			break;

		case 0x70: 					// MOV M, B
			{
				uint16_t offset = (state->h << 8) | state->l;
				state->memory[offset] = state->b;
			}
			break;
		case 0x71: 					// MOV M, C
			{
				uint16_t offset = (state->h << 8) | state->l;
				state->memory[offset] = state->c;
			}
			break;
		case 0x72: 					// MOV M, D
			{
				uint16_t offset = (state->h << 8) | state->l;
				state->memory[offset] = state->d;
			}
			break;
		case 0x73: 					// MOV M, E
			{
				uint16_t offset = (state->h << 8) | state->l;
				state->memory[offset] = state->e;
			}
			break;
		case 0x74: 					// MOV M, H
			{
				uint16_t offset = (state->h << 8) | state->l;
				state->memory[offset] = state->h;
			}
			break;
		case 0x75: 					// MOV M, L
			{
				uint16_t offset = (state->h << 8) | state->l;
				state->memory[offset] = state->l;
			}
			break;
		case 0x76: 					// HLT
			state->pc++;
			// CPU should halt, not further activity until interrupt occurs
			// Partial implementation
			break;
		case 0x77: 					// MOV M, A
			{
				uint16_t offset = (state->h << 8) | state->l;
				state->memory[offset] = state->a;
			}
			break;
		case 0x78:					// MOV A, B
			state->a = state->b;
			break;
		case 0x79:					// MOV A, C
			state->a = state->c;
			break;
		case 0x7a:					// MOV A, D
			state->a = state->d;
			break;
		case 0x7b:					// MOV A, E
			state->a = state->e;
			break;
		case 0x7c: 					// MOV A, H
			state->a = state->h;
			break;
		case 0x7d:					// MOV A, L
			state->a = state->l;
			break;
		case 0x7e: 					// MOV A, M
			{
				uint16_t offset = (state->h << 8) | state->l;
				state->a = state->memory[offset];
			}
			break;
		case 0x7f: 					// MOV A, A
			state->a = state->a;
			break;

		case 0x80:					// ADD B 
			{
				uint16_t answer = (uint16_t)state->a + (uint16_t)state->b;
				state->cc.z = ((answer & 0xff) == 0) ? 1 : 0;
				state->cc.s = ((answer & 0x80) != 0) ? 1 : 0;
				state->cc.cy = (answer > 0xff) ? 1 : 0;
				state->cc.p = parity(answer & 0xff);
				state->a = answer & 0xff;
			}
			break;
		case 0x81: 					// ADD C
			{
				uint16_t answer = (uint16_t)state->a + (uint16_t)state->c;
				state->cc.z = ((answer & 0xff) == 0) ? 1 : 0;
				state->cc.s = ((answer & 0x80) != 0) ? 1 : 0;
				state->cc.cy = (answer > 0xff) ? 1 : 0;
				state->cc.p = parity(answer & 0xff);
				state->a = answer & 0xff;
			}
			break;
		case 0x82: 					// ADD D
			{
				uint16_t answer = (uint16_t)state->a + (uint16_t)state->d;
				state->cc.z = ((answer & 0xff) == 0) ? 1 : 0;
				state->cc.s = ((answer & 0x80) != 0) ? 1 : 0;
				state->cc.cy = (answer > 0xff) ? 1 : 0;
				state->cc.p = parity(answer & 0xff);
				state->a = answer & 0xff;
			}
			break;
		case 0x83: 					// ADD E
			{
				uint16_t answer = (uint16_t)state->a + (uint16_t)state->e;
				state->cc.z = ((answer & 0xff) == 0) ? 1 : 0;
				state->cc.s = ((answer & 0x80) != 0) ? 1 : 0;
				state->cc.cy = (answer > 0xff) ? 1 : 0;
				state->cc.p = parity(answer & 0xff);
				state->a = answer & 0xff;
			}
			break;
		case 0x84: 					// ADD H
			{
				uint16_t answer = (uint16_t)state->a + (uint16_t)state->h;
				state->cc.z = ((answer & 0xff) == 0) ? 1 : 0;
				state->cc.s = ((answer & 0x80) != 0) ? 1 : 0;
				state->cc.cy = (answer > 0xff) ? 1 : 0;
				state->cc.p = parity(answer & 0xff);
				state->a = answer & 0xff;
			}
			break;
		case 0x85: 					// ADD L
			{
				uint16_t answer = (uint16_t)state->a + (uint16_t)state->l;
				state->cc.z = ((answer & 0xff) == 0) ? 1 : 0;
				state->cc.s = ((answer & 0x80) != 0) ? 1 : 0;
				state->cc.cy = (answer > 0xff) ? 1 : 0;
				state->cc.p = parity(answer & 0xff);
				state->a = answer & 0xff;
			}
			break;
		case 0x86: 					// ADD M
			{
				uint16_t offset = (state->h << 8) | (state->l);
				uint16_t answer = (uint16_t)state->a + state->memory[offset];
				state->cc.z = ((answer & 0xff) == 0) ? 1 : 0;
				state->cc.s = ((answer & 0x80) != 0) ? 1 : 0;
				state->cc.cy = (answer > 0xff) ? 1 : 0;
				state->cc.p = parity(answer & 0xff);
				state->a = answer & 0xff;
			}
			break;
		case 0x87: 					// ADD A
			{
				uint16_t answer = (uint16_t)state->a + (uint16_t)state->a;
				state->cc.z = ((answer & 0xff) == 0) ? 1 : 0;
				state->cc.s = ((answer & 0x80) != 0) ? 1 : 0;
				state->cc.cy = (answer > 0xff) ? 1 : 0;
				state->cc.p = parity(answer & 0xff);
				state->a = answer & 0xff;
			}
			break;
		case 0x88: UnimplementedInstruction(state); break;
		case 0x89: UnimplementedInstruction(state); break;
		case 0x8a: UnimplementedInstruction(state); break;
		case 0x8b: UnimplementedInstruction(state); break;
		case 0x8c: UnimplementedInstruction(state); break;
		case 0x8d: UnimplementedInstruction(state); break;
		case 0x8e: UnimplementedInstruction(state); break;
		case 0x8f: UnimplementedInstruction(state); break;

		case 0x90: 					// SUB B
			{
				uint8_t x = state->a - state->b;
				state->cc.z = (x == 0) ? 1 : 0;
				state->cc.s = ((x & 0x80) == 0x80) ? 1 : 0;
				state->cc.p = parity(x);
				state->cc.cy = (state->a < opcode[1]) ? 1 : 0; // this is absolutely wrong
				state->a = x;		// Might be right, chance that its wrong
			}
			break;
		case 0x91: 					// SUB C
			{
				uint8_t x = state->a - state->c;
				state->cc.z = (x == 0) ? 1 : 0;
				state->cc.s = ((x & 0x80) == 0x80) ? 1 : 0;
				state->cc.p = parity(x);
				state->cc.cy = (state->a < opcode[1]) ? 1 : 0;
				state->a = x;
			}
			break;
		case 0x92: 					// SUB D
			{
				uint8_t x = state->a - state->d;
				state->cc.z = (x == 0) ? 1 : 0;
				state->cc.s = ((x & 0x80) == 0x80) ? 1 : 0;
				state->cc.p = parity(x);
				state->cc.cy = (state->a < opcode[1]) ? 1 : 0;
				state->a = x;
			}
			break;
		case 0x93: 					// SUB E
			{
				uint8_t x = state->a - state->e;
				state->cc.z = (x == 0) ? 1 : 0;
				state->cc.s = ((x & 0x80) == 0x80) ? 1 : 0;
				state->cc.p = parity(x);
				state->cc.cy = (state->a < opcode[1]) ? 1 : 0;
				state->a = x;
			}
			break;
		case 0x94: 					// SUB H
			{
				uint8_t x = state->a - state->h;
				state->cc.z = (x == 0) ? 1 : 0;
				state->cc.s = ((x & 0x80) == 0x80) ? 1 : 0;
				state->cc.p = parity(x);
				state->cc.cy = (state->a < opcode[1]) ? 1 : 0;
				state->a = x;
			}
			break;
		case 0x95: 					// SUB L
			{
				uint8_t x = state->a - state->l;
				state->cc.z = (x == 0) ? 1 : 0;
				state->cc.s = ((x & 0x80) == 0x80) ? 1 : 0;
				state->cc.p = parity(x);
				state->cc.cy = (state->a < opcode[1]) ? 1 : 0;
				state->a = x;
			}
			break;
		case 0x96: 					// SUB M
			{
				uint16_t offset = (state->h << 8) | state->l;
				uint8_t x = state->a - state->memory[offset];
				state->cc.z = (x == 0) ? 1 : 0;
				state->cc.s = ((x & 0x80) == 0x80) ? 1 : 0;
				state->cc.p = parity(x);
				state->cc.cy = (state->a < opcode[1]) ? 1 : 0;
				state->a = x;
			}
			break;
		case 0x97: 					// SUB A
			{
				uint8_t x = state->a - state->a;
				state->cc.z = (x == 0) ? 1 : 0;
				state->cc.s = ((x & 0x80) == 0x80) ? 1 : 0;
				state->cc.p = parity(x);
				state->cc.cy = (state->a < opcode[1]) ? 1 : 0;
				state->a = x;
			}
			break;
		case 0x98: 					// SBB B
			{
				uint8_t x = state->a - state->b;
				if (state->cc.cy) {
					x--;

				}

			}
			break;
		case 0x99: UnimplementedInstruction(state); break;
		case 0x9a: UnimplementedInstruction(state); break;
		case 0x9b: UnimplementedInstruction(state); break;
		case 0x9c: UnimplementedInstruction(state); break;
		case 0x9d: UnimplementedInstruction(state); break;
		case 0x9e: UnimplementedInstruction(state); break;
		case 0x9f: UnimplementedInstruction(state); break;

		case 0xa0: 					// ANA B
			state->a = state->a & state->b;
			state->cc.cy = state->cc.ac = 0;
			state->cc.z = (state->a == 0) ? 1 : 0;
			state->cc.s = ((state->a & 0x80) == 0x80) ? 1 : 0;
			state->cc.p = parity(state->a);
			break;
		case 0xa1: 					// ANA C
			state->a = state->a & state->c;
			state->cc.cy = state->cc.ac = 0;
			state->cc.z = (state->a == 0) ? 1 : 0;
			state->cc.s = ((state->a & 0x80) == 0x80) ? 1 : 0;
			state->cc.p = parity(state->a);
			break;
		case 0xa2: 					// ANA D
			state->a = state->a & state->d;
			state->cc.cy = state->cc.ac = 0;
			state->cc.z = (state->a == 0) ? 1 : 0;
			state->cc.s = ((state->a & 0x80) == 0x80) ? 1 : 0;
			state->cc.p = parity(state->a);
			break;
		case 0xa3: 					// ANA E
			state->a = state->a & state->e;
			state->cc.cy = state->cc.ac = 0;
			state->cc.z = (state->a == 0) ? 1 : 0;
			state->cc.s = ((state->a & 0x80) == 0x80) ? 1 : 0;
			state->cc.p = parity(state->a);
			break;
		case 0xa4: 					// ANA H
			state->a = state->a & state->h;
			state->cc.cy = state->cc.ac = 0;
			state->cc.z = (state->a == 0) ? 1 : 0;
			state->cc.s = ((state->a & 0x80) == 0x80) ? 1 : 0;
			state->cc.p = parity(state->a);
			break;
		case 0xa5: 					// ANA L
			state->a = state->a & state->l;
			state->cc.cy = state->cc.ac = 0;
			state->cc.z = (state->a == 0) ? 1 : 0;
			state->cc.s = ((state->a & 0x80) == 0x80) ? 1 : 0;
			state->cc.p = parity(state->a);
			break;
		case 0xa6: 					// ANA M
			{
				uint16_t offset = (state->h << 8) | state->l;
				state->a = state->a & state->memory[offset];
				state->cc.cy = state->cc.ac = 0;
				state->cc.z = (state->a == 0) ? 1 : 0;
				state->cc.s = ((state->a & 0x80) == 0x80) ? 1 : 0;
				state->cc.p = parity(state->a);
				break;
			}
			break;
		case 0xa7: 					// ANA A
			state->a = state->a & state->a;
			state->cc.cy = state->cc.ac = 0;
			state->cc.z = (state->a == 0) ? 1 : 0;
			state->cc.s = ((state->a & 0x80) == 0x80) ? 1 : 0;
			state->cc.p = parity(state->a);
			break;
		case 0xa8: 					// XRA B
			state->a = state->a ^ state->b;
			state->cc.cy = state->cc.ac = 0;
			state->cc.z = (state->a == 0) ? 1 : 0;
			state->cc.s = ((state->a & 0x80) == 0x80) ? 1 : 0;
			state->cc.p = parity(state->a);
			break;
		case 0xa9: 					// XRA C
			state->a = state->a ^ state->c;
			state->cc.cy = state->cc.ac = 0;
			state->cc.z = (state->a == 0) ? 1 : 0;
			state->cc.s = ((state->a & 0x80) == 0x80) ? 1 : 0;
			state->cc.p = parity(state->a);
			break;
		case 0xaa: 					// XRA D
			state->a = state->a ^ state->d;
			state->cc.cy = state->cc.ac = 0;
			state->cc.z = (state->a == 0) ? 1 : 0;
			state->cc.s = ((state->a & 0x80) == 0x80) ? 1 : 0;
			state->cc.p = parity(state->a);
			break;
		case 0xab: 					// XRA E
			state->a = state->a ^ state->e;
			state->cc.cy = state->cc.ac = 0;
			state->cc.z = (state->a == 0) ? 1 : 0;
			state->cc.s = ((state->a & 0x80) == 0x80) ? 1 : 0;
			state->cc.p = parity(state->a);
			break;
		case 0xac: 					// XRA H
			state->a = state->a ^ state->h;
			state->cc.cy = state->cc.ac = 0;
			state->cc.z = (state->a == 0) ? 1 : 0;
			state->cc.s = ((state->a & 0x80) == 0x80) ? 1 : 0;
			state->cc.p = parity(state->a);
			break;
		case 0xad: 					// XRA L
			state->a = state->a ^ state->l;
			state->cc.cy = state->cc.ac = 0;
			state->cc.z = (state->a == 0) ? 1 : 0;
			state->cc.s = ((state->a & 0x80) == 0x80) ? 1 : 0;
			state->cc.p = parity(state->a);
			break;
		case 0xae: 					// XRA M
			{
				uint16_t offset = (state->h << 8) | state->l;
				state->a = state->a ^ state->memory[offset];
				state->cc.cy = state->cc.ac = 0;
				state->cc.z = (state->a == 0) ? 1 : 0;
				state->cc.s = ((state->a & 0x80) == 0x80) ? 1 : 0;
				state->cc.p = parity(state->a);
				break;
			}
			break;
		case 0xaf: 					// XRA A
			state->a = state->a ^ state->a;
			state->cc.cy = state->cc.ac = 0;
			state->cc.z = (state->a == 0) ? 1 : 0;
			state->cc.s = ((state->a & 0x80) == 0x80) ? 1 : 0;
			state->cc.p = parity(state->a);
			break;

		case 0xb0: 					// ORA B
			state->a = state->a | state->b;
			state->cc.cy = state->cc.ac = 0;
			state->cc.z = (state->a == 0) ? 1 : 0;
			state->cc.s = ((state->a & 0x80) == 0x80) ? 1 : 0;
			state->cc.p = parity(state->a);
			break;
		case 0xb1: 					// ORA C
			state->a = state->a | state->c;
			state->cc.cy = state->cc.ac = 0;
			state->cc.z = (state->a == 0) ? 1 : 0;
			state->cc.s = ((state->a & 0x80) == 0x80) ? 1 : 0;
			state->cc.p = parity(state->a);
			break;
		case 0xb2: 					// ORA D
			state->a = state->a | state->d;
			state->cc.cy = state->cc.ac = 0;
			state->cc.z = (state->a == 0) ? 1 : 0;
			state->cc.s = ((state->a & 0x80) == 0x80) ? 1 : 0;
			state->cc.p = parity(state->a);
			break;
		case 0xb3: 					// ORA E
			state->a = state->a | state->e;
			state->cc.cy = state->cc.ac = 0;
			state->cc.z = (state->a == 0) ? 1 : 0;
			state->cc.s = ((state->a & 0x80) == 0x80) ? 1 : 0;
			state->cc.p = parity(state->a);
			break;
		case 0xb4: 					// ORA H
			state->a = state->a | state->h;
			state->cc.cy = state->cc.ac = 0;
			state->cc.z = (state->a == 0) ? 1 : 0;
			state->cc.s = ((state->a & 0x80) == 0x80) ? 1 : 0;
			state->cc.p = parity(state->a);
			break;
		case 0xb5: 					// ORA L
			state->a = state->a | state->l;
			state->cc.cy = state->cc.ac = 0;
			state->cc.z = (state->a == 0) ? 1 : 0;
			state->cc.s = ((state->a & 0x80) == 0x80) ? 1 : 0;
			state->cc.p = parity(state->a);
			break;
		case 0xb6: 					// ORA M
			{
				uint16_t offset = (state->h << 8) | state->l;
				state->a = state->a | state->memory[offset];
				state->cc.cy = state->cc.ac = 0;
				state->cc.z = (state->a == 0) ? 1 : 0;
				state->cc.s = ((state->a & 0x80) == 0x80) ? 1 : 0;
				state->cc.p = parity(state->a);
			}
			break;
		case 0xb7: 					// ORA A
			state->a = state->a | state->a;
			state->cc.cy = state->cc.ac = 0;
			state->cc.z = (state->a == 0) ? 1 : 0;
			state->cc.s = ((state->a & 0x80) == 0x80) ? 1 : 0;
			state->cc.p = parity(state->a);
			break;
		case 0xb8: 					// CMP B
			{
				uint8_t x = state->a - state->b;
				state->cc.z = (x == 0) ? 1 : 0;
				state->cc.s = ((x & 0x80) == 0x80) ? 1 : 0;
				state->cc.p = parity(x);
				state->cc.cy = (state->a < opcode[1]) ? 1 : 0;
			}
			break;
		case 0xb9: 					// CMP C
			{
				uint8_t x = state->a - state->c;
				state->cc.z = (x == 0) ? 1 : 0;
				state->cc.s = ((x & 0x80) == 0x80) ? 1 : 0;
				state->cc.p = parity(x);
				state->cc.cy = (state->a < opcode[1]) ? 1 : 0;
			}
			break;
		case 0xba: 					// CMP D
			{
				uint8_t x = state->a - state->d;
				state->cc.z = (x == 0) ? 1 : 0;
				state->cc.s = ((x & 0x80) == 0x80) ? 1 : 0;
				state->cc.p = parity(x);
				state->cc.cy = (state->a < opcode[1]) ? 1 : 0;
			}
			break;
		case 0xbb: 					// CMP E
			{
				uint8_t x = state->a - state->e;
				state->cc.z = (x == 0) ? 1 : 0;
				state->cc.s = ((x & 0x80) == 0x80) ? 1 : 0;
				state->cc.p = parity(x);
				state->cc.cy = (state->a < opcode[1]) ? 1 : 0;
			}
			break;
		case 0xbc: 					// CMP H
			{
				uint8_t x = state->a - state->h;
				state->cc.z = (x == 0) ? 1 : 0;
				state->cc.s = ((x & 0x80) == 0x80) ? 1 : 0;
				state->cc.p = parity(x);
				state->cc.cy = (state->a < opcode[1]) ? 1 : 0;
			}
			break;
		case 0xbd: 					// CMP L
			{
				uint8_t x = state->a - state->l;
				state->cc.z = (x == 0) ? 1 : 0;
				state->cc.s = ((x & 0x80) == 0x80) ? 1 : 0;
				state->cc.p = parity(x);
				state->cc.cy = (state->a < opcode[1]) ? 1 : 0;
			}
			break;
		case 0xbe: 					// CMP M
			{
				uint16_t offset = (state->h << 8) | state->l;
				uint8_t x = state->a - state->memory[offset];
				state->cc.z = (x == 0) ? 1 : 0;
				state->cc.s = ((x & 0x80) == 0x80) ? 1 : 0;
				state->cc.p = parity(x);
				state->cc.cy = (state->a < opcode[1]) ? 1 : 0;
			}
			break;
		case 0xbf: 					// CMP A
			{
				uint8_t x = state->a - state->a;
				state->cc.z = (x == 0) ? 1 : 0;
				state->cc.s = ((x & 0x80) == 0x80) ? 1 : 0;
				state->cc.p = parity(x);
				state->cc.cy = (state->a < opcode[1]) ? 1 : 0;
			}
			break;

		case 0xc0: 					// RNZ
			{
				if (state->cc.z == 0) {
					state->pc = state->memory[state->sp] | (state->memory[state->sp + 1] << 8);
					state->sp += 2;
				}
			}
			break;
		case 0xc1: 					// POP B
			{
				state->c = state->memory[state->sp];
				state->b = state->memory[state->sp + 1];
				state->sp += 2;
			}
			break;
		case 0xc2: 					// JNZ addr
			if (state->cc.z == 0)
				state->pc = (opcode[2] << 8) | opcode[1];
			else
				state->pc += 2;
			break;
		case 0xc3:					// JMP addr
			state->pc = (opcode[2] << 8) | opcode[1];
			break;
		case 0xc4: 					// CNZ
			{
				if (state->cc.z == 0) {
					uint16_t ret = state->pc + 2;
					state->memory[state->sp - 1] = (ret >> 8) & 0xff;
					state->memory[state->sp - 2] = (ret & 0xff);
					state->sp = state->sp - 2;
					state->pc = (opcode[2] << 8) | opcode[1];
				} else {
					state->pc += 2;
				}
			}
			break;
		case 0xc5: 					// PUSH B
			{
				state->memory[state->sp - 1] = state->b;
				state->memory[state->sp - 2] = state->c;
				state->sp = state->sp - 2;
			}
			break;
		case 0xc6: 					// ADI D8
			{
				uint16_t answer = (uint16_t)state->a + (uint16_t)opcode[1];
				state->cc.z = ((answer & 0xff) == 0) ? 1 : 0;
				state->cc.s = ((answer & 0x80) != 0) ? 1 : 0;
				state->cc.cy = (answer > 0xff) ? 1 : 0;
				state->cc.p = parity(answer & 0xff);
				state->a = answer & 0xff;
				state->pc++;
			}
			break;
		case 0xc7: 					// RST 0
			// CALL $0
			{
				uint16_t ret = state->pc + 2;
				state->memory[state->sp - 1] = (ret >> 8) & 0xff;
				state->memory[state->sp - 2] = (ret & 0xff);
				state->sp = state->sp - 2;
				state->pc = 0x0;
			}
			break;
		case 0xc8: 					// RZ
			{
				if (state->cc.z == 1) {
					state->pc = state->memory[state->sp] | (state->memory[state->sp + 1] << 8);
					state->sp += 2;
				}
			}
			break;
		case 0xc9:					// RET
			state->pc = state->memory[state->sp] | (state->memory[state->sp + 1] << 8);
			state->sp += 2;
			break;
		case 0xca: 					// JZ
			if (state->cc.z == 1)
				state->pc = (opcode[2] << 8) | opcode[1];
			else
				state->pc += 2;
			break;
		case 0xcb: break;			// NOP
		case 0xcc: 					// CZ
			{
				if (state->cc.z == 1) {
					uint16_t ret = state->pc + 2;
					state->memory[state->sp - 1] = (ret >> 8) & 0xff;
					state->memory[state->sp - 2] = (ret & 0xff);
					state->sp = state->sp - 2;
					state->pc = (opcode[2] << 8) | opcode[1];
				} else {
					state->pc += 2;
				}
			}
			break;
		case 0xcd: 					// CALL addr
			{
				uint16_t ret = state->pc + 2;
				state->memory[state->sp - 1] = (ret >> 8) & 0xff;
				state->memory[state->sp - 2] = (ret & 0xff);
				state->sp = state->sp - 2;
				state->pc = (opcode[2] << 8) | opcode[1];
			}
			break;
		case 0xce: UnimplementedInstruction(state); break;
		case 0xcf: 					// RST 1
			// CALL $8
			{
				uint16_t ret = state->pc + 2;
				state->memory[state->sp - 1] = (ret >> 8) & 0xff;
				state->memory[state->sp - 2] = (ret & 0xff);
				state->sp = state->sp - 2;
				state->pc = 0x8;
			}
			break;

		case 0xd0: 					// RNC
			{
				if (state->cc.cy == 0) {
					state->pc = state->memory[state->sp] | (state->memory[state->sp + 1] << 8);
					state->sp += 2;
				}
			}
			break;
		case 0xd1: 					// POP D
			{
				state->e = state->memory[state->sp];
				state->d = state->memory[state->sp + 1];
				state->sp += 2;
			}
			break;
		case 0xd2: 					// JNC addr
			if (state->cc.cy == 0)
				state->pc = (opcode[2] << 8) | opcode[1];
			else
				state->pc += 2;
			break;
		case 0xd3: 					// OUT D8
			// This is weird, as it pushes out data out to a device
			// Since this is a two byte instruction, just increment
			// pc by 1 and move on.
			// UnimplementedInstruction(state);
			state->pc++;
			break;
		case 0xd4: 					// CNC
			{
				if (state->cc.cy == 0) {
					uint16_t ret = state->pc + 2;
					state->memory[state->sp - 1] = (ret >> 8) & 0xff;
					state->memory[state->sp - 2] = (ret & 0xff);
					state->sp = state->sp - 2;
					state->pc = (opcode[2] << 8) | opcode[1];
				} else {
					state->pc += 2;
				}
			}
			break;
		case 0xd5: 					// PUSH D
			{
				state->memory[state->sp - 1] = state->d;
				state->memory[state->sp - 2] = state->e;
				state->sp = state->sp - 2;
			}
			break;
		case 0xd6: 					// SUI D8
			{
				uint8_t x = state->a - opcode[1];
				state->cc.z = (x == 0) ? 1 : 0;
				state->cc.s = ((x & 0x80) == 0x80) ? 1 : 0;
				state->cc.p = parity(x);
				state->cc.cy = (state->a < opcode[1]) ? 1 : 0;
				state->a = x;
				state->pc++;
			}
			break;
		case 0xd7: 					// RST 2
			// CALL $10
			{
				uint16_t ret = state->pc + 2;
				state->memory[state->sp - 1] = (ret >> 8) & 0xff;
				state->memory[state->sp - 2] = (ret & 0xff);
				state->sp = state->sp - 2;
				state->pc = 0x10;
			}
			break;
		case 0xd8: 					// RC
			{
				if (state->cc.cy == 1) {
					state->pc = state->memory[state->sp] | (state->memory[state->sp + 1] << 8);
					state->sp += 2;
				}
			}
			break;
		case 0xd9: break;			// NOP
		case 0xda: 					// JC addr
			if (state->cc.cy == 1)
				state->pc = (opcode[2] << 8) | opcode[1];
			else
				state->pc += 2;
			break;
		case 0xdb: 					// IN D8
			// This is weird, as it takes in data from a device
			// Since this is a two byte instruction, just increment
			// pc by 1 and move on.
			// UnimplementedInstruction(state);
			state->pc++;
			break;
		case 0xdc: 					// CC
			{
				if (state->cc.cy == 1) {
					uint16_t ret = state->pc + 2;
					state->memory[state->sp - 1] = (ret >> 8) & 0xff;
					state->memory[state->sp - 2] = (ret & 0xff);
					state->sp = state->sp - 2;
					state->pc = (opcode[2] << 8) | opcode[1];
				} else {
					state->pc += 2;
				}
			}
			break;
		case 0xdd: break;			// NOP
		case 0xde: UnimplementedInstruction(state); break;
		case 0xdf: 					// RST 3
			// CALL $18
			{
				uint16_t ret = state->pc + 2;
				state->memory[state->sp - 1] = (ret >> 8) & 0xff;
				state->memory[state->sp - 2] = (ret & 0xff);
				state->sp = state->sp - 2;
				state->pc = 0x18;
			}
			break;

		case 0xe0: 					// RPO
			{
				if (state->cc.p == 0) {
					state->pc = state->memory[state->sp] | (state->memory[state->sp + 1] << 8);
					state->sp += 2;
				}
			}
			break;
		case 0xe1: 					// POP H
			{
				state->l = state->memory[state->sp];
				state->h = state->memory[state->sp + 1];
				state->sp += 2;
			}
			break;
		case 0xe2: 					// JPO
			if (state->cc.p == 0)
				state->pc = (opcode[2] << 8) | opcode[1];
			else
				state->pc += 2;
			break;
		case 0xe3: 					// XTHL
			{
				uint8_t save1 = state->l;
				uint8_t save2 = state->h;
				state->l= state->memory[state->sp];
				state->h = state->memory[state->sp + 1];
				state->memory[state->sp] = save1;
				state->memory[state->sp + 1] = save2;
			}
			break;
		case 0xe4: 					// CPO
			{
				if (state->cc.p == 0) {
					uint16_t ret = state->pc + 2;
					state->memory[state->sp - 1] = (ret >> 8) & 0xff;
					state->memory[state->sp - 2] = (ret & 0xff);
					state->sp = state->sp - 2;
					state->pc = (opcode[2] << 8) | opcode[1];
				} else {
					state->pc += 2;
				}
			}
			break;
		case 0xe5: 					// PUSH H
			{
				state->memory[state->sp - 1] = state->h;
				state->memory[state->sp - 2] = state->l;
				state->sp = state->sp - 2;
			}
			break;
		case 0xe6: 					// ANI D8
			{
				uint8_t x = state->a & opcode[1];
				state->cc.z = (x == 0) ? 1 : 0;
				state->cc.s = ((x & 0x80) == 0x80) ? 1 : 0;
				state->cc.p = parity(x);
				state->cc.cy = 0;
				state->a = x;
				state->pc++;
			}
			break;
		case 0xe7: 					// RST 4
			// CALL $20
			{
				uint16_t ret = state->pc + 2;
				state->memory[state->sp - 1] = (ret >> 8) & 0xff;
				state->memory[state->sp - 2] = (ret & 0xff);
				state->sp = state->sp - 2;
				state->pc = 0x20;
			}
			break;
		case 0xe8: 					// RPE
			{
				if (state->cc.p == 1) {
					state->pc = state->memory[state->sp] | (state->memory[state->sp + 1] << 8);
					state->sp += 2;
				}
			}
			break;
		case 0xe9: 					// PCHL
			{
				uint16_t hl = (state->h << 8) | state->l;
				state->pc = hl;
			}
			break;
		case 0xea: 					// JPE
			if (state->cc.p == 1)
				state->pc = (opcode[2] << 8) | opcode[1];
			else
				state->pc += 2;
			break;
		case 0xeb: 					// XCHG
			{
				uint8_t save1 = state->d;
				uint8_t save2 = state->e;
				state->d = state->h;
				state->e = state->l;
				state->h = save1;
				state->l = save2;
			}
			break;
		case 0xec: 					// CPE
			{
				if (state->cc.p == 1) {
					uint16_t ret = state->pc + 2;
					state->memory[state->sp - 1] = (ret >> 8) & 0xff;
					state->memory[state->sp - 2] = (ret & 0xff);
					state->sp = state->sp - 2;
					state->pc = (opcode[2] << 8) | opcode[1];
				} else {
					state->pc += 2;
				}
			}
			break;
		case 0xed: break;			// NOP
		case 0xee: 					// XRI D8
			state->a = state->a ^ opcode[1];
			state->cc.cy = state->cc.ac = 0;
			state->cc.z = (state->a == 0) ? 1 : 0;
			state->cc.s = ((state->a & 0x80) == 0x80) ? 1 : 0;
			state->cc.p = parity(state->a);
			state->pc++;
			break;
		case 0xef: 					// RST 5
			// CALL $28
			{
				uint16_t ret = state->pc + 2;
				state->memory[state->sp - 1] = (ret >> 8) & 0xff;
				state->memory[state->sp - 2] = (ret & 0xff);
				state->sp = state->sp - 2;
				state->pc = 0x28;
			}
			break;

		case 0xf0: 					// RP
			{
				if (state->cc.s == 0) {
					state->pc = state->memory[state->sp] | (state->memory[state->sp + 1] << 8);
					state->sp += 2;
				}
			}
			break;
		case 0xf1: 					// POP PSW
			{
				state->a = state->memory[state->sp + 1];
				uint8_t psw = state->memory[state->sp];
				state->cc.z = ((psw & 0x01) == 0x01) ? 1 : 0;
				state->cc.s = ((psw & 0x02) == 0x02) ? 1 : 0;
				state->cc.p = ((psw & 0x04) == 0x04) ? 1 : 0;
				state->cc.cy = ((psw & 0x08) == 0x05) ? 1 : 0;
				state->cc.ac = ((psw & 0x10) == 0x10) ? 1 : 0;
				state->sp += 2;
			}
			break;
		case 0xf2:					// JP
			if (state->cc.s == 0)
				state->pc = (opcode[2] << 8) | opcode[1];
			else
				state->pc += 2;
			break;
		case 0xf3: 					// DI
			state->int_enable = 0;
			break;
		case 0xf4: 					// CP
			{
				if (state->cc.s == 0) {
					uint16_t ret = state->pc + 2;
					state->memory[state->sp - 1] = (ret >> 8) & 0xff;
					state->memory[state->sp - 2] = (ret & 0xff);
					state->sp = state->sp - 2;
					state->pc = (opcode[2] << 8) | opcode[1];
				} else {
					state->pc += 2;
				}
			}
			break;
		case 0xf5: 					// PUSH PSW
			{
				state->memory[state->sp - 1] = state->a;
				uint8_t psw = (state->cc.z | 
								state->cc.s << 1 | 
								state->cc.p << 2 |
								state->cc.cy << 3 |
								state->cc.ac << 4);
				state->memory[state->sp - 2] = psw;
				state->sp = state->sp - 2;
			}
			break;
		case 0xf6: 					// ORI D8
			state->a = state->a | opcode[1];
			state->cc.cy = state->cc.ac = 0;
			state->cc.z = (state->a == 0) ? 1 : 0;
			state->cc.s = ((state->a & 0x80) == 0x80) ? 1 : 0;
			state->cc.p = parity(state->a);
			state->pc++;
			break;
		case 0xf7: 					// RST 6
			// CALL $30
			{
				uint16_t ret = state->pc + 2;
				state->memory[state->sp - 1] = (ret >> 8) & 0xff;
				state->memory[state->sp - 2] = (ret & 0xff);
				state->sp = state->sp - 2;
				state->pc = 0x30;
			}
			break;
		case 0xf8: 					// RM
			{
				if (state->cc.s == 1) {
					state->pc = state->memory[state->sp] | (state->memory[state->sp + 1] << 8);
					state->sp += 2;
				}
			}
			break;
		case 0xf9: 					// SPHL
			{
				uint16_t hl = (state->h << 8) | state->l;
				state->sp = hl;
			}
			break;
		case 0xfa: 					// JM
			if (state->cc.s == 1)
				state->pc = (opcode[2] << 8) | opcode[1];
			else
				state->pc += 2;
			break;
		case 0xfb: 					// EI
			state->int_enable = 1;
			break;
		case 0xfc: 					// CM
			{
				if (state->cc.s == 1) {
					uint16_t ret = state->pc + 2;
					state->memory[state->sp - 1] = (ret >> 8) & 0xff;
					state->memory[state->sp - 2] = (ret & 0xff);
					state->sp = state->sp - 2;
					state->pc = (opcode[2] << 8) | opcode[1];
				} else {
					state->pc += 2;
				}
			}
			break;
		case 0xfd: break;			// NOP
		case 0xfe: 					// CPI D8
			{
				uint8_t x = state->a - opcode[1];
				state->cc.z = (x == 0) ? 1 : 0;
				state->cc.s = ((x & 0x80) == 0x80) ? 1 : 0;
				state->cc.p = parity(x);
				state->cc.cy = (state->a < opcode[1]) ? 1 : 0;
				state->pc++;
			}
			break;
		case 0xff: 					// RST 7
			// CALL $38
			{
				uint16_t ret = state->pc + 2;
				state->memory[state->sp - 1] = (ret >> 8) & 0xff;
				state->memory[state->sp - 2] = (ret & 0xff);
				state->sp = state->sp - 2;
				state->pc = 0x38;
			}
			break;

		default: UnimplementedInstruction(state); break;
	}

	printf("\t");
	printf("%c", state->cc.z ? 'z' : '.');
	printf("%c", state->cc.s ? 's' : '.');
	printf("%c", state->cc.p ? 'p' : '.');
	printf("%c", state->cc.cy ? 'c' : '.');
	printf("%c\n", state->cc.ac ? 'a' : '.');
	printf("\tA $%02x B $%02x C $%02x D $%02x E $%02x H $%02x L $%02x SP $%04x\n",
			state->a, state->b, state->c, state->d, state->e, state->h, state->l,
			state->sp);

	return 0;
}