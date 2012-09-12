/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
var CODE = 1089; // Assignment to lists with more than one item is not supported.

//-----------------------------------------------------------
startTest();
//-----------------------------------------------------------

var expected = "Error #" + CODE;
var result = "no error";
try {
    var x1 = new XMLList("<a>1</a><a>2</a><a>3</a>");
    x1.length = 0;
} catch (err) {
    result = grabError(err, err.toString());
} finally {
    AddTestCase("Runtime Error", expected, result);
}

//-----------------------------------------------------------
test();
//-----------------------------------------------------------
