/*
 * Copyright (c) 2009 Carnegie Mellon University.
 *     All rights reserved.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing,
 *  software distributed under the License is distributed on an "AS
 *  IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 *  express or implied.  See the License for the specific language
 *  governing permissions and limitations under the License.
 *
 * For more about this software visit:
 *
 *      http://www.graphlab.ml.cmu.edu
 *
 */

#define __STDC_LIMIT_MACROS 1
#define __STDC_CONSTANT_MACROS 1

#include <vector>
#include <string>
#include <fstream>
#include <memory>
#include <climits>

#include <graphlab.hpp>
#include <graphlab/engine/warp_engine.hpp>


#include "llvm/IRReader/IRReader.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/IR/Module.h"

using namespace graphlab;

// The graph type is determined by the vertex and edge data types
typedef distributed_graph<std::string , graphlab::empty> graph_type;
typedef warp::warp_engine<graph_type> warp_engine_type;

//is this done per machine?
//TODO make sure this works
llvm::Module* M = NULL;

//aw hell
std::map<graphlab::vertex_id_type,std::string> fun_names;

//fuck global variables this is terrible
std::map<std::string,llvm::Function*> functions;

int counter = 0;

/*
 * A simple function used by graph.transform_vertices(init_vertex);
 * to initialize the vertes data.
 */
void init_vertex(graph_type::vertex_type& vertex) {
  std::map<graphlab::vertex_id_type,std::string>::iterator i = fun_names.find(vertex.id());
  if (i != fun_names.end())
    vertex.data() = i->second;
}


// float pagerank_map(graph_type::edge_type edge, graph_type::vertex_type other) {
//   return other.data() / other.num_out_edges();
// }


void signal_neighbor(warp_engine_type::context& context,
                     graph_type::edge_type edge, graph_type::vertex_type other) {
  context.signal(other);
}



void pagerank(warp_engine_type::context& context,
              graph_type::vertex_type vertex) {
  context.cout() << "vertex data is: " << vertex.data() << std::endl;
  if (functions.find(vertex.data()) != functions.end()) {
    llvm::Function* f = functions.find(vertex.data())->second;
    for (llvm::Function::iterator i = f->begin(); i != f->end(); i++) {
	for (llvm::BasicBlock::iterator j = i->begin(); j != i->end(); j++) {
          counter++;
          //context.cout() << "instruction: " << std::endl;
	}
    }
  }
}



/*
 * We want to save the final graph so we define a write which will be
 * used in graph.save("path/prefix", pagerank_writer()) to save the graph.
 */
struct pagerank_writer {
  std::string save_vertex(graph_type::vertex_type v) {
    std::stringstream strm;
    strm << v.id() << "\t" << v.data() << "\n";
    return strm.str();
  }
  std::string save_edge(graph_type::edge_type e) { return ""; }
}; // end of pagerank writer


bool line_parser(graph_type& graph,
		 const std::string& filename,
		 const std::string& textline) {
    std::stringstream strm(textline);
    std::string vert, other_vert;
    graphlab::vertex_id_type vid, other_vid;
    std::string delim;

    strm.ignore(99,'x');
    strm >> vert;
    strm >> delim;
    if (delim == "->") {
	strm.ignore(99,'x');
	strm >> other_vert;
	//sscanf is ugly as balls but it works
	const char* x = ("0x" + vert).c_str();
	sscanf(x,"%x",&vid);
	const char* y = ("0x" + other_vert).c_str();
	sscanf(y,"%x",&other_vid);
	graph.add_edge(vid, other_vid);
    } else {
	//fml using sscanf sucks
	const char* x = ("0x" + vert).c_str();
	sscanf(x,"%x",&vid);

	bool on = false;
	std::string fun_name = "";
	for (std::string::iterator i = delim.begin(); i != delim.end(); i++) {
	    if (*i == '}')
		on = false;
	    if (on)
		fun_name += *i;
	    if (*i == '{')
		on = true;
	}
	if (fun_name == "external") {
	    fun_name = "external function";
	}
        std::cout << "adding vertex: " << fun_name << std::endl;
	fun_names.insert(std::pair<graphlab::vertex_id_type,std::string>(vid,fun_name));
	graph.add_vertex(vid, fun_name);
    }
    
    return true;
}
    
int main(int argc, char** argv) {
  global_logger().set_log_level(LOG_INFO);
  // Initialize control plain using mpi
  mpi_tools::init(argc, argv);
  distributed_control dc;

  // Parse command line options -----------------------------------------------
  command_line_options clopts("PageRank algorithm.");
  std::string graph_dir, InputFilename;
  std::string format = "tsv";
  clopts.attach_option("graph", graph_dir, "The graph file. ");
  clopts.attach_option("code", InputFilename, "The llvm bitcode file. ");
  clopts.attach_option("format", format, "The graph file format");
  size_t iterations = 10;
  clopts.attach_option("iterations", iterations,
                       "Number of asynchronous iterations to run");
  std::string saveprefix;
  clopts.attach_option("saveprefix", saveprefix,
                       "Prefix to save the output pagerank in");

  if(!clopts.parse(argc, argv)) {
    dc.cout() << "Error in parsing command line arguments." << std::endl;
    return EXIT_FAILURE;
  }

  // Load the input module...
  llvm::SMDiagnostic Err;
  llvm::LLVMContext Context;
  M = llvm::ParseIRFile(InputFilename, Err, Context);

  //make map of names to functions
  for (llvm::Module::iterator iter = M->begin(); iter != M->end(); iter++) {
      functions.insert(std::pair<std::string,llvm::Function*>(iter->getName(),&(*iter)));
  }
  
  llvm::Module::iterator i = M->begin();
  for (; i != M->end(); i++) {
      std::string s = i->getName();
      dc.cout() << s << std::endl;
  }
  
  // Build the graph ----------------------------------------------------------
  graph_type graph(dc, clopts);
  //dc.cout() << "Loading graph in format: "<< format << std::endl;
  graph.load(graph_dir, line_parser);
  // must call finalize before querying the graph
  graph.finalize();

  // Initialize the vertex data
  graph.transform_vertices(init_vertex);

  timer ti;
  warp_engine_type engine(dc, graph, clopts);
  engine.signal_all();
  engine.set_update_function(pagerank);
  engine.start();

  dc.cout() << "Finished Running in " << ti.current_time()
            << " seconds." << std::endl;

  dc.cout() << "counter is: " << counter << std::endl;

  // Save the final graph -----------------------------------------------------
  if (saveprefix != "") {
    graph.save(saveprefix, pagerank_writer(),
               false,    // do not gzip
               true,     // save vertices
               false);   // do not save edges
  }

  mpi_tools::finalize();
} // End of main




