#pragma once

namespace HardwareAnalyzer
{
	namespace MUC = winrt::Microsoft::UI::Composition;
	namespace MUCSB = winrt::Microsoft::UI::Composition::SystemBackdrops;
	namespace MUX = winrt::Microsoft::UI::Xaml;
	namespace WS = winrt::Windows::System;

	template <typename T>
	class MicaWindow
	{
	public:
		void InitializeMica(T* pWindow)
		{
			m_window = pWindow;

			SetBackground();
			m_closedRevoker = m_window->Closed(winrt::auto_revoke, [&](auto&&, auto&&)
				{
					if (nullptr != m_backdropController)
					{
						m_backdropController.Close();
						m_backdropController = nullptr;
					}

					if (nullptr != m_dispatcherQueueController)
					{
						m_dispatcherQueueController.ShutdownQueueAsync();
						m_dispatcherQueueController = nullptr;
					}
				});
		}

	private:
		void SetBackground()
		{
			if (MUCSB::MicaController::IsSupported())
			{
				// We ensure that there is a Windows.System.DispatcherQueue on the current thread.
				// Always check if one already exists before attempting to create a new one.
				if (nullptr == WS::DispatcherQueue::GetForCurrentThread() &&
					nullptr == m_dispatcherQueueController)
				{
					m_dispatcherQueueController = CreateSystemDispatcherQueueController();
				}

				// Setup the SystemBackdropConfiguration object.
				SetupSystemBackdropConfiguration();

				// Setup Mica on the current Window.
				m_backdropController = MUCSB::MicaController();
				m_backdropController.SetSystemBackdropConfiguration(m_configuration);
				m_backdropController.AddSystemBackdropTarget(
					m_window->template try_as<MUC::ICompositionSupportsSystemBackdrop>());
			}
			else
			{
				// The backdrop material is not supported.
			}
		}

		WS::DispatcherQueueController CreateSystemDispatcherQueueController()
		{
			DispatcherQueueOptions options
			{
				sizeof(DispatcherQueueOptions),
				DQTYPE_THREAD_CURRENT,
				DQTAT_COM_NONE
			};

			ABI::Windows::System::IDispatcherQueueController* ptr{ nullptr };
			winrt::check_hresult(CreateDispatcherQueueController(options, &ptr));
			return { ptr, winrt::take_ownership_from_abi };
		}

		void SetupSystemBackdropConfiguration()
		{
			m_configuration = MUCSB::SystemBackdropConfiguration();

			// Activation state.
			m_activatedRevoker = m_window->Activated(winrt::auto_revoke,
				[&](auto&&, const MUX::WindowActivatedEventArgs& args)
				{
					m_configuration.IsInputActive(
						MUX::WindowActivationState::Deactivated != args.
						WindowActivationState());
				});

			// Initial state.
			m_configuration.IsInputActive(true);

			// Application theme.
			m_rootElement = m_window->Content().template try_as<MUX::FrameworkElement>();
			if (nullptr != m_rootElement)
			{
				m_themeChangedRevoker = m_rootElement.ActualThemeChanged(winrt::auto_revoke,
					[&](auto&&, auto&&)
					{
						m_configuration.Theme(
							ConvertToSystemBackdropTheme(
								m_rootElement.ActualTheme()));
					});

				// Initial state.
				m_configuration.Theme(
					ConvertToSystemBackdropTheme(m_rootElement.ActualTheme()));
			}
		}

		MUCSB::SystemBackdropTheme ConvertToSystemBackdropTheme(const MUX::ElementTheme& theme)
		{
			switch (theme)
			{
			case MUX::ElementTheme::Dark:
				return MUCSB::SystemBackdropTheme::Dark;
			case MUX::ElementTheme::Light:
				return MUCSB::SystemBackdropTheme::Light;
			default:
				return MUCSB::SystemBackdropTheme::Default;
			}
		}

	private:
		T* m_window;

		MUCSB::SystemBackdropConfiguration m_configuration{ nullptr };
		MUCSB::MicaController m_backdropController{ nullptr };
		MUX::Window::Activated_revoker m_activatedRevoker;
		MUX::Window::Closed_revoker m_closedRevoker;
		MUX::FrameworkElement::ActualThemeChanged_revoker m_themeChangedRevoker;
		MUX::FrameworkElement m_rootElement{ nullptr };
		WS::DispatcherQueueController m_dispatcherQueueController{ nullptr };
	};
}