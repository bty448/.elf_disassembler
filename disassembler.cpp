#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <iterator>
#include <algorithm>
#include <map>

using namespace std;

struct sectionInfo {
	string name;

	uint32_t offset;
	uint32_t length;
	uint32_t index;
	bool exists;

	sectionInfo() {
		exists = false;
	}

	sectionInfo(uint32_t _offset, uint32_t _length, uint32_t _index, bool _exists) {
		offset = _offset;
		length = _length;
		index = _index;
		exists = _exists;
	}
};
sectionInfo programmSectionInfo;
sectionInfo symtabSectionInfo;
sectionInfo strtabSectionInfo;

vector<unsigned char> bytes;
map<uint32_t, string> marks, strtab, result;
vector<string> output;
const uint32_t instructionLength = 4;
const uint32_t sectionHeaderLength = 40;
const uint32_t symtabElementLength = 16;
uint32_t maxMarkLength = 0;

bool checkIfFileIsElf() {
	if (bytes[0] == 0x7f && bytes[1] == 0x45 && bytes[2] == 0x4c && bytes[3] == 0x46) {
		return true;
	}
	cout << "This is not an ELF file.\n";
	return false;
}

bool checkBitFormat() {
	if (bytes[4] == 0x01) {
		return true;
	}
	cout << "The format is not 32-bit.\n";
	return false;
}

bool checkEndianness() {
	if (bytes[5] == 0x01) {
		return true;
	}
	cout << "The endianness is not little-endian.\n";
	return false;
}

bool checkISA() {
	if (bytes[18] == 0xf3) {
		return true;
	}
	cout << "The ISA is not RISC-V.\n";
	return false;
}

bool checkHeader() {
	return checkIfFileIsElf() && checkBitFormat() && checkEndianness() && checkISA();
}

uint32_t binPow(uint32_t base, uint32_t power) {
	if (power == 0) {
		return 1;
	} else if (power % 2 == 1) {
		return base * binPow(base, power - 1);
	} else {
		uint32_t x = binPow(base, power / 2);
		return x * x;
	}
}

uint32_t readLittleEndian(uint32_t start, uint32_t end) {
	uint32_t result = 0;
	for (int i = end; i >= start; i--) {
		result += binPow(16, 2 * (i - start)) * (uint32_t) bytes[i];
	}
	return result;
}

uint32_t getExecutionStartAdress() {
	uint32_t adress = readLittleEndian(24, 27);
	return adress;
}

uint32_t getSectionHeadersOffset() {
	uint32_t offset = readLittleEndian(32, 35);
	return offset;
}

void findSections() {
	uint32_t cnt = 0;
	for (uint32_t i = getSectionHeadersOffset(); i < bytes.size(); i += sectionHeaderLength) {
		uint32_t sh_type = readLittleEndian(i + 4, i + 7);
		uint32_t offset = readLittleEndian(i + 16, i + 19);
		uint32_t length = readLittleEndian(i + 20, i + 23);
		if (sh_type == 0x1 && !programmSectionInfo.exists) {
			programmSectionInfo = sectionInfo(offset, length, cnt, true);
		} else if (sh_type == 0x2 && !symtabSectionInfo.exists) {
			symtabSectionInfo = sectionInfo(offset, length, cnt, true);
		} else if (sh_type == 0x3 && !strtabSectionInfo.exists) {
			strtabSectionInfo = sectionInfo(offset, length, cnt, true);
		}
		cnt++;
	}
}

void getMarks() {
	for (uint32_t i = symtabSectionInfo.offset; i < symtabSectionInfo.offset + symtabSectionInfo.length; i += symtabElementLength) {
		uint32_t st_name = readLittleEndian(i, i + 3);
		uint32_t st_value = readLittleEndian(i + 4, i + 7);
		uint32_t st_size = readLittleEndian(i + 8, i + 11);
		uint16_t st_shndx = readLittleEndian(i + 14, i + 15);
		if (!(st_shndx == programmSectionInfo.index && strtab.find(st_name) == strtab.end())) {
			maxMarkLength = max(maxMarkLength, (uint32_t) strtab[st_name].size());
			marks[st_value] = strtab[st_name];
		}
	}
}

void getStringsFromStrTab() {
	string cur = "";
	int st = 0;
	for (int i = strtabSectionInfo.offset; i < strtabSectionInfo.offset + strtabSectionInfo.length; i++) {
		if (bytes[i] != 0) {
			cur += bytes[i];
		} else if (cur.size() > 0) {
			strtab[st] = cur;
			cur = "";
			st = i + 1 - strtabSectionInfo.offset;
		}
	}
}

string uintToBinaryString(uint32_t a) {
	string res = "";
	while (a > 0) {
		res += to_string(a % 2);
		a /= 2;
	}
	while (res.size() < instructionLength * 8) {
		res += "0";
	}
	reverse(res.begin(), res.end());
	return res;
}

string binToHex(string bin) {
	string res = "";
	reverse(bin.begin(), bin.end());
	while (bin.size() % 4 != 0) {
		bin += "0";
	}
	for (int i = 0; i < bin.size(); i += 4) {
		int value = 0;
		for (int j = i; j < i + 4; j++) {
			value += (bin[j] - '0') * binPow(2, j - i);
		}
		if (value < 10) {
			res += to_string(value);
		} else {
			switch (value) {
				case 10 : res += "a"; break;
				case 11 : res += "b"; break;
				case 12 : res += "c"; break;
				case 13 : res += "d"; break;
				case 14 : res += "e"; break;
				case 15 : res += "f"; break;
			}
		}
	}
	reverse(res.begin(), res.end());
	return res;
}

string uintTo8Hex(uint32_t value) {
	return binToHex(uintToBinaryString(value));
}

uint32_t binaryToUInt(string s) {
	uint32_t res = 0;
	reverse(s.begin(), s.end());
	for (int i = 0; i < s.size(); i++) {
		res += (s[i] - '0') * binPow(2, i);
	}
	return res;
}

int binaryToInt(string s) {
	int res = 0;
	reverse(s.begin(), s.end());
	for (int i = 0; i < s.size() - 1; i++) {
		res += (s[i] - '0') * binPow(2, i);
	}
	res += (s[s.size() - 1] - '0') * (-binPow(2, s.size() - 1));
	return res;
}

string binToUintStr(string s) {
	return to_string(binaryToUInt(s));
}

string binToIntStr(string s) {
	return to_string(binaryToInt(s));
}

string getRegName(int x) {
	if (x == 0) {
		return "zero";
	} else if (x == 1) {
		return "ra";
	} else if (x == 2) {
		return "sp";
	} else if (x == 3) {
		return "gp";
	} else if (x == 4) {
		return "tp";
	} else if (x == 5 || x == 6 || x == 7) {
		return "t" + to_string(x - 5);
	} else if (x == 8 || x == 9) {
		return "s" + to_string(x - 8);
	} else if (10 <= x && x <= 17) {
		return "a" + to_string(x - 10);
	} else if (18 <= x && x <= 27) {
		return "s" + to_string(x - 16);
	} else if (28 <= x && x <= 31) {
		return "t" + to_string(x - 25);
	}
	return "Error register name";
}

string decodeI1(string instruction) {
	return getRegName(binaryToUInt(instruction.substr(20, 5)))
	+ ", " 
	+ binToIntStr(instruction.substr(0, 12)) 
	+ "("
	+ getRegName(binaryToUInt(instruction.substr(12, 5)))
	+ ")";
}

string decodeI2(string instruction) {
	return getRegName(binaryToUInt(instruction.substr(20, 5)))
	+ ", "
	+ getRegName(binaryToUInt(instruction.substr(12, 5)))
	+ ", "
	+ binToIntStr(instruction.substr(0, 12));
}

string decodeU(string instruction) {
	return getRegName(binaryToUInt(instruction.substr(20, 5)))
	+ ", "
	+ binToIntStr(instruction.substr(0, 20) + "000000000000");
}

string decodeR(string instruction) {
	return getRegName(binaryToUInt(instruction.substr(20, 5)))
	+ ", "
	+ getRegName(binaryToUInt(instruction.substr(12, 5)))
	+ ", "
	+ getRegName(binaryToUInt(instruction.substr(7, 5)));
}

string decodeR2(string instruction) {
	return getRegName(binaryToUInt(instruction.substr(20, 5)))
	+ ", "
	+ getRegName(binaryToUInt(instruction.substr(12, 5)))
	+ ", "
	+ binToIntStr(instruction.substr(7, 5));
}

string decodeB(string instruction) {
	string imm = binToIntStr(instruction.substr(0, 1) 
	+ instruction.substr(24, 1)
	+ instruction.substr(1, 6)
	+ instruction.substr(20, 4)
	+ "0");
	return getRegName(binaryToUInt(instruction.substr(12, 5)))
	+ ", "
	+ getRegName(binaryToUInt(instruction.substr(7, 5)))
	+ ", pc + "
	+ imm;
}

string decodeS(string instruction) {
	string imm = binToIntStr(instruction.substr(0, 7) + instruction.substr(20, 5));
	return getRegName(binaryToUInt(instruction.substr(7, 5)))
	+ ", "
	+ imm
	+ "("
	+ getRegName(binaryToUInt(instruction.substr(12, 5)))
	+ ")";
}

string decodeCSR1(string instruction) {
	return getRegName(binaryToUInt(instruction.substr(20, 5))) 
	+ ", " 
	+ binToIntStr(instruction.substr(0, 12)) 
	+ ", " 
	+ getRegName(binaryToUInt(instruction.substr(12, 5)));
}

string decodeCSR2(string instruction) {
	return getRegName(binaryToUInt(instruction.substr(20, 5))) 
	+ ", " 
	+ binToIntStr(instruction.substr(0, 12)) 
	+ ", " 
	+ binToUintStr(instruction.substr(12, 5));
}

void decodeInstruction(uint32_t code, uint32_t address) {
	string instruction = uintToBinaryString(code);
	string opcode = instruction.substr(25);
	string res = "";
	if (opcode == "0110111") {
		res += "lui " +  decodeU(instruction);
	} else if (opcode == "0010111") {
		res += "auipc " + decodeU(instruction);
	} else if (opcode == "1101111") {
		int imm = binaryToInt(instruction.substr(0, 1)
			+ instruction.substr(13, 8)
			+ instruction.substr(11, 1)
			+ instruction.substr(1, 10)
			+ "0");
		res += "jal "
		+ getRegName(binaryToUInt(instruction.substr(20, 5)))
		+ ", pc + "
		+ to_string(imm)
		+ "\t# 0x"
		+ uintTo8Hex(address + imm);
	} else if (opcode == "1100111") {
		res += "jalr " + decodeI1(instruction);
	} else if (opcode == "1100011") {
		string funct3 = instruction.substr(17, 3);
		if (funct3 == "000") {
			res += "beq " + decodeB(instruction);
		} else if (funct3 == "001") {
			res += "bne " + decodeB(instruction);
		} else if (funct3 == "100") {
			res += "blt " + decodeB(instruction);
		} else if (funct3 == "101") {
			res += "bge " + decodeB(instruction);
		} else if (funct3 == "110") {
			res += "bltu " + decodeB(instruction);
		} else if (funct3 == "111") {
			res += "bgeu " + decodeB(instruction);
		}
	} else if (opcode == "0000011") {
		string funct3 = instruction.substr(17, 3);
		if (funct3 == "000") {
			res += "lb " + decodeI1(instruction);
		} else if (funct3 == "001") {
			res += "lh " + decodeI1(instruction);
		} else if (funct3 == "010") {
			res += "lw " + decodeI1(instruction);
		} else if (funct3 == "100") {
			res += "lbu " + decodeI1(instruction);
		} else if (funct3 == "101") {
			res += "lhu " + decodeI1(instruction);
		}
	} else if (opcode == "0100011") {
		string funct3 = instruction.substr(17, 3);
		if (funct3 == "000") {
			res += "sb " + decodeS(instruction);
		} else if (funct3 == "001") {
			res += "sh " + decodeS(instruction);
		} else if (funct3 == "010") {
			res += "sw " + decodeS(instruction);
		}
	} else if (opcode == "0010011") {
		string funct3 = instruction.substr(17, 3);
		if (funct3 == "000") {
			res += "addi " + decodeI2(instruction);
		} else if (funct3 == "010") {
			res += "slti " + decodeI2(instruction);
		} else if (funct3 == "011") {
			res += "sltiu " + decodeI2(instruction);
		} else if (funct3 == "100") {
			res += "xori " + decodeI2(instruction);
		} else if (funct3 == "110") {
			res += "ori " + decodeI2(instruction);
		} else if (funct3 == "111") {
			res += "andi " + decodeI2(instruction);
		} else {
			string funct7 = instruction.substr(0, 7);
			if (funct3 == "001") {
				res += "slli " + decodeR2(instruction);
			} else if (funct3 == "101") {
				if (funct7 == "0000000") {
					res += "srli " + decodeR2(instruction);
				} else if (funct7 == "0100000") {
					res += "srai " + decodeR2(instruction);
				}
			}
		}
	} else if (opcode == "0110011") {
		string funct7 = instruction.substr(0, 7);
		string funct3 = instruction.substr(17, 3);
		if (funct3 == "000") {
			if (funct7 == "0000000") {
				res += "add " + decodeR(instruction);
			} else if (funct7 == "0100000") {
				res += "sub " + decodeR(instruction);
			} else if (funct7 == "0000001") {
				res += "mul " + decodeR(instruction);
			}
		} else if (funct3 == "001") {
			if (funct7 == "0000000") {
				res += "sll " + decodeR(instruction);
			} else if (funct7 == "0000001") {
				res += "mulh " + decodeR(instruction);
			}
		} else if (funct3 == "010") {
			if (funct7 == "0000000") {
				res += "slt " + decodeR(instruction);
			} else if (funct7 == "0000001") {
				res += "mulhsu " + decodeR(instruction);
			}
		} else if (funct3 == "011") {
			if (funct7 == "0000000") {
				res += "sltu " + decodeR(instruction);
			} else if (funct7 == "0000001") {
				res += "mulhu " + decodeR(instruction);
			}
		} else if (funct3 == "100") {
			if (funct7 == "0000000") {
				res += "xor " + decodeR(instruction);
			} else if (funct7 == "0000001") {
				res += "div " + decodeR(instruction);
			}
		} else if (funct3 == "101") {
			if (funct7 == "0000000") {
				res += "srl " + decodeR(instruction);
			} else if (funct7 == "0100000") {
				res += "sra " + decodeR(instruction);
			} else if (funct7 == "0000001") {
				res += "divu " + decodeR(instruction);
			}
		} else if (funct3 == "110") {
			if (funct7 == "0000000") {
				res += "or " + decodeR(instruction);
			} else if (funct7 == "0000001") {
				res += "rem " + decodeR(instruction);
			}
		} else if (funct3 == "111") {
			if (funct7 == "0000000") {
				res += "and " + decodeR(instruction);
			} else if (funct7 == "0000001") {
				res += "remu " + decodeR(instruction);
			}
		}
	} else if (opcode == "1110011") {
		string funct3 = instruction.substr(17, 3);
		string funct5 = instruction.substr(20, 5);
		string funct12 = instruction.substr(0, 12);
		if (funct3 == "000") {
			if (funct12 == "000000000000") {
				res += "ecall";
			} else if (funct12 == "000000000001") {
				res += "ebreak";
			}
		} else if (funct3 == "001") {
			res += "csrrw " + decodeCSR1(instruction);
		} else if (funct3 == "010") {
			res += "csrrs " + decodeCSR1(instruction);
		} else if (funct3 == "011") {
			res += "csrrc " + decodeCSR1(instruction);
		} else if (funct3 == "101") {
			res += "csrrwi " + decodeCSR2(instruction);
		} else if (funct3 == "110") {
			res += "csrrsi " + decodeCSR2(instruction);
		} else if (funct3 == "111") {
			res += "csrrci " + decodeCSR2(instruction);
		}
	}
	if (res == "") {
		res += "unknown command";
	}
	result[address] = res;
}

void readProgramm(uint32_t startProgrammAddress) {
	for (uint32_t i = programmSectionInfo.offset; i < programmSectionInfo.offset + programmSectionInfo.length; i += instructionLength) {
		decodeInstruction(readLittleEndian(i, i + 3), startProgrammAddress + i - programmSectionInfo.offset);
	}
}

void compileOutput() {
	for (auto it = result.begin(); it != result.end(); it++) {
		string curLine = uintTo8Hex(it->first);
		uint32_t usedSpace = 0;
		if (marks.find(it->first) != marks.end()) {
			curLine += " <" + marks[it->first] + "> ";
			usedSpace = 2 + (uint32_t) marks[it->first].size() + 2;
		} 
		for (uint32_t i = usedSpace; i < maxMarkLength + 4; i++) {
			curLine += " ";
		}
		curLine += it->second;
		output.push_back(curLine);
	}
}

int main(int argc, char** argv) {
	if (argc == 1) {
		cout << "Input file name not attached.\n";
		return 0;
	}
	ifstream ifile;
	try {
		ifile.open(argv[1], ios::in | ios::binary);
	} catch (exception e) {
		cout << "Error while openning input file: " << e.what() << "\n";
		return 0;
	}
	char buffer[256];
	do {
		ifile.read(buffer, 256);
		for (int i = 0; i < ifile.gcount(); i++) {
			bytes.push_back((unsigned char) buffer[i]);
		}
	} while (ifile);
	ifile.close();

	if (!checkHeader()) {
		return 0;
	}

	int startProgrammAddress = getExecutionStartAdress();

	findSections();

	if (!programmSectionInfo.exists) {
		cout << "No .text section header found.\n";
		return 0;
	}

	if (!strtabSectionInfo.exists) {
		cout << "strtab wasn't found\n";
	} else {
		getStringsFromStrTab();
	}

	if (!symtabSectionInfo.exists) {
		cout << "symtab wasn't found\n";
	} else {
		getMarks();
	}

	readProgramm(startProgrammAddress);
	
	compileOutput();

	if (argc > 2) {
		ofstream ofile;
		try {
			ofile.open(argv[2]);
		} catch (exception e) {
			cout << "Error while openning output file: " << e.what() << "\n";
			return 0;
		}
		for (int i = 0; i < output.size(); i++) {
			ofile << output[i] << "\n";
		}
		ofile.close();
	} else {
		for (int i = 0; i < output.size(); i++) {
			cout << output[i] << "\n";
		}
	}
}