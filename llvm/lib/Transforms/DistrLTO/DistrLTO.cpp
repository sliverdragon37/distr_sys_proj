//===- Hello.cpp - Example code from "Writing an LLVM Pass" ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements two versions of the LLVM "Hello World" pass described
// in docs/WritingAnLLVMPass.html
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "hello"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include <map>
#include <set>
#include <string>
using namespace llvm;
using namespace std;

STATISTIC(HelloCounter, "Counts number of functions greeted");

namespace {

    //represent allocation sites with a single int
    typedef int alloc_site;

    //ID for each function call
    typedef int ID;
    
    //represent sets of allocation sites with sets of ints
    typedef set<alloc_site> alloc_sites;
    
    //contexts are mappings of formal parameters to possible sources
    typedef const map<string,alloc_sites> context;

    //analysis results for one particular context
    struct results {

	//IDs of callers which called us
	set<ID> callers;

	//TODO more data here
	//alloc_sites for each pointer at each position
	
    };


    //analysis results of calls made to external functions
    //we don't actually need to memoize this?
    //typedef map<pair<string,context>,alloc_sites> call_results;

    struct Distr : public ModulePass {
	//results to update when we get new info
        map<ID,*results> to_update;

	//results for all contexts
	map<context,results> cs_results;

	static char ID; // Pass identification, replacement for typeid
	Distr() : ModulePass(ID) {}

	virtual bool runOnModule(Module &M) {
	    errs().write_escaped("This is a module\n");
	    ++HelloCounter;
	    errs().write_escaped(M.getModuleIdentifier()) << '\n';
	    errs().write_escaped(M.getDataLayout()) << '\n';
	    errs().write_escaped(M.getTargetTriple()) << "\n\n\n";
	    //functions in the program
	    for (auto i = M.begin(); i != M.end(); i++) {
		errs().write_escaped("function: ");
		errs().write_escaped(i->getName()) << '\n';

		//basic blocks in the function
		for (auto j = i->begin(); j != i->end(); j++) {
		    //errs().write_escaped(j->getName()) << '\n';

		    //instructions in the function
		    for (auto instr = j->begin(); instr != j->end(); instr++){
			errs().write_escaped(instr->getOpcodeName()) << "\n";
		    }
		}
	    }

	    //basic design:
	    // *go through and count allocation sites in entire program
	    // *initialize data structures
	    
	    return false;
	}
    };
}

char Distr::ID = 0;
static RegisterPass<Distr> X("hello", "Hello World Pass");

namespace {
  // Hello2 - The second implementation with getAnalysisUsage implemented.
  struct Hello2 : public FunctionPass {
    static char ID; // Pass identification, replacement for typeid
    Hello2() : FunctionPass(ID) {}

    virtual bool runOnFunction(Function &F) {
      ++HelloCounter;
      errs() << "Hello: ";
      errs().write_escaped(F.getName()) << '\n';
      return false;
    }

    // We don't modify the program, so we preserve all analyses
    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.setPreservesAll();
    }
  };
}

char Hello2::ID = 0;
static RegisterPass<Hello2>
Y("hello2", "Hello World Pass (with getAnalysisUsage implemented)");
