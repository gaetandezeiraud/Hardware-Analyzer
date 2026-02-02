#pragma once

#include "MainWindow.g.h"
#include "MicaWindow.h"
#include "HardwareInfo.h"
#include "MacOSHardwareInfo.h"

namespace winrt::HardwareAnalyzer::implementation
{
	struct MainWindow
		: MainWindowT<MainWindow>
		, public ::HardwareAnalyzer::MicaWindow<MainWindow>
	{
		MainWindow();

		void BrowseButton_Click(const Windows::Foundation::IInspectable& sender,
			const Microsoft::UI::Xaml::RoutedEventArgs& args);
		void AnalyzeButton_Click(const Windows::Foundation::IInspectable& sender,
			const Microsoft::UI::Xaml::RoutedEventArgs& args);

		void DropZone_Drop(const Windows::Foundation::IInspectable& sender,
			const Microsoft::UI::Xaml::DragEventArgs& e);
		void DropZone_DragOver(const Windows::Foundation::IInspectable& sender,
			const Microsoft::UI::Xaml::DragEventArgs& e);
		void DropZone_DragLeave(const Windows::Foundation::IInspectable& sender,
			const Microsoft::UI::Xaml::DragEventArgs& e);

		void PlatformSelector_SelectionChanged(const Windows::Foundation::IInspectable& sender,
			const Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs& e);

	private:
		fire_and_forget OpenFilePicker();
		fire_and_forget ProcessDroppedFile(const Microsoft::UI::Xaml::DragEventArgs& e);
		fire_and_forget LoadImageFile(Windows::Storage::StorageFile file);
		Windows::Foundation::IAsyncAction AnalyzeImage();
		Windows::Foundation::IAsyncAction AnalyzeWindowsImage();
		Windows::Foundation::IAsyncAction AnalyzeMacOSImage();
		Microsoft::UI::Windowing::AppWindow GetAppWindowForCurrentWindow();

	private:
		Windows::Storage::StorageFile m_currentFile{ nullptr };
		::HardwareAnalyzer::TargetPlatform m_selectedPlatform{ ::HardwareAnalyzer::TargetPlatform::Windows };
	};
}

namespace winrt::HardwareAnalyzer::factory_implementation
{
	struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
	{
	};
}
