// Copyright 2022-2023 Dezeiraud Gaëtan. All Rights Reserved

#pragma once

#include "App.xaml.g.h"

namespace winrt::HardwareAnalyzer::implementation
{
	struct App : AppT<App>
	{
		App();

		void OnLaunched(const Microsoft::UI::Xaml::LaunchActivatedEventArgs&);

	private:
		Microsoft::UI::Xaml::Window window{ nullptr };
	};
}
