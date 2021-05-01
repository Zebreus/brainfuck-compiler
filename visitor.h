//
// Created by lennart on 27.04.21.
//

#ifndef BRAINFUCK_VISITOR_H
#define BRAINFUCK_VISITOR_H

#include <fstream>
#include <iostream>

#include "antlr4-runtime/antlr4-runtime.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DIBuilder.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Utils.h"
#include "parser/brainfuckLexer.h"
#include "parser/brainfuckParser.h"
#include "parser/brainfuckVisitor.h"

using namespace llvm;

class Visitor: public brainfuckVisitor {
 private:
  LLVMContext llvmContext;
  IRBuilder<> builder;
  std::unique_ptr<Module> module;

  struct DebugInfo {
    std::unique_ptr<DIBuilder> builder;
    DICompileUnit* compileUnit = nullptr;
    DIType* intType = nullptr;
    DIType* pointerType = nullptr;
    DIType* voidType = nullptr;
    DIType* getIntType();
    DIType* getPointerType();
    DIType* getVoidType();
    std::stack<DIScope*> stack;
    Visitor* parent = nullptr;
    void setLocation(int line, int col, DIScope* scope = nullptr);
    DISubprogram* createFunction(const std::string&, int line, int column, DISubroutineType* type = nullptr);
  } debug;

  FunctionType* putcharType;
  FunctionType* getcharType;
  Function* putcharFunction;
  Function* getcharFunction;

  Function* mainFunction;
  Function* currentFunction;
  Value* stack;
  //Value* stackOffset;

  void initializeMainFunction(Function* mainFunction);

  void generateLeft();
  void generateRight();
  void generateIncrement();
  void generateDecrement();
  void generateRead();
  void generateWrite();

 public:
  Visitor();

  antlrcpp::Any visitFile(brainfuckParser::FileContext* context) override;
  antlrcpp::Any visitStatements(brainfuckParser::StatementsContext* context) override;
  // Returns a
  antlrcpp::Any visitBlock(brainfuckParser::BlockContext* context) override;
  antlrcpp::Any visitStatement(brainfuckParser::StatementContext* context) override;
};

#endif  // BRAINFUCK_VISITOR_H
