#pragma once

namespace ThreadNameSpace
{

using namespace System;
using namespace System::Threading;
using namespace System::Windows::Forms;

	public ref class ThreadExample
	{
	public:

	   // The ThreadProc method is called when the thread starts.
	   // It loops ten times, writing to the console and yielding 
	   // the rest of its time slice each time, and then ends.
	   static void ThreadProc(System::Object ^obj)
	   {

		   //System::Windows::Forms::Form frm = dynamic_cast(obj);
		   //String ^str = obj->ToString();
		   //obj->

		   //Windows::Forms::Form j = obj->GetType(Windows::Forms::Form);//dynamic_cast<Windows::Forms::Form>(obj);

		   //MessageBox::Show(str);

		  for ( int i = 0; i < 10; i++ )
		  {
			 //Console::Write(  "ThreadProc: " );
			 //Console::WriteLine( i );
	         
			 // Yield the rest of the time slice.
			 Thread::Sleep( 0 );

		  }
	   }

	};
}