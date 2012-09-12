/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var SECTION = "Definitions\const";                  // provide a document reference (ie, ECMA section)
var VERSION = "ActionScript 3.0";           // Version of JavaScript or ECMA
var TITLE   = "const inside a global function";       // Provide ECMA section title or a description
var BUGNUMBER = "";

startTest();

function const1Arg( arg1 ) {

    const localVar = arg1;
    return localVar;
}

AddTestCase( "Calling a global function which declares a local const variable", -1, const1Arg( -1 ));


test();     // Leave this function alone.
            // This function is for executing the test case and then
            // displaying the result on to the console or the LOG file.
