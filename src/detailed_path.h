#ifndef _INFEASIBLE_PATH_H
#define _INFEASIBLE_PATH_H

#include <elm/genstruct/SLList.h> 
#include <elm/genstruct/Vector.h>
#include <elm/string/String.h>
#include <otawa/cfg/CFG.h>
#include <otawa/cfg/Edge.h>
#include <elm/util/Option.h>

using elm::genstruct::SLList;
using elm::genstruct::Vector;
using otawa::Edge;
using otawa::Block;
using otawa::BasicBlock;
using otawa::SynthBlock;

class DetailedPath
{
public:
	class FlowInfo;
	class Iterator;

	DetailedPath();
	DetailedPath(const SLList<Edge*>& edge_list);
	DetailedPath(const DetailedPath& dp);
	
	// SLList methods
	inline void clear() { _path.clear(); }
	void addLast(Edge* e);
	inline void addLast(const FlowInfo& fi) { _path.addLast(fi); }
	inline bool contains(const FlowInfo &fi) const { return _path.contains(fi); }
	inline void remove(Iterator &iter) { _path.remove(iter); }
	// inline void remove(Iterator &iter) { _path.remove(iter.getFlowInfoIter()); }
	inline void removeLast() { _path.removeLast(); }
	
	// events
	void onLoopEntry(Block* loop_header);
	void onLoopExit(Option<Block*> new_loop_header);
	void onCall(otawa::SynthBlock* sb);
	void onReturn(otawa::SynthBlock* sb);

	// utility
	bool weakEqualsTo(const DetailedPath& dp) const;
	void addEnclosingLoop(Block* loop_header);
	void merge(const Vector<DetailedPath>& detailed_paths);
	void optimize();
	bool hasAnEdge() const;
	Edge* firstEdge() const;
	Edge* lastEdge() const;
	int countEdges() const;
	SLList<Edge*> toOrderedPath() const;
	elm::String toString(bool colored = true) const;
	inline const SLList<FlowInfo>& path() const { return _path; }
	inline bool operator==(const DetailedPath& dp) const { return _path == dp._path; }
	inline const DetailedPath* operator->(void) const { return this; }
	friend io::Output& operator<<(io::Output& out, const DetailedPath& dp) { return dp.print(out); }

	// FlowInfo class
	class FlowInfo
	{
	public:
		enum kind_t
		{
			KIND_EDGE, // Edge
			KIND_LOOP_ENTRY, // BasicBlock
			KIND_LOOP_EXIT, // BasicBlock
			KIND_CALL, // SynthBlock
			KIND_RETURN, // SynthBlock
		};
		FlowInfo(kind_t kind, BasicBlock* bb) : _kind(kind), _identifier(bb) { ASSERT(isBasicBlockKind(kind)); }
		FlowInfo(kind_t kind, SynthBlock* sb) : _kind(kind), _identifier(sb) { ASSERT(isSynthBlockKind(kind)); }
		FlowInfo(kind_t kind, Edge* e) : _kind(kind), _identifier(e) { ASSERT(isEdgeKind(kind)); }
		FlowInfo(const FlowInfo& fi) { _kind = fi._kind; _identifier = fi._identifier; }
		inline bool isEdge() const { return _kind == KIND_EDGE; }
		inline bool isLoopEntry() const { return _kind == KIND_LOOP_ENTRY; }
		inline bool isLoopExit() const { return _kind == KIND_LOOP_EXIT; }
		inline bool isCall() const { return _kind == KIND_CALL; }
		inline bool isReturn() const { return _kind == KIND_RETURN; }
		inline Edge* getEdge() const { ASSERT(isEdgeKind(_kind)); return (Edge*)_identifier; } // TODO! remove the ASSERTs?
		inline BasicBlock* getBasicBlock() const { ASSERT(isBasicBlockKind(_kind)); return (BasicBlock*)_identifier; }
		inline SynthBlock* getSynthBlock() const { ASSERT(isSynthBlockKind(_kind)); return (SynthBlock*)_identifier; }
		inline BasicBlock* getLoopHeader() const { return getBasicBlock(); }
		inline SynthBlock* getCaller() const { return getSynthBlock(); }
		elm::String toString(bool colored = true) const;
		inline bool operator==(const FlowInfo& fi) const { return (_kind == fi._kind) && (_identifier == fi._identifier); }
		inline FlowInfo& operator=(const FlowInfo& fi) { _kind = fi._kind; _identifier = fi._identifier; return *this; }
		inline const FlowInfo* operator->(void) const { return this; }
		friend io::Output& operator<<(io::Output& out, const FlowInfo& fi) { return fi.print(out); }

	private:
		inline bool isBasicBlockKind(kind_t kind) const
			{ return (kind == KIND_LOOP_ENTRY) || (kind == KIND_LOOP_EXIT); }
		inline bool isSynthBlockKind(kind_t kind) const
			{ return (kind == KIND_CALL) || (kind == KIND_RETURN); }
		inline bool isEdgeKind(kind_t kind) const
			{ return (kind == KIND_EDGE); }
		io::Output& print(io::Output& out) const;

		kind_t _kind;
		void* _identifier; // BasicBlock*, Edge*
	}; // FlowInfo class


	// EdgeIterator class
	class EdgeIterator: public PreIterator<EdgeIterator, Edge*> {
	public:
		inline EdgeIterator(const DetailedPath& dpath) : dpath_iter(dpath._path), done(false) { goToNextEdge(); }
		inline EdgeIterator(const EdgeIterator& source): dpath_iter(source.dpath_iter), done(false) { goToNextEdge(); }
		inline EdgeIterator& operator=(const EdgeIterator& source) { dpath_iter = source.dpath_iter; return *this; }

		inline bool ended(void) const { return done == true; }
		inline Edge* item(void) const { return dpath_iter.item().getEdge(); }
		inline void next(void) { dpath_iter++; goToNextEdge(); }

	private:
		// this is the only method of this class that doesn't assume we are positionned on a "valid state" (that is, done || dpath_iter->isEdge())
		void goToNextEdge() {
			while(!dpath_iter.ended())
			{
				if((*dpath_iter).isEdge())
					return; // we still have an edge to read
				dpath_iter++;
			}
			done = true; // only non-edges
		}

		SLList<FlowInfo>::Iterator dpath_iter;
		bool done;
	}; // EdgeIterator class

	// Iterator class
	class Iterator: public SLList<FlowInfo>::Iterator {
	public:
		inline Iterator(const DetailedPath& dpath) : SLList<FlowInfo>::Iterator(dpath._path) { }
	};

	/*
	// old Iterator class
	class Iterator: public PreIterator<Iterator, const FlowInfo&> {
	public:
		// inline Iterator() { }
		inline Iterator(const DetailedPath& dpath) : _iter(dpath._path) { }
		inline Iterator(const Iterator& iter) : _iter(iter._iter) { }

		inline bool ended(void) const { return _iter.ended(); }
		inline const FlowInfo& item(void) const { return _iter.item(); }
		inline void next(void) { _iter.next(); }

		// inline const SLList<FlowInfo>::Iterator& getFlowInfoIter(void) { return _iter; }
	private:
		SLList<FlowInfo>::Iterator _iter;
	}; // Iterator class
	*/

private:
	void removeDuplicates();
	void removeAntagonists();
	void removeCallsAtEndOfPath();
	io::Output& print(io::Output& out) const;
	
	SLList<FlowInfo> _path;
}; // DetailedPath class

#endif