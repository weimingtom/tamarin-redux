/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
    var SECTION = "15.3.1.1-3";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "The Function Constructor Called as a Function";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    
    test();
    
function getTestCases() {
    var array = new Array();
    var item = 0;

    var args = "";

    for ( var i = 0; i < 2000; i++ ) {
        args += "arg"+i;
        if ( i != 1999 ) {
            args += ",";
        }
    }

    var s = "";

    for ( var i = 0; i < 2000; i++ ) {
        s += ".0005";
        if ( i != 1999 ) {
            s += ",";
        }
    }
    var thisError="no error";
    try{
        var MyFunc = Function( args, "var r=0; for (var i = 0; i < MyFunc.length; i++ ){ if ( eval('arg'+i) == void 0) break; else r += eval('arg'+i); }; return r");
    }catch(e1:EvalError){
        thisError=(e1.toString()).substring(0,22);
    }finally{
        array[item++] = new TestCase( SECTION,"Function('function body') not supported","EvalError: Error #1066",thisError);
    }
    return array;
}
