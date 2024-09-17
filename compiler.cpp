#include <iostream>
#include <fstream>
#include <unordered_map>
#include <stack>
#include <vector>

using namespace std;

enum Op {
  MoveRight,
  MoveLeft,
  Inc,
  Dec,
  Write,
  Read,
  JumpIfZero,
  JumpUnlessZero
};

vector<Op> readFile(string fileName) {
  ifstream fileStream(fileName);

  if(!fileStream.is_open()) {
    cerr << "Unable to open file " << fileName << endl;
    exit(-1);
  }

  vector<Op> retVec;

  char currChar;
  while(fileStream.get(currChar)) {
    switch(currChar) {
      case '>':
        retVec.push_back(MoveRight);
        break;
      case '<':
        retVec.push_back(MoveLeft);
        break;
      case '+':
        retVec.push_back(Inc);
        break;
      case '-':
        retVec.push_back(Dec);
        break;
      case '.':
        retVec.push_back(Write);
        break;
      case ',':
        retVec.push_back(Read);
        break;
      case '[':
        retVec.push_back(JumpIfZero);
        break;
      case ']':
        retVec.push_back(JumpUnlessZero);
        break;
      default:
        continue;
    }
  }

  return retVec;
}

unordered_map<size_t, size_t> initializeLoopBrackets(const vector<Op>& code) {
  stack<size_t> leftBrackLocs;
  unordered_map<size_t, size_t> loopMap;
  for(size_t i = 0; i < code.size(); ++i) {
    if(code[i] == JumpIfZero)
      leftBrackLocs.push(i);
    else if(code[i] == JumpUnlessZero) {
      loopMap[leftBrackLocs.top()] = i;
      loopMap[i] = leftBrackLocs.top();
      leftBrackLocs.pop();
    }
  }

  return loopMap;
}

void interpret(const vector<Op>& ops) {
  constexpr size_t TAPE_SIZE = 10000;
  unsigned char tape[TAPE_SIZE]{};
  size_t index = TAPE_SIZE / 2;

  unordered_map<size_t, size_t> matchingLoopBracket = initializeLoopBrackets(ops);

  for(size_t IP = 0; IP < ops.size(); ++IP) {
    Op op = ops[IP];
    switch(op) {
      case MoveRight: {
        if(index >= TAPE_SIZE - 1) {
          cerr << "Overflowed tape size" << endl;
          exit(-1);
        }
        ++index;
        break;
      } 
      case MoveLeft: {
        if(index == 0) {
          cerr << "Underflowed tape size" << endl;
          exit(-1);
        }
        --index;
        break;
      } 
      case Inc: {
        ++tape[index];
        break;
      } 
      case Dec: {
        --tape[index];
        break;
      } 
      case Write: {
        cout << tape[index];
        break;
      } 
      case Read: {
        unsigned char val;
        cin >> val;
        tape[index] = val;
        break;
      } 
      case JumpIfZero: {
        if(tape[index] == 0) {
          IP = matchingLoopBracket[IP] - 1;
        }
        break;
      } 
      case JumpUnlessZero: {
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

  const vector<Op> ops = readFile(argv[1]);

  interpret(ops);
}

