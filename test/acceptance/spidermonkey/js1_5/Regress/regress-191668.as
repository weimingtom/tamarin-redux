/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

startTest();

/*
 *
 * Date:    03 February 2003
 * SUMMARY: Testing script containing <!- at internal buffer boundary.
 * JS parser must look for HTML comment-opener <!--, but mustn't disallow <!-
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=191668
 *
 */
//-----------------------------------------------------------------------------
var gTestfile = 'regress-191668.js';
var UBound = 0;
var BUGNUMBER = 191668;
var summary = 'Testing script containing <!- at internal buffer boundary';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];

var N = 512;
var j = 0;
var str = 'if (0<!-0) ++j;';

for (var i=0; i!=N; ++i)
{
  if (0<!-0) ++j;
}

status = inSection(1);
actual = j;
expect = N;
addThis();



//-----------------------------------------------------------------------------
addtestcases();
//-----------------------------------------------------------------------------



function addThis()
{
  statusitems[UBound] = status;
  actualvalues[UBound] = actual;
  expectedvalues[UBound] = expect;
  UBound++;
}


function addtestcases()
{

  printBugNumber(BUGNUMBER);
  printStatus(summary);

  for (var i=0; i<UBound; i++)
  {
    AddTestCase(statusitems[i], expectedvalues[i], actualvalues[i]);
  }


}

test();
