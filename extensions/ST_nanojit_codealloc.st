// -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*-
// vi: set ts=4 sw=4 expandtab: (add to ~/.vimrc: set modeline modelines=5)
//
// ***** BEGIN LICENSE BLOCK *****
// Version: MPL 1.1/GPL 2.0/LGPL 2.1
//
// The contents of this file are subject to the Mozilla Public License Version
// 1.1 (the "License"); you may not use this file except in compliance with
// the License. You may obtain a copy of the License at
// http://www.mozilla.org/MPL/
//
// Software distributed under the License is distributed on an "AS IS" basis,
// WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
// for the specific language governing rights and limitations under the
// License.
//
// The Original Code is [Open Source Virtual Machine.].
//
// The Initial Developer of the Original Code is
// Adobe System Incorporated.
// Portions created by the Initial Developer are Copyright (C) 2004-2011
// the Initial Developer. All Rights Reserved.
//
// Contributor(s):
//   Adobe AS3 Team
//
// Alternatively, the contents of this file may be used under the terms of
// either the GNU General Public License Version 2 or later (the "GPL"), or
// the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
// in which case the provisions of the GPL or the LGPL are applicable instead
// of those above. If you wish to allow use of your version of this file only
// under the terms of either the GPL or the LGPL, and not to allow others to
// use your version of this file under the terms of the MPL, indicate your
// decision by deleting the provisions above and replace them with the notice
// and other provisions required by the GPL or the LGPL. If you do not delete
// the provisions above, a recipient may use your version of this file under
// the terms of any one of the MPL, the GPL or the LGPL.
//
// ***** END LICENSE BLOCK ***** */

%%component nanojit
%%category  codealloc

%%prefix

using nanojit::NIns;
using nanojit::CodeList;
using nanojit::CodeAlloc;

static bool verbose = false;

class Randomizer {
private:

    TRandomFast seed;

public:

    Randomizer()
    {
        MathUtils::initRandom(&seed);
    }
    
    // Return integer in range [min,max).

    uint32_t range(uint32_t min, uint32_t max)
    {
        return min + MathUtils::Random(max - min, &seed);
    }
};

class CodeAllocDriver {
public:

    struct CodeListProfile {
        uint32_t largefreq;     // 1 in 'largefreq' chance codelist will be "large"
        uint32_t smallmin;      // Minimum size of "small" codelist
        uint32_t smallmax;      // Maximum size of "small" codelist
        uint32_t largemin;      // Minimum size of "large" codelist
        uint32_t largemax;      // Maximum size of "large" coelist
    };

    struct AllocationProfile {
        uint32_t limit;         // Largest block allocation permitted, zero for no limit
        uint32_t leftfreq;      // 1 in 'leftfreq' chance returned space will start at beginning of block
    };

    CodeAllocDriver(uint32_t max_codelists, CodeListProfile& codelist_profile, AllocationProfile& alloc_profile);
    
    void run(uint32_t iterations);
        
private:

    Randomizer rand;
    uint32_t n_active;          // Maximum number of simultaneously-allocated codelists
    CodeListProfile cp;         // Parameters controlling distribution of codelist sizes
    AllocationProfile ap;       // Parameters controlling allocation of each codelist

    void makeCodeList(uint32_t index, CodeAlloc& alloc, CodeList*& code);
};

CodeAllocDriver::CodeAllocDriver(uint32_t max_codelists, CodeListProfile& codelist_profile, AllocationProfile& alloc_profile)
    : n_active(max_codelists), cp(codelist_profile), ap(alloc_profile)
{}

// Create a codelist at the specified index in the codelist pool.
// The length of the codelist is generated pseudo-randomly.

void CodeAllocDriver::makeCodeList(uint32_t index, CodeAlloc& alloc, CodeList*& code)
{
     NIns* start;
     NIns* end;
  
     int32_t size = ((rand.range(0, cp.largefreq) > 0)
                     ? rand.range(cp.smallmin, cp.smallmax)
                     : rand.range(cp.largemin, cp.largemax));
     if (verbose)
         AvmLog("code-heap-test: codelist %d size %d\n", index, size);
     while (size > 0) {
         alloc.alloc(start, end, ap.limit);
         uint32_t blksize = uint32_t(end - start);
         if (verbose)
             AvmLog("code-heap-test: alloc  %x-%x %d\n", start, start, blksize);
         size -= blksize;
         if (size < 0) {
             // Last allocated block was not completely used, so we will return some of it.
             // We choose randomly whether to return a prefix of the block, or a segment out
             // of the middle.  This doesn't correspond precisely to normal usage in nanojit.
             // Note that if we are not retaining at least two bytes, we cannot split in the middle.
             int32_t excess = -size;
             if (rand.range(0, ap.leftfreq) > 0 || blksize - excess < 2) {
                 // Hole starts at left
                 // Note that hole at right is deliberately not handled in CodegenAlloc.cpp, results in assert.
                 if (verbose)
                     AvmLog("code-heap-test: hole l %x-%x %d\n", start, start + excess, excess);
                 alloc.addRemainder(code, start, end, start, start + excess);
             } else {
                 // Hole will go in middle
                 uint32_t offset = rand.range(1, blksize - excess);
                 if (verbose)
                     AvmLog("code-heap-test: hole m %x-%x %d\n", start + offset, start + offset + excess, excess);
                 alloc.addRemainder(code, start, end, start + offset, start + offset + excess);
             }
         } else {
             // Add entire block to codelist.
             CodeAlloc::add(code, start, end);
         }
     }
}

// Repeatedly construct and destroy codelists.
// We maintain a pool of codelists.  On each iteration, we replace one
// of the existing codelists with a new one, freeing the old.

void CodeAllocDriver::run(uint32_t iterations)
{
    CodeAlloc alloc;

    CodeList** codelists = mmfx_new_array(CodeList*, n_active);

    VMPI_memset(codelists, 0, n_active * sizeof(CodeList*));

    for (uint32_t i = 0; i < iterations; i++) {
        uint32_t victim = rand.range(0, n_active);
        alloc.freeAll(codelists[victim]);
        makeCodeList(victim, alloc, codelists[victim]);
        //alloc.logStats();
        debug_only( alloc.sanity_check(); )
    }

    mmfx_delete_array(codelists);
}

%%methods

typedef CodeAllocDriver::CodeListProfile CodeListProfile;
typedef CodeAllocDriver::AllocationProfile AllocationProfile;

static CodeListProfile cp = { 5, 1, 1*1024, 1, 16*1024 };

static AllocationProfile ap[] = { { 0,     2 },
                                  { 128,   2 },
                                  { 512,   2 },
                                  { 1024,  2 },
                                  { 2048,  2 },
                                  { 4096,  2 } };

static uint32_t n_ap = sizeof(ap) / sizeof(AllocationProfile);

%%test allocfree
for (uint32_t i = 0; i < n_ap; i++) {
    CodeAllocDriver* driver = mmfx_new(CodeAllocDriver(20, cp, ap[i]));
    driver->run(20000);
    mmfx_delete(driver);
 }
// We pass if we don't crash or assert.
%%verify true
