/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var SECTION = "Directives\const";               // provide a document reference (ie, ECMA section)
var VERSION = "ActionScript 3.0";           // Version of JavaScript or ECMA
var TITLE   = "'const' as a subsitute for 'var'";       // Provide ECMA section title or a description
var BUGNUMBER = "";

startTest();                // leave this alone


/*===========================================================================*/

// Test case for checking the CONST keyword.

// Using 'const' instead of 'var' should not report an error.
const myConst = 10;

AddTestCase( "Testing the 'const' keyword: const myConst = 10;", 10, myConst );


test();             // Leave this function alone.
            // This function is for executing the test case and then
            // displaying the result on to the console or the LOG file.
