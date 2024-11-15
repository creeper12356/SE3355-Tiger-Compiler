### Description

Build an LLVM IR tree for two simple programs. This exercise aims to introduce the IR tree construction syntax of LLVM and the various IR tree statements that may be used in the lab in the future.

It is a warm-up exercise for the subsequent lab. It is very helpful for familiarizing oneself with LLVM.

In this experiment, you should modify the file genCalculateDistance.cpp/genEasy.cpp. We have provided some hints in the comments.

In these two files, there is already a lot of code, such as generating global variables and structure types. These parts are not subject to assessment requirements in this lab, but they will be very helpful for subsequent labs. Therefore, you should carefully read these sections and refer to them to implement the subsequent labs.

### Procedure

1. This lab contains two pre written C++ programs, located in the origin folder. You should first read these two programs to understand the content of these two simple C++ programs.

2. When compiling these two programs through LLVM, the IR tree files of LLVM will be generated first. These two files have been provided in the origin folder. You should try to read them and understand their correspondence with the source code.

3. What you need to do is to implement genCalculateDistance.cpp/genEasy.cpp, which will generate two similar llvm ir tree files in the out folder. These two files should have the same functionality as the files in the origin folder.

4. The content of the test is to attempt to compile the intermediate tree file you generated, in order to expect it to be consistent with the functionality of the C++ source code.

### Hints

In this lab, the LLVM components you may need to use include the following:

1. llvm::AllocaInst
2. llvm::StoreInst
3. llvm::BasicBlock
4. llvm::LoadInst
5. llvm::IRBuilderBase::CreateICmpSLT
6. llvm::IRBuilderBase::CreateSExt
7. llvm::IRBuilderBase::CreateGEP
8. llvm::IRBuilderBase::CreatePHI
9. llvm::IRBuilderBase::CreateNSWAdd
10. llvm::IRBuilderBase::CreateCondBr
11. llvm::IRBuilderBase::CreateCall
12. llvm::IRBuilderBase::CreateRet

You can find the usage of them [here](https://releases.llvm.org/1.1/docs/ProgrammersManual.html).


### Environment

You will use **a new** code framework. What you need to do now is to pull the latest update of the code framework if there are any.

```
shell% git fetch upstream
shell% git checkout -b lab5-part0 upstream/lab5-part0
shell% git push -u origin
```

You don't need to merge/rebase any commits of previous labs.

### Grade Test

The lab environment contains a grading script named as **scripts/grade.sh**, you can use it to evaluate your code, and that's how we grade your code, too. If you pass all the tests, the script will print a successful hint, otherwise, it will output some error messages. You can execute the script with the following commands.

Remember grading your lab under docker or unix shell! Never run these commands under windows cmd.

```
shell% make
shell% ...
shell% [^_^]: Pass #If you pass all the tests, you will see these messages.
shell% TOTAL SCORE: 100
```
