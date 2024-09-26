#include <iostream>
#include <fstream>
#include <optional>
#include <string>
#include <unordered_map>
#include <stack>
#include <vector>
#include <utility>
#include <array>
#include <memory>

using namespace std;

enum Op {
  MoveRight,
  MoveLeft,
  Inc,
  Dec,
  Write,
  Read,
  JumpIfZero,
  JumpUnlessZero,
  EndOfFile,
  Zero,
  SimpleLoop,
  Sum,
  MulAdd
};

string instrStr(const string& str) {
  return "\t" + str + "\n";
}

struct Instr {
  virtual string str() const = 0;
  virtual ~Instr() {}
  Op op;
};

struct MoveRightInstr : public virtual Instr {
  MoveRightInstr() {op = MoveRight;}

  string str() const override {
    return instrStr("inc\t%rdi");
  }
};

struct MoveLeftInstr : public virtual Instr {
  MoveLeftInstr() {op = MoveLeft;}

  string str() const override {
    return instrStr("dec\t%rdi");
  }
};

struct IncInstr : public virtual Instr {
  IncInstr() {op = Inc;}

  string str() const override {
    return instrStr("incb\t(%rdi)");
  }
};

struct DecInstr : public virtual Instr {
  DecInstr() {op = Dec;}

  string str() const override {
    return instrStr("decb\t(%rdi)");
  }
};

struct WriteInstr : public virtual Instr {
  WriteInstr() {op = Write;}

  string str() const override {
    string assembly;
    assembly += instrStr("push\t%rdi");
    assembly += instrStr("movb\t(%rdi), %dil");
    assembly += instrStr("call\tputchar");
    assembly += instrStr("pop\t%rdi");
    return assembly;
  }
};

struct ReadInstr : public virtual Instr {
  ReadInstr() {op = Read;}

  string str() const override {
    string assembly;
    assembly += instrStr("push\t%rdi");
    assembly += instrStr("call\tgetchar");
    assembly += instrStr("pop\t%rdi");
    assembly += instrStr("movb\t%al, (%rdi)");
    return assembly;
  }
};

struct JumpIfZeroInstr : public virtual Instr {
  JumpIfZeroInstr(const string& ownLabel, const string& targetLabel)
                : ownLabel(ownLabel), targetLabel(targetLabel) {op = JumpIfZero;}

  string str() const override {
    string assembly;
    assembly += ownLabel + ":\n";
    assembly += instrStr("cmpb\t$0, (%rdi)");
    assembly += instrStr("je\t"+targetLabel);
    return assembly;
  }

private:
  string ownLabel, targetLabel;
};

struct JumpUnlessZeroInstr : public virtual Instr {
  JumpUnlessZeroInstr(const string& ownLabel, const string& targetLabel)
                : ownLabel(ownLabel), targetLabel(targetLabel) {op = JumpUnlessZero;}

  string str() const override {
    string assembly;
    assembly += ownLabel + ":\n";
    assembly += instrStr("cmpb\t$0, (%rdi)");
    assembly += instrStr("jne\t"+targetLabel);
    return assembly;
  }

private:
  string ownLabel, targetLabel;
};

struct EndOfFileInstr : public virtual Instr {
  EndOfFileInstr() {op = EndOfFile;}

  string str() const override {
    return instrStr("ret");
  }
};

struct ZeroInstr : public virtual Instr {
  ZeroInstr() {op = Zero;}

  string str() const override {
    return instrStr("movb\t$0, (%rdi)");
  }
};

struct SumInstr : public virtual Instr {
  SumInstr(const int64_t amount, const int64_t offset)
          : amount(amount), offset(offset) {op = Sum;}

  string str() const override {
    const string offsetStr = (offset == 0) ? "" : to_string(offset);
    
    return instrStr("addb\t$"+to_string(amount)+", "+offsetStr+"(%rdi)");
  }
private:
  int64_t amount;
  int64_t offset;
};

struct MulAddInstr : public virtual Instr {
  MulAddInstr(const int64_t amount, const int64_t offset, const bool posInc)
          : amount(amount), offset(offset), posInc(posInc) {op = MulAdd;}

  string str() const override {
    const string offsetStr = (offset == 0) ? "" : to_string(offset);
    string assembly;
    assembly += instrStr("movb\t(%rdi), %al");
    if(posInc)
      assembly += instrStr("xorb\t$-1, %al");
    assembly += instrStr("movb\t$"+to_string(amount)+", %r10b");
    assembly += instrStr("mulb\t%r10b");
    assembly += instrStr("addb\t%al, "+offsetStr+"(%rdi)");
    return assembly;
  }
private:
  int64_t amount;
  int64_t offset;
  bool posInc;
};

array<char, EndOfFile> enumToChar{{'>', '<', '+', '-', '.', ',', '[', ']'}};

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

/**
 * @brief Returns pair, first one going to matching brace, second one indicating current position's name
 * 
 * @param code 
 * @return pair<unordered_map<size_t, string>, unordered_map<size_t, string>> 
 */
pair<unordered_map<size_t, string>, unordered_map<size_t, string>> initializeLoopBracketLabels(const vector<Op>& code) {
  stack<size_t> leftBrackLocs;
  unordered_map<size_t, string> loopMap;
  unordered_map<size_t, string> ownNameMap;

  bool canBeInnerLoop = false;

  size_t currLabelCounter = 0;

  for(size_t i = 0; i < code.size(); ++i) {
    if(code[i] == JumpIfZero) {
      leftBrackLocs.push(i);

      canBeInnerLoop = true;
    }
    else if(code[i] == JumpUnlessZero) {
      const size_t lhs = leftBrackLocs.top();
      loopMap[lhs] = "label" + to_string(currLabelCounter + 1);
      loopMap[i] = "label" + to_string(currLabelCounter);
      ownNameMap[lhs] = "label" + to_string(currLabelCounter);
      ownNameMap[i] = "label" + to_string(currLabelCounter + 1);
      currLabelCounter += 2;
      leftBrackLocs.pop();
    }
  }

  return {loopMap, ownNameMap};
}

string initializeProgram() {
  constexpr size_t TAPESIZE = 10'000;
  static_assert(TAPESIZE % 2 == 0, "Tapesize must be even to by symmetric");
  // use calloc to initialize all memory to 0
  return ".global main\n"
        "main:\n"
        "\tsubq\t$8, %rsp\n"
        "\tmovl\t$"+to_string(TAPESIZE)+", %edi\n"
        "\tmovl\t$1, %esi\n"
        "\tcall\tcalloc\n"
        "\tleaq\t"+to_string(TAPESIZE/2)+"(%rax), %rdi\n"
        "\tcall\tbf_main\n"
        "\tmovl\t$0, %eax\n"
        "\taddq\t$8, %rsp\n"
        "\tret\n"
        "\n"
        "bf_main:\n";
}

vector<unique_ptr<Instr>> parse(const vector<Op>& ops) {
  // Note: %rdi will hold the current index on the tape
  // Except when calling putchar or getchar, then %rdi
  // will be pushed onto the stack

  const auto& [matchingBracketLabelMap, ownLabelMap] = initializeLoopBracketLabels(ops);
  vector<unique_ptr<Instr>> instructions;

  for(size_t IP = 0; IP < ops.size(); ++IP) {
    const Op currInstr = ops[IP];

    switch(currInstr) {
      case MoveRight: {
        instructions.push_back(make_unique<MoveRightInstr>());
        break;
      }
      case MoveLeft: {
        instructions.push_back(make_unique<MoveLeftInstr>());
        break;
      }
      case Inc: {
        instructions.push_back(make_unique<IncInstr>());
        break;
      }
      case Dec: {
        instructions.push_back(make_unique<DecInstr>());
        break;
      }
      case Write: {
        instructions.push_back(make_unique<WriteInstr>());
        break;
      }
      case Read: {
        instructions.push_back(make_unique<ReadInstr>());
        break;
      }
      case JumpIfZero: {
        const string thisLabel = ownLabelMap.at(IP);
        const string targetLabel = matchingBracketLabelMap.at(IP);
        instructions.push_back(make_unique<JumpIfZeroInstr>(thisLabel, targetLabel));
        break;
      }
      case JumpUnlessZero: {
        const string thisLabel = ownLabelMap.at(IP);
        const string targetLabel = matchingBracketLabelMap.at(IP);
        instructions.push_back(make_unique<JumpUnlessZeroInstr>(thisLabel, targetLabel));
        break;
      }
      case EndOfFile: {
        instructions.push_back(make_unique<EndOfFileInstr>());
        break;
      }
      default: {
        continue;
      }
    }
  }

  return instructions;
}

vector<unique_ptr<Instr>> removeZeroLoops(vector<unique_ptr<Instr>>& instrs) {
  for(size_t i = 0; i < instrs.size() - 2; ++i) {
    long iterOffset = static_cast<long>(i);
    if(instrs[i]->op == JumpIfZero && (instrs[i + 1]->op == Dec || instrs[i + 1]->op == Inc) && instrs[i + 2]->op == JumpUnlessZero) {
      instrs.erase(instrs.begin() + iterOffset, instrs.begin() + iterOffset + 3);
      instrs.insert(instrs.begin() + iterOffset, make_unique<ZeroInstr>());
    }
  }
  
  return std::move(instrs);
}

// Assumes loop has no IO and pointer always go back to start
// Generates only instructions inside the loop, not loops brackets
vector<unique_ptr<Instr>> generateSimplifiedLoopInstrs(const unordered_map<int64_t,int64_t>& incrementAtOffset) {
  vector<unique_ptr<Instr>> newInstrs;

  const int64_t inducInc = incrementAtOffset.at(0);


  // This is a loop that we can't remove the brackets from
  if(inducInc != 1 && inducInc != -1) {
    for(const auto& [offset, amount] : incrementAtOffset) {
      newInstrs.push_back(make_unique<SumInstr>(amount, offset));
    }
  }
  else { // truly simple loop
    for(const auto& [offset, amount] : incrementAtOffset) {
      if(offset == 0)
        continue;
      const bool posInc = inducInc > 0;
      newInstrs.push_back(make_unique<MulAddInstr>(amount, offset, posInc));
    }
    newInstrs.push_back(make_unique<ZeroInstr>());
  }

  return newInstrs;
}

optional<vector<unique_ptr<Instr>>> checkSimpleLoop(vector<unique_ptr<Instr>>& instrs, const size_t begin, const size_t end) {
  int currMemOffset = 0;
  unordered_map<int64_t, int64_t> incrementAtOffset;

  for(size_t i = begin; i < end; ++i) {
    const Op op = instrs.at(i)->op;
    if(op == MoveRight)
      ++currMemOffset;
    else if(op == MoveLeft)
      --currMemOffset;
    else if(op == Inc)
      ++incrementAtOffset[currMemOffset];
    else if(op == Dec) 
      --incrementAtOffset[currMemOffset];
    else if(op == JumpIfZero || op == JumpUnlessZero)
      continue;
    else
      return {};
  }

  if(!incrementAtOffset.count(0))
    return {};

  // don't check that increments by 1 or -1, can still simplify those instructions somewhat

  if(currMemOffset != 0)
    return {};

  auto newLoopInstrs = generateSimplifiedLoopInstrs(incrementAtOffset);
  const int64_t inducInc = incrementAtOffset.at(0);

  // can't remove loop bounds
  if(inducInc != 1 && inducInc != -1) {
    newLoopInstrs.insert(newLoopInstrs.begin(), std::move(instrs.at(begin)));
    newLoopInstrs.insert(newLoopInstrs.end(), std::move(instrs.at(end - 1)));
  }

  return newLoopInstrs;
}


vector<unique_ptr<Instr>> simplifySimpleLoops(vector<unique_ptr<Instr>>& instrs) {
  bool canBeSimpleLoop = false;
  size_t lhsIndex = 0;

  for(size_t i = 0; i < instrs.size() - 2; ++i) {
    if(instrs[i]->op == JumpIfZero) {
      canBeSimpleLoop = true;
      lhsIndex = i;
    }
    else if(instrs[i]->op == JumpUnlessZero && canBeSimpleLoop) {
      auto loopInstr = checkSimpleLoop(instrs, lhsIndex, i + 1);
      if(loopInstr) {
        long lhsIterOffset = static_cast<long>(lhsIndex);
        long iterOffset = static_cast<long>(i);
        auto& loopInstrs = loopInstr.value();

        instrs.erase(instrs.begin() + lhsIterOffset, instrs.begin() + iterOffset + 1);
        instrs.insert(instrs.begin() + lhsIterOffset, make_move_iterator(loopInstrs.begin()), make_move_iterator(loopInstrs.end()));
        i = static_cast<size_t>(lhsIterOffset + (loopInstrs.end() - loopInstrs.begin()));
      }
      canBeSimpleLoop = false;
    }
  }
  
  return std::move(instrs);
}


vector<unique_ptr<Instr>> optimize(vector<unique_ptr<Instr>>& instrs) {
  auto noZeroLoopInstrs = removeZeroLoops(instrs);
  return simplifySimpleLoops(noZeroLoopInstrs);
}

string compile(const vector<unique_ptr<Instr>>& instrs) {
  string assembly = initializeProgram();
  for(const auto& instr : instrs) {
    assembly += instr->str();
  }
  return assembly;
}

bool checkValidInstrs(const vector<Op>& ops) {
  stack<Op> lhsBrackets;

  for(const auto op : ops) {
    if(op == JumpIfZero)
      lhsBrackets.push(op);
    else if(op == JumpUnlessZero) {
      if(lhsBrackets.empty())
        return false;

      lhsBrackets.pop();
    }
  }

  return true;
}

int main(int argc, char** argv) {
  if(argc != 2 && argc != 3) {
    cerr << "Need exactly one file argument to compile with possible opt flag" << endl;
    exit(-1);
  }

  const vector<Op> ops = readFile(argv[argc - 1]);

  if(!checkValidInstrs(ops)) {
    cerr << "Loop brackets do not match, aborting." << endl;
    exit(-1);
  }

  vector<unique_ptr<Instr>> instrs = parse(ops);

  instrs = optimize(instrs);

  string program = compile(instrs);

  cout << program << endl;
}

