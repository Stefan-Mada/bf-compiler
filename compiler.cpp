#include <iostream>
#include <fstream>

using namespace std;

string readFile(string fileName) {
  ifstream fileStream(fileName);

  if(!fileStream.is_open()) {
    cerr << "Unable to open file " << fileName << endl;
    exit(-1);
  }

  string code;
  string line;

  while(getline(fileStream, line))
    code += line + "\n";

  return code;
}

void interpret(const string& code) {
  constexpr size_t TAPE_SIZE = 10000;
  unsigned char tape[TAPE_SIZE]{};
  size_t index = TAPE_SIZE / 2;

  for(size_t IP = 0; IP < code.size(); ++IP) {
    const unsigned char op = static_cast<unsigned char>(code[IP]);
    switch(op) {
      case '>': {
        if(index >= TAPE_SIZE - 1) {
          cerr << "Overflowed tape size" << endl;
          exit(-1);
        }
        ++index;
        break;
      } 
      case '<': {
        if(index == 0) {
          cerr << "Underflowed tape size" << endl;
          exit(-1);
        }
        --index;
        break;
      } 
      case '+': {
        ++tape[index];
        break;
      } 
      case '-': {
        --tape[index];
        break;
      } 
      case '.': {
        cout << tape[index];
        break;
      } 
      case ',': {
        unsigned char val;
        cin >> val;
        tape[index] = val;
        break;
      } 
      case '[': {
        if(tape[index] == 0) {
          int innerLoopStartsSeen = 0;
          while(code[IP] != ']' || innerLoopStartsSeen != 1) {
            if(code[IP] == '[') // will always start at 1
              ++innerLoopStartsSeen;
            else if(code[IP] == ']')
              --innerLoopStartsSeen;

            ++IP;
          }
          --IP; // loop will increment this by 1 on iteration
        }
        break;
      } 
      case ']': {
        if(tape[index] != 0) {
          int innerLoopEndsSeen = 0;
          while(code[IP] != '[' || innerLoopEndsSeen != 1) {
            if(code[IP] == ']') // will always start at 1
              ++innerLoopEndsSeen;
            else if(code[IP] == '[')
              --innerLoopEndsSeen;

            --IP;
          }
          --IP; // loop will increment this by 1 on iteration
        }
        break;
      } 
      default: {
        continue;
      }
    }
  }
}

int main(int argc, char** argv) {
  if(argc != 2) {
    cerr << "Need exactly one file argument to interpret" << endl;
    exit(-1);
  }

  const string code = readFile(argv[1]);

  interpret(code);
}

