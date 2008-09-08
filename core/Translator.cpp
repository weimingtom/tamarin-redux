/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is [Open Source Virtual Machine.].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2004-2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adobe AS3 Team
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "avmplus.h"

namespace avmplus
{
#ifdef AVMPLUS_WORD_CODE
	using namespace MMgc;
	
	class TranslatedCode : public GCObject
	{
	public:
		uint32 data[1];  // more follows
	};

#ifdef AVMPLUS_DIRECT_THREADED
	Translator::Translator(MethodInfo* info, void** opcode_labels)
#else
	Translator::Translator(MethodInfo* info)
#endif
		: info(info)
		, backpatches(NULL)
		, labels(NULL)
		, exception_fixes(NULL)
		, buffers(NULL)
		, buffer_offset(0)
		, spare_buffer(NULL)
#ifdef AVMPLUS_DIRECT_THREADED
		, opcode_labels(opcode_labels)
#endif
		, exceptions_consumed(false)
		, dest(NULL)
		, dest_limit(NULL)
		, pool(NULL)
		, code_start(NULL)
	{
		AvmCore *core = info->core();
		
		const byte* pos = info->body_pos;
		info->word_code.max_stack = AvmCore::readU30(pos);
		info->word_code.local_count = AvmCore::readU30(pos);
		info->word_code.init_scope_depth = AvmCore::readU30(pos);
		info->word_code.max_scope_depth = AvmCore::readU30(pos);
		AvmCore::readU30(pos);  // code_length
		code_start = pos;
		pool = info->pool;
		
		if (pool->word_code.cpool_mn == NULL)
			pool->word_code.cpool_mn = new (sizeof(PrecomputedMultinames) + (pool->constantMnCount - 1)*sizeof(Multiname)) PrecomputedMultinames(core->GetGC(), pool);
		
		// FIXME: If info->exceptions is not NULL then copy into info->word_code.exceptions, 
		// but 'from', 'to', and 'target' fields must be updated.
		
		computeExceptionFixups();
		
		refill();
#ifdef AVMPLUS_PEEPHOLE_OPTIMIZER
		state = 0;
#endif
	}

	Translator::~Translator()
	{
		cleanup();
	}

#define DELETE_LIST(T, v) \
	do { \
		T* tmp1 = v; \
		while (tmp1 != NULL) { \
			T* tmp2 = tmp1; \
			tmp1 = tmp1->next; \
			delete tmp2; \
		} \
		v = NULL; \
	} while (0)
	
	void Translator::cleanup() 
	{
		DELETE_LIST(backpatch_info, backpatches);
		DELETE_LIST(label_info, labels);
		DELETE_LIST(catch_info, exception_fixes);
		DELETE_LIST(buffer_info, buffers);
		if (spare_buffer) {
			delete spare_buffer;
			spare_buffer = NULL;
		}
	}
	
	void Translator::refill() 
	{
		if (buffers != NULL) {
			buffers->entries_used = dest - buffers->data;
			buffer_offset += buffers->entries_used;
		}
		buffer_info* b;
		if (spare_buffer != NULL) {
			b = spare_buffer;
			spare_buffer = NULL;
		}
		else
			b = new buffer_info;
		b->next = buffers;
		buffers = b;
		dest = b->data;
		dest_limit = dest + sizeof(b->data)/sizeof(b->data[0]);
	}
	
	void Translator::emitRelativeOffset(uint32 base_offset, const byte *base_pc, int32 offset) 
	{
		if (offset < 0) {
			// There must be a label for the target location
			uint32 old_offset = (base_pc - code_start) + offset;
			label_info* l = labels;
			while (l != NULL && l->old_offset != old_offset)
				l = l->next;
			AvmAssert(l != NULL);
			*dest++ = l->new_offset - base_offset;
		}
		else
			makeAndInsertBackpatch(base_pc + offset, base_offset);
	}
	
	void Translator::makeAndInsertBackpatch(const byte* target_pc, uint32 patch_offset)
	{
		// Leave a backpatch for the target location.  Backpatches are sorted in
		// increasing address order always.
		backpatch_info* b = new backpatch_info;
		b->target_pc = target_pc;
		b->patch_loc = dest;
		b->patch_offset = patch_offset;
		backpatch_info* q = backpatches;
		backpatch_info* qq = NULL;
#ifdef _DEBUG
		while (q != NULL) {
			AvmAssert(q->patch_offset != patch_offset);
			q = q->next;
		}
		q = backpatches;
#endif
		while (q != NULL && q->target_pc < b->target_pc) {
			qq = q;
			q = q->next;
		}
		if (qq == NULL) {
			b->next = backpatches;
			backpatches = b;
		}
		else {
			b->next = q;
			qq->next = b;
		}
		*dest++ = 0x80000000U;
	}
	
	void Translator::computeExceptionFixups() 
	{
		AvmCore *core = info->core();
	
		if (info->exceptions == NULL)
			return;
		
		DELETE_LIST(catch_info, exception_fixes);
		
		ExceptionHandlerTable* old_table = info->exceptions;
		int exception_count = old_table->exception_count;
		size_t extra = sizeof(ExceptionHandler)*(exception_count - 1);
		ExceptionHandlerTable* new_table = new (core->GetGC(), extra) ExceptionHandlerTable(exception_count);
		
		// Insert items in the exn list for from, to, and target, with the pc pointing
		// to the correct triggering instruction in the ABC and the update loc
		// pointing to the location to be patched; and a flag is_int_offset (if false
		// it's a sintptr).
		
		for ( int i=0 ; i < exception_count ; i++ ) {
			
			new_table->exceptions[i].traits = old_table->exceptions[i].traits;
			new_table->exceptions[i].scopeTraits = old_table->exceptions[i].scopeTraits;
			
			catch_info* p[3];
			
			p[0] = new catch_info;
			p[0]->pc = code_start + info->exceptions->exceptions[i].from;
			p[0]->is_target = false;
			p[0]->fixup_loc = (void*)&(new_table->exceptions[i].from);
			
			p[1] = new catch_info;
			p[1]->pc = code_start + info->exceptions->exceptions[i].to;
			p[1]->is_target = false;
			p[1]->fixup_loc = (void*)&(new_table->exceptions[i].to);
			
			p[2] = new catch_info;
			p[2]->pc = code_start + info->exceptions->exceptions[i].target;
			p[2]->is_target = true;
			p[2]->fixup_loc = (void*)&(new_table->exceptions[i].target);
			
			if (p[0]->pc > p[1]->pc) {
				catch_info* tmp = p[0];
				p[0] = p[1];
				p[1] = tmp;
			}
			if (p[1]->pc > p[2]->pc) {
				catch_info* tmp = p[1];
				p[1] = p[2];
				p[2] = tmp;
			}
			if (p[0]->pc > p[1]->pc) {
				catch_info* tmp = p[0];
				p[0] = p[1];
				p[1] = tmp;
			}
			
			int j=0;
			catch_info* e = exception_fixes;
			catch_info* ee = NULL;
			while (j < 3 && e != NULL) {
				if (e->pc > p[j]->pc) {
					if (ee == NULL) 
						exception_fixes = p[j];
					else 
						ee->next = p[j];
					p[j]->next = e;
					e = p[j];
					j++;
				}
				else {
					ee = e;
					e = e->next;
				}
			}
			while (j < 3) {
				if (ee == NULL) 
					exception_fixes = p[j];
				else 
					ee->next = p[j];
				p[j]->next = e;
				ee = p[j];
				j++;
			}
		}
		
		WB(core->GetGC(), info, &info->word_code.exceptions, new_table);
		
#ifdef _DEBUG
		if (exception_fixes != NULL) {
			catch_info* ee = exception_fixes;
			catch_info* e = ee->next;
			AvmAssert(ee->pc <= e->pc);
			ee = e;
			e = e->next;
		}
#endif
	}

	void Translator::fixExceptionsAndLabels(const byte *pc) 
	{
#ifdef AVMPLUS_PEEPHOLE_OPTIMIZER
		// Do not optimize across control flow targets, so flush the peephole window here
		if (exception_fixes != NULL && exception_fixes->pc == pc || backpatches != NULL && backpatches->target_pc == pc)
			peepFlush();
#endif

		while (exception_fixes != NULL && exception_fixes->pc <= pc) {
			AvmAssert(exception_fixes->pc == pc);
			exceptions_consumed = true;
			if (exception_fixes->is_target)
				*(sintptr*)(exception_fixes->fixup_loc) = (sintptr)(buffer_offset + (dest - buffers->data));
			else
				*(int*)(exception_fixes->fixup_loc) = (int)(buffer_offset + (dest - buffers->data));
			catch_info* tmp = exception_fixes;
			exception_fixes = exception_fixes->next;
			delete tmp;
		}
		
		while (backpatches != NULL && backpatches->target_pc <= pc) {
			AvmAssert(backpatches->target_pc == pc);
			AvmAssert(*backpatches->patch_loc == 0x80000000U);
			*backpatches->patch_loc = buffer_offset + (dest - buffers->data) - backpatches->patch_offset;
			backpatch_info* tmp = backpatches;
			backpatches = backpatches->next;
			delete tmp;
		}
	}
	
#define CHECK(n) \
		if (dest+n > dest_limit) refill();

#ifdef AVMPLUS_DIRECT_THREADED
#  define NEW_OPCODE(opcode) \
	((uint32)(opcode >= 255 ? opcode_labels[(opcode>>8) + 256] : opcode_labels[opcode])); \
	AvmAssert(((uint32)(opcode >= 255 ? opcode_labels[(opcode>>8)+256] : opcode_labels[opcode])) != 0)
#else
#  ifdef _DEBUG
#    define NEW_OPCODE(opcode)  opcode | (opcode << 16)  // debugging...
#  else
#    define NEW_OPCODE(opcode)  opcode
#  endif
#endif
					
	// These take no arguments
	void Translator::emitOp0(const byte *pc, int opcode) {
#ifdef _DEBUG
		switch (opcode) {
			case OP_add:
			case OP_add_i:
			case OP_astypelate:
			case OP_bitand:
			case OP_bitnot:
			case OP_bitor:
			case OP_bitxor:
			case OP_bkpt:
			case OP_checkfilter:
			case OP_coerce_b:
			case OP_coerce_a:
			case OP_coerce_d:
			case OP_coerce_i:
			case OP_coerce_o:
			case OP_coerce_s:
			case OP_coerce_u:
			case OP_convert_b:
			case OP_convert_d:
			case OP_convert_i:
			case OP_convert_o:
			case OP_convert_s:
			case OP_convert_u:
			case OP_decrement:
			case OP_decrement_i:
			case OP_divide:
			case OP_dup:
			case OP_dxnslate:
			case OP_equals:
			case OP_esc_xelem:
			case OP_esc_xattr:
			case OP_getglobalscope:
			case OP_getlocal0:
			case OP_getlocal1:
			case OP_getlocal2:
			case OP_getlocal3:
			case OP_greaterequals:
			case OP_greaterthan:
			case OP_hasnext:
			case OP_increment:
			case OP_increment_i:
			case OP_in:
			case OP_instanceof:
			case OP_istypelate:
			case OP_lessequals:
			case OP_lessthan:
			case OP_lshift:
			case OP_modulo:
			case OP_multiply:
			case OP_multiply_i:
			case OP_negate:
			case OP_negate_i:
			case OP_newactivation:
			case OP_nextname:
			case OP_nextvalue:
			case OP_not:
			case OP_pop:
			case OP_popscope:
			case OP_pushfalse:
			case OP_pushnan:
			case OP_pushnull:
			case OP_pushscope:
			case OP_pushtrue:
			case OP_pushwith:
			case OP_pushundefined:
			case OP_returnvalue:
			case OP_returnvoid:
			case OP_rshift:
			case OP_setlocal0:
			case OP_setlocal1:
			case OP_setlocal2:
			case OP_setlocal3:
			case OP_strictequals:
			case OP_subtract:
			case OP_subtract_i:
			case OP_swap:
			case OP_throw:
			case OP_typeof:
			case OP_urshift:
				break;
			default:
				AvmAssert(!"Unknown OP0");
		}
#endif // _DEBUG
		(void)pc;
		CHECK(1);
		*dest++ = NEW_OPCODE(opcode);
#ifdef AVMPLUS_PEEPHOLE_OPTIMIZER
		peep(opcode, dest-1);
#endif
	}

#ifdef _DEBUG
#  define CHECK_OP1(opcode, tag) \
	switch (opcode) { \
		case OP_applytype: \
		case OP_astype: \
		case OP_bkptline: \
		case OP_call: \
		case OP_coerce: \
		case OP_construct: \
		case OP_constructsuper: \
		case OP_debugline: \
		case OP_debugfile: \
		case OP_declocal: \
		case OP_declocal_i: \
		case OP_deleteproperty: \
		case OP_dxns: \
		case OP_finddef: \
		case OP_findproperty: \
		case OP_findpropstrict: \
		case OP_getdescendants: \
		case OP_getglobalslot: \
		case OP_getlex: \
		case OP_getlocal: \
		case OP_getouterscope: \
		case OP_getproperty: \
		case OP_getslot: \
		case OP_getsuper: \
		case OP_inclocal: \
		case OP_inclocal_i: \
		case OP_initproperty: \
		case OP_istype: \
		case OP_kill: \
		case OP_newarray: \
		case OP_newcatch: \
		case OP_newclass: \
		case OP_newfunction: \
		case OP_newobject: \
		case OP_pushdouble: \
		case OP_pushnamespace: \
		case OP_pushstring: \
		case OP_setglobalslot: \
 		case OP_setlocal: \
		case OP_setproperty: \
		case OP_setslot: \
		case OP_setsuper: \
			break; \
		default: \
			AvmAssert(!"Unknown " tag); \
	}
#else
#  define CHECK_OP1(opcode, tag)
#endif
	
	// These take one U30 argument
	void Translator::emitOp1(const byte *pc, int opcode)
	{
		CHECK_OP1(opcode, "OP1")
		CHECK(2);
		pc++;
		*dest++ = NEW_OPCODE(opcode);
		*dest++ = AvmCore::readU30(pc);
#ifdef AVMPLUS_PEEPHOLE_OPTIMIZER
		peep(opcode, dest-2);
#endif
	}
	
	// These take one U30 argument, and the argument is explicitly passed here (result of optimization)
	void Translator::emitOp1(int opcode, uint32 operand)
	{
#ifdef _DEBUG
		switch (opcode) {
			case OP_getscopeobject:
			case OP_ext_get2locals:
			case OP_ext_get3locals:
			case OP_ext_storelocal:
				break;
			default:
				CHECK_OP1(opcode, "OP1/imm")
		}
#endif // _DEBUG
		CHECK(2);
		*dest++ = NEW_OPCODE(opcode);
		*dest++ = operand;
#ifdef AVMPLUS_PEEPHOLE_OPTIMIZER
		peep(opcode, dest-2);
#endif
	}
	
#ifdef _DEBUG
#  define CHECK_OP2(opcode, tag) \
	switch (opcode) { \
		case OP_hasnext2: \
		case OP_callstatic: \
		case OP_callmethod: \
		case OP_callproperty: \
		case OP_callproplex: \
		case OP_callpropvoid: \
		case OP_constructprop: \
		case OP_callsuper: \
		case OP_callsupervoid: \
			break; \
		default: \
			AvmAssert(!"Unknown " tag); \
		}
#else
#  define CHECK_OP2(opcode, tag)	
#endif // _DEBUG

	// These take two U30 arguments
	void Translator::emitOp2(const byte *pc, int opcode)
	{
		CHECK_OP2(opcode, "OP2")
		CHECK(3);
		pc++;
		*dest++ = NEW_OPCODE(opcode);
		*dest++ = AvmCore::readU30(pc);
		*dest++ = AvmCore::readU30(pc);
#ifdef AVMPLUS_PEEPHOLE_OPTIMIZER
		peep(opcode, dest-3);
#endif
	}
	
	void Translator::emitOp2(int opcode, uint32 op1, uint32 op2)
	{
		CHECK_OP2(opcode, "OP2/imm");
		CHECK(3);
		*dest++ = NEW_OPCODE(opcode);
		*dest++ = op1;
		*dest++ = op2;
#ifdef AVMPLUS_PEEPHOLE_OPTIMIZER
		peep(opcode, dest-3);
#endif
	}

	
	// These take one S24 argument that is PC-relative.  If the offset is negative
	// then the target must be a LABEL instruction, and we can just look it up.
	// Otherwise, we enter the target offset into an ordered list with the current
	// transformed PC and the location to backpatch.
	void Translator::emitRelativeJump(const byte *pc, int opcode)
	{
#ifdef _DEBUG
		switch (opcode) {
			case OP_jump:
			case OP_iftrue:
			case OP_iffalse:
			case OP_ifeq:
			case OP_ifne:
			case OP_ifstricteq:
			case OP_ifstrictne:
			case OP_iflt:
			case OP_ifnlt:
			case OP_ifgt:
			case OP_ifngt:
			case OP_ifle:
			case OP_ifnle:
			case OP_ifge:
			case OP_ifnge:
				break;
			default:
				AvmAssert(!"Unknown relative jump opcode");
		}
#endif // _DEBUG
		CHECK(2);
		pc++;
		int32 offset = AvmCore::readS24(pc);
		pc += 3;
		*dest++ = NEW_OPCODE(opcode);
		uint32 base_offset = buffer_offset + (dest - buffers->data) + 1;
		emitRelativeOffset(base_offset, pc, offset);
#ifdef AVMPLUS_PEEPHOLE_OPTIMIZER
		peep(opcode, dest-2);
		AvmAssert(state == 0);		// Never allow a jump instruction to be in the middle of a match
#endif
	}
	
	void Translator::emitLabel(const byte *pc) 
	{
#ifdef AVMPLUS_PEEPHOLE_OPTIMIZER
		// Do not optimize across control control flow targets, so flush the peephole window here.
		peepFlush();
#endif
		label_info* l = new label_info;
		l->old_offset = pc-code_start;
		l->new_offset = buffer_offset + (dest - buffers->data);
		l->next = labels;
		labels = l;
	}

#ifdef DEBUGGER
	void Translator::emitDebug(const byte *pc) 
	{
		CHECK(5);
		pc++;
		byte debug_type = *pc++;
		uint32 index = AvmCore::readU30(pc);
		byte reg = *pc++;
		uint32 extra = AvmCore::readU30(pc);
		// 4 separate operands to match the value in the operand count table,
		// though obviously we could pack debug_type and reg into one word and
		// we could also omit extra.
		*dest++ = NEW_OPCODE(OP_debug);
		*dest++ = debug_type;
		*dest++ = index;
		*dest++ = reg;
		*dest++ = extra;
	}
#endif
	
	void Translator::emitPushbyte(const byte *pc) 
	{
		CHECK(2);
		pc++;
		*dest++ = NEW_OPCODE(OP_ext_pushbits);
		*dest++ = (((sint8)*pc++) << 3) | kIntegerType;
#ifdef AVMPLUS_PEEPHOLE_OPTIMIZER
		peep(OP_ext_pushbits, dest-2);
#endif
	}
	
	void Translator::emitPushshort(const byte *pc) 
	{
		CHECK(2);
		pc++;
		*dest++ = NEW_OPCODE(OP_ext_pushbits);
		*dest++ = ((signed short)AvmCore::readU30(pc) << 3) | kIntegerType;
#ifdef AVMPLUS_PEEPHOLE_OPTIMIZER
		peep(OP_ext_pushbits, dest-2);
#endif
	}
	
	void Translator::emitGetscopeobject(const byte *pc) 
	{
		CHECK(2);
		pc++;
		*dest++ = NEW_OPCODE(OP_getscopeobject);
		*dest++ = *pc++;
#ifdef AVMPLUS_PEEPHOLE_OPTIMIZER
		peep(OP_getscopeobject, dest-2);
#endif
	}
	
	void Translator::emitPushint(const byte *pc)
	{
		pc++;
		int32 value = pool->cpool_int[AvmCore::readU30(pc)];
		if ((value & 0xF0000000U) == 0xF0000000U || (value & 0xF0000000U) == 0) {
			CHECK(2);
			*dest++ = NEW_OPCODE(OP_ext_pushbits);
			*dest++ = (value << 3) | kIntegerType;
#ifdef AVMPLUS_PEEPHOLE_OPTIMIZER
			peep(OP_ext_pushbits, dest-2);
#endif
		}
		else {
			union {
				double d;
				uint32 bits[2];
			} v;
			v.d = (double)value;
			CHECK(3);
			*dest++ = NEW_OPCODE(OP_ext_push_doublebits);
			*dest++ = v.bits[0];
			*dest++ = v.bits[1];
#ifdef AVMPLUS_PEEPHOLE_OPTIMIZER
			peep(OP_ext_push_doublebits, dest-3);
#endif
		}
	}

	void Translator::emitPushuint(const byte *pc)
	{
		pc++;
		uint32 value = pool->cpool_uint[AvmCore::readU30(pc)];
		if ((value & 0xF0000000U) == 0) {
			CHECK(2);
			*dest++ = NEW_OPCODE(OP_ext_pushbits);
			*dest++ = (value << 3) | kIntegerType;
#ifdef AVMPLUS_PEEPHOLE_OPTIMIZER
			peep(OP_ext_pushbits, dest-2);
#endif
		}
		else {
			union {
				double d;
				uint32 bits[2];
			} v;
			v.d = (double)value;
			CHECK(3);
			*dest++ = NEW_OPCODE(OP_ext_push_doublebits);
			*dest++ = v.bits[0];
			*dest++ = v.bits[1];
#ifdef AVMPLUS_PEEPHOLE_OPTIMIZER
			peep(OP_ext_push_doublebits, dest-3);
#endif
		}
	}
	
	void Translator::emitLookupswitch(const byte *pc)
	{
#ifdef AVMPLUS_PEEPHOLE_OPTIMIZER
		// Avoid a lot of hair by flushing before LOOKUPSWITCH and not peepholing after.
		peepFlush();
#endif
		const byte* base_pc = pc;
		pc++;
		uint32 base_offset = buffer_offset + (dest - buffers->data);
		int32 default_offset = AvmCore::readS24(pc);
		pc += 3;
		uint32 case_count = AvmCore::readU30(pc);
		CHECK(3);
		*dest++ = NEW_OPCODE(OP_lookupswitch);
		emitRelativeOffset(base_offset, base_pc, default_offset);
		*dest++ = case_count;
		
		for ( uint32 i=0 ; i <= case_count ; i++ ) {
			int32 offset = AvmCore::readS24(pc);
			pc += 3;
			CHECK(1);
			emitRelativeOffset(base_offset, base_pc, offset);
		}
#ifdef AVMPLUS_PEEPHOLE_OPTIMIZER
		// need a forward declaration for toplevel.
//		AvmAssert(toplevel[OP_lookupswitch] == 0);
#endif
	}
	
	// 'OP_abs_jump' is an ABC-only construct, it boils away in the translation,
	// both here and to MIR/LIR.  It says: My first operand (one word in 32-bit
	// mode, two words in 64-bit mode) is a raw pointer into a buffer of ABC code.
	// My second operand is the number of bytes of code starting at that address.
	// Continue translating from that address as if it were a linear part
	// of the current code vector.  In other words, it's a forwarding pointer.
	
	void Translator::emitAbsJump(const byte *new_pc)
	{
		code_start = new_pc;
		
		// When performing a jump:
		//  - require that backpatches and labels no longer reference the old
		//    code vector; those sets must both be empty.  (We could clear out
		//    labels, alternatively, but that appears not to be required.)
		//  - recompute all the exception information, and require that none of it
		//    has been consumed -- this is the only thing that makes sense, and appears
		//    to be the view the verifier sanctions.  (A full definition for the
		//    semantics of abs_jump is sorely needed.)
		
		AvmAssert(!exceptions_consumed);
		AvmAssert(backpatches == NULL);
		AvmAssert(labels == NULL);
		computeExceptionFixups();
	}
	
	void Translator::epilogue() 
	{
		AvmAssert(backpatches == NULL);
		AvmAssert(exception_fixes == NULL);

		buffers->entries_used = dest - buffers->data;
		uint32 total_size = buffer_offset + buffers->entries_used;
		
		TranslatedCode* code_anchor = (TranslatedCode*)info->core()->GetGC()->Alloc(sizeof(TranslatedCode) + (total_size - 1)*sizeof(uint32), GC::kZero);
		uint32* code = code_anchor->data;
		
		// reverse the list of buffers
		buffer_info* first = buffers;
		buffer_info* next = first->next;
		first->next = NULL;
		while (next != NULL) {
			buffer_info* tmp = next->next;
			next->next = first;
			first = next;
			next = tmp;
		}
		buffers = first;
		
		// move the data
		uint32* ptr = code;
		while (first != NULL) {
			memcpy(ptr, first->data, first->entries_used*sizeof(uint32));
			ptr += first->entries_used;
			first = first->next;
		}
		AvmAssert(ptr == code + total_size);
		
		info->word_code.code_anchor = code_anchor;
		info->word_code.body_pos = code;
#ifdef SUPERWORD_PROFILING
		info->word_code.body_end = ptr;
		info->word_code.dumped = false;
#endif

		cleanup();
	}

#ifdef AVMPLUS_PEEPHOLE_OPTIMIZER
	
	// Peephole optimization.
	//
	// This is a state machine driven peephole optimizer.  The tables 'states', 'transitions',
	// and 'toplevel' are generated by the program utils/peephole.as based on the patterns
	// described in utils/peephole.txt, which are in turn hand-selected with aid of the
	// dynamic instruction profiling infrastructure built into Tamarin - see comments in
	// utils/superwordprof.c for help on how to use that.
	//
	// The state machine is deterministic and attempts to match the instruction stream to
	// the patterns, and to reduce longer strings of instructions to shorter strings.
	// A reduction is possible when the machine enters a final state.  However, the
	// machine is greedy and may leave the final state looking for a longer match.  As
	// the longer match may fail, the machine maintains a stack of final states it may
	// backtrack to.  A match may fail in two ways, either because a state is reached
	// from which there is no move to a final state on the actual input, or if a final
	// state is reached but the guard condition for the state is not satisfied.  The
	// guard is only tested when the machine is ready to commit; for that reason, a stack
	// of backtrack states is required (instead of a single backtrack state).  The guard
	// is mixed in with the commit code in order to keep code size down, though it
	// probably does not matter much.
	//
	// This machine is not powerful enough to handle all deterministic automata that
	// result from a translation of a nondeterministic set of patterns (as may result if
	// some patterns are non-prefix subpatterns of other patterns), as the machine insists
	// on reducing the entire prefix matched so far, not a suffix of that prefix.  That
	// can likely be fixed without too much hassle by tracking the start of each instruction
	// and making the action part a little richer.
	//
	// The peephole optimizer function peep() /must/ be called every time an instruction 
	// has been emitted to the instruction stream, as the state machine in the peephole
	// optimizer tracks the emitted instruction stream (it does not inspect it repeatedly).
	// The operands to peep() are the symbolic opcode that was just emitted and the address
	// at which that opcode was emitted.
	//
	// If optimization must not cross some instruction boundary (as for control flow targets)
	// then peepFlush() must be called before instructions are emitted for the point beyond
	// the boundary.
	//
	// It is possible to optimize the entry to peep, the in-line test is 
	//     state==0 && toplevel[toplevel_index] == 0
	// where toplevel_index is computed from the opcode, maybe worth simplifying that in 
	// order to make this test faster.  Anyhow, if the test is true then peep() need 
	// not be called as there will not be a state transition.  This factoid may be useful
	// if emitOp0, emitOp1, and emitOp2 are in-lined into the verifier.

	
	// In practice, most of these fields don't need to be uint32.  But for the 
	// same reason, there aren't a lot of savings to be had by shrinking them.
	
	struct state_t {
		uint32 isFinal;
		uint32 numTransitions;
		uint32 transitionPtr;
		uint32 guardAndAction;
	};
	
	struct transition_t {
		uint32 opcode;
		uint32 next_state;
	};
	
	// Note that transitions in a run are always sorted in increasing token value order,
	// so it's possible to binary search the run.
	
	// Begin code that should be generated
	
	static state_t states[] = {
	{ 0, 0, 0, 0 },    // 0 is never a valid state
	{ 0, 1, 0, 0 },    // 0x62
	{ 1, 1, 1, 1 },    // 0x62 0x62
	{ 1, 0, 0, 2 },    // 0x62 0x62 0x62
	{ 0, 1, 2, 0 },    // 0x63
	{ 1, 0, 0, 3 },    // 0x63 0x62
	};
	
	static transition_t transitions[] = {
	{ OP_getlocal, 2 },
	{ OP_getlocal, 3 }, 
	{ OP_getlocal, 5 },
	};
	
	static uint32 toplevel[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 1, 4, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0,
	};
	
	bool Translator::commit(uint32 action) {
		switch (action) {
			case 1:
				AvmAssert(O[0] == OP_getlocal && O[1] == OP_getlocal);
				if (!(I[0][1] < 65536 && I[1][1] < 65536)) return false;
				S[0] = OP_ext_get2locals;
				R[0] = NEW_OPCODE(OP_ext_get2locals);
				R[1] = (I[1][1] << 16) | I[0][1];
				return replace(0, 2, 2);
			case 2:
				AvmAssert(O[0] == OP_getlocal && O[1] == OP_getlocal && O[2] == OP_getlocal);
				if (!(I[0][1] < 1024 && I[1][1] < 1024 && I[2][1] < 1024)) return false;
				S[0] = OP_ext_get3locals;
				R[0] = NEW_OPCODE(OP_ext_get3locals);
				R[1] = (I[2][1] << 20) | (I[1][1] << 10) | I[0][1];
				return replace(0, 3, 2);
			case 3:
				AvmAssert(O[0] == OP_setlocal && O[1] == OP_getlocal);
				if (!(I[0][1] == I[1][1])) return false;
				S[0] = OP_ext_storelocal;
				R[0] = NEW_OPCODE(OP_ext_storelocal);
				R[1] = I[0][1];
				return replace(0, 2, 2);
			default:
				AvmAssert(!"Should never get here");
				return false;
		}
	}
	
	// End code that should be generated

	// Invariants here:
	//
	//   - Lookupswitch never appears in the peephole window (reduces complexity
	//     and guarantees we won't ever have more than one buffer boundary crossing)
	//
	//   - Relative branch instructions only ever appear as the last instruction in 
	//     the window.  At that point, if it is a forward branch, then the backpatch
	//     may not be the first backpatch in the list, but it will usually be near
	//     the beginning (most branches are short).  Backpatches are uniquely 
	//     identified by the patch location they point to so it's always safe to
	//     remove one if we're squashing a branch instruction.
	//
	//     That means that if the peephole optimizer processes a branch instruction
	//     then it /must/ reduce at that point, it can't wait until the next
	//     instruction even if the current state is a final state.
	//
	//   - If the optimizer inserts a branch then the address in the branch must
	//     be absolute.  If the branch is backward it must be the negative of the
	//     absolute word offset of the target.  If the branch is forward it must
	//     be the positive absolute ABC byte offset of the branch target; a backpatch
	//     structure will be created in the latter case.
	
	bool Translator::replace(uint32 start, uint32 old_instr, uint32 new_words) 
	{
		// Undo any relative offsets in the last instruction

		if (isJumpInstruction(O[nextI - 1])) {
			
			AvmAssert(I[nextI - 1] + 2 == dest);
			
			uint32 offset = I[nextI - 1][1];
			if (offset == 0x80000000U) {
				// Forward branch, must find and nuke the backpatch
				backpatch_info *b = backpatches;
				backpatch_info *b2 = NULL;
				while (b != NULL && b->patch_loc != &I[nextI - 1][1])
					b2 = b, b = b->next;
				AvmAssert(b != NULL);
				if (b2 == NULL)
					backpatches = b->next;
				else
					b2->next = b->next;
				// b is unlinked
				// Install the ABC byte offset from the backpatch structure (will be positive)
				I[nextI - 1][1] = b->target_pc - code_start;
				delete b;
			}
			else {
				// Backward branch
				AvmAssert((int32)I[nextI - 1][1] < 0);
				// Install the negative of the absolute word offset of the target
				I[nextI - 1][1] = -int32(buffer_offset + (dest - buffers->data) + (int32)I[nextI - 1][1]);
			}
		}
		
		// Catenate unconsumed instructions onto R (it's easier than struggling with
		// moving instructions across buffer boundaries)

		uint32 k = new_words;
		for ( uint32 n=start + old_instr ; n < nextI ; n++ ) {
			uint32 len = calculateInstructionWidth(O[n]);
			S[k] = O[n];
			for ( uint32 j=0 ; j < len ; j++ )
				R[k++] = I[n][j];
		}
		
		// Unlink the last buffer segment if we took everything from it, push it onto
		// a reserve (there can only ever be one free).  We know I[nextI-1] points into the
		// current buffer, so check if I[start] is between the start of the buffer and
		// the last instruction.
		
		if (!(buffers->data <= I[start] && I[start] <= I[nextI-1])) {
			spare_buffer = buffers;
			buffers = buffers->next;
			spare_buffer->next = NULL;
			dest = I[start];
			dest_limit = buffers->data + sizeof(buffers->data)/sizeof(buffers->data[0]);
			buffer_offset -= buffers->entries_used;
		}
		else
			dest = I[start];
		
		// Emit the various instructions from new_data, handling branches specially

		uint32 i=0;
		while (i < k) {
			if (isJumpInstruction(S[i])) {
				CHECK(2);
				*dest++ = R[i++];
				int32 offset = (int32)R[i++];
				if (offset >= 0) {
					// Forward jump
					// Install a new backpatch structure
					makeAndInsertBackpatch(code_start + offset, buffer_offset + (dest + 1 - buffers->data));
				}
				else {
					// Backward jump
					// Compute new jump offset
					*dest = -int32(buffer_offset + (dest + 1 - buffers->data) + offset);
					dest++;
				}
			}
			else {
				switch (calculateInstructionWidth(S[i])) {
					default:
						AvmAssert(!"Can't happen");
					case 1:
						CHECK(1);
						*dest++ = R[i++];
						break;
					case 2:
						CHECK(2);
						*dest++ = R[i++];
						*dest++ = R[i++];
						break;
					case 3:
						CHECK(3);
						*dest++ = R[i++];
						*dest++ = R[i++];
						*dest++ = R[i++];
						break;
					case 5:  // OP_debug
						CHECK(5);
						*dest++ = R[i++];
						*dest++ = R[i++];
						*dest++ = R[i++];
						*dest++ = R[i++];
						*dest++ = R[i++];
						break;
				}
			}
		}
		
		return true;  // always
	}

	// OPTIMIZEME - instruction width lookup must be fast.
	// Should use a table lookup for all the opcodes.
	
	uint32 Translator::calculateInstructionWidth(uint32 opcode)
	{
		if (opcode < 255)
			return opOperandCount[opcode] + 1;
		switch (opcode) {
			case OP_ext_pushbits:
			case OP_ext_get2locals:
			case OP_ext_get3locals:
			case OP_ext_storelocal:
				return 2;
			case OP_ext_push_doublebits:
				return 3;
			default:
				AvmAssert(!"Should not happen");
				return 1;
		}
	}
	
	// OPTIMIZEME - determining jump instruction must be fast.
	// Should use a fast lookup or an inlinable
	// comparison (all these instructions are in a dense range).
	
	bool Translator::isJumpInstruction(uint32 opcode)
	{
		switch (opcode) {
			case OP_jump:
			case OP_iftrue:
			case OP_iffalse:
			case OP_ifeq:
			case OP_ifne:
			case OP_ifstricteq:
			case OP_ifstrictne:
			case OP_iflt:
			case OP_ifnlt:
			case OP_ifgt:
			case OP_ifngt:
			case OP_ifle:
			case OP_ifnle:
			case OP_ifge:
			case OP_ifnge:
				return true;
			default:
				return false;
		}
	}
	
	void Translator::peep(uint32 opcode, uint32* loc)
	{
		state_t *s;
		transition_t *t;
		uint32 i, limit, next_state, toplevel_index;
		
		AvmAssert(opcode != OP_lookupswitch);

		if (state == 0) 
			goto initial_state;
		
		// Search for a transition from the current state to a next
		// state on input 'opcode'.  
		//
		// OPTIMIZEME - search for next state
		// This search is sequential now, but things are set up so 
		// that we can use binary search later: transitions are sorted
		// in token value order
		
		O[nextI] = opcode;
		I[nextI] = loc;
		nextI++;
		s = &states[state];
		t = &transitions[s->transitionPtr];
		limit = s->numTransitions;
		
		i = 0;
		while (i < limit && t->opcode != opcode) 
			i++, t++;
		
		next_state = (i == limit) ? 0 : t->next_state;
		
		if (next_state != 0) {

			// Advance
			//
			// There is a next state, so push the current state on the backtrack
			// stack if it is final, and move to the next state.  If that state has
			// successor states then return, as the search continues.  Otherwise, the
			// next state must be final and we try to accept.
			//
			// (The shortcut of checking the successors is necessary for correctness,
			// as otherwise the peephole window could contain a branch in the non-final
			// position.)
			
			if (s->isFinal) {
				int bi = backtrack_idx++;
				backtrack_stack[bi].state = state;
				backtrack_stack[bi].nextI = nextI;
			}
			state = next_state;
			s = &states[state];
			if (s->numTransitions > 0)
				return;
			
			next_state = 0;
			AvmAssert(s->isFinal);
		}

		// Accept
		//
		// The next state is 0.  Commit to 'state' if it is final; otherwise to 
		// successive backtrack states.  Committing means checking the guard
		// (which may fail, forcing further backtracking) and if the guard passes
		// then performing the transformation.  The commit function is generated,
		// see above; the replace logic is in the function replace() above.
		
		if (s->isFinal && commit(s->guardAndAction)) 
			goto accepted;
		
		for ( int bi=backtrack_idx-1 ; bi >= 0 ; bi-- ) {
			state_t *b = &states[backtrack_stack[bi].state];
			AvmAssert(b->isFinal);
			if (commit(b->guardAndAction)) 
				goto accepted;
		}

		// If we failed to find an accepting state then fall through to initial_state
		// to reset the machine.

	initial_state:
		toplevel_index = opcode < 255 ? opcode : 256 + (opcode >> 8);
		AvmAssert(toplevel_index < sizeof(toplevel)/sizeof(toplevel[0]));

		state = toplevel[toplevel_index];  // may remain 0
		nextI = 0;
		backtrack_idx = 0;
		if (state != 0) {
			O[nextI] = opcode;
			I[nextI] = loc;
			nextI++;
		}
		return;
		
	accepted:
		// FIXME - improve state selection following acceptance
		//
		// We should do better here: we should enter at the last opcode left behind in
		// the stream.  The replace() function could record that opcode for us.
		state = 0;
	}
	
	// FIXME - improve peephole window flushing
	//
	// Here we should probably not just set the state to 0, but force a commit to a 
	// pending backtrack state if there is one.  The idea is to sync the instruction
	// stream so that we don't peephole across a label or an exception handling
	// boundary.

	void Translator::peepFlush()
	{
		if (state != 0)
			state = 0;
	}

#endif  // AVMPLUS_PEEPHOLE_OPTIMIZER

#endif // AVMPLUS_WORD_CODE
}
