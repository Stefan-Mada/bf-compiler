#include <iostream>
#include <fstream>
#include <unordered_map>
#include <stack>
#include <vector>
#include <utility>
#include <array>
#include <algorithm>

using namespace std;

static bool profile = 0;


enum Op {
  MoveRight,
  MoveLeft,
  Inc,
  Dec,
  Write,
  Read,
  JumpIfZero,
  JumpUnlessZero,
  EndOfFile
};

array<char, EndOfFile> enumToChar{{'>', '<', '+', '-', '.', ',', '[', ']'}};
array<size_t, EndOfFile> instrFreq;


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

  retVec.push_back(EndOfFile);

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

  static const void* jumpTable[] = {&&LabMoveRight, &&LabMoveLeft, &&LabInc, &&LabDec, &&LabWrite, 
                                    &&LabRead, &&LabJumpIfZero, &&LabJumpUnlessZero, &&LabEndOfFile};

  size_t IP = 0;
  goto *jumpTable[ops[IP]];

LabMoveRight: {
    if(index >= TAPE_SIZE - 1) {
      cerr << "Overflowed tape size" << endl;
      exit(-1);
    }
    if(profile)
      ++instrFreq[MoveRight];
    ++index;
    goto *jumpTable[ops[++IP]];
  } 
LabMoveLeft: {
    if(index == 0) {
      cerr << "Underflowed tape size" << endl;
      exit(-1);
    }
    if(profile)
      ++instrFreq[MoveLeft];

    --index;
    goto *jumpTable[ops[++IP]];
  } 
LabInc: {
    if(profile)
      ++instrFreq[Inc];

    ++tape[index];
    goto *jumpTable[ops[++IP]];
  } 
LabDec: {
    if(profile)
      ++instrFreq[Dec];

    --tape[index];
    goto *jumpTable[ops[++IP]];
  } 
LabWrite: {
    if(profile)
      ++instrFreq[Write];

    cout << tape[index];
    goto *jumpTable[ops[++IP]];
  } 
LabRead: {
    if(profile)
      ++instrFreq[Read];

    unsigned char val;
    cin >> val;
    tape[index] = val;
    goto *jumpTable[ops[++IP]];
  } 
LabJumpIfZero: {
    if(profile)
      ++instrFreq[JumpIfZero];

    if(tape[index] == 0) {
      IP = matchingLoopBracket[IP];
      goto *jumpTable[ops[IP]];
    }
    goto *jumpTable[ops[++IP]];
  } 
LabJumpUnlessZero: {
    if(profile)
      ++instrFreq[JumpUnlessZero];

    if(tape[index] != 0) {
      IP = matchingLoopBracket[IP];
      goto *jumpTable[ops[IP]];
    }
    goto *jumpTable[ops[++IP]];
  } 
LabEndOfFile:
  return;
}

int main(int argc, char** argv) {
  if(argc != 2 && argc != 3) {
    cerr << "Need exactly one file argument to interpret, optional -p parameter first" << endl;
    exit(-1);
  }

  if(argc == 3 && string(argv[1]) != "-p") {
    cerr << "Second parameter must be -p" << endl;
    exit(-1);
  }
  else if (argc == 3)
    profile = true;


  const vector<Op> ops = readFile(argv[argc - 1]);

  interpret(ops);

  if(profile) {
    cout << "\n\n=====PROFILING=====\n";
    vector<pair<char, size_t>> instrFreqVec;
    for(size_t instrType = 0; instrType < EndOfFile; ++instrType) {
      instrFreqVec.push_back({enumToChar[instrType], instrFreq[instrType]});
    }

    sort(instrFreqVec.begin(), instrFreqVec.end(), [&](auto a, auto b){return a.second > b.second;});

    for(const auto& [op, freq] : instrFreqVec) {
      cout << op << " : " << freq << "\n";
    }
  }
}

