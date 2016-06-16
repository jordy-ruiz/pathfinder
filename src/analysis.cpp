/*
 * General analysis methods
 */

#include <ctime> // clock
#include <cmath> // sqrt
#include <iostream> // std::cout
#include <iomanip> // std::setprecision
#include <otawa/cfg/Edge.h>
#include <otawa/hard/Platform.h>
#include <otawa/cfg/features.h> // COLLECTED_CFG_FEATURE
#include <otawa/dfa/State.h> // INITIAL_STATE_FEATURE
#include <otawa/prog/WorkSpace.h>
#include <sys/time.h>
#ifdef OSLICE
	#include <oslice_features.h>
#endif
#include "analysis_state.h"
#include "cfg_features.h"
#include "GlobalDominance.h"
#include "progress.h"
#include "smt.h"
#include "GlobalDominance.h"

/**
 * @class Analysis
 * @brief Perform an infeasible path analysis on a CFG 
 */
Analysis::Analysis(WorkSpace *ws, PropList &props, int flags, int merge_thresold, int nb_cores)
	: state_size_limit(merge_thresold), nb_cores(nb_cores), flags(flags)
{
	ws->require(dfa::INITIAL_STATE_FEATURE, props); // dfa::INITIAL_STATE
	ws->require(COLLECTED_CFG_FEATURE, props); // INVOLVED_CFGS
	// MyTransformer t; t.process(ws);
	if(flags & VIRTUALIZE_CFG)
		ws->require(VIRTUALIZED_CFG_FEATURE, props); // inline calls
	if(flags & SLICE_CFG) {
		// oslice::SLICING_CFG_OUTPUT_PATH(props) = "slicing.dot";
		// oslice::SLICED_CFG_OUTPUT_PATH(props) = "sliced.dot";
#		ifdef OSLICE_PATH
			ws->require(oslice::COND_BRANCH_COLLECTOR_FEATURE, props);
			ws->require(oslice::SLICER_FEATURE, props);
#		else
			elm::cerr << color::IYel() << "WARNING: slicing unavailable. Rebuild with -D OSLICE_PATH=[path to oslice]" << color::RCol() << endl;
#		endif
	}
	gdom = new GlobalDominance(INVOLVED_CFGS(ws), GlobalDominance::EDGE_DOM | GlobalDominance::EDGE_POSTDOM); // no block dom
	ws->require(LOOP_HEADERS_FEATURE, props); // LOOP_HEADER, BACK_EDGE
	ws->require(LOOP_INFO_FEATURE, props); // LOOP_EXIT_EDGE

	this->context.dfa_state = dfa::INITIAL_STATE(ws); // initial state
	this->context.sp = ws->platform()->getSP()->number(); // id of the stack pointer
	this->context.max_tempvars = (unsigned int)ws->process()->maxTemp(); // maximum number of tempvars used
	this->context.max_registers = (unsigned int)ws->platform()->regCount(); // count of registers
}

Analysis::~Analysis()
{
	delete gdom;
}

/**
  * @fn const Vector<DetailedPath>& Analysis::run(const WorkSpace* ws);
  * @brief Run the analysis on the main CFG
  */
const Vector<DetailedPath>& Analysis::run(const WorkSpace* ws)
{
	ASSERTP(INVOLVED_CFGS(ws)->count() > 0, "no CFG found"); // make sure we have at least one CFG
	return run(INVOLVED_CFGS(ws)->get(0));
}

/**
  * @fn const Vector<DetailedPath>& Analysis::run(CFG *cfg);
  * @brief Run the analysis on a specific CFG
  */
const Vector<DetailedPath>& Analysis::run(CFG *cfg)
{
	if(flags&SHOW_PROGRESS) progress = new Progress(cfg);
	DBG("Using SMT solver: " << (flags&DRY_RUN ? "(none)" : SMT::printChosenSolverInfo()))
	DBG("Stack pointer identified to " << context.sp)

    struct timeval tim;
	std::time_t start = clock();
	sw.start();
    gettimeofday(&tim, NULL);
    t::int64 t1 = tim.tv_sec*1000000+tim.tv_usec;
	
	processCFG(cfg);

    gettimeofday(&tim, NULL);
    t::int64 t2 = tim.tv_sec*1000000+tim.tv_usec;
	sw.stop();
	std::time_t end = clock();
	
	postProcessResults(cfg);
	printResults((end-start)*1000/CLOCKS_PER_SEC, (t2-t1)/1000);
	if(flags&SHOW_PROGRESS) delete progress;
	return infeasiblePaths();
}

/**
 * @fn Vector<DetailedPath> Analysis::infeasiblePaths();
 * @brief Retrieve the vector of infeasible paths generated by the analysis
 */

/*
 * @fn void Analysis::debugProgress(int block_id, bool enable_smt) const;
 * Print progress of analysis
 */
/*void Analysis::debugProgress(int block_id, bool enable_smt) const
{
	if(dbg_verbose >= DBG_VERBOSE_RESULTS_ONLY && (dbg_flags&DBG_PROGRESS))
	{
		static int processed_bbs = 0;
		if(enable_smt)
			++processed_bbs; // only increase processed_bbs when we are in a state where we are no longer looking for a fixpoint
		cout << "[" << processed_bbs*100/bb_count << "%] Processed Block #" << block_id << " of " << bb_count << "        " << endl << "\e[1A";
	}
}*/

/*
 * @fn void Analysis::wl_push(Block* b);
 * @brief push b in wl, ensuring unicity in wl
 */
void Analysis::wl_push(Block* b)
{
	ASSERTP(!b->isUnknown(), "Block " << b << " is unknown, not supported by analysis.");
	if(b->isCall()) 
		b = b->toSynth()->callee()->entry(); // call becomes callee entry
	if(b->isExit())
		b = getCaller(b, b); // exit becomes caller (remains exit if no caller)
	wl.push(b);
	// DBGG("-\twl ← wl ∪ " << b)
}

/**
  * @fn inline static loopheader_status_t Analysis::loopStatus(Block* h);
  * @brief Give the loop status of a Block
  * @param h Block to examinate
  * @return The loop status
*/

/**
  * @fn Block* Analysis::insAlias(Block* b);
  * @brief Substitue a block with the appropriate block to get ingoing edges from
  * @return The Block to substitute b with (by default, b)
*/
Block* Analysis::insAlias(Block* b)
{
	if(b->isEntry()) // entry becomes caller
	{
		Option<Block*> rtn = getCaller(b->cfg());
		ASSERTP(rtn, "insAlias called on main entry - no alias with ins exists")
		return rtn;
	}
	else if(b->isCall()) // call becomes exit
		return b->toSynth()->callee()->exit();
	return b;
}
/**
 * @fn static Vector<Edge*> Analysis::allIns (Block* h);
 * @brief Collect all edges pointing to a block
 * @param h Block to collect incoming edges from
 * @return return the vector of selected edges
 */
Vector<Edge*> Analysis::allIns(Block* h)
{
	Vector<Edge*> rtn(4);
	for(Block::EdgeIter i(insAlias(h)->ins()); i; i++)
		rtn.push(*i);
	if(dbg_verbose < DBG_VERBOSE_RESULTS_ONLY) cout << endl;
	DBGG("-" << color::ICya() << h << color::RCol() << " " << printFixPointStatus(h))
	DBG("collecting allIns...")
	return rtn;
}
/**
 * @fn static Vector<Edge*> Analysis::backIns(Block* h);
 * @brief Collect all back-edges pointing to a block
 * @param h Block to collect incoming edges from
 * @return return the vector of selected edges
 */
Vector<Edge*> Analysis::backIns(Block* h)
{
	Vector<Edge*> rtn(4);
	for(Block::EdgeIter i(insAlias(h)->ins()); i; i++)
		if(BACK_EDGE(*i))
			rtn.push(*i);
	if(dbg_verbose < DBG_VERBOSE_RESULTS_ONLY) cout << endl;
	DBGG("-" << color::ICya() << h << color::RCol() << " " << printFixPointStatus(h))
	DBG("collecting backIns...")
	return rtn;
}
/**
 * @fn static Vector<Edge*> Analysis::nonBackIns(Block* h);
 * @brief Collect all edges pointing to a block that are not back edges of a loop
 * @param h Block to collect incoming edges from
 * @return return the vector of selected edges
 */
Vector<Edge*> Analysis::nonBackIns(Block* h)
{
	Vector<Edge*> rtn(4);
	for(Block::EdgeIter i(insAlias(h)->ins()); i; i++)
		if(!BACK_EDGE(*i))
			rtn.push(*i);
	if(dbg_verbose < DBG_VERBOSE_RESULTS_ONLY) cout << endl;
	DBGG("-" << color::ICya() << h << color::RCol() << " " << printFixPointStatus(h))
	DBG("collecting nonBackIns...")
	return rtn;
}

/**
  * @brief check that all the loops this exits from are "LEAVE" status
  * aka e ∈ exits\{EX_h | src(e) ∈ L_h ∧ status_h ≠ LEAVE}
*/
bool Analysis::isAllowedExit(Edge* exit_edge)
{
	Block* outer_lh = LOOP_EXIT_EDGE(exit_edge);
	for(LoopHeaderIter lh(exit_edge->source()); lh; lh++)
	{
		if(loopStatus(lh) != LEAVE)
			return false;
		if(lh == outer_lh) // stop here
			break;	
	}
	return true;
}

/* for e ∈ outs \ {EX_h | b ∈ L_h ∧ status_h ≠ LEAVE} */
Vector<Edge*> Analysis::outsWithoutUnallowedExits(Block* b)
{
	if(b->isExit()) {
		DBGG(color::IGre() << "Reached end of program.")
		return nullVector<Edge*>();
	}
	Vector<Edge*> rtn(4);
	for(Block::EdgeIter i(b->outs()); i; i++)
		if(! LOOP_EXIT_EDGE.exists(*i) || isAllowedExit(*i))
			rtn.push(*i);
	ASSERTP(rtn, "outsWithoutUnallowedExits found no outs!")
	if(dbg_verbose < DBG_VERBOSE_RESULTS_ONLY)
	{
		Vector<Edge*>::Iterator i(rtn);
		// DBGG("-\toutput to " << i->source())
		for(; i; i++)
#ifndef NO_UTF8
			DBGG(color::Bold() << "\t\t└▶" << color::RCol() << i->target())
#else
			DBGG(color::Bold() << "\t\t->" << color::RCol() << i->target())
#endif
	}
	return rtn;
}

/**
 * @fn String Analysis::printFixPointStatus(Block* b);
  * @brief Short display of the fixpoint status of the current and enclosing loops (including caller CFGs)
  * @param b The block to process
  * @return The String contain the output
  */
String Analysis::printFixPointStatus(Block* b)
{
	String rtn = "[";
	for(LoopHeaderIter i(b); i; i++)
	{
		switch(loopStatus(*i))
		{
			case ENTER:
				rtn = rtn.concat(color::IRed() + "E");
			break;
			case FIX:
				rtn = rtn.concat(color::Yel() + "F");
			break;
			case LEAVE:
				rtn = rtn.concat(color::IGre() + "L");
			break;
		}
	}
	return rtn + color::RCol() + "]";
}


/**
 * @fn static bool Analysis::isSubPath(const OrderedPath& included_path, const Path& path_set);
 * @brief Checks if 'included_path' is a part of the set of paths "path_set", that is if 'included_path' includes all the edges in the Edge set of path_set
 * @return true if it is a subpath
*/
bool Analysis::isSubPath(const OrderedPath& included_path, const Path& path_set) 
{
	for(Path::Iterator iter(path_set); iter; iter++)
		if(!included_path.contains(*iter))
			return false;
	return true;
}

/**
 * @brief Get pretty printing for any unordered Path (Set of Edge*)
 * 
 * @param path Path to parse
 * @return String representing the path
 */
elm::String Analysis::pathToString(const Path& path)
{
	elm::String str = "[";
	bool first = true;
	for(Analysis::Path::Iterator iter(path); iter; iter++)
	{
		if(first)
			first = false;
		else
			str = str.concat(_ << ", ");
		str = str.concat(_ << (*iter)->source()->cfg() << ":" << (*iter)->source()->index() << "->" << (*iter)->target()->cfg() << ":" << (*iter)->target()->index());
	}
	str = str.concat(_ << "]");
	return str;
}

/**
 * @brief Get pretty printing for any OrderedPath (SLList of Edge*)
 * 
 * @param path OrderedPath to parse
 * @return String representing the path
 */
elm::String Analysis::orderedPathToString(const OrderedPath& path)
{
	elm::String str;
	bool first = true;
	int lastid = 0; // -Wmaybe-uninitialized
	for(OrderedPath::Iterator iter(path); iter; iter++)
	{
		ASSERTP(first || (*iter)->source()->index() == lastid, "OrderedPath previous target and current source do not match! ex: 1->2, 2->4, 3->5");
		if(first)
		{
#ifndef NO_UTF8
			if((*iter)->source()->index() == 0)
				str = _ << "ε";
			else
#endif
				str = _ << (*iter)->source()->index();
			first = false;
		}
		str = _ << str << "->" << (*iter)->target()->index();
		lastid = (*iter)->target()->index();
	}
	if(str.isEmpty())
		return "(empty)";
	return str;
}

/**
 * @fn void Analysis::printResults(int exec_time_ms) const;
 * @brief Print results after a CFG analysis completes
 * @param exec_time_ms Measured execution time of the analysis (in ms)
 */
void Analysis::printResults(int exec_time_ms, int real_time_ms) const
{
	if(dbg_verbose == DBG_VERBOSE_NONE)
		return;
	const int infeasible_paths_count = infeasible_paths.count();
	if(dbg_verbose == DBG_VERBOSE_ALL)
	{
		if(dbg_flags&DBG_NO_TIME)
			DBG(color::BIGre() << infeasible_paths_count << " infeasible path" << (infeasible_paths_count == 1 ? "" : "s") << " found: ")
		else
			DBG(color::BIGre() << infeasible_paths_count << " infeasible path" << (infeasible_paths_count == 1 ? "" : "s") << " found: "
				<< "(" << (real_time_ms>=1000 ? ((float)real_time_ms)/(float(1000)) : real_time_ms) << (real_time_ms>=1000 ? "s" : "ms") << ")")
		if(dbg_flags&DBG_RESULT_IPS)
			for(Vector<DetailedPath>::Iterator iter(infeasible_paths); iter; iter++)
				DBG(color::IGre() << "    * [" << *iter << "]")
	}
	else // not all verbose
	{
		if(dbg_flags&DBG_RESULT_IPS)
			for(Vector<DetailedPath>::Iterator iter(infeasible_paths); iter; iter++)
				cout << "    * [" << *iter << "]" << endl;
		cout << color::BIGre() << infeasible_paths_count << color::RCol() << " infeasible path(s) found.";
		if(! (dbg_flags&DBG_NO_TIME))
		{
		    std::ios_base::fmtflags oldflags = std::cout.flags();
		    std::streamsize oldprecision = std::cout.precision();
			std::cout << std::fixed << std::setprecision(3) << color::IYel().chars() << " (" << real_time_ms/1000.f << "s)" << color::RCol().chars();
		    if(dbg_flags&DBG_DETAILED_STATS)
				std::cout << color::Yel().chars() << " [" << sw.delay()/1000000.f << " of " << exec_time_ms/1000.f << "s]" << color::RCol().chars();
		    std::cout.flags(oldflags);
		    std::cout.precision(oldprecision);
			std::cout << endl;
		}
		else elm::cout << endl;
	}
	cout << "Minimized+Unminimized => Total w/o min. : " << color::On_Bla() << color::IGre() << infeasible_paths_count-ip_stats.getUnminimizedIPCount() << color::RCol() <<
			"+" << color::Yel() << ip_stats.getUnminimizedIPCount() << color::RCol() << " => " << color::IRed() << ip_stats.getIPCount() << color::RCol() << endl;
	if(dbg_flags&DBG_DETAILED_STATS && infeasible_paths_count > 0)
	{
		int sum_path_lengths = 0, squaredsum_path_lengths = 0, one_edges = 0;
		for(Vector<DetailedPath>::Iterator iter(infeasible_paths); iter; iter++)
		{
			one_edges += iter->countEdges() == 1;
			sum_path_lengths += iter->countEdges();
			squaredsum_path_lengths += iter->countEdges() * iter->countEdges();
		}
		float average_length = (float)sum_path_lengths / (float)infeasible_paths_count;
		float norm2 = sqrt((float)squaredsum_path_lengths / (float)infeasible_paths_count);
	    std::ios_base::fmtflags oldflags = std::cout.flags();
	    std::streamsize oldprecision = std::cout.precision();
		std::cout << std::fixed << std::setprecision(2) << " (Average: " << average_length << ", Norm2: " << norm2
			<< ", #1edge: " << one_edges << "/" << infeasible_paths_count << ")" << endl;
		std::cout.flags(oldflags);
		std::cout.precision(oldprecision);
	}
}

// returns edge to remove
Option<Edge*> f_dom(GlobalDominance* gdom, Edge* e1, Edge* e2) {
	DBG("\tdom(" << e1 << ", " << e2 << "): " << DBG_TEST(gdom->dom(e1, e2), false))
	if(gdom->dom(e1, e2))
		return elm::some(e1);
	return elm::none;
}
// returns edge to remove
Option<Edge*> f_postdom(GlobalDominance* gdom, Edge* e1, Edge* e2) {
	DBG("\tpostdom(" << e2 << ", " << e1 << "): " << DBG_TEST(gdom->postdom(e2, e1), false))
	if(gdom->postdom(e2, e1))
		return elm::some(e2);
	return elm::none;
}

int Analysis::simplifyUsingDominance(Option<Edge*> (*f)(GlobalDominance* gdom, Edge* e1, Edge* e2))
{
	int changed_count = 0;
	for(Vector<DetailedPath>::MutableIterator dpiter(infeasible_paths); dpiter; dpiter++)
	{
		DetailedPath& dp = dpiter.item();
		DBG(dp << "...")
		bool hasChanged = false, changed = false;
		do
		{
			changed = false;
			elm::Option<DetailedPath::FlowInfo> prev = elm::none;
			for(DetailedPath::Iterator i(dp); i; i++)
			{
				if(i->isEdge())
				{
					if(prev)
						if(Option<Edge*> edge_to_remove = f(gdom, (*prev).getEdge(), i->getEdge()))
						{
							dp.remove(edge_to_remove); // search and destroy
							changed = true;
							hasChanged = true;
							break;
						}
					prev = elm::some(*i);
				}
			}
		} while(changed);
		if(hasChanged) {
			dp.removeCallsAtEndOfPath();
			DBG("\t...to " << dp)
			changed_count++;
		}
	}
	return changed_count;
}

void Analysis::postProcessResults(CFG *cfg)
{
	if(! flags&POST_PROCESSING)
		return;
	DBG(color::On_IGre() << "post-processing..." << color::RCol())
	// elm::log::Debug::setDebugFlag(true);
	// elm::log::Debug::setVerboseLevel(1);
	/*otawa::Edge* program_entry_edge = theOnly(cfg->entry()->outs());
	for(Vector<DetailedPath>::MutableIterator dpiter(infeasible_paths); dpiter; dpiter++)
	{
		DBG("\tcontains entry: " << DBG_TEST(dp.contains(program_entry_edge), false))
		if(dp.contains(program_entry_edge))
			dp.remove(program_entry_edge);
	}*/ // should be removed by dominance anyway
	int changed_count = simplifyUsingDominance(&f_dom);
	DBGG("Dominance: minimized " << changed_count << " infeasible paths.")
	changed_count = simplifyUsingDominance(&f_postdom);
	DBGG("Post-dominance: minimized " << changed_count << " infeasible paths.")

	/*int changed_count = 0;
	for(Vector<DetailedPath>::MutableIterator dpiter(infeasible_paths); dpiter; dpiter++)
	{
		DetailedPath& dp = dpiter.item();
		DBG(dp << "...")
		bool hasChanged = false, changed = false;
		do
		{
			changed = false;
			elm::Option<DetailedPath::FlowInfo> prev = elm::none;
			for(DetailedPath::Iterator i(dp); i; i++)
			{
				if(i->isEdge())
				{
					if(prev)
					{
						DBG("\tdom(" << (*prev).getEdge() << ", " << i->getEdge() << "): " << 
							DBG_TEST(gdom->dom((*prev).getEdge(), i->getEdge()), false))
						if(gdom->dom((*prev).getEdge(), i->getEdge()))
						{
							dp.remove((*prev).getEdge()); // search and destroy
							changed = true;
							hasChanged = true;
							break;
						}
					}
					prev = elm::some(*i);
				}
			}
		} while(changed);
		if(hasChanged)
		{
			dp.optimize();
			DBG("\t...to " << dp)
			changed_count++;
		}
	}*/
}
