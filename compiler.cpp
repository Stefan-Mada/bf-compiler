#include <cctype>
#include <cstdint>
#include <cstdlib>
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
#include <sys/mman.h>
#include <cstring>
#include <sstream>

using namespace std;

constexpr size_t TAPESIZE = 320'000;

// Handle CLI arguments

// https://blog.vito.nyc/posts/min-guide-to-cli/
struct MySettings {
  bool help{false};
  bool simplifySimpleLoops {true};
  bool vectorizeMemScans {true};
  bool runInstCombine {true};
  bool partialEval {true};
  bool justInTime {false};
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

  S("--partial-eval", partialEval, stringToBool(arg)),

  S("--just-in-time", justInTime, stringToBool(arg)),

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

string hexToStr(const string& hex) {
  size_t len = hex.length();
  std::string newString;
  for(size_t i=0; i< len; i+=2)
  {
      std::string byte = hex.substr(i,2);
      char chr = static_cast<char>(static_cast<int>(strtol(byte.c_str(), nullptr, 16)));
      newString.push_back(chr);
  }

  return newString;
}

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
  virtual string assemble() const = 0;
  virtual ~Instr() {}
  Op op;
};

struct MoveRightInstr : public virtual Instr {
  MoveRightInstr() {op = MoveRight;}

  string str() const override {
    return instrStr("inc\t%rdi");
  }

  string assemble() const override {
    return hexToStr("48ffc7");
  }
};

struct MoveLeftInstr : public virtual Instr {
  MoveLeftInstr() {op = MoveLeft;}

  string str() const override {
    return instrStr("dec\t%rdi");
  }

  string assemble() const override {
    return hexToStr("48ffcf");
  }
};

struct IncInstr : public virtual Instr {
  IncInstr() {op = Inc;}

  string str() const override {
    return instrStr("incb\t(%rdi)");
  }

  string assemble() const override {
    return hexToStr("fe07");
  }
};

struct DecInstr : public virtual Instr {
  DecInstr() {op = Dec;}

  string str() const override {
    return instrStr("decb\t(%rdi)");
  }

  string assemble() const override {
    return hexToStr("fe0f");
  }
};

string getPtrRelOffset(intptr_t ptr1, intptr_t ptr2) {
  intptr_t diff = ptr1 - ptr2;
  stringstream ss;
  ss << hex << diff;

  string diffStr = ss.str();
  if(diffStr.size() < 8)
    diffStr = diffStr.insert(0, 8 - diffStr.size(), '0');
  if(diffStr.size() > 8)
    diffStr = diffStr.substr(diffStr.size() - 8, 8);

  swap(diffStr[0], diffStr[6]);
  swap(diffStr[1], diffStr[7]);
  swap(diffStr[2], diffStr[4]);
  swap(diffStr[3], diffStr[5]);

  return diffStr;
}

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

  string assemble() const override {
      throw invalid_argument("This instruction can not assemble without parameter");
    return hexToStr("57408a3fe8000000005f");
  }
  string assemble(unsigned char* startAddr) const {
    intptr_t funcPtr = reinterpret_cast<intptr_t>(putchar);
    intptr_t nextInstrAddr = reinterpret_cast<intptr_t>(startAddr) + 9;
    string ptrRelOffset = getPtrRelOffset(funcPtr, nextInstrAddr);

    return hexToStr("57408a3fe8"+ptrRelOffset+"5f");
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

  string assemble() const override {
    throw invalid_argument("This instruction can not assemble currently");
  }

  string assemble(unsigned char* startAddr) const {
    intptr_t funcPtr = reinterpret_cast<intptr_t>(getchar);
    intptr_t nextInstrAddr = reinterpret_cast<intptr_t>(startAddr) + 6;
    string ptrRelOffset = getPtrRelOffset(funcPtr, nextInstrAddr);

    return hexToStr("57e8"+ptrRelOffset+"5f8807");
  }
};

struct JumpInstr : public virtual Instr {
  
  // returns own label and target label
  pair<string, string> getLabels() const {
    return {ownLabel, targetLabel};
  }

  void setZeroTarget(unsigned char* ptr) {
    jumpOnZeroTarget = ptr;
  }

  void setNotZeroTarget(unsigned char* ptr) {
    jumpNotZeroTarget = ptr;
  }

  void setInstrStartAddr(unsigned char* ptr) {
    instrStartAddr = ptr;
  }

protected:
  string ownLabel, targetLabel;
  unsigned char* jumpOnZeroTarget = nullptr;
  unsigned char* jumpNotZeroTarget = nullptr;
  unsigned char* instrStartAddr = nullptr;
};

struct JumpIfZeroInstr : public virtual JumpInstr {
  JumpIfZeroInstr(const string& iOwnLabel, const string& iTargetLabel)
                {op = JumpIfZero; ownLabel = iOwnLabel; targetLabel = iTargetLabel; }

  string str() const override {
    string assembly;
    assembly += ownLabel + ":\n";
    assembly += instrStr("cmpb\t$0, (%rdi)");
    assembly += instrStr("je\t"+targetLabel);
    return assembly;
  }

  // returns with 13 no-ops (what will be filled in later)
  string assemble() const override {
    if(!jumpOnZeroTarget && !jumpNotZeroTarget) {
      string noOpStr;
      for(size_t i = 0; i < 17; ++i)
        noOpStr += "90";

      // intel syntax:
      // mov    rax,rdi
      // ret
      return hexToStr("4889f8c3"+noOpStr);  
    }
    else if(jumpOnZeroTarget && !jumpNotZeroTarget) {
      intptr_t instrAfterJumpPtr = reinterpret_cast<intptr_t>(instrStartAddr) + 12;
      intptr_t jzTarget = reinterpret_cast<intptr_t>(jumpOnZeroTarget);
      string ptrRelOffset = getPtrRelOffset(jzTarget, instrAfterJumpPtr);

      // intel syntax:
      // cmp    BYTE PTR [rdi],0x0
      // je     ptrRelOffset
      // ret
      return hexToStr("4889f8803f000f84"+ptrRelOffset+"c3");  
    }
    else if(!jumpOnZeroTarget && jumpNotZeroTarget) {
      intptr_t instrAfterJumpPtr = reinterpret_cast<intptr_t>(instrStartAddr) + 12;
      intptr_t jnzTarget = reinterpret_cast<intptr_t>(jumpNotZeroTarget);
      string ptrRelOffset = getPtrRelOffset(jnzTarget, instrAfterJumpPtr);

      // intel syntax:
      // cmp    BYTE PTR [rdi],0x0
      // jne    ptrRelOffset
      // ret
      return hexToStr("4889f8803f000f85"+ptrRelOffset+"c3");  
    }
    else { // both 
      intptr_t instrAfterJzJumpPtr = reinterpret_cast<intptr_t>(instrStartAddr) + 12;
        intptr_t instrAfterJnzJumpPtr = reinterpret_cast<intptr_t>(instrStartAddr) + 17;
      intptr_t jzTarget = reinterpret_cast<intptr_t>(jumpOnZeroTarget);
      intptr_t jnzTarget = reinterpret_cast<intptr_t>(jumpNotZeroTarget);
      string jzTargetRelOffset = getPtrRelOffset(jzTarget, instrAfterJzJumpPtr);
      string jnzTargetRelOffset = getPtrRelOffset(jnzTarget, instrAfterJnzJumpPtr);

      // intel syntax:
      // cmp    BYTE PTR [rdi],0x0
      // je     jzTargetRelOffset
      // jmp    jnzTargetRelOffset
      return hexToStr("4889f8803f000f84"+jzTargetRelOffset+"e9"+jnzTargetRelOffset);  
    }
  }
};

struct JumpUnlessZeroInstr : public virtual JumpInstr {
  JumpUnlessZeroInstr(const string& iOwnLabel, const string& iTargetLabel)
                {op = JumpUnlessZero; ownLabel = iOwnLabel; targetLabel = iTargetLabel; }

  string str() const override {
    string assembly;
    assembly += ownLabel + ":\n";
    assembly += instrStr("cmpb\t$0, (%rdi)");
    assembly += instrStr("jne\t"+targetLabel);
    return assembly;
  }

  // returns with 13 no-ops (what will be filled in later)
  string assemble() const override {
    if(!jumpOnZeroTarget && !jumpNotZeroTarget) {
      string noOpStr;
      for(size_t i = 0; i < 17; ++i)
        noOpStr += "90";
      
      // intel syntax:
      // mov    rax,rdi
      // ret
      return hexToStr("4889f8c3"+noOpStr);  
    }
    else if(jumpOnZeroTarget && !jumpNotZeroTarget) {
      throw std::invalid_argument("This should not be possible");
    }
    else if(!jumpOnZeroTarget && jumpNotZeroTarget) {
      intptr_t instrAfterJumpPtr = reinterpret_cast<intptr_t>(instrStartAddr) + 12;
      intptr_t jnzTarget = reinterpret_cast<intptr_t>(jumpNotZeroTarget);
      string ptrRelOffset = getPtrRelOffset(jnzTarget, instrAfterJumpPtr);

      // intel syntax:
      // cmp    BYTE PTR [rdi],0x0
      // jne    ptrRelOffset
      // ret
      return hexToStr("4889f8803f000f85"+ptrRelOffset+"c3");  
    }
    else { // both 
      intptr_t instrAfterJzJumpPtr = reinterpret_cast<intptr_t>(instrStartAddr) + 12;
      intptr_t instrAfterJnzJumpPtr = reinterpret_cast<intptr_t>(instrStartAddr) + 17;
      intptr_t jzTarget = reinterpret_cast<intptr_t>(jumpOnZeroTarget);
      intptr_t jnzTarget = reinterpret_cast<intptr_t>(jumpNotZeroTarget);
      string jzTargetRelOffset = getPtrRelOffset(jzTarget, instrAfterJzJumpPtr);
      string jnzTargetRelOffset = getPtrRelOffset(jnzTarget, instrAfterJnzJumpPtr);

      // intel syntax:
      // cmp    BYTE PTR [rdi],0x0
      // je     jzTargetRelOffset
      // jmp    jnzTargetRelOffset
      return hexToStr("4889f8803f000f84"+jzTargetRelOffset+"e9"+jnzTargetRelOffset);  
    }
  }
};

struct EndOfFileInstr : public virtual Instr {
  EndOfFileInstr() {op = EndOfFile;}

  string str() const override {
    return instrStr("ret");
  }

  string assemble() const override {
    return hexToStr("c3");
  }
};

struct ZeroInstr : public virtual Instr {
  ZeroInstr() {op = Zero;}

  string str() const override {
    return instrStr("movb\t$0, (%rdi)");
  }

  string assemble() const override {
    throw invalid_argument("This instruction can not assemble currently");
  }
};

struct SumInstr : public virtual Instr {
  SumInstr(const int64_t amount, const int64_t offset)
          : amount(amount), offset(offset) {op = Sum;}

  string str() const override {
    const string offsetStr = (offset == 0) ? "" : to_string(offset);
    
    return instrStr("addb\t$"+to_string(amount)+", "+offsetStr+"(%rdi)");
  }

  string assemble() const override {
    throw invalid_argument("This instruction can not assemble currently");
  }

  pair<int64_t, int64_t> amountAndOffset() const {
    return {amount, offset};
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
    if(posInc) {
      assembly += instrStr("xorb\t$-1, %al");
      assembly += instrStr("addb\t$1, %al");
    }
    assembly += instrStr("movb\t$"+to_string(amount)+", %r10b");
    assembly += instrStr("mulb\t%r10b");
    assembly += instrStr("addb\t%al, "+offsetStr+"(%rdi)");
    return assembly;
  }

  string assemble() const override {
    throw invalid_argument("This instruction can not assemble currently");
  }

  tuple<int64_t, int64_t, bool> amountOffsetPosInc() const {
    return {amount, offset, posInc};
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

  string assemble() const override {
    throw invalid_argument("This instruction can not assemble currently");
  }

  int64_t getAmount() const {
    return amount;
  }
private:
  int64_t amount;
};


struct MemScanInstr : public virtual Instr {
  MemScanInstr(const int64_t stride)
          : absoluteStride(abs(stride)), isNeg(stride < 0) {
    op = MemScan;

    if(!validStride(stride))
      throw invalid_argument("Memscan stride of " + to_string(stride) + " is not supported");
  }

  string str() const override {
    string assembly;
    assembly += instrStr("vpxor\t%xmm0, %xmm0, %xmm0");

    if(isNeg) {
      assembly += instrStr("mov\t%rdi, %r10");
      assembly += instrStr("sub\t$31, %r10");
      assembly += instrStr("vpcmpeqb\t(%r10), %ymm0, %ymm0");
    }
    else
      assembly += instrStr("vpcmpeqb\t(%rdi), %ymm0, %ymm0");

    if(absoluteStride != 1) {
      const string maskLabel = ".STRIDE" + to_string(absoluteStride) + "MASK" + ((isNeg) ? "NEG" : "");
      assembly += instrStr("vpand\t"+maskLabel+"(%rip), %ymm0, %ymm0");
    }

    assembly += instrStr("vpmovmskb\t%ymm0, %r10");
    if(isNeg) {
      assembly += instrStr("lzcntl\t%r10d, %r10d");
      assembly += instrStr("sub\t%r10, %rdi");
    }
    else{ 
      assembly += instrStr("tzcntl\t%r10d, %r10d");
      assembly += instrStr("add\t%r10, %rdi");
    }
    return assembly;
  }

  string assemble() const override {
    throw invalid_argument("This instruction can not assemble currently");
  }

  static constexpr bool validStride(const int64_t stride) {
    return stride == 1 || stride == 2 || stride == 4 || stride == -1 || stride == -2 || stride == -4;
  }

  int64_t getStride() const {
    return isNeg ? -absoluteStride : absoluteStride;
  }
private:
  int64_t absoluteStride;
  bool isNeg;
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
  ".STRIDE2MASKNEG:\n" \
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
  "\t.byte	255                             # 0xff\n" \
  ".STRIDE4MASK:\n" \
	"\t.byte	255                             # 0xff\n" \
  ".STRIDE4MASKNEG:\n" \
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
  "\t.byte	0                               # 0x0\n" \
	"\t.byte	255                             # 0xff\n";
}

string initializeProgram() {
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
vector<unique_ptr<Instr>> generateMemScanInstructions(vector<unique_ptr<Instr>>& instrs, const size_t begin, const size_t end, const int64_t stride) {
  vector<unique_ptr<Instr>> newInstrs;

  newInstrs.push_back(std::move(instrs[begin]));
  newInstrs.push_back(make_unique<MemScanInstr>(stride));
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

unordered_map<size_t, size_t> initializeLoopBracketIndexes(vector<unique_ptr<Instr>>& instrs) {
  unordered_map<size_t, size_t> matchingIndex;
  unordered_map<string, size_t> indexOfLabel;

  for(size_t i = 0; i < instrs.size(); ++i) {
    const auto& instr = instrs.at(i);
    if(const JumpInstr *const jump = dynamic_cast<JumpInstr*>(instr.get())) {
      const auto& [thisLabel, targetLabel] = jump->getLabels();

      if(indexOfLabel.find(targetLabel) == indexOfLabel.end()) {
        indexOfLabel[thisLabel] = i;
      }
      else {
        const auto indexOfTarget = indexOfLabel[targetLabel];
        matchingIndex[i] = indexOfTarget;
        matchingIndex[indexOfTarget] = i;
      }
    }
  }

  return matchingIndex;
}

bool loopContainsRead(const vector<unique_ptr<Instr>>& instrs, const size_t start) {
  int lhsSeen = 0;
  for(size_t i = start; i < instrs.size(); ++i) {
    if(instrs.at(i)->op == Read)
      return true;
    else if(instrs.at(i)->op == JumpUnlessZero) {
      if(--lhsSeen == 0)
        break;
    }
    else if(instrs.at(i)->op == JumpIfZero)
      ++lhsSeen;
  }
  return false;
}

vector<unique_ptr<Instr>> partialEval(vector<unique_ptr<Instr>>& instrs, const MySettings& settings) {
  if(!settings.partialEval)
    return std::move(instrs);

  unordered_map<int64_t, unsigned char> valAtOffset;
  unordered_set<size_t> loopDoesntContainRead;
  int64_t offset = 0;
  int64_t curPartialEvalOffset = 0;
  vector<unique_ptr<Instr>> newInstrs;
  unordered_set<int64_t> offsetsThatPrintedNonzero;

  const unordered_map<size_t, size_t> matchingLoopBracket = initializeLoopBracketIndexes(instrs);
  const size_t instrSize = instrs.size();
  for(size_t IP = 0; IP < instrSize; ++IP) {
    const auto& instr = instrs[IP];

    switch(instr->op) {
      case MoveRight:
        ++offset;
        break;
      case MoveLeft:
        --offset;
        break;
      case Inc:
        if(++valAtOffset[offset] == 0)
          valAtOffset.erase(offset);
        break;
      case Dec:
        if(--valAtOffset[offset] == 0)
          valAtOffset.erase(offset);
        break;
      case Write:
        newInstrs.push_back(make_unique<AddMemPointerInstr>(offset - curPartialEvalOffset));
        newInstrs.push_back(make_unique<ZeroInstr>());
        newInstrs.push_back(make_unique<SumInstr>(valAtOffset[offset], 0));
        if(valAtOffset[offset] == 0) {
          valAtOffset.erase(offset);
          if(offsetsThatPrintedNonzero.count(offset))
            offsetsThatPrintedNonzero.erase(offset);
        }
        else
          offsetsThatPrintedNonzero.insert(offset);

        newInstrs.push_back(make_unique<WriteInstr>());
        curPartialEvalOffset = offset;
        break;
      case Read:
        for(const auto [memOffset, val] : valAtOffset) {
          newInstrs.push_back(make_unique<AddMemPointerInstr>(memOffset - curPartialEvalOffset));
          newInstrs.push_back(make_unique<ZeroInstr>());
          newInstrs.push_back(make_unique<SumInstr>(val, 0));
          curPartialEvalOffset = memOffset;
        }

        for(const int64_t offsetThatMustZero : offsetsThatPrintedNonzero) {
          if(valAtOffset.find(offsetThatMustZero) != valAtOffset.end())
            continue;
          newInstrs.push_back(make_unique<AddMemPointerInstr>(offsetThatMustZero - curPartialEvalOffset));
          newInstrs.push_back(make_unique<ZeroInstr>());
          curPartialEvalOffset = offsetThatMustZero;
        }
        offsetsThatPrintedNonzero.clear();

        newInstrs.push_back(make_unique<AddMemPointerInstr>(offset - curPartialEvalOffset));

        instrs.erase(instrs.begin(), instrs.begin() + static_cast<long>(IP));
        instrs.insert(instrs.begin(), make_move_iterator(newInstrs.begin()), make_move_iterator(newInstrs.end()));
        IP = instrSize;
        break;
      case JumpIfZero:
        if(loopDoesntContainRead.find(IP) == loopDoesntContainRead.end()) {
          if(loopContainsRead(instrs, IP)) {
            for(const auto [memOffset, val] : valAtOffset) {
              newInstrs.push_back(make_unique<AddMemPointerInstr>(memOffset - curPartialEvalOffset));
              newInstrs.push_back(make_unique<ZeroInstr>());
              newInstrs.push_back(make_unique<SumInstr>(val, 0));
              curPartialEvalOffset = memOffset;
            }

            for(const int64_t offsetThatMustZero : offsetsThatPrintedNonzero) {
              if(valAtOffset.find(offsetThatMustZero) != valAtOffset.end())
                continue;
              newInstrs.push_back(make_unique<AddMemPointerInstr>(offsetThatMustZero - curPartialEvalOffset));
              newInstrs.push_back(make_unique<ZeroInstr>());
              curPartialEvalOffset = offsetThatMustZero;
            }
            offsetsThatPrintedNonzero.clear();

            newInstrs.push_back(make_unique<AddMemPointerInstr>(offset - curPartialEvalOffset));

            instrs.erase(instrs.begin(), instrs.begin() + static_cast<long>(IP));
            instrs.insert(instrs.begin(), make_move_iterator(newInstrs.begin()), make_move_iterator(newInstrs.end()));
            IP = instrSize;
            break;
          }
          loopDoesntContainRead.insert(IP);
        }

        if(valAtOffset.find(offset) == valAtOffset.end())
          IP = matchingLoopBracket.at(IP) - 1;
        break;
      case JumpUnlessZero:
        if(valAtOffset.find(offset) != valAtOffset.end())
          IP = matchingLoopBracket.at(IP) - 1;
        break;
      case EndOfFile:
        instrs.erase(instrs.begin(), instrs.begin() + static_cast<long>(IP));
        instrs.insert(instrs.begin(), make_move_iterator(newInstrs.begin()), make_move_iterator(newInstrs.end()));
        IP = instrSize;
        break;
      case Zero:
        valAtOffset.erase(offset);
        break;
      case Sum: {
        const auto& [amount, furtherOffset] = dynamic_cast<SumInstr*>(instr.get())->amountAndOffset();
        valAtOffset[offset + furtherOffset] += amount;

        if(valAtOffset[offset + furtherOffset] == 0)
          valAtOffset.erase(offset + furtherOffset);
        break;
      }
      case MulAdd: {
        const auto& [amount, furtherOffset, posInc] = dynamic_cast<MulAddInstr*>(instr.get())->amountOffsetPosInc();
        unsigned char repeatAmount = valAtOffset[offset];
        if(valAtOffset[offset] == 0)
          valAtOffset.erase(offset);

        if(posInc)
          repeatAmount = ~repeatAmount + 1;

        unsigned char mulResult = repeatAmount * amount;
        valAtOffset[offset + furtherOffset] += mulResult;

        if(valAtOffset[offset + furtherOffset] == 0)
          valAtOffset.erase(offset + furtherOffset);
        break;
      }
      case AddMemPtr: {
        const auto amount = dynamic_cast<AddMemPointerInstr*>(instr.get())->getAmount();

        offset += amount;
        break;
      }
      case MemScan: {
        const auto stride = dynamic_cast<MemScanInstr*>(instr.get())->getStride();

        offset += stride;
        break;
      }
      default:
        throw invalid_argument("Unsupported op type in partial evaluator: " + to_string(instr->op));
        break;      
    }
  }

  return std::move(instrs);
}


vector<unique_ptr<Instr>> optimize(vector<unique_ptr<Instr>>& instrs, const MySettings& settings) {
  auto simplifiedLoops = simplifyLoops(instrs, settings);
  auto instCombinedInstrs = instCombine(simplifiedLoops, settings);
  return partialEval(instCombinedInstrs, settings);
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



struct BasicBlock {
  BasicBlock(vector<unique_ptr<Instr>>& inputInstrs, size_t startIndex, size_t endIndex, size_t bbIndex) : bbIndex(bbIndex) {
    long startLongCast = static_cast<long>(startIndex);
    long endLongCast = static_cast<long>(endIndex);

    instrs.insert(instrs.begin(), make_move_iterator(inputInstrs.begin() + startLongCast), make_move_iterator(inputInstrs.begin() + endLongCast));
  }
  /**
  * @brief Generates the encoded instructions in memory starting at blockStartMemory, 
  *        and returns a pointer to the next valid position to insert memory.
  * 
  * @param blockStartMemory 
  * @return unsigned* 
  */
  unsigned char* generateBasicBlockInstrs(unsigned char* const blockStartMemory) {
    unsigned char* currMemPos = markBasicBlockStartAddr(blockStartMemory);

    for(size_t i = 0; i < instrs.size(); ++i) {
      const auto& instr = instrs[i];

      switch(instr->op) {
        case Write: {
          const WriteInstr *const writeInstr = dynamic_cast<WriteInstr*>(instr.get());
          const string objcode = writeInstr->assemble(currMemPos);
          memcpy(currMemPos, objcode.c_str(), objcode.size());
          instrToMemAddr.push_back(currMemPos);
          currMemPos += objcode.size();
          break;
        }
        case Read: {
          const ReadInstr *const readInstr = dynamic_cast<ReadInstr*>(instr.get());
          const string objcode = readInstr->assemble(currMemPos);
          memcpy(currMemPos, objcode.c_str(), objcode.size());
          instrToMemAddr.push_back(currMemPos);
          currMemPos += objcode.size();
          break;
        }
        case JumpIfZero: 
        case JumpUnlessZero: {
          JumpInstr* jumpInstr = dynamic_cast<JumpInstr*>(instr.get());
          jumpInstr->setInstrStartAddr(currMemPos);
          // implicit fallthrough
        }
        default: {
          const string objcode = instr->assemble();
          memcpy(currMemPos, objcode.c_str(), objcode.size());
          instrToMemAddr.push_back(currMemPos);
          currMemPos += objcode.size();
          break;
        }
      }
    }

    return currMemPos;
  }

  void setTailOnZeroMemAddr(unsigned char* const nextMemAddr) {
    auto jumpInstr = dynamic_cast<JumpInstr*>(instrs.back().get());
    jumpInstr->setZeroTarget(nextMemAddr);

    const string objcode = jumpInstr->assemble();
    memcpy(instrToMemAddr.back(), objcode.c_str(), objcode.size());
  }

  void setTailOnNotZeroMemAddr(unsigned char* const nextMemAddr) {
    auto jumpInstr = dynamic_cast<JumpInstr*>(instrs.back().get());
    jumpInstr->setNotZeroTarget(nextMemAddr);

    const string objcode = jumpInstr->assemble();
    memcpy(instrToMemAddr.back(), objcode.c_str(), objcode.size());
  }

  unsigned char* getFinalInstrMemAddr() {
    return instrToMemAddr.back();
  }

private:
  unsigned char* markBasicBlockStartAddr(unsigned char* const blockStartMemory) {
    unsigned char* currMemPos = blockStartMemory;
    long bbIndexLong = static_cast<long>(bbIndex);
    string indexToLittleEndianHex = getPtrRelOffset(reinterpret_cast<intptr_t>(bbIndexLong), 0);
    // mov DWORD PTR [rsi], bbIndex     ; Moves 4 bytes (32 bits) to the address in rsi
    const string objcode = hexToStr("c706" + indexToLittleEndianHex);
    memcpy(currMemPos, objcode.c_str(), objcode.size());
    instrToMemAddr.push_back(currMemPos);
    currMemPos += objcode.size();

    return currMemPos;
  }
  vector<unique_ptr<Instr>> instrs;
  vector<unsigned char*> instrToMemAddr;
  size_t bbIndex;
};

void executeJIT(vector<unique_ptr<Instr>>& instrs) {
  constexpr size_t memorySize = 2<<14;
  auto* execMemVoidPtr = mmap(nullptr, memorySize, 
                          PROT_READ | PROT_WRITE | PROT_EXEC,
                          MAP_ANON | MAP_PRIVATE, 
                          -1, 0);

  auto *execMemPtr = static_cast<unsigned char*>(execMemVoidPtr);
  vector<BasicBlock> basicBlocks;

  // map where to jump for [ and ]
  const unordered_map<size_t, size_t> matchingLoopBracket = initializeLoopBracketIndexes(instrs);
  unordered_map<size_t, size_t> jzInstrToBB;


  // create tape
  unsigned char *const tapePtr = static_cast<unsigned char*>(calloc(TAPESIZE, 1)) + TAPESIZE / 2;
  unsigned char* currTapePtr = tapePtr;

  // create function call to get to executable code
  // updates finalBBIndex after every call
  // returns final memory cell pointed to before exiting
  typedef unsigned char* (*fptr)(unsigned char* currTapePtr, unsigned* finalBBIndex);
  fptr my_fptr = reinterpret_cast<fptr>(reinterpret_cast<long>(execMemPtr));

  for(size_t lhs = 0, rhs = 0; rhs < instrs.size(); ++rhs) {
    const auto& instr = instrs[rhs];
    const Op op = instr->op;

    if(op == JumpIfZero || op == JumpUnlessZero || op == EndOfFile) {
      const size_t nextBBIndex = basicBlocks.size();
      basicBlocks.emplace_back(instrs, lhs, rhs + 1, nextBBIndex);
      auto* nextExecMemPtr = basicBlocks.back().generateBasicBlockInstrs(execMemPtr);

      if(op == JumpIfZero)
        jzInstrToBB[rhs] = nextBBIndex;

      // if jumpUnlessZero, then we already can form the backedge to the jumpifzero instruction
      if(op == JumpUnlessZero) {
        auto targetLoopInstrIndex = matchingLoopBracket.at(rhs);
        auto targetBBIndex = jzInstrToBB[targetLoopInstrIndex];
        auto targetJumpAddr = basicBlocks[targetBBIndex].getFinalInstrMemAddr();
        basicBlocks.back().setTailOnNotZeroMemAddr(targetJumpAddr);
      }

      for(size_t i = 0; i < 100; ++i)
        cout << execMemPtr[i];
      cout << flush;

      unsigned lastBBIndex;

      // jump to memory
      currTapePtr = my_fptr(currTapePtr, &lastBBIndex);

      BasicBlock& lastBB = basicBlocks[lastBBIndex];

      if(op == JumpIfZero) {
        if(*currTapePtr == 0) {
          lastBB.setTailOnZeroMemAddr(nextExecMemPtr);
          rhs = matchingLoopBracket.at(rhs);
          lhs = rhs + 1;
        }
        else {
          lastBB.setTailOnNotZeroMemAddr(nextExecMemPtr);
          lhs = rhs + 1;
        }
      }
      else if(op == JumpUnlessZero) {
        lastBB.setTailOnZeroMemAddr(nextExecMemPtr);
        lhs = rhs + 1;
      }
      execMemPtr = nextExecMemPtr;
    }
  }



  // cout << totalObjCode;
  // cout << flush;




  return;
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

  if(settings.justInTime) {
    executeJIT(instrs);
    return EXIT_SUCCESS;
  }
  string program = compile(instrs);

  if(!settings.outfile)
    cout << program << endl;
  else {
    ofstream MyFile(settings.outfile.value());
    MyFile << program << endl;
    MyFile.close();
  }
}

