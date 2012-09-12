/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

startTest();

var gTestfile = 'regress-245113.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 245113;
var summary = 'instanceof operator should return false for class prototype';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

Date.prototype.test = function() {
  return (this instanceof Date);
};

String.prototype.test = function() {
  return (this instanceof String);
};

status = summary + inSection(1);
expect = false;
actual = (Date.prototype.test instanceof Date);
AddTestCase(status, expect, actual);

status = summary + inSection(2);
expect = false;
actual = Date.prototype.test();
AddTestCase(status, expect, actual);

status = summary + inSection(3);
expect = false;
actual = String.prototype.test();
AddTestCase(status, expect, actual);

status = summary + inSection(4);
expect = true;
actual = (new Date()).test();
AddTestCase(status, expect, actual);

status = summary + inSection(5);
expect = true;
actual = (new String()).test();
AddTestCase(status, expect, actual);


test();
