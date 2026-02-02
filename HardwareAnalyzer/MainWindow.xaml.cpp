#include "pch.h"
#include "MainWindow.xaml.h"
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

#include "HardwareInfo.h"
#include "MacOSHardwareInfo.h"
#include "OcrService.h"
#include "ResultsDialog.h"
#include "MacOSResultsDialog.h"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::ApplicationModel::Resources;

namespace winrt
{
	using namespace Microsoft::UI::Composition::SystemBackdrops;
	using namespace Windows::UI::Composition;
}

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Windows::Storage::Pickers;
using namespace Windows::ApplicationModel;
using namespace Controls;

namespace winrt::HardwareAnalyzer::implementation
{
	MainWindow::MainWindow()
	{
		Title(L"Hardware Analyzer");
		ExtendsContentIntoTitleBar(true);

		auto appWindow = GetAppWindowForCurrentWindow();
		appWindow.ResizeClient({ 600, 600 });

		InitializeComponent();
		InitializeMica(this);

		SetTitleBar(AppTitleBar());
	}

	void MainWindow::BrowseButton_Click(const winrt::Windows::Foundation::IInspectable&, const RoutedEventArgs&)
	{
		OpenFilePicker();
	}

	void MainWindow::AnalyzeButton_Click(const winrt::Windows::Foundation::IInspectable&, const RoutedEventArgs&)
	{
		AnalyzeImage();
	}

	void MainWindow::DropZone_Drop(const winrt::Windows::Foundation::IInspectable&, const DragEventArgs& e)
	{
		ProcessDroppedFile(e);
		DropZone().BorderBrush(Microsoft::UI::Xaml::Media::SolidColorBrush(Windows::UI::Colors::Gray()));
	}

	void MainWindow::DropZone_DragOver(const winrt::Windows::Foundation::IInspectable&, const DragEventArgs& e)
	{
		e.AcceptedOperation(Windows::ApplicationModel::DataTransfer::DataPackageOperation::Copy);
		e.Handled(true);
		DropZone().BorderBrush(Microsoft::UI::Xaml::Media::SolidColorBrush(Windows::UI::Colors::DodgerBlue()));
	}

	void MainWindow::DropZone_DragLeave(const winrt::Windows::Foundation::IInspectable&, const DragEventArgs&)
	{
		DropZone().BorderBrush(Microsoft::UI::Xaml::Media::SolidColorBrush(Windows::UI::Colors::Gray()));
	}

	fire_and_forget MainWindow::OpenFilePicker()
	{
		FileOpenPicker picker;

		winrt::HardwareAnalyzer::MainWindow thisWindow = *this;
		com_ptr<IWindowNative> windowNative = thisWindow.as<IWindowNative>();

		HWND hWnd;
		windowNative->get_WindowHandle(&hWnd);

		auto initializeWithWindow{ picker.as<IInitializeWithWindow>() };
		initializeWithWindow->Initialize(hWnd);

		picker.FileTypeFilter().Append(L".png");
		picker.FileTypeFilter().Append(L".jpg");
		picker.FileTypeFilter().Append(L".jpeg");
		picker.FileTypeFilter().Append(L".bmp");
		picker.FileTypeFilter().Append(L".gif");

		auto file{ co_await picker.PickSingleFileAsync() };
		if (file)
		{
			LoadImageFile(file);
		}
	}

	fire_and_forget MainWindow::ProcessDroppedFile(const DragEventArgs& e)
	{
		auto deferral = e.GetDeferral();

		auto items{ co_await e.DataView().GetStorageItemsAsync() };
		for (auto&& item : items)
		{
			if (item.IsOfType(Windows::Storage::StorageItemTypes::File))
			{
				auto file = item.as<Windows::Storage::StorageFile>();
				if (file)
				{
					auto ext = file.FileType();
					// Check if it's an image file
					if (ext == L".png" || ext == L".jpg" || ext == L".jpeg" ||
						ext == L".bmp" || ext == L".gif" ||
						ext == L".PNG" || ext == L".JPG" || ext == L".JPEG" ||
						ext == L".BMP" || ext == L".GIF")
					{
						LoadImageFile(file);
						break;
					}
				}
			}
		}

		deferral.Complete();
	}

	fire_and_forget MainWindow::LoadImageFile(Windows::Storage::StorageFile file)
	{
		m_currentFile = file;

		// Show preview
		auto stream{ co_await file.OpenAsync(Windows::Storage::FileAccessMode::Read) };
		Microsoft::UI::Xaml::Media::Imaging::BitmapImage bitmapImage;
		co_await bitmapImage.SetSourceAsync(stream);

		PreviewImage().Source(bitmapImage);
		PreviewImage().Visibility(Visibility::Visible);
		DropPrompt().Visibility(Visibility::Collapsed);
		AnalyzeButton().IsEnabled(true);
	}

	Windows::Foundation::IAsyncAction MainWindow::AnalyzeImage()
	{
		if (!m_currentFile)
			co_return;

		if (m_selectedPlatform == ::HardwareAnalyzer::TargetPlatform::macOS)
		{
			co_await AnalyzeMacOSImage();
		}
		else
		{
			co_await AnalyzeWindowsImage();
		}

		co_return;
	}

	Windows::Foundation::IAsyncAction MainWindow::AnalyzeWindowsImage()
	{
		if (!m_currentFile)
			co_return;

		auto dispatcherQueue = Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();

		// Show loading
		LoadingPanel().Visibility(Visibility::Visible);
		PreviewImage().Visibility(Visibility::Collapsed);
		AnalyzeButton().IsEnabled(false);

		try
		{
			// Perform OCR
			auto ocrText = co_await OcrService::PerformOcrAsync(m_currentFile);
			std::wstring text(ocrText.c_str());

			// Parse hardware info
			auto info = ::HardwareAnalyzer::HardwareAnalyzerService::ParseOcrText(text);

			// Analyze hardware
			auto results = ::HardwareAnalyzer::HardwareAnalyzerService::AnalyzeHardware(info);

			// Calculate score
			int score = ::HardwareAnalyzer::HardwareAnalyzerService::CalculateGlobalScore(results);

			// Show results on UI thread
			dispatcherQueue.TryEnqueue([this, info, results, score]() {
				LoadingPanel().Visibility(Visibility::Collapsed);
				PreviewImage().Visibility(Visibility::Visible);
				AnalyzeButton().IsEnabled(true);
				::HardwareAnalyzer::ResultsDialog::Show(this->Content().as<UIElement>().XamlRoot(), info, results, score);
				});
		}
		catch (const winrt::hresult_error& ex)
		{
			dispatcherQueue.TryEnqueue([this, ex]() {
				LoadingPanel().Visibility(Visibility::Collapsed);
				PreviewImage().Visibility(Visibility::Visible);
				AnalyzeButton().IsEnabled(true);

				ResourceLoader resourceLoader;
				ContentDialog errorDlg;
				errorDlg.XamlRoot(this->Content().as<UIElement>().XamlRoot());
				errorDlg.Title(box_value(resourceLoader.GetString(L"ErrorTitle")));
				errorDlg.Content(box_value(resourceLoader.GetString(L"ErrorAnalyzePrefix") + ex.message()));
				errorDlg.CloseButtonText(resourceLoader.GetString(L"OKButton"));
				errorDlg.ShowAsync();
				});
		}

		co_return;
	}

	Windows::Foundation::IAsyncAction MainWindow::AnalyzeMacOSImage()
	{
		if (!m_currentFile)
			co_return;

		auto dispatcherQueue = Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();

		// Show loading
		LoadingPanel().Visibility(Visibility::Visible);
		PreviewImage().Visibility(Visibility::Collapsed);
		AnalyzeButton().IsEnabled(false);

		try
		{
			// Perform OCR
			auto ocrText = co_await OcrService::PerformOcrAsync(m_currentFile);
			std::wstring text(ocrText.c_str());

			// Parse macOS hardware info
			auto info = ::HardwareAnalyzer::MacOSHardwareAnalyzerService::ParseMacOSOcrText(text);

			// Analyze macOS hardware
			auto results = ::HardwareAnalyzer::MacOSHardwareAnalyzerService::AnalyzeMacOSHardware(info);

			// Calculate score
			int score = ::HardwareAnalyzer::MacOSHardwareAnalyzerService::CalculateGlobalScore(results);

			// Show results on UI thread
			dispatcherQueue.TryEnqueue([this, info, results, score]() {
				LoadingPanel().Visibility(Visibility::Collapsed);
				PreviewImage().Visibility(Visibility::Visible);
				AnalyzeButton().IsEnabled(true);
				::HardwareAnalyzer::MacOSResultsDialog::Show(this->Content().as<UIElement>().XamlRoot(), info, results, score);
				});
		}
		catch (const winrt::hresult_error& ex)
		{
			dispatcherQueue.TryEnqueue([this, ex]() {
				LoadingPanel().Visibility(Visibility::Collapsed);
				PreviewImage().Visibility(Visibility::Visible);
				AnalyzeButton().IsEnabled(true);

				ResourceLoader resourceLoader;
				ContentDialog errorDlg;
				errorDlg.XamlRoot(this->Content().as<UIElement>().XamlRoot());
				errorDlg.Title(box_value(resourceLoader.GetString(L"ErrorTitle")));
				errorDlg.Content(box_value(resourceLoader.GetString(L"ErrorAnalyzePrefix") + ex.message()));
				errorDlg.CloseButtonText(resourceLoader.GetString(L"OKButton"));
				errorDlg.ShowAsync();
				});
		}

		co_return;
	}

	void MainWindow::PlatformSelector_SelectionChanged(const winrt::Windows::Foundation::IInspectable& sender, const Controls::SelectionChangedEventArgs&)
	{
		// Get the ComboBox from sender since PlatformSelector might not be generated yet
		if (auto comboBox = sender.try_as<Controls::ComboBox>())
		{
			int selectedIndex = comboBox.SelectedIndex();
			m_selectedPlatform = (selectedIndex == 1) 
				? ::HardwareAnalyzer::TargetPlatform::macOS 
				: ::HardwareAnalyzer::TargetPlatform::Windows;
		}
	}

	Microsoft::UI::Windowing::AppWindow MainWindow::GetAppWindowForCurrentWindow()
	{
		winrt::HardwareAnalyzer::MainWindow thisWindow = *this;
		com_ptr<IWindowNative> windowNative = thisWindow.as<IWindowNative>();

		HWND hWnd;
		windowNative->get_WindowHandle(&hWnd);

		Microsoft::UI::WindowId windowId;
		windowId = Microsoft::UI::GetWindowIdFromWindow(hWnd);

		Microsoft::UI::Windowing::AppWindow appWindow = Microsoft::UI::Windowing::AppWindow::GetFromWindowId(windowId);

		return appWindow;
	}
}
