//
// Created by lennart on 27.04.21.
//

#include "visitor.h"

antlrcpp::Any Visitor::visitFile(brainfuckParser::FileContext* context) {
  std::cout << "Visiting File" << std::endl;

  // Create Main Function
  FunctionType* mainFunctionType = FunctionType::get(Type::getInt32Ty(llvmContext), {}, false);
  mainFunction = Function::Create(mainFunctionType, Function::ExternalLinkage, "main", *module);
  currentFunction = mainFunction;

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
  stack = builder.CreateAlloca(Type::getInt32Ty(llvmContext), 0, stackSize, "stack");
  // TODO Find out what the size value means
  builder.CreateMemSet(stack, ConstantInt::get(llvmContext, APInt(8, 0)), 500 * 4, MaybeAlign(4), false);
  stackOffset = ConstantInt::get(llvmContext, APInt(32, 0));

  DIType* stackType = debug.getPointerType();
  // DILocalVariable* stackVar = debug.builder->createAutoVariable(SP, "stack", SP->getFile(), 1, stackType, true);
  DILocalVariable* stackVar = debug.builder->createAutoVariable(SP, StringRef("stack"), SP->getFile(), 1, stackType, false);
  DIExpression* expression = debug.builder->createExpression();
  DILocation* location = DILocation::get(SP->getContext(), 1, 0, SP);
  debug.builder->insertDeclare(stack, stackVar, expression, location, BB);
  /*DILocalVariable *stackVariable = debug.builder->createParameterVariable(
      SP, "stack", ++ArgIdx, debug.compileUnit->getFile(), context->getStart()->getLine(), KSDbgInfo.getDoubleTy(),
      true);
  debug.builder->insertDeclare(stack)*/

  // Value* printInt = ConstantInt::get(llvmContext, APInt(32, 42));
  // builder.CreateCall(putcharType, putcharFunction, {printInt}, "call");

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

  int line = context->getStart()->getLine();
  int column = context->getStart()->getCharPositionInLine();

  std::string functionName = "block";
  functionName.append(std::to_string(line));
  functionName.append("_");
  functionName.append(std::to_string(column));

  //
  std::vector<Type*> types{Type::getInt32PtrTy(llvmContext), Type::getInt32Ty(llvmContext)};
  FunctionType* blockFunctionType = FunctionType::get(Type::getInt32Ty(llvmContext), types, false);
  Function* function = Function::Create(blockFunctionType, Function::ExternalLinkage, functionName, *module);
  auto returnStackOffset = builder.CreateCall(blockFunctionType, function, {stack, stackOffset});

  // Save last state
  auto lastStack = stack;
  auto lastStackOffset = stackOffset;
  auto lastFunction = currentFunction;
  auto lastInsertPoint = builder.GetInsertBlock();

  BasicBlock* entryBlock = BasicBlock::Create(llvmContext, "entry", function);
  BasicBlock* mainBlock = BasicBlock::Create(llvmContext, "main", function);
  BasicBlock* exitBlock = BasicBlock::Create(llvmContext, "exit", function);
  //  BasicBlock *recurseBlock = BasicBlock::Create(llvmContext, "recurse", function);

  function->getArg(0)->setName("stack");
  function->getArg(1)->setName("stackOffset");

  currentFunction = function;

  // Function
  DISubprogram* subprogram = debug.createFunction(functionName, line, column);
  function->setSubprogram(subprogram);
  debug.stack.push(subprogram);

  // Create entry block
  debug.setLocation(line, column);
  builder.SetInsertPoint(entryBlock);
  stack = function->getArg(0);
  Value* stackOffsetArgument = function->getArg(1);
  Value* currentStackAddress1 = builder.CreateGEP(stack, stackOffsetArgument, "currentStackAddress");
  Value* currentStackValue1 = builder.CreateLoad(currentStackAddress1, "currentStackValue");
  Value* skipBlockConditionVariable = builder.CreateICmpNE(currentStackValue1,
                                                           ConstantInt::get(llvmContext, APInt(32, 0)),
                                                           "skipBlockConditionVariable");
  builder.CreateCondBr(skipBlockConditionVariable, mainBlock, exitBlock);

  // Create exit block
  debug.setLocation(context->getStop()->getLine(), context->getStop()->getCharPositionInLine());
  builder.SetInsertPoint(exitBlock);
  PHINode* exitPhiNode = builder.CreatePHI(Type::getInt32Ty(llvmContext), 2, "exitPhiNode");
  exitPhiNode->addIncoming(stackOffsetArgument, entryBlock);
  builder.CreateRet(exitPhiNode);

  // Prepare main block
  debug.setLocation(line, column);
  builder.SetInsertPoint(mainBlock);
  PHINode* mainPhiNode = builder.CreatePHI(Type::getInt32Ty(llvmContext), 2, "stackOffset");
  mainPhiNode->addIncoming(stackOffsetArgument, entryBlock);
  stackOffset = mainPhiNode;

  // Insert body
  visitStatements(context->statements());
  mainPhiNode->addIncoming(stackOffset, mainBlock);

  // Insert end decision
  debug.setLocation(context->getStop()->getLine(), context->getStop()->getCharPositionInLine());
  Value* currentStackAddress = builder.CreateGEP(stack, stackOffset, "currentStackAddress");
  Value* currentStackValue = builder.CreateLoad(currentStackAddress, "currentStackValue");
  Value* conditionVar = builder.CreateICmpNE(currentStackValue, ConstantInt::get(llvmContext, APInt(32, 0)), "blockEndCondition");
  builder.CreateCondBr(conditionVar, mainBlock, exitBlock);

  // Finish end block
  exitPhiNode->addIncoming(stackOffset, mainBlock);
  // debug.setLocation(line, column);

  stackOffset = returnStackOffset;

  debug.builder->finalizeSubprogram(subprogram);
  debug.stack.pop();

  // Restore last state
  builder.SetInsertPoint(lastInsertPoint);
  stack = lastStack;
  currentFunction = lastFunction;

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

Visitor::Visitor(): llvmContext(), builder(llvmContext) {
  debug.parent = this;
  InitializeNativeTarget();
  InitializeNativeTargetAsmParser();
  // InitializeAllAsmPrinters();
  module = std::make_unique<Module>("brainfuckModule", llvmContext);

  module->addModuleFlag(Module::Warning, "Debug Info Version", DEBUG_METADATA_VERSION);

  debug.builder = std::make_unique<DIBuilder>(*module);
  debug.compileUnit = debug.builder
                          ->createCompileUnit(dwarf::DW_LANG_C, debug.builder->createFile("main.bf", "."), "Brainfuck Compiler", false, "", 0);

  // Add extern functions
  putcharType = FunctionType::get(Type::getInt32Ty(llvmContext), Type::getInt32Ty(llvmContext), false);
  putcharFunction = Function::Create(putcharType, Function::ExternalLinkage, "putchar", *module);
  getcharType = FunctionType::get(Type::getInt32Ty(llvmContext), {}, false);
  getcharFunction = Function::Create(getcharType, Function::ExternalLinkage, "getchar", *module);

  // TheModule->setDataLayout(TheJIT->getTargetMachine().createDataLayout());
}

void Visitor::initializeMainFunction(Function* mainFunction) {
  BasicBlock* BB = BasicBlock::Create(llvmContext, "entry", mainFunction);
  builder.SetInsertPoint(BB);
  // builder.SetInsertPoint(&(mainFunction->getEntryBlock()));
  Value* stackSize = ConstantInt::get(llvmContext, APInt(32, 500));
  stack = builder.CreateAlloca(Type::getInt32Ty(llvmContext), 0, stackSize, "stack");
  stackOffset = ConstantInt::get(llvmContext, APInt(32, 0));

  Value* printInt = ConstantInt::get(llvmContext, APInt(32, 42));
  builder.CreateCall(putcharType, putcharFunction, {printInt}, "call");
}

void Visitor::generateLeft() {
  stackOffset = builder.CreateAdd(stackOffset, ConstantInt::get(llvmContext, APInt(32, 1)), "stackOffset");
}
void Visitor::generateRight() {
  stackOffset = builder.CreateSub(stackOffset, ConstantInt::get(llvmContext, APInt(32, 1)), "stackOffset");
}
void Visitor::generateIncrement() {
  DILabel* label = debug.builder->createLabel(debug.stack.top(),
                                              "increment",
                                              debug.compileUnit->getFile(),
                                              builder.getCurrentDebugLocation().getLine(),
                                              false);

  Value* currentStackAddress = builder.CreateGEP(stack, stackOffset, "currentStackAddress");
  Value* oldStackValue = builder.CreateLoad(currentStackAddress, "oldStackValue");
  Value* stackValue = builder.CreateAdd(oldStackValue, ConstantInt::get(llvmContext, APInt(32, 1)), "stackValue");
  builder.CreateStore(stackValue, currentStackAddress);
}
void Visitor::generateDecrement() {
  Value* currentStackAddress = builder.CreateGEP(stack, stackOffset, "currentStackAddress");
  Value* oldStackValue = builder.CreateLoad(currentStackAddress, "oldStackValue");
  Value* stackValue = builder.CreateSub(oldStackValue, ConstantInt::get(llvmContext, APInt(32, 1)), "stackValue");
  builder.CreateStore(stackValue, currentStackAddress);
}
void Visitor::generateRead() {
  Value* currentStackAddress = builder.CreateGEP(stack, stackOffset, "currentStackAddress");
  Value* stackValue = builder.CreateCall(getcharType, getcharFunction, {}, "readStackValue");
  builder.CreateStore(stackValue, currentStackAddress);
}
void Visitor::generateWrite() {
  Value* currentStackAddress = builder.CreateGEP(stack, stackOffset, "currentStackAddress");
  Value* stackValue = builder.CreateLoad(currentStackAddress, "stackValue");
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

void Visitor::DebugInfo::setLocation(int line, int column, DIScope* scope) {
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

DISubprogram* Visitor::DebugInfo::createFunction(const std::string& name, int line, int column, DISubroutineType* type) {
  if(type == nullptr) {
    std::vector<Metadata*> values;
    values.push_back(getIntType());
    values.push_back(getPointerType());
    values.push_back(getIntType());
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
