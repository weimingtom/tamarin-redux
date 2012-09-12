/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var SECTION = "Vector";
var VERSION = "Bug 592735: code coverage";
startTest();

writeHeaderToLog( SECTION + " Vector_double_AS3 methods based on missing code coverage");

function g(x) : Number { return x }

var v:Vector.<Number> = new Vector.<Number>;
var x:Number;
x = 20;
// avmplus::NativeID::__AS3___vec_Vector_double_AS3_push_thunk(MethodEnv*,uint32_t,Atom*)
v.push(g(x)/10);
AddTestCase("Vector_double_AS3_push", 1, v.length);
v.push();
AddTestCase("Vector_double_AS3_push null", 1, v.length);

// avmplus::NativeID::__AS3___vec_Vector_double_AS3_unshift_thunk(MethodEnv*,uint32_t,Atom*)
x = 10;
v.unshift(g(x)/10);
AddTestCase("Vector_double_AS3_unshift", "1,2", v.toString());
v.unshift();
AddTestCase("Vector_double_AS3_unshift null", "1,2", v.toString());

// avmplus::NativeID::__AS3___vec_Vector_double_AS3_shift_thunk(MethodEnv*,uint32_t,Atom*)
AddTestCase("Vector_double_AS3_shift", 1, v.shift());
AddTestCase("Vector_double_AS3_shift length", 1, v.length);

// avmplus::NativeID::__AS3___vec_Vector_double_AS3_pop_thunk(MethodEnv*,uint32_t,Atom*)
v.pop();
AddTestCase("Vector_double_AS3_pop", 0, v.length);

// avmplus::NativeID::__AS3___vec_Vector_double_length_set_thunk(MethodEnv*,uint32_t,Atom*)
v.length = 2;
AddTestCase("Vector_double_length_set", 2, v.length);

// avmplus::NativeID::__AS3___vec_Vector_double_fixed_set_thunk(MethodEnv*,uint32_t,Atom*)
// avmplus::NativeID::__AS3___vec_Vector_double_fixed_get_thunk(MethodEnv*,uint32_t,Atom*)
v.fixed = true;
AddTestCase("Vector_double_fixed_get", true, v.fixed);



v.fixed = false;

// __AS3___vec_Vector_double_private__map_thunk(MethodEnv*,uint32_t,Atom*)
x = 10;
v[0] = (g(x)/10);
x = 20;
v[1] = (g(x)/10);
function mapper1(value, index, obj)
{
    return value+1;
}
AddTestCase("Vector_double_private__map", "2,3", v.map(mapper1).toString()); // NOTE: this returns a new vector and does not alter

// __AS3___vec_Vector_double_private__reverse_thunk(MethodEnv*,uint32_t,Atom*)
AddTestCase("Vector_double_private__reverse", "2,1", v.reverse().toString());

test();
