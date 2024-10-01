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

unordered_map<string, size_t> loopFreq;
unordered_map<size_t, string> loopAtIndex;
unordered_map<string, bool> isSimpleLoop;


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

bool checkSimpleLoop(const vector<Op>& code) {
  int currMemOffset = 0;
  int currBaseInc = 0;
  for(const auto& op : code) {
    if(op == Read || op == Write)
      return false;

    if(op == MoveRight)
      ++currMemOffset;
    else if(op == MoveLeft)
      --currMemOffset;
    else if(currMemOffset == 0 && op == Inc)
      ++currBaseInc;
    else if(currMemOffset == 0 && op == Dec)
      --currBaseInc;
  }

  if(currBaseInc != -1 && currBaseInc != 1)
    return false;

  return currMemOffset == 0;
}

unordered_map<size_t, size_t> initializeLoopBrackets(const vector<Op>& code) {
  stack<size_t> leftBrackLocs;
  unordered_map<size_t, size_t> loopMap;
  bool canBeInnerLoop = false;

  for(size_t i = 0; i < code.size(); ++i) {
    if(code[i] == JumpIfZero) {
      leftBrackLocs.push(i);

      canBeInnerLoop = true;
    }
    else if(code[i] == JumpUnlessZero) {
      const size_t lhs = leftBrackLocs.top();
      loopMap[lhs] = i;
      loopMap[i] = lhs;
      leftBrackLocs.pop();

      if(profile && canBeInnerLoop) {
        const vector<Op> loopCode = vector(code.begin() + static_cast<long>(lhs), code.begin() + static_cast<long>(i) + 1);
        vector<char> loopChars(loopCode.size());
        transform(loopCode.cbegin(), loopCode.cend(), loopChars.begin(), [&](Op a){return enumToChar[a];});
        string loopString(loopChars.begin(), loopChars.end());

        loopAtIndex[lhs] = loopString;
        loopFreq[loopString] = 0;
        isSimpleLoop[loopString] = checkSimpleLoop(loopCode);
      }

      canBeInnerLoop = false;
    }
  }

  return loopMap;
}

void interpret(const vector<Op>& ops) {
  constexpr size_t TAPE_SIZE = 320'000;
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

    tape[index] = getchar();
    goto *jumpTable[ops[++IP]];
  } 
LabJumpIfZero: {
    if(profile)
      ++instrFreq[JumpIfZero];

    if(tape[index] == 0) {
      IP = matchingLoopBracket[IP];
      goto *jumpTable[ops[IP]];
    }
    else if(profile && loopAtIndex.count(IP))
      ++loopFreq[loopAtIndex[IP]];

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

    // now onto loops

    vector<pair<string, size_t>> simpleLoopFreq, complexLoopFreq;

    for(const auto& [loop, freq] : loopFreq) {
      if(isSimpleLoop[loop])
        simpleLoopFreq.push_back({loop, freq});
      else
        complexLoopFreq.push_back({loop, freq});
    }

    sort(simpleLoopFreq.begin(), simpleLoopFreq.end(), [&](auto a, auto b){return a.second > b.second;});
    sort(complexLoopFreq.begin(), complexLoopFreq.end(), [&](auto a, auto b){return a.second > b.second;});

    cout << "\n===Simple Loops===\n";
    for(const auto& [loop, freq] : simpleLoopFreq) {
      cout << loop << " : " << freq << "\n";
    }

    cout << "\n===Complex Loops===\n";
    for(const auto& [loop, freq] : complexLoopFreq) {
      cout << loop << " : " << freq << "\n";
    }
  }
}

