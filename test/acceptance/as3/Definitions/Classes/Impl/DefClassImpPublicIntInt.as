/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
import DefClassImpPublicIntInt.*;
var SECTION = "Definitions";       // provide a document reference (ie, Actionscript section)
var VERSION = "AS3";        // Version of ECMAScript or ActionScript
var TITLE   = "Default class implements public interface";       // Provide ECMA section title or a description
var BUGNUMBER = "";

startTest();                // leave this alone


///////////////////////////////////////////////////////////////
// add your tests here
  

var obj:DefaultClassAccesor = new DefaultClassAccesor();


//Default class implement two public interface with a public method
AddTestCase("Default class implement two public interface with a public method", "PASSED",obj.deffunc());


////////////////////////////////////////////////////////////////

//}

test();       // leave this alone.  this executes the test cases and
              // displays results.
