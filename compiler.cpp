#include <iostream>
#include <fstream>
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
  NotAnOp
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
  ZeroInstr() {op = NotAnOp;}

  string str() const override {
    return instrStr("movb\t$0, (%rdi)");
  }
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

vector<unique_ptr<Instr>> optimize(vector<unique_ptr<Instr>>& instrs) {
  return removeZeroLoops(instrs);
}

string compile(const vector<unique_ptr<Instr>>& instrs) {
  string assembly = initializeProgram();
  for(const auto& instr : instrs) {
    assembly += instr->str();
  }
  return assembly;
}

int main(int argc, char** argv) {
  if(argc != 2 ) {
    cerr << "Need exactly one file argument to compile" << endl;
    exit(-1);
  }

  const vector<Op> ops = readFile(argv[argc - 1]);

  vector<unique_ptr<Instr>> instrs = parse(ops);

  instrs = optimize(instrs);

  string program = compile(instrs);

  cout << program << endl;
}

