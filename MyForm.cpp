#include "MyForm.h"

using namespace System;
using namespace System::Windows::Forms;
using namespace System::Diagnostics;

//namespace RutEpsonFlashTool

[STAThread]
void Main(array<System::String^>^ args)
{
	Application::EnableVisualStyles();
	Application::SetCompatibleTextRenderingDefault(false);

	RutEpsonFlashTool::MyForm form;

	Application::Run(% form);
		
}

