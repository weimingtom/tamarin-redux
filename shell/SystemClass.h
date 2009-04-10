/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Portions created by the Initial Developer are Copyright (C) 2004-2006
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

#ifndef __avmshell_SystemClass__
#define __avmshell_SystemClass__


namespace avmshell
{
	// this class exists solely to test native classes that use MI.
	class MIClass : public ClassClosure
	{
	public:
		MIClass(VTable* cvtable) : ClassClosure(cvtable) {}
		~MIClass() {}
	};

	// this class exists solely to test native classes that use MI.
	class MixinClassThatDoesNotDescendFromScriptObject
	{
	public:
		const double factor;
		MixinClassThatDoesNotDescendFromScriptObject(double f) : factor(f) {}
		// evil, wrong version that we DO NOT WANT
		double plus(double v) { return v * factor; }
	};
	
	// this class exists solely to test native classes that use MI.
	class MIObjectImpl : public ScriptObject
	{
	public:
		const double amount;
		MIObjectImpl(VTable* vtable, ScriptObject* prototype, double a) : ScriptObject(vtable, prototype), amount(a) {}
		double plus(double v) { return v + amount; }
	};

	// this class exists solely to test native classes that use MI.
	class MIObject : public MIObjectImpl, public MixinClassThatDoesNotDescendFromScriptObject
	{
	public:
		MIObject(VTable* vtable, ScriptObject* prototype) : MIObjectImpl(vtable, prototype, 1), MixinClassThatDoesNotDescendFromScriptObject(2) {}
		~MIObject() {}
	};

	/**
	 * A simple class that has some native methods.
	 * Included as an example for writers of native methods,
	 * and also to provide some useful QA instrumentation.
	 */
	class SystemClass : public ClassClosure
	{
		uint64_t initialTime;
 
	public:
		SystemClass(VTable* cvtable);
		~SystemClass();

		// set by shell
		static int user_argc;
		static char **user_argv;

		/**
		 * Implementation of System.exit
		 * AS usage: System.exit(status);
		 * Exits the VM with OS exit code specified by  status.
		 */
		void exit(int status);

		/**
		 * Implementation of System.getAvmplusVersion
		 * AS usage: System.getAvmplusVersion();
		 * Returns the current version of AVM+ in the form
		 * "1.0 d100"
		 */
		Stringp getAvmplusVersion();

		/**
		 * Implementation of System.exec
		 * AS usage: exitCode = System.exec("command");
		 * Executes the specified command line and returns
		 * the status code
		 */
		int exec(Stringp command);

		void trace(ArrayObject* a);
		void write(Stringp s);

		
		/**
		 * @name Debugging Extensions
		 */
		/*@{*/
		void debugger();
		bool isDebugger();
		/*@}*/

				/**
		 * @name ActionScript Extensions
		 * ActionScript extensions to ECMAScript
		 */
		/*@{*/		
		unsigned getTimer();
		/*@}*/

		ArrayObject * getArgv();

		Stringp readLine();

		double get_totalMemory();
		double get_freeMemory();
		double get_privateMemory();

		// function exists solely to test native-methods with custom namespaces
		void ns_example_nstest() { }

		// function exists solely to test ScriptObject::isGlobalObject
		bool isGlobal(Atom o);
    };
}

#endif /* __avmshell_SystemClass__ */
