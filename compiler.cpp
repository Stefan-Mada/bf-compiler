#include <cctype>
#include <iostream>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <stack>
#include <unordered_set>
#include <vector>
#include <utility>
#include <array>
#include <memory>
#include <functional>

using namespace std;

// Handle CLI arguments

// https://blog.vito.nyc/posts/min-guide-to-cli/
struct MySettings {
  bool help{false};
  bool simplifySimpleLoops {true};
  bool vectorizeMemScans {true};
  bool runInstCombine {true};
  optional<string> infile;
  optional<string> outfile;
};

bool stringToBool(string str) {
  const string original = str;
  transform(str.begin(), str.end(), str.begin(), ::tolower);
  const unordered_set<string> trueOptions = {"true", "yes", "1"};
  const unordered_set<string> falseOptions = {"false", "no", "0"};

  if(trueOptions.find(str) != trueOptions.end())
    return true;
  else if(falseOptions.find(str) != falseOptions.end())
    return false;
  else {
    cerr << "Unable to parse boolean " << original << ", exiting." << endl;
    exit(-1);
  }
}

typedef function<void(MySettings&)> NoArgHandle;

#define S(str, f, v) {str, [](MySettings& s) {s.f = v;}}
const unordered_map<string, NoArgHandle> NoArgs {
  S("--help", help, true),
  S("-h", help, true),
};
#undef S

typedef function<void(MySettings&, const string&)> OneArgHandle;

#define S(str, f, v) \
  {str, [](MySettings& s, const string& arg) { s.f = v; }}

const unordered_map<string, OneArgHandle> OneArgs {
  S("--simplify-loops", simplifySimpleLoops, stringToBool(arg)),

  S("--vectorize-mem-scans", vectorizeMemScans, stringToBool(arg)),

  S("--run-inst-combine", runInstCombine, stringToBool(arg)),

  S("-o", outfile, arg)
};
#undef S

MySettings parse_settings(int argc, const char* const* const argv) {
  MySettings settings;

  // argv[0] is traditionally the program name, so start at 1
  for(int i {1}; i < argc; i++) {
    string opt {argv[i]};

    // Is this a NoArg?
    if(auto j {NoArgs.find(opt)}; j != NoArgs.end())
      j->second(settings); // Yes, handle it!

    // No, how about a OneArg?
    else if(auto k {OneArgs.find(opt)}; k != OneArgs.end())
      // Yes, do we have a parameter?
      if(++i < argc)
        // Yes, handle it!
        k->second(settings, {argv[i]});
      else
        // No, and we cannot continue, throw an error
        throw std::runtime_error {"missing param after " + opt};

    // No, has infile been set yet?
    else if(!settings.infile)
      // No, use this as the input file
      settings.infile = argv[i];

    // Yes, possibly throw here, or just print an error
    else
      cerr << "unrecognized command-line option " << opt << endl;
  }

  return settings;
}

// end CLI arguments


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
  MulAdd,
  AddMemPtr,
  MemScan
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

struct AddMemPointerInstr : public virtual Instr {
  AddMemPointerInstr(const int64_t amount)
          : amount(amount) {op = AddMemPtr;}

  string str() const override {    
    return instrStr("add\t$"+to_string(amount)+", %rdi");
  }
private:
  int64_t amount;
};

struct MemScanInstr : public virtual Instr {
  MemScanInstr(const int64_t stride)
          : stride(stride) {
    op = MemScan;

    if(!validStride(stride))
      throw invalid_argument("Memscan stride of " + to_string(stride) + " is not supported");
  }

  string str() const override {
    string assembly;
    assembly += instrStr("vpxor\t%xmm0, %xmm0, %xmm0");
    assembly += instrStr("vpcmpeqb\t(%rdi), %ymm0, %ymm0");
    if(stride == 2)
      assembly += instrStr("vpand\t.STRIDE2MASK(%rip), %ymm0, %ymm0");
    else if(stride == 4)
      assembly += instrStr("vpand\t.STRIDE4MASK(%rip), %ymm0, %ymm0");
    assembly += instrStr("vpmovmskb\t%ymm0, %r10");
    assembly += instrStr("tzcntl\t%r10d, %r10d");
    assembly += instrStr("add\t%r10, %rdi");
    return assembly;
  }

  static constexpr bool validStride(const int64_t stride) {
    return stride == 1 || stride == 2 || stride == 4;
  }

private:
  int64_t stride;
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

string intializeVectorMasks() {
  return ".STRIDE2MASK:\n" \
	"\t.byte	255                             # 0xff\n" \
	"\t.byte	0                               # 0x0\n" \
	"\t.byte	255                             # 0xff\n" \
	"\t.byte	0                               # 0x0\n" \
	"\t.byte	255                             # 0xff\n" \
	"\t.byte	0                               # 0x0\n" \
	"\t.byte	255                             # 0xff\n" \
	"\t.byte	0                               # 0x0\n" \
	"\t.byte	255                             # 0xff\n" \
	"\t.byte	0                               # 0x0\n" \
	"\t.byte	255                             # 0xff\n" \
	"\t.byte	0                               # 0x0\n" \
	"\t.byte	255                             # 0xff\n" \
	"\t.byte	0                               # 0x0\n" \
	"\t.byte	255                             # 0xff\n" \
	"\t.byte	0                               # 0x0\n" \
	"\t.byte	255                             # 0xff\n" \
	"\t.byte	0                               # 0x0\n" \
	"\t.byte	255                             # 0xff\n" \
	"\t.byte	0                               # 0x0\n" \
	"\t.byte	255                             # 0xff\n" \
	"\t.byte	0                               # 0x0\n" \
	"\t.byte	255                             # 0xff\n" \
	"\t.byte	0                               # 0x0\n" \
	"\t.byte	255                             # 0xff\n" \
	"\t.byte	0                               # 0x0\n" \
	"\t.byte	255                             # 0xff\n" \
	"\t.byte	0                               # 0x0\n" \
	"\t.byte	255                             # 0xff\n" \
	"\t.byte	0                               # 0x0\n" \
	"\t.byte	255                             # 0xff\n" \
  "\t.byte	0                               # 0x0\n" \
  ".STRIDE4MASK:\n" \
	"\t.byte	255                             # 0xff\n" \
	"\t.byte	0                               # 0x0\n" \
	"\t.byte	0                             # 0xff\n" \
	"\t.byte	0                               # 0x0\n" \
	"\t.byte	255                             # 0xff\n" \
	"\t.byte	0                               # 0x0\n" \
	"\t.byte	0                             # 0xff\n" \
	"\t.byte	0                               # 0x0\n" \
	"\t.byte	255                             # 0xff\n" \
	"\t.byte	0                               # 0x0\n" \
	"\t.byte	0                             # 0xff\n" \
	"\t.byte	0                               # 0x0\n" \
	"\t.byte	255                             # 0xff\n" \
	"\t.byte	0                               # 0x0\n" \
	"\t.byte	0                             # 0xff\n" \
	"\t.byte	0                               # 0x0\n" \
	"\t.byte	255                             # 0xff\n" \
	"\t.byte	0                               # 0x0\n" \
	"\t.byte	0                             # 0xff\n" \
	"\t.byte	0                               # 0x0\n" \
	"\t.byte	255                             # 0xff\n" \
	"\t.byte	0                               # 0x0\n" \
	"\t.byte	0                             # 0xff\n" \
	"\t.byte	0                               # 0x0\n" \
	"\t.byte	255                             # 0xff\n" \
	"\t.byte	0                               # 0x0\n" \
	"\t.byte	0                             # 0xff\n" \
	"\t.byte	0                               # 0x0\n" \
	"\t.byte	255                             # 0xff\n" \
	"\t.byte	0                               # 0x0\n" \
	"\t.byte	0                             # 0xff\n" \
  "\t.byte	0                               # 0x0\n";
}

string initializeProgram() {
  constexpr size_t TAPESIZE = 320'000;
  static_assert(TAPESIZE % 2 == 0, "Tapesize must be even to by symmetric");
  // use calloc to initialize all memory to 0
  string vectorMasks = intializeVectorMasks();

  return vectorMasks + ".global main\n"
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

// Generates only instructions inside the loop, not loops brackets
vector<unique_ptr<Instr>> generateSimplifiedLoopInstrs(const unordered_map<int64_t,int64_t>& incrementAtOffset) {
  vector<unique_ptr<Instr>> newInstrs;

  const int64_t inducInc = incrementAtOffset.at(0);

  for(const auto& [offset, amount] : incrementAtOffset) {
    if(offset == 0)
      continue;
    const bool posInc = inducInc > 0;
    newInstrs.push_back(make_unique<MulAddInstr>(amount, offset, posInc));
  }
  newInstrs.push_back(make_unique<ZeroInstr>());

  return newInstrs;
}

// Generates loop brackets as well
vector<unique_ptr<Instr>> generateMemScanInstructions(vector<unique_ptr<Instr>>& instrs, const size_t begin, const size_t end, const int64_t offset) {
  vector<unique_ptr<Instr>> newInstrs;
  newInstrs.push_back(std::move(instrs[begin]));
  newInstrs.push_back(make_unique<MemScanInstr>(offset));
  newInstrs.push_back(std::move(instrs[end - 1]));

  return newInstrs;
}


optional<vector<unique_ptr<Instr>>> checkSimpleOrMemScanLoop(vector<unique_ptr<Instr>>& instrs, const size_t begin, const size_t end, const MySettings& settings) {
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

  // Memory scan loops that go up by 1 and don't change any values
  if(settings.vectorizeMemScans && MemScanInstr::validStride(currMemOffset) && incrementAtOffset.empty())
    return generateMemScanInstructions(instrs, begin, end, currMemOffset);

  if(!settings.simplifySimpleLoops)
    return {};

  if(!incrementAtOffset.count(0))
    return {};

  const int64_t inducInc = incrementAtOffset.at(0);
  if(inducInc != 1 && inducInc != -1)
    return {};

  if(currMemOffset != 0)
    return {};

  auto newLoopInstrs = generateSimplifiedLoopInstrs(incrementAtOffset);

  return newLoopInstrs;
}


vector<unique_ptr<Instr>> simplifyLoops(vector<unique_ptr<Instr>>& instrs, const MySettings& settings) {
  if(!settings.simplifySimpleLoops && !settings.vectorizeMemScans)
    return std::move(instrs);

  bool canBeSimpleLoop = false;
  size_t lhsIndex = 0;

  for(size_t i = 0; i < instrs.size() - 2; ++i) {
    if(instrs[i]->op == JumpIfZero) {
      canBeSimpleLoop = true;
      lhsIndex = i;
    }
    else if(instrs[i]->op == JumpUnlessZero && canBeSimpleLoop) {
      auto loopInstr = checkSimpleOrMemScanLoop(instrs, lhsIndex, i + 1, settings);
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

vector<unique_ptr<Instr>> instCombine(vector<unique_ptr<Instr>>& instrs, const MySettings& settings) {
  if(!settings.runInstCombine)
    return std::move(instrs);

  int currMemOffset = 0;
  unordered_map<int64_t, int64_t> incrementAtOffset;

  size_t lhs = 0;
  for(size_t rhs = 0; rhs < instrs.size(); ++rhs) {
    const Op op = instrs.at(rhs)->op;
    if(op == MoveRight)
      ++currMemOffset;
    else if(op == MoveLeft)
      --currMemOffset;
    else if(op == Inc)
      ++incrementAtOffset[currMemOffset];
    else if(op == Dec) 
      --incrementAtOffset[currMemOffset];
    else if(rhs < lhs + 2) { // >[>.
      lhs = rhs + 1;
      incrementAtOffset.clear();
      currMemOffset = 0;
    }
    else{
      vector<unique_ptr<Instr>> newInstrs;
      for(const auto& [offset, amount] : incrementAtOffset) {
        newInstrs.push_back(make_unique<SumInstr>(amount, offset));
      }

      if(currMemOffset != 0)
        newInstrs.push_back(make_unique<AddMemPointerInstr>(currMemOffset));

      long lhsIterOffset = static_cast<long>(lhs);
      long rhsIterOffset = static_cast<long>(rhs);

      instrs.erase(instrs.begin() + lhsIterOffset, instrs.begin() + rhsIterOffset);
      instrs.insert(instrs.begin() + lhsIterOffset, make_move_iterator(newInstrs.begin()), make_move_iterator(newInstrs.end()));

      lhs = static_cast<size_t>(lhsIterOffset + (newInstrs.end() - newInstrs.begin())) + 1;
      rhs = lhs - 1;
      incrementAtOffset.clear();
      currMemOffset = 0;    
    }
  }

  return std::move(instrs);
}

vector<unique_ptr<Instr>> optimize(vector<unique_ptr<Instr>>& instrs, const MySettings& settings) {
  auto simplifiedLoops = simplifyLoops(instrs, settings);
  return instCombine(simplifiedLoops, settings);
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
  MySettings settings = parse_settings(argc, argv);

  if(settings.help) {
    cout << "Usage: " << argv[0] << " " << "<input> [options]\n\n";
    cout << "Options:\n";
    cout << "These options take no arguments after them:\n";
    for(const auto& [key, value] : NoArgs)
      cout << "\t" << key << "\n";

    cout << "These options take one argument after them:\n";
    for(const auto& [key, value] : OneArgs)
      cout << "\t" << key << "\n";  
  }

  if(!settings.infile) {
    cerr << "Need an input bf program to read, aborting." << endl;
    exit(-1);
  }

  const vector<Op> ops = readFile(settings.infile.value());

  if(!checkValidInstrs(ops)) {
    cerr << "Loop brackets do not match, aborting." << endl;
    exit(-1);
  }

  vector<unique_ptr<Instr>> instrs = parse(ops);

  instrs = optimize(instrs, settings);

  string program = compile(instrs);

  if(!settings.outfile)
    cout << program << endl;
  else {
    ofstream MyFile(settings.outfile.value());
    MyFile << program << endl;
    MyFile.close();
  }
}

