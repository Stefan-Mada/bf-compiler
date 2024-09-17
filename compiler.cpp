#include <iostream>
#include <fstream>
#include <unordered_map>
#include <stack>

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

unordered_map<size_t, size_t> initializeLoopBrackets(const string& code) {
  stack<size_t> leftBrackLocs;
  unordered_map<size_t, size_t> loopMap;
  for(size_t i = 0; i < code.size(); ++i) {
    if(code[i] == '[')
      leftBrackLocs.push(i);
    else if(code[i] == ']') {
      loopMap[leftBrackLocs.top()] = i;
      loopMap[i] = leftBrackLocs.top();
      leftBrackLocs.pop();
    }
  }

  return loopMap;
}

void interpret(const string& code) {
  constexpr size_t TAPE_SIZE = 10000;
  unsigned char tape[TAPE_SIZE]{};
  size_t index = TAPE_SIZE / 2;

  unordered_map<size_t, size_t> matchingLoopBracket = initializeLoopBrackets(code);

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
          IP = matchingLoopBracket[IP] - 1;
        }
        break;
      } 
      case ']': {
        if(tape[index] != 0) {
          IP = matchingLoopBracket[IP] - 1;
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

