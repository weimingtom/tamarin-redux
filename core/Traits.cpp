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
 * Portions created by the Initial Developer are Copyright (C) 1993-2006
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
	using namespace MMgc;

	// -------------------------------------------------------------------
	// -------------------------------------------------------------------
	// -------------------------------------------------------------------
	// -------------------------------------------------------------------

	/*static*/ TraitsBindings* TraitsBindings::alloc(GC* gc, 
												Traits* _owner, 
												TraitsBindingsp _base, 
												MultinameHashtable* _bindings, 
												uint32_t slotCount, 
												uint32_t methodCount,
												uint32_t interfaceCapacity)
	{
		AvmAssert(interfaceCapacity > 0);

		const uint32_t extra = slotCount * sizeof(SlotInfo) + 
						methodCount * sizeof(MethodInfo) +
						interfaceCapacity * sizeof(InterfaceInfo);

		TraitsBindings* tb = new (gc, extra) TraitsBindings(_owner, _base, _bindings, slotCount, methodCount, interfaceCapacity);
		if (_base)
		{
			if (_base->slotCount)
			{
				const SlotInfo* src = &_base->getSlots()[0];
				SlotInfo* dst = &tb->getSlots()[0];
				VMPI_memcpy(dst, src, _base->slotCount * sizeof(SlotInfo));
				if (!_owner->isMachineType())
				{
					AvmAssert(tb->owner->m_sizeofInstance >= _base->owner->m_sizeofInstance);
					// upshift by 3 bits to affect only the offset part, leaving sst alone...
					// downshift by 2 bits because it's stored as offset of uint32 (not uint8)
					// thus net upshift of 1
					const uint32_t delta = (tb->owner->m_sizeofInstance - _base->owner->m_sizeofInstance)<<1;
					if (delta > 0)
					{
						for (uint32_t i = 0, n = _base->slotCount; i < n; ++i)
							dst[i].offsetAndSST = src[i].offsetAndSST + delta;
					}
				}
			}
			if (_base->methodCount)
				VMPI_memcpy(&tb->getMethods()[0], &_base->getMethods()[0], _base->methodCount * sizeof(MethodInfo));
		}
		return tb;
	}

	Binding TraitsBindings::findBinding(Stringp name) const
	{
		for (TraitsBindingsp self = this; self; self = self->base)
		{
			const Binding b = self->m_bindings->getName(name);
			if (b != BIND_NONE)
				return b;
		}
		return BIND_NONE;
	}

	Binding TraitsBindings::findBinding(Stringp name, Namespacep ns) const
	{
		for (TraitsBindingsp self = this; self; self = self->base)
		{
			const Binding b = self->m_bindings->get(name, ns);
			if (b != BIND_NONE)
				return b;
		}
		return BIND_NONE;
	}

	Binding TraitsBindings::findBinding(Stringp name, NamespaceSetp nsset) const
	{
		for (TraitsBindingsp self = this; self; self = self->base)
		{
			const Binding b = self->m_bindings->get(name, nsset);
			if (b != BIND_NONE)
				return b;
		}
		return BIND_NONE;
	}

	Binding TraitsBindings::findBindingAndDeclarer(const Multiname& mn, Traitsp& declarer) const
	{
		if (mn.isBinding())
		{
			for (TraitsBindingsp self = this; self; self = self->base)
			{
				Namespacep foundns = NULL;
				Binding const b = self->m_bindings->getMulti(mn, foundns);
				if (b != BIND_NONE)
				{
					declarer = self->owner;

					// if the member is 'protected' then we have to do extra work,
					// as we may have found it in a descendant's protected namespace -- 
					// we have to bounce up the inheritance chain and check our parent's 
					// protected namespace.
					while (foundns == declarer->protectedNamespace)
					{
						Traitsp declParent = declarer->base;
						if (!declParent || declParent->protectedNamespace == NULL)
							break;

						Binding const bp = declParent->getTraitsBindings()->findBinding(mn.getName(), declParent->protectedNamespace);
						if (bp != b)
							break;

						// self->owner->core->console<<"bounce "<<declarer<<" to "<<declParent<<"\n";
						declarer = declParent;
						foundns = declParent->protectedNamespace;
					}

					// self->owner->core->console<<"final declarer is "<<declarer<<"\n";
					return b;
				}
			}
		}
		declarer = NULL;
		return BIND_NONE;
	}

	bool TraitsBindings::addOneInterface(Traitsp intf)
	{
		AvmAssert(intf != NULL);
		AvmAssert(this->interfaceCapacity > 0);

		InterfaceInfo* set = getInterfaces();
        int32_t n = 7;
		const uint32_t bitMask = this->interfaceCapacity - 1;
		// skip lower 3 bits since they are always zero and don't contribute to hash
		uint32_t i = (uintptr_t(intf) >> 3) & bitMask;
        for (;;)
        {
            Traitsp k;
			// when adding, intf probably isn't present, check for null first
            if ((k = set[i].t) == NULL)
            {
				// since this is in the cache, no need for WB: 
				// intf isn't going to go away before we do
				set[i].t = intf;
				return true; // true == added
            }
            else if (k == intf)
            {
				// already added, we're done
                return false; // false == not added
            }
            else
            {
                i = (i + (n++)) & bitMask;                // quadratic probe
            }
        }
		// ironically, some compilers will complain that this is unreachable. Er, yeah...
		// AvmAssert(0); // never reached
	}

	bool FASTCALL TraitsBindings::containsInterface(Traitsp intf) const
	{
		AvmAssert(intf != NULL);
		AvmAssert(this->interfaceCapacity > 0);

		const InterfaceInfo* set = getInterfaces();
        int32_t n = 7;
		const uint32_t bitMask = this->interfaceCapacity - 1;
		// skip lower 3 bits since they are always zero and don't contribute to hash
		uint32_t i = (uintptr_t(intf) >> 3) & bitMask;
        for (;;)
        {
            Traitsp k;
			// containsInterface succeeds much more often than it fails (~10x) so check for that first
            if ((k = set[i].t) == intf)
            {
				return true; 
            }
            else if (k == NULL)
            {
                return false; 
            }
            else
            {
                i = (i + (n++)) & bitMask;                // quadratic probe
            }
        }
		// ironically, some compilers will complain that this is unreachable. Er, yeah...
		// AvmAssert(0); // never reached
	}

	void TraitsBindings::buildSlotDestroyInfo(MMgc::GC* gc, FixedBitSet& slotDestroyInfo) const
	{
		// this is really a compile-time assertion
		AvmAssert(kUnusedAtomTag == 0 && kObjectType == 1 && kStringType == 2 && kNamespaceType == 3);
		
		// not the same as slotCount since a slot of type double
		// takes two bits (in 32-bit builds). note that the bits are
		// always 4-byte chunks even in 64-bit builds!
		const uint32_t bitsNeeded = m_slotSize / sizeof(uint32_t);	// not sizeof(Atom)!
		AvmAssert(bitsNeeded * sizeof(uint32_t) == m_slotSize);		// should be even multiple!
		// allocate one extra bit and use it for "all-zero"
		slotDestroyInfo.resize(gc, bitsNeeded+1);
		
		const uint32_t sizeofInstance = this->owner->m_sizeofInstance;
		const TraitsBindings::SlotInfo* tbs		= getSlots();
		const TraitsBindings::SlotInfo* tbs_end	= tbs + slotCount;
		for ( ; tbs < tbs_end; ++tbs) 
		{
			// offset is pointed off the end of our object
			if (isAtomOrRCObjectSlot(tbs->sst())) 
			{
				//owner->core->console<<"SDI "<<owner<<" "<<sizeofInstance<<" "<<tbs->type<<" "<<tbs->offset()<<"\n";
				AvmAssert(tbs->offset() >= sizeofInstance);
				const uint32_t off = tbs->offset() - sizeofInstance;
				AvmAssert((off % 4) == 0);
				// if slot is "big" then this is the bit of the first 4 bytes. that's fine.
				slotDestroyInfo.set((off>>2)+1);	// +1 to leave room for bit 0
				slotDestroyInfo.set(0);				// bit 0 is "anyset" flag
			} 
			// otherwise leave the bit zero
		}

		// if nothing set, blow away what we built and realloc as single clear bit -- smaller and faster
		if (!slotDestroyInfo.test(0))
		{
			slotDestroyInfo.resize(gc, 1);
			AvmAssert(!slotDestroyInfo.test(0));
		}
	}

    bool TraitsBindings::checkOverride(AvmCore* core, MethodInfo* virt, MethodInfo* over) const
    {
		if (over == virt)
			return true;
			
		MethodSignaturep overms = over->getMethodSignature();
		MethodSignaturep virtms = virt->getMethodSignature();

        Traits* overTraits = overms->returnTraits();
        Traits* virtTraits = virtms->returnTraits();

        if (overTraits != virtTraits)
        {
#ifdef AVMPLUS_VERBOSE
            core->console << "\n";
            core->console << "return types dont match\n";
            core->console << "   virt " << virtTraits << " " << virt << "\n";
            core->console << "   over " << overTraits << " " << over << "\n";
#endif
            return false;
        }
		
        if (overms->param_count() != virtms->param_count() ||
            overms->optional_count() != virtms->optional_count())
        {
#ifdef AVMPLUS_VERBOSE
            core->console << "\n";
            core->console << "param count mismatch\n";
            core->console << "   virt params=" << virtms->param_count() << " optional=" << virtms->optional_count() << " " << virt << "\n";
            core->console << "   over params=" << overms->param_count() << " optional=" << overms->optional_count() << " " << virt << "\n";
#endif
            return false;
        }

		// allow subclass param 0 to implement or extend base param 0
		virtTraits = virtms->paramTraits(0);
		if (!containsInterface(virtTraits) || !Traits::isMachineCompatible(this->owner, virtTraits))
		{
			if (!this->owner->isMachineType() && virtTraits == core->traits.object_itraits)
			{
				over->setUnboxThis();
			}
			else
			{
				#ifdef AVMPLUS_VERBOSE
				core->console << "\n";
				core->console << "param 0 incompatible\n";
				core->console << "   virt " << virtTraits << " " << virt << "\n";
				core->console << "   over " << this->owner << " " << over << "\n";
				#endif
				return false;
			}
		}

        for (int k=1, p=overms->param_count(); k <= p; k++)
        {
            overTraits = overms->paramTraits(k);
            virtTraits = virtms->paramTraits(k);
            if (overTraits != virtTraits)
            {
				#ifdef AVMPLUS_VERBOSE
				core->console << "\n";
				core->console << "param " << k << " incompatible\n";
				core->console << "   virt " << virtTraits << " " << virt << "\n";
				core->console << "   over " << overTraits << " " << over << "\n";
				#endif
				return false;
            }
        }

		if (virt->unboxThis())
		{
			// the UNBOX_THIS flag is sticky, all the way down the inheritance tree
			over->setUnboxThis();
		}

        return true;
    }

	static bool isCompatibleOverrideKind(BindingKind baseKind, BindingKind overKind)
	{
		static const uint8_t kCompatibleBindingKinds[8] = 
		{
			0,														// BKIND_NONE
			(1<<BKIND_METHOD),										// BKIND_METHOD
			0,														// BKIND_VAR
			0,														// BKIND_CONST
			0,														// unused
			(1<<BKIND_GET) | (1<<BKIND_SET) | (1<<BKIND_GETSET),	// BKIND_GET
			(1<<BKIND_GET) | (1<<BKIND_SET) | (1<<BKIND_GETSET),	// BKIND_SET
			(1<<BKIND_GET) | (1<<BKIND_SET) | (1<<BKIND_GETSET)		// BKIND_GETSET
		};
		return (kCompatibleBindingKinds[baseKind] & (1<<overKind)) != 0;
	}

	bool TraitsBindings::checkLegalInterfaces(AvmCore* core) const
	{
		// make sure every interface method is implemented
		const TraitsBindings::InterfaceInfo* tbi		= this->getInterfaces();
		const TraitsBindings::InterfaceInfo* tbi_end	= tbi + this->interfaceCapacity;
		for ( ; tbi < tbi_end; ++tbi) 
		{
			Traitsp ifc = tbi->t;
			if (!ifc || !ifc->isInterface)
				continue;

			// don't need to bother checking interfaces in our parent.
			if (this->base && this->base->containsInterface(ifc))
				continue;

			TraitsBindingsp ifcd = ifc->getTraitsBindings();
			StTraitsBindingsIterator iter(ifcd);
			while (iter.next())
			{
				Stringp name = iter.key();
				if (!name) continue;
				Namespacep ns = iter.ns();
				Binding iBinding = iter.value();
				const BindingKind iBindingKind = AvmCore::bindingKind(iBinding);

				Binding cBinding = this->findBinding(name, ns);
				if (!isCompatibleOverrideKind(iBindingKind, AvmCore::bindingKind(cBinding)))
				{
					// Try again with public namespace that matches the version of the current traits
					const Binding pBinding = this->findBinding(name, core->getPublicNamespace(owner->pool));
					if (isCompatibleOverrideKind(iBindingKind, AvmCore::bindingKind(pBinding)))
						cBinding = pBinding;
				}

				if (iBinding == cBinding) 
					continue;

				if (!isCompatibleOverrideKind(iBindingKind, AvmCore::bindingKind(cBinding)))
					return false;

				switch (iBindingKind)
				{
					default:
					{
						AvmAssert(0);	// interfaces shouldn't have anything but methods, getters, and setters
						return false;
					}
					case BKIND_METHOD:
					{
						MethodInfo* virt = ifcd->getMethod(AvmCore::bindingToMethodId(iBinding));
						MethodInfo* over = this->getMethod(AvmCore::bindingToMethodId(cBinding));
						if (!checkOverride(core, virt, over))
							return false;
						break;
					}
					case BKIND_GET:
					case BKIND_SET:
					case BKIND_GETSET:
					{
						// check getter & setter overrides
						if (AvmCore::hasGetterBinding(iBinding))
						{
							if (!AvmCore::hasGetterBinding(cBinding))
								return false;
							
							MethodInfo* virt = ifcd->getMethod(AvmCore::bindingToGetterId(iBinding));
							MethodInfo* over = this->getMethod(AvmCore::bindingToGetterId(cBinding));
							if (!checkOverride(core, virt, over))
								return false;
						}

						if (AvmCore::hasSetterBinding(iBinding))
						{
							if (!AvmCore::hasSetterBinding(cBinding))
								return false;
								
							MethodInfo* virt = ifcd->getMethod(AvmCore::bindingToSetterId(iBinding));
							MethodInfo* over = this->getMethod(AvmCore::bindingToSetterId(cBinding));
							if (!checkOverride(core, virt, over))
								return false;
						}
					}
					break;
				} // switch
			} // for j
		} // for tbi
		return true;
	}
	
	void TraitsBindings::fixOneInterfaceBindings(Traitsp ifc, const Toplevel* toplevel)
	{
		if (owner->isInterface)
			return;

		if (!ifc->linked) 
		{
			// toplevel will be non-null only for the first call (will be null afterwards) --
			// but all our interfaces will be resolved by then
			AvmAssert(toplevel != NULL);
			ifc->resolveSignatures(toplevel);
		}

		TraitsBindingsp ifcd = ifc->getTraitsBindings();
		StTraitsBindingsIterator iter(ifcd);
		while (iter.next())
		{
			Stringp name = iter.key();
			if (!name) continue;
			Namespacep ns = iter.ns();
			Binding iBinding = iter.value();
			const BindingKind iBindingKind = AvmCore::bindingKind(iBinding);
			const Binding cBinding = this->findBinding(name, ns);
			if (!isCompatibleOverrideKind(iBindingKind, AvmCore::bindingKind(cBinding)))
			{
				// Try again with public namespace that matches the version of the current traits
				const Binding pBinding = this->findBinding(name, ifc->core->getPublicNamespace(owner->pool));
				if (isCompatibleOverrideKind(iBindingKind, AvmCore::bindingKind(pBinding)))
				{
					this->m_bindings->add(name, ns, pBinding);
				}
			}
		} 
	}
		
	void Traits::addVersionedBindings(MultinameHashtable* bindings,
									  Stringp name,
									  NamespaceSetp nss,
									  Binding binding) const
	{
		int32_t apis = 0;
		for (int i=0; i<nss->size; ++i) {
			 apis |= ApiUtils::getCompatibleAPIs(core, nss->namespaces[i]->getAPI());
		}
		Namespacep ns = ApiUtils::getVersionedNamespace(core, nss->namespaces[0], apis);
		bindings->add(name, ns, binding);
	}

	// -------------------------------------------------------------------
	// -------------------------------------------------------------------

	TraitsMetadata::MetadataPtr TraitsMetadata::getSlotMetadataPos(uint32_t i, PoolObject*& residingPool) const
	{
		AvmAssert(i < slotCount);
		residingPool = NULL;
		for (TraitsMetadatap self = this; self && (i < self->slotCount); self = self->base)
		{
			MetadataPtr pos = self->slotMetadataPos[i];
			if (pos)
			{
				residingPool = self->residingPool;
				return pos;
			}
		}
		return NULL;
	}

	TraitsMetadata::MetadataPtr TraitsMetadata::getMethodMetadataPos(uint32_t i, PoolObject*& residingPool) const
	{
		AvmAssert(i < methodCount);
		residingPool = NULL;
		for (TraitsMetadatap self = this; self && (i < self->methodCount); self = self->base)
		{
			MetadataPtr pos = self->methodMetadataPos[i];
			if (pos)
			{
				residingPool = self->residingPool;
				return pos;
			}
		}
		return NULL;
	}

	// -------------------------------------------------------------------
	// -------------------------------------------------------------------

    Traits::Traits(PoolObject* _pool, 
				   Traits* _base,
				   uint32_t _sizeofInstance,
				   TraitsPosPtr traitsPos,
				   TraitsPosType posType) : 
		core(_pool->core),
		base(_base),
		pool(_pool),
		m_traitsPos(traitsPos),
		m_tbref(_pool->core->GetGC()->emptyWeakRef),
		m_tmref(_pool->core->GetGC()->emptyWeakRef),
		m_sizeofInstance(uint16_t(_sizeofInstance)),
		builtinType(BUILTIN_none),
		m_posType(uint8_t(posType))
    {
		AvmAssert(m_tbref->get() == NULL);
		AvmAssert(m_tmref->get() == NULL);
		AvmAssert(BUILTIN_COUNT <= 32);
		AvmAssert(_sizeofInstance <= 0xffff);
		AvmAssert(m_slotDestroyInfo.allocatedSize() == 0);
#ifdef _DEBUG
		switch (posType)
		{
			case TRAITSTYPE_NVA:
			case TRAITSTYPE_RT:
				AvmAssert(m_traitsPos == 0);
				break;
			default:
				AvmAssert(m_traitsPos != 0);
				break;
		}
#endif
    }

	/*static*/ Traits* Traits::newTraits(PoolObject* pool,
							Traits *base,
							uint32_t objectSize,
							TraitsPosPtr traitsPos,
							TraitsPosType posType)
    {
		AvmAssert(posType != TRAITSTYPE_CATCH);
		AvmAssert(pool != NULL);
		Traits* traits = new (pool->core->GetGC()) Traits(pool, base, objectSize, traitsPos, posType);
		return traits;
	}

	/*static*/ Traits* Traits::newCatchTraits(const Toplevel* toplevel, PoolObject* pool, TraitsPosPtr traitsPos, Stringp name, Namespacep ns)
	{
		AvmAssert(pool != NULL);
		Traits* traits = new (pool->core->GetGC()) Traits(pool, NULL, sizeof(ScriptObject), traitsPos, TRAITSTYPE_CATCH);
		traits->final = true;
		traits->set_names(ns, name);
		traits->resolveSignatures(toplevel);
		return traits;
	}

	Traits* Traits::_newParameterizedTraits(Stringp name, Namespacep ns, Traits* _base)
	{
		Traits* newtraits = Traits::newTraits(this->pool, _base, this->getSizeOfInstance(), NULL, TRAITSTYPE_RT);
		newtraits->m_needsHashtable = this->m_needsHashtable;
		newtraits->set_names(ns, name);
		return newtraits;
	}

	TraitsPosPtr Traits::traitsPosStart() const
	{
		TraitsPosPtr pos = m_traitsPos;
		switch (posType())
		{
			case TRAITSTYPE_INSTANCE_FROM_ABC:	
				pos = skipToInstanceInitPos(pos);
				// fall thru, no break

			case TRAITSTYPE_CLASS_FROM_ABC:
			case TRAITSTYPE_SCRIPT_FROM_ABC:
				AvmCore::skipU30(pos, 1);		// skip in init_index
				break;

			case TRAITSTYPE_ACTIVATION:
				// nothing to do
				break;

			case TRAITSTYPE_CATCH:
			case TRAITSTYPE_NVA:
			case TRAITSTYPE_RT:
				pos = NULL;
				break;
		}
		return pos;
	}

	TraitsPosPtr Traits::skipToInstanceInitPos(TraitsPosPtr pos) const
	{
		AvmAssert(posType() == TRAITSTYPE_INSTANCE_FROM_ABC && pos != NULL);
		AvmCore::skipU30(pos, 2);		// skip the QName & base traits 
		const uint8_t theflags = *pos++;		
		const bool hasProtected = (theflags & 8) != 0;
		if (hasProtected)
		{
			AvmCore::skipU30(pos);
		}
		const uint32_t interfaceCount = AvmCore::readU30(pos);
		AvmCore::skipU30(pos, interfaceCount);
		return pos;
	}

	static bool is8ByteSlot(Traits* slotTE)
	{
		#ifdef AVMPLUS_64BIT
		const uint32_t BIG_TYPE_MASK = ~((1U<<BUILTIN_int) | (1U<<BUILTIN_uint) | (1U<<BUILTIN_boolean));
		#else
		const uint32_t BIG_TYPE_MASK = (1U<<BUILTIN_number);
		#endif
		
		return ((1 << Traits::getBuiltinType(slotTE)) & BIG_TYPE_MASK) != 0;
	}

	static int32_t pad4(int32_t& hole, int32_t& nextSlotOffset)
	{
		// 4-aligned, 4-byte field
		int32_t slotOffset;
		if (hole != -1)
		{
			// this is a trick: if we have 4-8-4, this allows us
			// to quietly rearrange into 4-4-8
			slotOffset = hole;
			hole = -1;
		}
		else
		{
			slotOffset = nextSlotOffset;
			nextSlotOffset += 4;
		}
		return slotOffset;
	}
	
	static int32_t pad8(int32_t& hole, int32_t& nextSlotOffset)
	{
		// 8-aligned, 8-byte field
		if (nextSlotOffset & 7)
		{
			AvmAssert((nextSlotOffset % 4) == 0);	// should always be a multiple of 4
			hole = nextSlotOffset;
			nextSlotOffset += 4;
		}
		int32_t slotOffset = nextSlotOffset;
		nextSlotOffset += 8;
		return slotOffset;
	}

	static SlotStorageType bt2sst(BuiltinType bt)
	{
		AvmAssert(bt != BUILTIN_void);
		switch (bt)
		{
			case BUILTIN_int:		return SST_int32;
			case BUILTIN_uint:		return SST_uint32;
			case BUILTIN_number:	return SST_double;
			case BUILTIN_boolean:	return SST_bool32;
			case BUILTIN_any:		return SST_atom;
			case BUILTIN_object:	return SST_atom;
			case BUILTIN_string:	return SST_string;
			case BUILTIN_namespace:	return SST_namespace;
			default:				return SST_scriptobject;
		}
	}

	// the logic for assigning slot id's is used in several places, so it's now collapsed here
	// rather than redundantly sprinkled thru several bits of code.
	class SlotIdCalcer
	{
	private:
		uint32_t m_slotCount;
		bool m_earlySlotBinding;
	public:
		SlotIdCalcer(uint32_t _baseSlotCount, bool _earlySlotBinding) : 
			m_slotCount(_baseSlotCount), 
			m_earlySlotBinding(_earlySlotBinding) 
		{
		}

		uint32_t calc_id(uint32_t id)
		{
			if (!id || !m_earlySlotBinding)
			{
				id = ++m_slotCount;
			}
			if (m_slotCount < id)
				m_slotCount = id;
			return id - 1;
		}
		
		uint32_t slotCount() const { return m_slotCount; }
	};
	
	struct NameEntry
	{
		const uint8_t* meta_pos;
		uint32_t qni, id, info, value_index;
		CPoolKind value_kind;
		TraitKind kind;
		uint8_t tag;
		
		void readNameEntry(const uint8_t*& pos);
	};
	
	void NameEntry::readNameEntry(const uint8_t*& pos)
	{
		qni = AvmCore::readU30(pos);
		tag = *pos++;
		kind = (TraitKind) (tag & 0x0f);
		value_kind = CONSTANT_unused_0x00;
		value_index = 0;

		// Read in the trait entry.
		switch (kind)
		{
			case TRAIT_Slot:
			case TRAIT_Const:
				id = AvmCore::readU30(pos);				// slot id
				info = AvmCore::readU30(pos);			// value type
				value_index = AvmCore::readU30(pos);	// value index
				if (value_index)
					value_kind = (CPoolKind)*pos++;		// value kind
				break;
			case TRAIT_Class:
				id = AvmCore::readU30(pos);		// slot id
				info = AvmCore::readU30(pos);	// classinfo
				break;
			case TRAIT_Getter:
			case TRAIT_Setter:
			case TRAIT_Method:
				AvmCore::skipU30(pos);			// disp id (never used)
				id = AvmCore::readU30(pos);		// method index
				info = 0;
				break;
			default:
				// unsupported traits type -- can't happen, caught in AbcParser::parseTraits
				AvmAssert(0);
				break;
		}
		meta_pos = pos;
		if (tag & ATTR_metadata)
		{
			uint32_t metaCount = AvmCore::readU30(pos);
			AvmCore::skipU30(pos, metaCount);
		}
	}
	
 	bool Traits::allowEarlyBinding() const
 	{
 		// the compiler can early bind to a type's slots when it's defined
 		// or when the base class came from another abc file and has zero slots
 		// this ensures you cant use the early opcodes to access an external type's
 		// private members.
		TraitsBindingsp tb = this->base ? this->base->getTraitsBindings() : NULL;
		while (tb != NULL && tb->slotCount > 0)
		{
			if (tb->owner->pool != this->pool && tb->slotCount > 0)
			{
				return false;
			}
			tb = tb->base;
		}
		return true;
	}

	void Traits::buildBindings(TraitsBindingsp basetb, 
								MultinameHashtable* bindings, 
								uint32_t& slotCount, 
								uint32_t& methodCount,
								const Toplevel* toplevel) const
	{
		const uint8_t* pos = traitsPosStart();

		const uint32_t baseSlotCount = basetb ? basetb->slotCount : 0;
		const uint32_t baseMethodCount = basetb ? basetb->methodCount : 0;

		//slotCount = baseSlotCount;
		methodCount = baseMethodCount;
		
		SlotIdCalcer sic(baseSlotCount, this->allowEarlyBinding());

		NameEntry ne;
		const uint32_t nameCount = pos ? AvmCore::readU30(pos) : 0;
		for (uint32_t i = 0; i < nameCount; i++)
		{
			ne.readNameEntry(pos);
			Multiname mn;
			this->pool->resolveQName(ne.qni, mn, toplevel);
			Stringp name = mn.getName();
			Namespacep ns;
			NamespaceSetp compat_nss;
			if (mn.namespaceCount() > 1) {
				ns = mn.getNsset()->namespaces[0];
				compat_nss = mn.getNsset();
			}
			else {
				ns = mn.getNamespace();
				compat_nss = new (core->GetGC()) NamespaceSet(ns);
			}

			switch (ne.kind)
			{
				case TRAIT_Slot:
				case TRAIT_Const:
				case TRAIT_Class:
				{
					uint32_t slot_id = sic.calc_id(ne.id);
					if (toplevel)
					{
						AvmAssert(!this->linked);
						
						// first time thru, we must do some additional verification checks... these were formerly 
						// done in AbcParser::parseTraits but require the base class to be resolved first, so we
						// now defer it to here.
						
						// illegal raw slot id.
						if (ne.id > nameCount)
							toplevel->throwVerifyError(kCorruptABCError);
						
						// slots are final.
						if (basetb && slot_id < basetb->slotCount) 
							toplevel->throwVerifyError(kIllegalOverrideError, core->toErrorString(mn), core->toErrorString(base));

						// a slot cannot override anything else.
						if (bindings->get(name, ns) != BIND_NONE)
							toplevel->throwVerifyError(kCorruptABCError);

						// In theory we should reject duplicate slots here; 
						// in practice we don't, as it causes problems with some existing content
						//if (basetb->findBinding(name, ns) != BIND_NONE)
						//	toplevel->throwVerifyError(kIllegalOverrideError, toplevel->core()->toErrorString(qn), toplevel->core()->toErrorString(this));
						
						// Interfaces cannot have slots.
						if (this->isInterface)
							toplevel->throwVerifyError(kIllegalSlotError, core->toErrorString(this));

					}
					AvmAssert(!(ne.id > nameCount));						// unhandled verify error
					AvmAssert(!(basetb && slot_id < basetb->slotCount));	// unhandled verify error
					AvmAssert(!(bindings->get(name, ns) != BIND_NONE));		// unhandled verify error
					addVersionedBindings(bindings, name, compat_nss, AvmCore::makeSlotBinding(slot_id, ne.kind==TRAIT_Slot ? BKIND_VAR : BKIND_CONST));
					break;
				}
				case TRAIT_Method:
				{
					Binding baseBinding = this->getOverride(basetb, ns, name, ne.tag, toplevel);
					if (baseBinding == BIND_NONE)
					{
						addVersionedBindings(bindings, name, compat_nss, AvmCore::makeMGSBinding(methodCount, BKIND_METHOD));
						// accessors require 2 vtable slots, methods only need 1.
						methodCount += 1;
					}
					else if (AvmCore::isMethodBinding(baseBinding))
					{
						// something got overridden, need new name entry for this subclass
						// but keep the existing disp_id
						addVersionedBindings(bindings, name, compat_nss, baseBinding);
					}
					else
					{
						if (toplevel)
							toplevel->throwVerifyError(kCorruptABCError);
						AvmAssert(!"unhandled verify error");
					}
					break;
				}
				case TRAIT_Getter:
				case TRAIT_Setter:
				{
					// if nothing already is defined in this class, use base class in case getter/setter has already been defined.
					Binding baseBinding = bindings->get(name, ns);
					if (baseBinding == BIND_NONE)
						baseBinding = this->getOverride(basetb, ns, name, ne.tag, toplevel);
						
					const BindingKind us = (ne.kind == TRAIT_Getter) ? BKIND_GET : BKIND_SET;
					const BindingKind them = (ne.kind == TRAIT_Getter) ? BKIND_SET : BKIND_GET;
					if (baseBinding == BIND_NONE)
					{
						addVersionedBindings(bindings, name, compat_nss, AvmCore::makeMGSBinding(methodCount, us));
						// accessors require 2 vtable slots, methods only need 1.
						methodCount += 2;
					}
					else if (AvmCore::isAccessorBinding(baseBinding))
					{
						// something maybe got overridden, need new name entry for this subclass
						// but keep the existing disp_id
						// both get & set bindings use the get id.	set_id = get_id + 1.
						if (AvmCore::bindingKind(baseBinding) == them)
						{
							baseBinding = AvmCore::makeGetSetBinding(baseBinding);
						}
						addVersionedBindings(bindings, name, compat_nss, baseBinding);
					}
					else
					{
						if (toplevel)
							toplevel->throwVerifyError(kCorruptABCError);
						AvmAssert(!"unhandled verify error");
					}
					break;
				}

				default:
					// unsupported traits type -- can't happen, caught in AbcParser::parseTraits
					AvmAssert(0);
					break;
			}
		} // for i
		
		slotCount = sic.slotCount();
	}

	uint32_t Traits::finishSlotsAndMethods(TraitsBindingsp basetb, 
									TraitsBindings* tb, 
									const Toplevel* toplevel,
									AbcGen* abcGen) const
	{
		const uint8_t* pos = traitsPosStart();

		SlotIdCalcer sic(basetb ? basetb->slotCount : 0, this->allowEarlyBinding());
		int32_t nextSlotOffset = this->getSlotAreaStart();
		int32_t hole = -1;

		NameEntry ne;
		const uint32_t nameCount = pos ? AvmCore::readU30(pos) : 0;
		for (uint32_t i = 0; i < nameCount; i++)
        {
			ne.readNameEntry(pos);
			Multiname mn;
			this->pool->resolveQName(ne.qni, mn, toplevel);
			Namespacep ns = mn.getNamespace();
			Stringp name = mn.getName();
			// NOTE only one versioned namespace from the set needed here

			switch (ne.kind)
            {
				case TRAIT_Slot:
				case TRAIT_Const:
				case TRAIT_Class:
				{
					uint32_t slotid = sic.calc_id(ne.id);
					// note, for TRAIT_Class, AbcParser::parseTraits has already verified that pool->cinits[ne.info] is not null
					Traitsp slotType = (ne.kind == TRAIT_Class) ? 
										pool->getClassTraits(ne.info) :
										pool->resolveTypeName(ne.info, toplevel);
					uint32_t slotOffset = is8ByteSlot(slotType) ? 
											pad8(hole, nextSlotOffset) : 
											pad4(hole, nextSlotOffset);	// all slots get at least 4 bytes, even bool
					tb->setSlotInfo(slotid, slotType, bt2sst(getBuiltinType(slotType)), slotOffset);
					
					if (abcGen)
						genDefaultValue(ne.value_index, slotid, toplevel, slotType, ne.value_kind, *abcGen);
					break;
				}
				case TRAIT_Getter:
				case TRAIT_Setter:
				case TRAIT_Method:
				{
					const Binding b = tb->m_bindings->get(name, ns);
					AvmAssert(b != BIND_NONE);
					const uint32 disp_id = uint32(uintptr_t(b) >> 3) + (ne.kind == TRAIT_Setter);
					MethodInfo* f = this->pool->getMethodInfo(ne.id);
					//AvmAssert(f->declaringTraits() == this);
					tb->setMethodInfo(disp_id, f);
					break;
				}

				default:
					// unsupported traits type -- can't happen, caught in AbcParser::parseTraits
					AvmAssert(0);
					break;
            }
        } // for i
		
		// check for sparse slot table -- anything not specified will default to * (but we must allocate space for it)
		for (uint32_t i = 0; i < tb->slotCount; i++)
		{
			if (tb->getSlotOffset(i) > 0)
				continue;

			#ifdef AVMPLUS_VERBOSE
			if (pool->verbose)
			{
				core->console << "WARNING: slot " << i+1 << " on " << this << " not defined by compiler.  Using *\n";
			}
			#endif

			const Traitsp slotType = NULL;
			const uint32_t slotOffset = is8ByteSlot(slotType) ? 
									pad8(hole, nextSlotOffset) : 
									pad4(hole, nextSlotOffset);
			tb->setSlotInfo(i, slotType, SST_atom, slotOffset);
		}
		return nextSlotOffset;
	}

	static const uint8_t* skipToInterfaceCount(const uint8_t* pos)
	{
		AvmAssert(pos != NULL);
		AvmCore::skipU30(pos, 2);		// skip the QName + basetraits
		const uint8_t theflags = *pos++;		
		if (theflags & 8)
			AvmCore::skipU30(pos);	// skip protected namespace
		return pos;
	}
	
	// Flex apps often have many interfaces redundantly listed, so first time thru,
	// eliminate redundant ones.
	void Traits::countInterfaces(const Toplevel* toplevel, List<Traitsp, LIST_NonGCObjects>& seen)
	{
		for (Traitsp self = this; self != NULL; self = self->base)
		{
			if (seen.indexOf(self) < 0)
				seen.add(self);

			if (self->posType() == TRAITSTYPE_INSTANCE_FROM_ABC)
			{
				const uint8_t* pos = skipToInterfaceCount(self->m_traitsPos);
				const uint32_t interfaceCount = AvmCore::readU30(pos);
				for (uint32_t j = 0; j < interfaceCount; j++)
				{
					Traitsp intf = self->pool->resolveTypeName(pos, toplevel);
					AvmAssert(intf && intf->isInterface);
					// an interface can "extend" multiple other interfaces, so we must recurse here.
					intf->countInterfaces(toplevel, seen);
				}
			}
		}
	}
	
	// Note that the interface list for a given TraitsBindings is flat and inclusive.
	// in the original version of Tamarin, the interface list was flat, and included
	// all Interfaces implemented by the Traits, as well as all parent Traits... but
	// not the Traits itself (ie "this" is missing). 
	//
	// when TraitsBindings were added, we changed the interface list to be hierarchical,
	// so that only the interfaces not implemented by parents were in the list... thus
	// you had to walk up the tree to check for interface implementation. this proved
	// to be too slow (eg for containsInterface which is used heavily by coerceEnter),
	// thus the current implementation has gone back to a flat implementation, but
	// with 'this' included in the list this time.
	//
	// aside from containsInterface, this affected a few other areas of the code that
	// thought they had to walk the TraitsBindings inheritance tree to get all
	// interfaces (notable IMT thunk generation and TypeDescriber).
	bool Traits::addInterfaces(TraitsBindings* tb, const Toplevel* toplevel)
	{
		bool addedNewInterface = false;
		
		if (this->posType() == TRAITSTYPE_INSTANCE_FROM_ABC)
		{
			const uint8_t* pos = skipToInterfaceCount(this->m_traitsPos);
			const uint32_t interfaceCount = AvmCore::readU30(pos);
			for (uint32_t j = 0; j < interfaceCount; j++)
			{
				// never need to pass toplevel here: we've already validated the typenames in AbcParser::parseInstanceInfos
				Traitsp ifc = this->pool->resolveTypeName(pos, /*toplevel*/NULL);
				AvmAssert(ifc && ifc->isInterface);
				if (tb->addOneInterface(ifc))
				{
					addedNewInterface = true;
					tb->fixOneInterfaceBindings(ifc, toplevel);
				}

				// an interface can "extend" multiple other interfaces, so we must recurse here.
				if (ifc->addInterfaces(tb, toplevel))
					addedNewInterface = true;
			}
		}

		return addedNewInterface;
	}

	static uint8_t calcLog2(uint32_t cap)
	{
		uint8_t capLog = 1; // start with at least 2 entries
		while ((1U<<capLog) < cap)
		{
			++capLog;
		}
		AvmAssert((1U<<capLog) >= cap);
		return capLog;
	}

	TraitsBindings* Traits::_buildTraitsBindings(const Toplevel* toplevel, AbcGen* abcGen)
	{
		// no, this can be called before the resolved bit is set
		//AvmAssert(this->linked);
#ifdef AVMPLUS_VERBOSE
		if (pool->verbose)
		{
			core->console << "Generate TraitsBindings for "<<this<<"\n";
		}
#endif
		TraitsBindings* thisData = NULL;

		// if we know the cap we need, go there right away, otherwise start at small power of 2
		// this saves tremendously on subsequent builds of this set of bindings since we don't have to
		// waste time growing the MNHT as we build it
		const int32_t bindingCap = m_bindingCapLog2 ? (1 << m_bindingCapLog2) : 2;

		MMgc::GC* gc = core->GetGC();
		MultinameHashtable* bindings = new (gc) MultinameHashtable(bindingCap);
		AvmAssert(bindings->numQuads == bindingCap);
		
		if (this->posType() == TRAITSTYPE_CATCH)
		{
			const uint8_t* pos = m_traitsPos;

			Traits* t = this->pool->resolveTypeName(pos, toplevel);

			// this assumes we save name/ns in all builds, not just verbose
			NamespaceSetp nss = new (core->GetGC()) NamespaceSet(this->ns());
			NamespaceSetp compat_nss = nss;
			addVersionedBindings(bindings, this->name(), compat_nss, AvmCore::makeSlotBinding(0, BKIND_VAR));
			// the Sampler can call containsInterface() on a catch object, so we must ensure the list is valid.
			// we add ourself to it, and since the algorithm requires at least one unused entry in the table, we use a size of 2, not 1.
			thisData = TraitsBindings::alloc(gc, this, /*base*/NULL, bindings, /*slotCount*/1, /*methodCount*/0, /*interfaceCap*/2);
			thisData->setSlotInfo(0, t, bt2sst(getBuiltinType(t)), this->m_sizeofInstance);
			thisData->m_slotSize = is8ByteSlot(t) ? 8 : 4;
			thisData->addOneInterface(this);
		}
		else
		{
			if (m_interfaceCapLog2 == 0)
			{
				List<Traitsp, LIST_NonGCObjects> seen(gc);
				countInterfaces(toplevel, seen);
				// a little redundant, but clarity is king
				m_interfaceCapLog2 = calcLog2(MathUtils::nextPowerOfTwo((5*seen.size() >> 2) + 1));
				AvmAssert(m_interfaceCapLog2 > 0);
			}

			TraitsBindingsp basetb = this->base ? this->base->getTraitsBindings() : NULL;

			// Copy protected traits from base class into new protected namespace
			if (basetb && base->protectedNamespace && this->protectedNamespace)
			{
				StTraitsBindingsIterator iter(basetb);
				while (iter.next())
				{
					if (!iter.key()) continue;
					if (iter.ns() == base->protectedNamespace)
					{
						bindings->add(iter.key(), this->protectedNamespace, iter.value());
					}
				}
			}
			
			uint32_t slotCount = 0;
			uint32_t methodCount = 0;
			buildBindings(basetb, bindings, slotCount, methodCount, toplevel);
			
			thisData = TraitsBindings::alloc(gc, this, basetb, bindings, slotCount, methodCount, (1U << m_interfaceCapLog2));
			
			thisData->m_slotSize = finishSlotsAndMethods(basetb, thisData, toplevel, abcGen) - m_sizeofInstance;

			// we implement everything our parent does, so re-add from there 
			// rather than walking the whole hierarchy:
			if (basetb)
			{
				const TraitsBindings::InterfaceInfo* tbi = basetb->getInterfaces();
				const TraitsBindings::InterfaceInfo* tbi_end = tbi + basetb->interfaceCapacity;
				for ( ; tbi < tbi_end; ++tbi) 
				{
					if (tbi->t)
						thisData->addOneInterface(tbi->t);
				}
			}
			thisData->addOneInterface(this);
			this->m_implementsNewInterfaces = addInterfaces(thisData, toplevel);
		}

		// hashtable (if we have one) must start on pointer-sized boundary...
		// much easier to always round slotsize up unconditionally rather than
		// only for cases with hashtable, so that's what we'll do. (MMgc currently
		// allocate in 8-byte increments anyway, so we're not really losing any space.)
		thisData->m_slotSize = (thisData->m_slotSize + (sizeof(uintptr_t)-1)) & ~(sizeof(uintptr_t)-1);
		
		// remember the cap we need
		if (m_bindingCapLog2 == 0)
			m_bindingCapLog2 = calcLog2(thisData->m_bindings->numQuads);	// remember capacity, not count
		AvmAssert(m_bindingCapLog2 > 0);

#ifdef AVMPLUS_VERBOSE
		if (pool->verbose)
		{
			core->console << this << " bindings\n";
			StTraitsBindingsIterator iter(thisData);
			while (iter.next())
			{
				core->console << iter.key() << ":" << (uint32_t)(uintptr_t)(iter.value()) << "\n";
			}
			core->console << this << " end bindings \n";
		}
#endif
		
		AvmAssert(m_tbref->get() == NULL);
		m_tbref = thisData->GetWeakRef();
		core->tbCache()->add(thisData);
		return thisData;
	}

	TraitsMetadata* Traits::_buildTraitsMetadata()
	{
		AvmAssert(this->linked);

#ifdef AVMPLUS_VERBOSE
		if (pool->verbose)
		{
			core->console << "Generate TraitsMetadata for "<<this<<"\n";
		}
#endif
		TraitsBindingsp td = this->getTraitsBindings();
		
		MMgc::GC* gc = core->GetGC();
		
		TraitsMetadatap basetm = this->base ? this->base->getTraitsMetadata() : NULL;

		const uint32_t extra = td->slotCount * sizeof(TraitsMetadata::MetadataPtr) + td->methodCount * sizeof(TraitsMetadata::MetadataPtr);

		TraitsMetadata* tm = new (gc, extra) TraitsMetadata(basetm, this->pool, this->metadata_pos, td->slotCount, td->methodCount);
		tm->slotMetadataPos = (TraitsMetadata::MetadataPtr*)(tm + 1);
		tm->methodMetadataPos = (TraitsMetadata::MetadataPtr*)(tm->slotMetadataPos + tm->slotCount);

		const uint8_t* pos = traitsPosStart();
		const uint32_t nameCount = pos ? AvmCore::readU30(pos) : 0;
		SlotIdCalcer sic(td->base ? td->base->slotCount : 0, this->allowEarlyBinding());
		NameEntry ne;
		for (uint32_t i = 0; i < nameCount; i++)
        {
			ne.readNameEntry(pos);
			switch (ne.kind)
            {
				case TRAIT_Class:
					AvmAssert(0);
					// classes shouldn't have metadata, but just fall thru just in case
				case TRAIT_Slot:
				case TRAIT_Const:
				{
					const uint32_t slot_id = sic.calc_id(ne.id);
					if (ne.tag & ATTR_metadata)
						tm->slotMetadataPos[slot_id] = ne.meta_pos;
					break;
				}
				case TRAIT_Getter:
				case TRAIT_Setter:
				case TRAIT_Method:
				{
					if (ne.tag & ATTR_metadata)
					{
						Multiname qn;
						// passing NULL for toplevel here, since it's only used if a verification error occurs -- 
						// but if there was one, we would have encountered it during AbcParser::parseTraits.
						this->pool->resolveQName(ne.qni, qn, /*toplevel*/NULL);
						const Binding b = td->findBinding(qn.getName(), qn.getNamespace());
						AvmAssert(b != BIND_NONE);
						const uint32 disp_id = uint32(uintptr_t(b) >> 3) + (ne.kind == TRAIT_Setter);
						tm->methodMetadataPos[disp_id] = ne.meta_pos;
					}
					break;
				}

				default:
					// unsupported traits type -- can't happen, caught in AbcParser::parseTraits
					AvmAssert(0);
					break;
			}
        } // for i
			
		AvmAssert(m_tmref->get() == NULL);
		m_tmref = tm->GetWeakRef();
		core->tmCache()->add(tm);
		return tm;
	}

	void Traits::init_declaringScopes(const ScopeTypeChain* stc) 
	{ 
		AvmAssert(linked);
		if (!linked)
			return;
		
		if (this->init)
			this->init->init_declaringScope(stc);

		{
			TraitsBindingsp tb = this->getTraitsBindings();
			const TraitsBindings::BindingMethodInfo* tbm		= tb->getMethods();
			const TraitsBindings::BindingMethodInfo* tbm_end	= tbm + tb->methodCount;
			for ( ; tbm < tbm_end; ++tbm) 
			{
				if (tbm->f == NULL)
					continue;
					
				if (tbm->f->declaringTraits() == this)
				{
					tbm->f->init_declaringScope(stc);
				}
			}
		}
	}

	/**
	 * This must be called before any method is verified or any
	 * instances are created.  It is not done eagerly in AbcParser
	 * because doing so would prevent circular type references between
	 * slots of cooperating classes.
     *
     * Resolve the type and position/width of each slot.
	 */
	void Traits::resolveSignatures(const Toplevel* toplevel)
	{
		// toplevel actually can be null, when resolving the builtin classes...
		// but they should never cause verification errors in functioning builds
		//AvmAssert(toplevel != NULL);
		
		if (linked)
			return;

		MMgc::GC* gc = core->GetGC();
		for (Traits* t = this->base; t != NULL; t = t->base)
		{
			// make sure our base classes our resolved. (must be done before calling _buildTraitsBindings)
			if (!t->linked) 
				t->resolveSignatures(toplevel);
		}

		AbcGen gen(gc);	
		TraitsBindings* tb = _buildTraitsBindings(toplevel, &gen);
		this->genInitBody(toplevel, gen);

		// leave m_tmref as empty, we don't need it yet

		// all interfaces should have been resolved inside _buildTraitsBindings, UNLESS we are an interface ourself...
		// in which case let's do it now
		if (this->isInterface)
		{
			TraitsBindings::InterfaceInfo* tbi = tb->getInterfaces();
			TraitsBindings::InterfaceInfo* tbi_end = tbi + tb->interfaceCapacity;
			for ( ; tbi < tbi_end; ++tbi) 
			{
				Traits* ti = tbi->t;
				if (!ti || ti->linked || ti == this) continue;
				ti->resolveSignatures(toplevel);
			}
		}

		switch (posType())
		{
			case TRAITSTYPE_INSTANCE_FROM_ABC:
			case TRAITSTYPE_CLASS_FROM_ABC:
			case TRAITSTYPE_SCRIPT_FROM_ABC:
			case TRAITSTYPE_ACTIVATION:
			case TRAITSTYPE_CATCH:
				m_totalSize = m_sizeofInstance + tb->m_slotSize;
				break;
			case TRAITSTYPE_NVA:
			case TRAITSTYPE_RT:
				m_totalSize = m_sizeofInstance;
				break;
		}
		
		AvmAssert(m_totalSize >= m_sizeofInstance);
		if (m_needsHashtable || (base && base->base && base->m_hashTableOffset && !isXMLType()))
		{
			// slotSize is already rounded up to pointer-sized boundary, but totalsize might not be
			// (eg for bool/int/uint, which have weird sizes)
			m_totalSize = ((m_totalSize+(sizeof(uintptr_t)-1))&~(sizeof(uintptr_t)-1));
			m_hashTableOffset = m_totalSize;
			m_totalSize += sizeof(InlineHashtable);
			AvmAssert(builtinType == BUILTIN_boolean ? true : (m_hashTableOffset & 3) == 0);
			AvmAssert((m_hashTableOffset & (sizeof(uintptr_t)-1)) == 0);
			AvmAssert((m_totalSize & (sizeof(uintptr_t)-1)) == 0);
		}
		
		// make sure all the methods have resolved types
		{
			const TraitsBindings::BindingMethodInfo* tbm		= tb->getMethods();
			const TraitsBindings::BindingMethodInfo* tbm_end	= tbm + tb->methodCount;
			for ( ; tbm < tbm_end; ++tbm) 
			{
				// don't assert: could be null if only one of a get/set pair is implemented
				//AvmAssert(tbm->f != NULL);
				if (tbm->f != NULL)
				{
					tbm->f->resolveSignature(toplevel);
				}
			}
		}

		if (this->init != NULL)
			this->init->resolveSignature(toplevel);

		bool legal = true;
		TraitsBindingsp tbbase = tb->base;	// might be null

		if (tbbase && tbbase->methodCount > 0)
		{
			// check concrete overrides
			const TraitsBindings::BindingMethodInfo* basetbm		= tbbase->getMethods();
			const TraitsBindings::BindingMethodInfo* basetbm_end	= basetbm + tbbase->methodCount;
			const TraitsBindings::BindingMethodInfo* tbm			= tb->getMethods();
			for ( ; basetbm < basetbm_end; ++basetbm, ++tbm) 
			{
				if (basetbm->f != NULL && basetbm->f != tbm->f)
					legal &= tb->checkOverride(core, basetbm->f, tbm->f);
			}
		}

		if (legal && !this->isInterface)
		{
			legal &= tb->checkLegalInterfaces(core);
		}

		if (!legal)
		{
			AvmAssert(!linked);
			Multiname qname(ns(), name());
			if (toplevel)
				toplevel->throwVerifyError(kIllegalOverrideError, core->toErrorString(&qname), core->toErrorString(this));
			AvmAssert(!"unhandled verify error");
		}

		tb->buildSlotDestroyInfo(gc, m_slotDestroyInfo);

		linked = true;
	}

	// static
	bool Traits::isMachineCompatible(const Traits* a, const Traits* b)
	{
		return (a == b) ||
			// *, Object, and Void are each represented as Atom
			((!a || a->builtinType == BUILTIN_object || a->builtinType == BUILTIN_void) &&
			(!b || b->builtinType == BUILTIN_object || b->builtinType == BUILTIN_void)) ||
			// all other non-pointer types have unique representations
			(a && b && !a->isMachineType() && !b->isMachineType());
	}

#if VMCFG_METHOD_NAMES
	Stringp Traits::format(AvmCore* core, bool includeAllNamespaces) const
	{
		if (name() != NULL)
			return Multiname::format(core, ns(), name(), false, !includeAllNamespaces);

		return core->concatStrings(core->newConstantStringLatin1("Traits@"),
									   core->formatAtomPtr((uintptr)this));
	}
#endif

	void Traits::genDefaultValue(uint32_t value_index, uint32_t slot_id, const Toplevel* toplevel, Traitsp slotType, CPoolKind kind, AbcGen& gen) const
	{
		// toplevel actually can be null, when resolving the builtin classes...
		// but they should never cause verification errors in functioning builds
		//AvmAssert(toplevel != NULL);

		Atom value = pool->getLegalDefaultValue(toplevel, value_index, kind, slotType);
		switch (Traits::getBuiltinType(slotType))
		{
			case BUILTIN_any:
			case BUILTIN_object:
			{
				if (value == 0)
					return;

				break;
			}
			case BUILTIN_number:
			{
				if (AvmCore::number_d(value) == 0)
					return;
				break;
			}
			case BUILTIN_boolean:
			{
				AvmAssert((uintptr_t(falseAtom)>>3) == 0);
				if (value == falseAtom)
					return;
				
				AvmAssert(value == trueAtom);
				break;
			}
			case BUILTIN_uint:
			case BUILTIN_int:
			{
				if (value == (0|kIntegerType))
					return;

				break;
			}
			case BUILTIN_namespace:
			case BUILTIN_string:
			default:
			{
				if (AvmCore::isNull(value))
					return;

				break;
			}
		}

		gen.getlocalN(0);
		if (value == undefinedAtom)
			gen.pushundefined();
		else if (AvmCore::isNull(value))
			gen.pushnull();
		else if (value == core->kNaN)
			gen.pushnan();
		else if (value == trueAtom)
			gen.pushtrue();
		else
			gen.pushconstant(kind, value_index);
		gen.setslot(slot_id);				
	}

	void Traits::genInitBody(const Toplevel* toplevel, AbcGen& gen)
	{
		// if initialization code gen is required, create a new method body and write it to traits->init->body_pos
		if (gen.size() == 0)
			return;
		
		MMgc::GC* gc = core->GetGC();
		AbcGen newMethodBody(gc, uint32_t(16 + gen.size()));	// @todo 16 is a magic value that was here before I touched the code -- I don't know the significance

		// insert body preamble
		if (this->init) 
		{
			const uint8_t* pos = this->init->abc_body_pos();
			if (!pos) 
				toplevel->throwVerifyError(kCorruptABCError);

			uint32_t maxStack = AvmCore::readU30(pos);
			// the code we're generating needs at least 2
			maxStack = maxStack > 1 ? maxStack : 2;
			newMethodBody.writeInt(maxStack); // max_stack
			newMethodBody.writeInt(AvmCore::readU30(pos)); //local_count
			newMethodBody.writeInt(AvmCore::readU30(pos)); //init_scope_depth
			newMethodBody.writeInt(AvmCore::readU30(pos)); //max_scope_depth

			// skip real code length
			uint32_t code_length = AvmCore::readU30(pos);
		
			// if first instruction is OP_constructsuper keep it as first instruction
			if (*pos == OP_constructsuper)
			{
				gen.getBytes().insert(0, OP_constructsuper);
				// don't invoke it again later
				pos++;
				code_length--;
			}
			gen.abs_jump(pos, code_length);	
			
			// this handles an obscure case: we have already resolved the signature for this
			// and have a MethodSignature cached, but we just (potentially) increased the value of
			// max_stack above. This updates the cached value of max_stack (iff we have
			// a MethodSignature cached for this->init)
			this->init->update_max_stack(maxStack);
		}
		else 
		{
			// make one
			this->init = new (gc) MethodInfo(MethodInfo::kInitMethodStub, this);

			newMethodBody.writeInt(2); // max_stack
			newMethodBody.writeInt(1); //local_count
			newMethodBody.writeInt(1); //init_scope_depth
			newMethodBody.writeInt(1); //max_scope_depth

			gen.returnvoid();
		}

		newMethodBody.writeInt((uint32_t)gen.size()); // code length
		newMethodBody.writeBytes(gen.getBytes());

		// no exceptions, when we jump to the real code, we'll read the exceptions for that code
		newMethodBody.writeInt(0);

		// the verifier and interpreter don't read the activation traits so stop here
		uint8_t* newBytes = (uint8_t*) gc->Alloc(newMethodBody.size());
		VMPI_memcpy(newBytes, newMethodBody.getBytes().getData(), newMethodBody.size());
		this->init->set_abc_body_pos_wb(gc, newBytes);
	}

	void Traits::destroyInstance(ScriptObject* obj) const
	{
		AvmAssert(linked);

		InlineHashtable* ht = m_hashTableOffset ? obj->getTableNoInit() : NULL;

		// start by clearing native space to zero (except baseclasses)
		union {
			char* p_8;
			uint32_t* p;
		};
		p_8 = (char*)obj + sizeof(AvmPlusScriptableObject);
		AvmAssert((uintptr_t(p) & 0x3) == 0);
		const uint32_t mysize = m_sizeofInstance - uint32_t(sizeof(AvmPlusScriptableObject));
		AvmAssert((mysize & 0x3) == 0); // we assume all sizes are multiples of 4

		const uint32_t slotAreaSize = getSlotAreaSize();
		if (!m_slotDestroyInfo.test(0))
		{
			AvmAssert(m_slotDestroyInfo.cap() == 1);
			// no RCObjects, so just zero it all... my, that was easy
			VMPI_memset(p, 0, mysize + slotAreaSize);
		}
		else
		{
			VMPI_memset(p, 0, mysize);
			p += (mysize>>2);

			AvmAssert(m_slotDestroyInfo.cap() >= 1);
			AvmAssert((uintptr_t(p) & 3) == 0);
			const uint32_t bitsUsed = slotAreaSize / sizeof(uint32_t);	// not sizeof(Atom)!
			for (uint32_t bit = 1; bit <= bitsUsed; bit++) 
			{
				if (m_slotDestroyInfo.test(bit))
				{
					#ifdef AVMPLUS_64BIT
					AvmAssert((uintptr_t(p) & 7) == 0);	// we had better be on an 8-byte boundary...
					#endif
					Atom a = *(const Atom*)p;
					RCObject* rc = NULL;
					if (atomKind(a) <= kNamespaceType)
					{
						rc = (RCObject*)atomPtr(a);
						if (rc)
						{
							AvmAssert(GC::GetGC(obj)->IsRCObject(rc));
							rc->DecrementRef();
						}
					}
				}
				*p++ = 0;
			}
		}

		// finally, zap the hashtable (if any)
		if(ht)
		{
			ht->destroy();
		}
		//for DictionaryObject also zero out the 
		//hashtable pointer stored at the offset address;
		if(isDictionary)
		{
			union {
				char* p_8;
				uintptr_t* ptr;
			};
			p_8 = (char*)obj + m_hashTableOffset;
			*ptr = 0;
		}
	}

	Stringp Traits::formatClassName()
	{
		Multiname qname(ns(), name());
		qname.setQName();
		StringBuffer buffer(core);
		buffer << qname;
		int length = buffer.length();
		if (length && buffer.c_str()[length-1] == '$') 
		{
			length--;
		} 
		return core->newStringUTF8(buffer.c_str(), length);
	}


	Binding Traits::getOverride(TraitsBindingsp basetb, Namespacep ns, Stringp name, int tag, const Toplevel* toplevel) const
	{
		Binding baseBinding = BIND_NONE;
		if (base)
		{
			const Namespacep lookupNS = (protectedNamespace == ns && base->protectedNamespace) ? (Namespacep)base->protectedNamespace : ns;
			AvmAssert(basetb != NULL);
			baseBinding = basetb->findBinding(name, lookupNS);
		}
		const BindingKind baseBindingKind = AvmCore::bindingKind(baseBinding);

		const TraitKind kind = TraitKind(tag & 0x0f);
		// some extra-picky compilers will complain about the values being out of
		// range for the comparison (if we use "kind") even with explicit int casting.
		// so recycle the expression for the assert.
		AvmAssert((tag & 0x0f) >= 0 && (tag & 0x0f) < TRAIT_COUNT); 

		static const uint8_t kDesiredKind[TRAIT_COUNT] = 
		{
			BKIND_NONE,						// TRAIT_Slot
			BKIND_METHOD,					// TRAIT_Method
			BKIND_GET,						// TRAIT_Getter
			BKIND_SET,						// TRAIT_Setter
			BKIND_NONE,						// TRAIT_Class
			BKIND_NONE,						// TRAIT_Function
			BKIND_NONE						// TRAIT_Const
		};

		const BindingKind desiredKind = BindingKind(kDesiredKind[kind]);
		const uint8_t dkMask = uint8_t(1 << desiredKind);

		// given baseBindingKind, what are legal desiredKinds?
		static const uint8_t kLegalBaseKinds[8] = 
		{
			(1<<BKIND_METHOD) | (1<<BKIND_GET) | (1<<BKIND_SET),	// BKIND_NONE
			(1<<BKIND_METHOD),										// BKIND_METHOD
			0,														// BKIND_VAR
			0,														// BKIND_CONST
			0,														// unused
			(1<<BKIND_GET) | (1<<BKIND_SET),						// BKIND_GET
			(1<<BKIND_GET) | (1<<BKIND_SET),						// BKIND_SET
			(1<<BKIND_GET) | (1<<BKIND_SET)							// BKIND_GETSET
		};

		if ((kLegalBaseKinds[baseBindingKind] & dkMask) == 0)
			goto failure;

		// given baseBindingKind, which desiredKinds *require* override?
		static const uint8_t kOverrideRequired[8] = 
		{
			0,														// BKIND_NONE
			(1<<BKIND_METHOD),										// BKIND_METHOD
			0,														// BKIND_VAR
			0,														// BKIND_CONST
			0,														// unused
			(1<<BKIND_GET),											// BKIND_GET
			(1<<BKIND_SET),											// BKIND_SET
			(1<<BKIND_GET) | (1<<BKIND_SET)							// BKIND_GETSET
		};
		
		if (((kOverrideRequired[baseBindingKind] & dkMask) ? ATTR_override : 0) != (tag & ATTR_override))
			goto failure;

		return baseBinding;

failure:

#ifdef AVMPLUS_VERBOSE
   		if (pool->verbose)
			core->console << "illegal override in "<< this << ": " << Multiname(ns,name) <<"\n";
#endif
		if (toplevel)
			toplevel->throwVerifyError(kIllegalOverrideError, toplevel->core()->toErrorString(Multiname(ns,name)), toplevel->core()->toErrorString(this));
		AvmAssert(!"unhandled verify error");
		return BIND_NONE;
	}

	TraitsBindings* FASTCALL Traits::_getTraitsBindings() 
	{ 
		AvmAssert(this->linked);
		// note: TraitsBindings are always built the first time in resolveSignature; this is only 
		// executed for subsequent re-buildings. Thus we pass NULL for toplevel (it's only used
		// for verification errors, but those will have been caught prior to this) and for
		// abcGen (since it only needs to be done once).
		TraitsBindings* tb = _buildTraitsBindings(/*toplevel*/NULL, /*abcGen*/NULL);
		return tb;
	}

	TraitsMetadata* FASTCALL Traits::_getTraitsMetadata() 
	{ 
		AvmAssert(this->linked);
		TraitsMetadata* tm = _buildTraitsMetadata();
 		return tm;
	}

}
