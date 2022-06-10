#include <iostream>

#include "antlr4-runtime/antlr4-runtime.h"
#include "parser/brainfuckLexer.h"
#include "parser/brainfuckParser.h"
#include "visitor.h"

int main() {
   //std::string inputString = "++++++++[>++++[>++>+++>+++>+<<<<-]>+>+>->>+[<]<-]>>.>---.+++++++..+++.>>.<-.<.+++.------.--------.>>+.>++.";
  // std::string inputString = ",++.[,..--------------------------------<>]"; // Space quits
  // std::string inputString = "++++++++++" "++++++++++" "++++++++++" "++++++++++" "++.";
   //antlr4::ANTLRInputStream input(inputString);
   //brainfuckLexer lexer(&input);

  constexpr auto filename = "../main.bf";

  std::ifstream stream;
  stream.open(filename);
  if(!stream.is_open()) {
    std::clog << "Failed to open " << filename << "\n";
    std::exit(1);
  }
  antlr4::ANTLRInputStream inputStream(stream);
  brainfuckLexer lexer(&inputStream);

  antlr4::CommonTokenStream tokenStream(&lexer);
  brainfuckParser parser(&tokenStream);
  brainfuckParser::FileContext* tree = parser.file();

  Visitor visitor;
  auto thing = visitor.visitFile(tree);

  std::cout << "Hello, World!" << std::endl;
  return 0;
}
