#pragma once
#include "pch.h"
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Media.Ocr.h>
#include <winrt/Windows.Graphics.Imaging.h>

namespace winrt::HardwareAnalyzer
{
	class OcrService
	{
	public:
		static Windows::Foundation::IAsyncOperation<winrt::hstring> PerformOcrAsync(Windows::Storage::StorageFile file);
	};
}
