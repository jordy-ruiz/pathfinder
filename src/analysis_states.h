/*
 *	
 *
 *	This file is part of OTAWA
 *	Copyright (c) 2006-2018, IRIT UPS.
 * 
 *	OTAWA is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	OTAWA is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with OTAWA; if not, write to the Free Software 
 *	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
 
#ifndef _ANALYSIS_STATES_H
#define _ANALYSIS_STATES_H

#include "analysis_state.h"
#include "loop_bound.h"

class Analysis::States : public elm::Lock {
public:
	class Iter;
	inline States() { }
	inline States(int cap) : s(cap) { }
	inline States(const Vector<Analysis::State>& state_vector) : s(state_vector) { }
	inline States(const States& ss) : s(ss.s) { }
	// return the unique state, or bottom if none. it is an error to call this when s.count() > 1
	inline const State& one() const { ASSERT(s.count() <= 1); return s ? s.first() : bottom; }
	// inline State& one() { ASSERT(s.count() <= 1); return s ? s.first() : bottom; }
	inline bool isEmpty() const { return s.isEmpty(); }
	inline int count() const { return s.count(); }
	inline const State& first() const { return s.first(); }
	inline State& first() { return s[0]; }
	inline const Vector<State>& states() const { return s; }
	inline Vector<State>& states() { return s; }
	inline void push(const Analysis::State& state) { return s.push(state); }
	inline void remove(const Iter& i) { s.remove(i); }

	void apply(const States& ss, VarMaker& vm, bool local_sp, bool dbg = true, bool clear_path = false);
	void appliedTo(const State& s, VarMaker& vm);
	void minimize(VarMaker& vm, bool clean) const;
	inline void removeTautologies(void) 			{ for(Iter i(this->s); i; i++) s[i].removeTautologies(); }
	inline void prepareFixPoint(void) 				{ for(Iter i(this->s); i; i++) s[i].prepareFixPoint(); }
	void finalizeLoop(OperandIter* n, VarMaker& vm)	{ for(Iter i(this->s); i; i++) s[i].finalizeLoop(n, vm); }
	inline void widening(const Operand* n) { ASSERT(s.count() <= 1); if(s.count() == 1) s[0].widening(n); } // needs max one state
	void printLoopBoundOf(const Operand *oi) const;
	void checkForSatisfiableSP() const;

	inline void clearPath() { for(Iter i(this->s); i; i++) s[i].clearPath(); }
	inline void resetSP()   { for(Iter i(this->s); i; i++) s[i].resetSP(); }
	inline void onCall(SynthBlock* sb)   { for(Iter i(this->s); i; i++) s[i].onCall(sb); }
	inline void onReturn(SynthBlock* sb) { for(Iter i(this->s); i; i++) s[i].onReturn(sb); }
	inline void onLoopExit (Block* b)    { for(Iter i(this->s); i; i++) s[i].onLoopExit(b); }
	// inline void onLoopEntry(Block* b)    { for(Iter i(this->s); i; i++) s[i].onLoopEntry(b); }
	void onLoopExit (Edge* e) {
		Block* h = LOOP_EXIT_EDGE(e);
		for(LoopHeaderIter i(e->source()); i.item() != h; i++) 
			onLoopExit(*i);
		onLoopExit(h);
	}

	// inline operator Vector<State>() { return s; }
	inline State& operator[](int i) { return s[i]; }
	inline State& operator[](const Iter& i) { return s[i]; }
	inline States& operator=(const Vector<State>& sv) { s = sv; return *this; }
	inline States& operator=(const States& ss) { s = ss.s; return *this; }
	inline friend io::Output& operator<<(io::Output& out, const States& ss) { return out << ss.s; };
	elm::String dump() const {
		elm::String rtn;
		for(Iter i(this->s); i; i++)
			rtn = rtn.concat(i->dumpEverything());
		return rtn;
	};

	class Iter: public Vector<State>::Iter {
	public:
		Iter(const States& ss) : Vector<State>::Iter(ss.s) { }
		Iter(const Vector<State>& sv) : Vector<State>::Iter(sv) { }
	};

private:
	Vector<State> s;
};

#endif