/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


    var SECTION = "12.5-1";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "The if statment";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();
    
function getTestCases() {
    var array = new Array();
    var item = 0;
    var MYVAR;
    if ( true )
        MYVAR='PASSED';
    else
        MYVAR= 'FAILED';
    array[item++] = new TestCase(   SECTION,
                                    "var MYVAR; if ( true ) MYVAR='PASSED'; else MYVAR= 'FAILED';",
                                    "PASSED",
                                     MYVAR);
    var MYVAR;
    if ( false )
        MYVAR='FAILED';
    else
        MYVAR= 'PASSED';
    array[item++] = new TestCase(  SECTION,
                                    "var MYVAR; if ( false ) MYVAR='FAILED'; else MYVAR= 'PASSED';",
                                    "PASSED",
                                     MYVAR);
    var MYVAR;
    if ( new Boolean(true) )
        MYVAR='PASSED';
    else
        MYVAR= 'FAILED';
    array[item++] = new TestCase(   SECTION,
                                    "var MYVAR; if ( new Boolean(true) ) MYVAR='PASSED'; else MYVAR= 'FAILED';",
                                    "PASSED",
                                     MYVAR);
    var MYVAR;
    if ( new Boolean(false) )
        MYVAR='PASSED';
    else
        MYVAR= 'FAILED';
    array[item++] = new TestCase(  SECTION,
                                    "var MYVAR; if ( new Boolean(false) ) MYVAR='PASSED'; else MYVAR= 'FAILED';",
                                    "FAILED",
                                    MYVAR);
    var MYVAR;
    if ( 1 )
        MYVAR='PASSED';
    else
        MYVAR= 'FAILED';
    array[item++] = new TestCase(   SECTION,
                                    "var MYVAR; if ( 1 ) MYVAR='PASSED'; else MYVAR= 'FAILED';",
                                    "PASSED",
                                    MYVAR);
    var MYVAR;
    if ( 0 )
        MYVAR='FAILED';
    else
        MYVAR= 'PASSED';
    array[item++] = new TestCase(  SECTION,
                                    "var MYVAR; if ( 0 ) MYVAR='FAILED'; else MYVAR= 'PASSED';",
                                    "PASSED",
                                     MYVAR);
    return array;
}
