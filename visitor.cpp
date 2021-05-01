//
// Created by lennart on 27.04.21.
//

#include "visitor.h"

antlrcpp::Any Visitor::visitFile(brainfuckParser::FileContext* context) {
  std::cout << "Visiting File" << std::endl;

  // Create Main Function
  FunctionType* mainFunctionType = FunctionType::get(Type::getInt32Ty(llvmContext), {}, false);
  Function* mainFunction = Function::Create(mainFunctionType, Function::ExternalLinkage, "main", *module);

  // Create debug information for main function
  std::vector<Metadata*> values;
  values.push_back(debug.getIntType());
  auto types = debug.builder->getOrCreateTypeArray(values);
  auto mainFunctionType2 = debug.builder->createSubroutineType(types);
  DISubprogram* SP = debug.createFunction("main", 1, 0, mainFunctionType2);
  mainFunction->setSubprogram(SP);
  debug.stack.push(SP);

  // Fill main function
  debug.setLocation(1, 0);
  BasicBlock* BB = BasicBlock::Create(llvmContext, "entry", mainFunction);
  builder.SetInsertPoint(BB);
  // builder.SetInsertPoint(&(mainFunction->getEntryBlock()));
  Value* stackSize = ConstantInt::get(llvmContext, APInt(32, 500));
  stackPointer = builder.CreateAlloca(Type::getInt32Ty(llvmContext), 0, stackSize, "stackPointer");
  // TODO Find out what the size value means
  builder.CreateMemSet(stackPointer, ConstantInt::get(llvmContext, APInt(8, 0)), 500 * 4, MaybeAlign(4), false);

  DIType* stackType = debug.getPointerType();
  // DILocalVariable* stackVar = debug.builder->createAutoVariable(SP, "stackPointer", SP->getFile(), 1, stackType, true);
  DILocalVariable* stackVar = debug.builder->createAutoVariable(SP, StringRef("stackPointer"), SP->getFile(), 1, stackType, false);
  DIExpression* expression = debug.builder->createExpression();
  DILocation* location = DILocation::get(SP->getContext(), 1, 0, SP);
  debug.builder->insertDeclare(stackPointer, stackVar, expression, location, BB);

  visitChildren(context);

  debug.setLocation(context->getStop()->getLine(), context->getStop()->getCharPositionInLine());
  builder.CreateRet(ConstantInt::get(llvmContext, APInt(32, 0)));

  debug.stack.pop();
  debug.builder->finalizeSubprogram((DISubprogram*)SP);

  debug.builder->finalize();
  std::string outputFile = "output.llvm";
  std::cout << "Output file: " << outputFile << std::endl;
  module->print(llvm::errs(), nullptr);
  std::error_code EC;
  raw_fd_ostream dest(outputFile, EC);
  if(EC) {
    std::clog << "Failed to open " << outputFile << ".";
    return 1;
  }
  module->print(dest, nullptr);

  return 0;
  // return mainFunction;
}

antlrcpp::Any Visitor::visitStatements(brainfuckParser::StatementsContext* context) {
  std::cout << "Visiting statements" << std::endl;
  debug.setLocation(context->getStart()->getLine(), context->getStart()->getCharPositionInLine());

  return visitChildren(context);
}
antlrcpp::Any Visitor::visitBlock(brainfuckParser::BlockContext* context) {
  std::cout << "Visiting block" << std::endl;

  unsigned int line = context->getStart()->getLine();
  unsigned int column = context->getStart()->getCharPositionInLine();

  std::string functionName = "block";
  functionName.append(std::to_string(line));
  functionName.append("_");
  functionName.append(std::to_string(column));

  // Create function
  std::vector<Type*> types{Type::getInt32PtrTy(llvmContext)};
  FunctionType* blockFunctionType = FunctionType::get(Type::getInt32PtrTy(llvmContext), types, false);
  Function* function = Function::Create(blockFunctionType, Function::InternalLinkage, functionName, *module);
  function->getArg(0)->setName("stackPointer");
  BasicBlock* entryBlock = BasicBlock::Create(llvmContext, "entry", function);
  BasicBlock* mainBlock = BasicBlock::Create(llvmContext, "main", function);
  BasicBlock* exitBlock = BasicBlock::Create(llvmContext, "exit", function);
  DISubprogram* subprogram = debug.createFunction(functionName, line, column);
  function->setSubprogram(subprogram);
  debug.stack.push(subprogram);

  // Create a call to the function with the current stackPointer
  Value* resultingStackPointer = builder.CreateCall(blockFunctionType, function, {stackPointer});

  // Save last state
  auto previousInsertPoint = builder.GetInsertBlock();

  // Create entry block
  debug.setLocation(line, column);
  builder.SetInsertPoint(entryBlock);
  Value* stackArgument = function->getArg(0);
  Value* stackValue = builder.CreateLoad(stackArgument, "stackValue");
  Value* skipBlockConditionVariable = builder.CreateICmpNE(stackValue, ConstantInt::get(llvmContext, APInt(32, 0)), "skipBlockConditionVariable");
  builder.CreateCondBr(skipBlockConditionVariable, mainBlock, exitBlock);

  // Create exit block
  debug.setLocation(context->getStop()->getLine(), context->getStop()->getCharPositionInLine());
  builder.SetInsertPoint(exitBlock);
  PHINode* exitPhiNode = builder.CreatePHI(Type::getInt32PtrTy(llvmContext), 2, "exitPhiNode");
  exitPhiNode->addIncoming(stackArgument, entryBlock);
  builder.CreateRet(exitPhiNode);

  // Prepare main block
  debug.setLocation(line, column);
  builder.SetInsertPoint(mainBlock);
  PHINode* mainPhiNode = builder.CreatePHI(Type::getInt32PtrTy(llvmContext), 2, "stackOffset");
  mainPhiNode->addIncoming(stackArgument, entryBlock);

  stackPointer = mainPhiNode;
  visitStatements(context->statements());
  mainPhiNode->addIncoming(stackPointer, mainBlock);
  exitPhiNode->addIncoming(stackPointer, mainBlock);

  debug.setLocation(context->getStop()->getLine(), context->getStop()->getCharPositionInLine());
  Value* stackValueAfterBlock = builder.CreateLoad(stackPointer, "stackValue");
  Value* conditionVar = builder.CreateICmpNE(stackValueAfterBlock, ConstantInt::get(llvmContext, APInt(32, 0)), "blockEndCondition");
  builder.CreateCondBr(conditionVar, mainBlock, exitBlock);

  // Set stackPointer to result of this block
  stackPointer = resultingStackPointer;

  debug.builder->finalizeSubprogram(subprogram);

  // Restore previous state
  debug.stack.pop();
  builder.SetInsertPoint(previousInsertPoint);

  return 0;
}
antlrcpp::Any Visitor::visitStatement(brainfuckParser::StatementContext* context) {
  std::cout << "Visiting statement" << std::endl;
  debug.setLocation(context->getStart()->getLine(), context->getStart()->getCharPositionInLine());

  // TODO Adjust parser to remove this if else chain
  if(context->Left()) {
    generateLeft();
  } else if(context->Right()) {
    generateRight();
  } else if(context->Increment()) {
    generateIncrement();
  } else if(context->Decrement()) {
    generateDecrement();
  } else if(context->Read()) {
    generateRead();
  } else if(context->Write()) {
    generateWrite();
  }
  return visitChildren(context);
}

Visitor::Visitor(): llvmContext(), builder(llvmContext), stackPointer(nullptr) {
  debug.parent = this;
  InitializeNativeTarget();
  InitializeNativeTargetAsmParser();
  module = std::make_unique<Module>("brainfuckModule", llvmContext);
  module->addModuleFlag(Module::Warning, "Debug Info Version", DEBUG_METADATA_VERSION);
  debug.builder = std::make_unique<DIBuilder>(*module);
  debug.compileUnit = debug.builder
                          ->createCompileUnit(dwarf::DW_LANG_C, debug.builder->createFile("main.bf", "."), "Brainfuck Compiler", false, "", 0);

  // Add declarations for library functions
  putcharType = FunctionType::get(Type::getInt32Ty(llvmContext), Type::getInt32Ty(llvmContext), false);
  putcharFunction = Function::Create(putcharType, Function::ExternalLinkage, "putchar", *module);
  getcharType = FunctionType::get(Type::getInt32Ty(llvmContext), {}, false);
  getcharFunction = Function::Create(getcharType, Function::ExternalLinkage, "getchar", *module);
}

void Visitor::generateLeft() {
  stackPointer = builder.CreateGEP(stackPointer, ConstantInt::get(llvmContext, APInt(32, 1)), "stackPointer");
}
void Visitor::generateRight() {
  stackPointer = builder.CreateGEP(stackPointer, ConstantInt::get(llvmContext, APInt(32, -1)), "stackPointer");
}
void Visitor::generateIncrement() {
  //  DILabel* label = debug.builder->createLabel(debug.stack.top(),
  //                                              "increment",
  //                                              debug.compileUnit->getFile(),
  //                                              builder.getCurrentDebugLocation().getLine(),
  //                                              false);
  Value* oldStackValue = builder.CreateLoad(stackPointer, "oldStackValue");
  Value* stackValue = builder.CreateAdd(oldStackValue, ConstantInt::get(llvmContext, APInt(32, 1)), "stackValue");
  builder.CreateStore(stackValue, stackPointer);
}
void Visitor::generateDecrement() {
  Value* oldStackValue = builder.CreateLoad(stackPointer, "oldStackValue");
  Value* stackValue = builder.CreateSub(oldStackValue, ConstantInt::get(llvmContext, APInt(32, 1)), "stackValue");
  builder.CreateStore(stackValue, stackPointer);
}
void Visitor::generateRead() {
  Value* stackValue = builder.CreateCall(getcharType, getcharFunction, {}, "stackValue");
  builder.CreateStore(stackValue, stackPointer);
}
void Visitor::generateWrite() {
  Value* stackValue = builder.CreateLoad(stackPointer, "stackValue");
  builder.CreateCall(putcharType, putcharFunction, {stackValue});
}
DIType* Visitor::DebugInfo::getIntType() {
  if(intType != nullptr) {
    return intType;
  }

  intType = builder->createBasicType("i32", 32, dwarf::DW_ATE_signed);
  return intType;
}

DIType* Visitor::DebugInfo::getPointerType() {
  if(pointerType != nullptr) {
    return pointerType;
  }

  pointerType = builder->createBasicType("i32*", 64, dwarf::DW_ATE_address);
  return pointerType;
}

void Visitor::DebugInfo::setLocation(unsigned int line, unsigned int column, DIScope* scope) {
  if(scope == nullptr) {
    if(!stack.empty()) {
      scope = stack.top();
    } else {
      scope = compileUnit;
    }
  }
  // DIScope* newScope = compileUnit;
  parent->builder.SetCurrentDebugLocation(DILocation::get(scope->getContext(), line, column, scope));
}

DISubprogram* Visitor::DebugInfo::createFunction(const std::string& name,
                                                 unsigned int line,
                                                 [[maybe_unused]] unsigned int column,
                                                 DISubroutineType* type) {
  if(type == nullptr) {
    std::vector<Metadata*> values;
    values.push_back(getPointerType());
    values.push_back(getPointerType());
    // values.push_back(getIntType());
    auto types = builder->getOrCreateTypeArray(values);
    type = builder->createSubroutineType(types);
  }

  DISubprogram* SP = builder->createFunction(compileUnit->getFile(),
                                             name,
                                             name,
                                             compileUnit->getFile(),
                                             line,
                                             type,
                                             line,
                                             DINode::FlagPrototyped,
                                             DISubprogram::SPFlagDefinition);
  return SP;
}
