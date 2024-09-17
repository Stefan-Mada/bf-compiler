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

int main(int argc, char** argv) {
  if(argc != 2) {
    cerr << "Need exactly one file argument to interpret" << endl;
    exit(-1);
  }

  string code = readFile(argv[1]);

  cout << code << endl;
}