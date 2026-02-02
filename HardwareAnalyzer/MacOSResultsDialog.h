#pragma once
#include "pch.h"
#include "MacOSHardwareInfo.h"
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Windows.ApplicationModel.Resources.h>

namespace HardwareAnalyzer
{
	class MacOSResultsDialog
	{
	public:
		static void Show(
			const winrt::Microsoft::UI::Xaml::XamlRoot& xamlRoot,
			const MacOSHardwareInfo& info,
			const std::vector<HardwareCheckResult>& results,
			int score);
	};
}
