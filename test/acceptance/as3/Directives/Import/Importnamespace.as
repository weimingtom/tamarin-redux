/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package Importnamespace  {

    public namespace N1
    
}



var SECTION = "Directives";                     // provide a document reference (ie, ECMA section)
var VERSION = "ActionScript 3.0";               // Version of JavaScript or ECMA
var TITLE   = "Import a public namespace";          // Provide ECMA section title or a description
var BUGNUMBER = "";

startTest();                                    // leave this alone


var x:Number = 3;
AddTestCase("Local variable", 3, x);


import Importnamespace.N1

class A{
    public namespace N2
    N1 var x:Number = 4;
    N2 var x:Number = 6;

public function a1(){
  return N1::x
}
public function a2(){
  return N2::x
}
}
var obj:A = new A();

AddTestCase("Public namespace imported", 4, obj.a1());
AddTestCase("Local namespace", 6, obj.a2());


/*===========================================================================*/


test();       // leave this alone.  this executes the test cases and
              // displays results.
