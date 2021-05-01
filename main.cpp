#include <iostream>

#include "antlr4-runtime/antlr4-runtime.h"
#include "parser/brainfuckLexer.h"
#include "parser/brainfuckParser.h"
#include "parser/brainfuckVisitor.h"
#include "visitor.h"

int main() {
  // std::string inputString = "++++++++[>++++[>++>+++>+++>+<<<<-]>+>+>->>+[<]<-]>>.>---.+++++++..+++.>>.<-.<.+++.------.--------.>>+.>++.";
  // std::string inputString = ",++.[,..--------------------------------<>]"; // Space quits
  // std::string inputString = "++++++++++" "++++++++++" "++++++++++" "++++++++++" "++.";
  // antlr4::ANTLRInputStream input(inputString);
  // brainfuckLexer lexer(&input);

  std::ifstream stream;
  stream.open("../main.bf");
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
