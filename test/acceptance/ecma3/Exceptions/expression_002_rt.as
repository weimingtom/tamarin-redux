/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
    var SECTION = "expressions-002.js";
    var VERSION = "JS1_4";
    var TITLE   = "Property Accessors";
    writeHeaderToLog( SECTION + " "+TITLE );

    startTest();

    var testcases = getTestCases();
    test();
    
function getTestCases() {
    var array = new Array();
    var item = 0;

    // go through all Native Function objects, methods, and properties and get their typeof.

    var PROPERTY = new Array();
    var p = 0;

    // try to access properties of primitive types

    OBJECT = new Property(  "undefined",    void 0,   "undefined",   NaN );

    var result = "Failed";
    var exception = "No exception thrown";
    var expect = "Passed";

    try {
        result = OBJECT.value.valueOf();
    } catch ( e:TypeError ) {
        result = expect;
        exception = e.toString();
    }finally{
        array[item++] = new TestCase(
        SECTION,
        "Get the value of an object whose value is undefined "+
        "(threw " + typeError(exception) +")",
        expect,
        result );
    }
    return array;
}


function Property( object, value, string, number ) {
    this.object = object;
    this.string = String(value);
    this.number = Number(value);
    this.valueOf = value;
}
