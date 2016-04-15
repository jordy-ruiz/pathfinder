/*
 * General analysis methods
 */

#include <ctime> // clock
#include <cmath> // sqrt
#include <iostream> // std::cout
#include <iomanip> // std::setprecision
#include <otawa/cfg/Edge.h>
#include "analysis_state.h"
#include "cfg_features.h"

/**
 * @class Analysis
 * @brief Perform an infeasible path analysis on a CFG 
 */
Analysis::Analysis(const context_t& context, int state_size_limit, int flags)
	: context(context), state_size_limit(state_size_limit), flags(flags)
	, ip_count(0), unminimized_ip_count(0)
	, loop_header_count(0), bb_count(-1) { }

const Vector<DetailedPath>& Analysis::run(CFG *cfg)
{
	bb_count = cfg->count()-1; // do not count ENTRY
	std::time_t timestamp = clock();
	// DBG("Using SMT solver: " << (flags&DRY_RUN ? "(none)" : SMT::printChosenSolverInfo()))
	DBG("Stack pointer identified to " << context.sp)
	processCFG(cfg);
	printResults((clock()-timestamp)*1000/CLOCKS_PER_SEC);
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
void Analysis::debugProgress(int block_id, bool enable_smt) const
{
	if(dbg_verbose >= DBG_VERBOSE_RESULTS_ONLY && (dbg_flags&DBG_PROGRESS))
	{
		static int processed_bbs = 0;
		if(enable_smt)
			++processed_bbs; // only increase processed_bbs when we are in a state where we are no longer looking for a fixpoint
		cout << "[" << processed_bbs*100/bb_count << "%] Processed Block #" << block_id << " of " << bb_count << "        " << endl << "\e[1A";
	}
}

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
	if(!wl.contains(b))
	{
		wl.push(b);
		// DBGG("-\twl ← wl ∪ " << b)
	}
}

/**
  * @fn inline static loopheader_status_t Analysis::loopStatus(Block* h);
  * @brief Give the loop status of a Block
  * @param h Block to examinate
  * @rtn The loop status
  */

/**
 * @fn Block* Analysis::insAlias(Block* b);
 * @brief Substitue a block with the appropriate block to get ingoing edges from
 * @rtn Block to substitute b with (by default, b)
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
 * @rtn return the vector of selected edges
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
 * @rtn return the vector of selected edges
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
 * @rtn return the vector of selected edges
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
		DBGG("-\toutput to " << i->source())
		for(; i; i++)
			DBGG("\t\t  ->" << i->target())
	}
	return rtn;
}

/**
 * @brief Short display of the fixpoint status of the current and enclosing loops (including caller CFGs)
 * @param b The block to process
 * @rtn The String contain the output
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

elm::String Analysis::wlToString() const
{
	elm::String rtn = "[";
	bool first = true;
	for(Vector<Block*>::Iterator iter(wl); iter; iter++)
	{
		if(first) first = false; else
			rtn = rtn.concat((CString)", ");
		// rtn = _ << rtn << (*iter)->id();
		rtn = _ << rtn << (*iter)->cfg() << ":" << (*iter)->index();
	}
	rtn = rtn.concat((CString)"]");
	return rtn;
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
 * @brief Print results after a CFG analysis completes
 * 
 * @param exec_time_ms Measured execution time of the analysis (in ms)
 */
void Analysis::printResults(int exec_time_ms) const
{
	if(dbg_verbose == DBG_VERBOSE_NONE)
		return;
	int infeasible_paths_count = infeasible_paths.count();
	if(dbg_flags&DBG_NO_TIME)
		DBG(color::BIGre() << infeasible_paths_count << " infeasible path" << (infeasible_paths_count == 1 ? "" : "s") << " found: ")
	else
		DBG(color::BIGre() << infeasible_paths_count << " infeasible path" << (infeasible_paths_count == 1 ? "" : "s") << " found: "
			<< "(" << (exec_time_ms>=1000 ? ((float)exec_time_ms)/(float(100)) : exec_time_ms) << (exec_time_ms>=1000 ? "s" : "ms") << ")")
	for(Vector<DetailedPath>::Iterator iter(infeasible_paths); iter; iter++)
	{
		if(dbg_verbose == DBG_VERBOSE_ALL)
			DBG(color::IGre() << "    * [" << *iter << "]")
		else if(dbg_verbose < DBG_VERBOSE_NONE)
			cout << "    * [" << *iter << "]" << endl;
	}
	if(dbg_verbose > DBG_VERBOSE_ALL && dbg_verbose < DBG_VERBOSE_NONE)
	{
		cout << color::BIGre() << infeasible_paths_count << color::RCol() << " infeasible path(s) found.";
		if(!(dbg_flags&DBG_NO_TIME))
		{
		    std::ios_base::fmtflags oldflags = std::cout.flags();
		    std::streamsize oldprecision = std::cout.precision();
			std::cout << std::fixed << std::setprecision(3) << color::IYel().chars() << " (" << ((float)exec_time_ms)/1000.f << "s)" << color::RCol().chars() << std::endl;
		    std::cout.flags(oldflags);
		    std::cout.precision(oldprecision);
		}
		else
			cout << endl;
	}
	cout << "Minimized+Unminimized => Total w/o min. : " << color::On_Bla() << color::IGre() << infeasible_paths_count-unminimized_ip_count << color::RCol() <<
			"+" << color::Yel() << unminimized_ip_count << color::RCol() << " => " << color::IRed() << ip_count << color::RCol() << endl;
	if(dbg_flags&DBG_AVG_IP_LENGTH && infeasible_paths_count > 0)
	{
		int sum_path_lengths = 0, squaredsum_path_lengths = 0;
		for(Vector<DetailedPath>::Iterator iter(infeasible_paths); iter; iter++)
		{
			sum_path_lengths += iter->countEdges();
			squaredsum_path_lengths += iter->countEdges() * iter->countEdges();
		}
		float average_length = (float)sum_path_lengths / (float)infeasible_paths_count;
		float norm2 = sqrt((float)squaredsum_path_lengths / (float)infeasible_paths_count);
	    std::ios_base::fmtflags oldflags = std::cout.flags();
	    std::streamsize oldprecision = std::cout.precision();
		std::cout << std::fixed << std::setprecision(2) << " (Average: " << average_length << ", Norm2: " << norm2 << ")" << endl;
		std::cout.flags(oldflags);
		std::cout.precision(oldprecision);
	}
}
