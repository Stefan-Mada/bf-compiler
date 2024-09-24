#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <stack>
#include <vector>
#include <utility>
#include <array>

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
  EndOfFile
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

string instrStr(const string& str) {
  return "\t" + str + "\n";
}

string compile(const vector<Op>& ops) {
  string assembly = initializeProgram();

  // Note: %rdi will hold the current index on the tape
  // Except when calling putchar or getchar, then %rdi
  // will be pushed onto the stack

  const auto& [matchingBracketLabelMap, ownLabelMap] = initializeLoopBracketLabels(ops);

  for(size_t IP = 0; IP < ops.size(); ++IP) {
    const Op currInstr = ops[IP];

    switch(currInstr) {
      case MoveRight: {
        assembly += instrStr("inc\t%rdi");
        break;
      }
      case MoveLeft: {
        assembly += instrStr("dec\t%rdi");
        break;
      }
      case Inc: {
        assembly += instrStr("incb\t(%rdi)");
        break;
      }
      case Dec: {
        assembly += instrStr("decb\t(%rdi)");
        break;
      }
      case Write: {
        assembly += instrStr("push\t%rdi");
        assembly += instrStr("movb\t(%rdi), %dil");
        assembly += instrStr("call\tputchar");
        assembly += instrStr("pop\t%rdi");
        break;
      }
      case Read: {
        assembly += instrStr("push\t%rdi");
        assembly += instrStr("call\tgetchar");
        assembly += instrStr("pop\t%rdi");
        assembly += instrStr("movb\t%al, (%rdi)");
        break;
      }
      case JumpIfZero: {
        const string thisLabel = ownLabelMap.at(IP);
        const string targetLabel = matchingBracketLabelMap.at(IP);
        assembly += thisLabel + ":\n";
        assembly += instrStr("cmpb\t$0, (%rdi)");
        assembly += instrStr("je\t"+targetLabel);
        break;
      }
      case JumpUnlessZero: {
        const string thisLabel = ownLabelMap.at(IP);
        const string targetLabel = matchingBracketLabelMap.at(IP);
        assembly += thisLabel + ":\n";
        assembly += instrStr("cmpb\t$0, (%rdi)");
        assembly += instrStr("jne\t"+targetLabel);
        break;
      }
      case EndOfFile: {
        assembly += instrStr("ret");
        break;
      }
      default: {
        continue;
      }
    }
  }

  return assembly;
}

int main(int argc, char** argv) {
  if(argc != 2 ) {
    cerr << "Need exactly one file argument to compile" << endl;
    exit(-1);
  }

  const vector<Op> ops = readFile(argv[argc - 1]);

  string program = compile(ops);

  cout << program << endl;
}

