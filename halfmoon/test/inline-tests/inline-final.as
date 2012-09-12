/*
Java -jar utils/asc.jar -import generated/builtin.abc -optimize -AS3  -in test/acceptance/shell.as  inline.as
*/

/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 2.0/GPL 2.0/LGPL 2.1
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is [Open Source Virtual Machine.].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2005-2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adobe AS3 Team
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
package inlinetest {

//var SECTION = "Definitions";       // provide a document reference (ie, Actionscript section)
//var VERSION = "AS3";        // Version of ECMAScript or ActionScript
//var TITLE   = "Super Behavior Tests";       // Provide ECMA section title or a description
//var BUGNUMBER = "";

startTest();                // leave this alone


///////////////////////////////////////////////////////////////
// add your tests here

	
    class base {
		final function f() : String {
            return "base f()";
        }
        function h() : String {
            return "base h()";
        }
    }
    
    class middle1 extends base {
        override function h() : String {
            return "middle1 h()";
        }
    }
    
    class middle2 extends middle1 {
        override function h() : String {
            return "middle2 h()";
        }
    }
    
function tester(){
	var seca = new middle2();
	//AddTestCase( "call to final f()", "base f()", seca.f() );
	return seca.f()
	}

trace("inline-final: can inline base.f because it's a final method")
var s = tester()
trace("returned from tester")
trace(s)
//
////////////////////////////////////////////////////////////////

test();       // leave this alone.  this executes the test cases and
              // displays results.


}
