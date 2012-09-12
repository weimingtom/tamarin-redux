/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

gTestfile = 'array_split_1.js';

/**
   File Name:          array_split_1.js
   ECMA Section:       Array.split()
   Description:

   These are tests from free perl suite.

   Author:             christine@netscape.com
   Date:               12 november 1997
*/

var SECTION = "String Split";
var VERSION = "JS1_2";
var TITLE   = "Array.split()";

startTest();  var testscases=[]; var index=0;

writeHeaderToLog( SECTION + " "+ TITLE);

function getTestCases() {
    var array = new Array();
    var item = 0;
    
    array[item++] = new TestCase( SECTION,
          "('a,b,c'.split(',')).length",
          3,
          ('a,b,c'.split(',')).length );

    array[item++] = new TestCase( SECTION,
          "('a,b'.split(',')).length",
          2,
          ('a,b'.split(',')).length );

    array[item++] = new TestCase( SECTION,
          "('a'.split(',')).length",
          1,
          ('a'.split(',')).length );

    // creates an array with a single element that is an emptystring
    array[item++] = new TestCase( SECTION,
          "(''.split(',')).length",
          1,
          (''.split(',')).length );

    return array;
}

var testcases = getTestCases();

test();
