#include "pch.h"
#include "OcrService.h"

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Media::Ocr;
using namespace winrt::Windows::Graphics::Imaging;
using namespace winrt::Windows::Storage::Streams;

namespace winrt::HardwareAnalyzer
{
	Windows::Foundation::IAsyncOperation<winrt::hstring> OcrService::PerformOcrAsync(Windows::Storage::StorageFile file)
	{
		// Open file and decode image
		auto stream{ co_await file.OpenAsync(Windows::Storage::FileAccessMode::Read) };
		auto decoder{ co_await BitmapDecoder::CreateAsync(stream) };
		auto softwareBitmap{ co_await decoder.GetSoftwareBitmapAsync() };

		// Get OCR engine - try user's language first, fallback to English
		OcrEngine ocrEngine{ nullptr };

		// Try to create engine with user's preferred language
		auto userLanguage = Windows::Globalization::Language(Windows::Globalization::ApplicationLanguages::Languages().GetAt(0));
		if (OcrEngine::IsLanguageSupported(userLanguage))
		{
			ocrEngine = OcrEngine::TryCreateFromLanguage(userLanguage);
		}

		// Fallback to available languages
		if (!ocrEngine)
		{
			auto availableLanguages = OcrEngine::AvailableRecognizerLanguages();
			for (auto const& lang : availableLanguages)
			{
				ocrEngine = OcrEngine::TryCreateFromLanguage(lang);
				if (ocrEngine)
					break;
			}
		}

		if (!ocrEngine)
		{
			co_return L"OCR not available";
		}

		// Perform OCR
		auto ocrResult{ co_await ocrEngine.RecognizeAsync(softwareBitmap) };

		co_return ocrResult.Text();
	}
}
