/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
var CODE = 1098; // Illegal prefix '_' for 'no namespace'.

//-----------------------------------------------------------
startTest();
//-----------------------------------------------------------

try {
    var z = "no error";
    ns = new Namespace ("prefix", "");
} catch (err) {
    z = err.toString();
} finally {
    AddTestCase("Runtime Error", "TypeError: Error #" + CODE, typeError(z));
}

//-----------------------------------------------------------
test();
//-----------------------------------------------------------
