#include "pch.h"
#include "ResultsDialog.h"

using namespace winrt;
using namespace winrt::Microsoft::Windows::ApplicationModel::Resources;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace HardwareAnalyzer
{
	void ResultsDialog::Show(
		const winrt::Microsoft::UI::Xaml::XamlRoot& xamlRoot,
		const HardwareInfo& info,
		const std::vector<HardwareCheckResult>& results,
		int score)
	{
		ResourceLoader resourceLoader;

		ContentDialog dlg;
		dlg.XamlRoot(xamlRoot);
		dlg.Title(box_value(resourceLoader.GetString(L"ResultsDialogTitle")));

		// Build content
		StackPanel mainPanel;
		mainPanel.Spacing(10);
		mainPanel.Padding(ThicknessHelper::FromLengths(0, 10, 0, 0));

		// Results list
		for (const auto& result : results)
		{
			Grid itemGrid;
			itemGrid.Margin(ThicknessHelper::FromLengths(0, 5, 0, 5));

			ColumnDefinition col1;
			col1.Width(GridLengthHelper::FromPixels(24));
			ColumnDefinition col2;
			col2.Width(GridLengthHelper::FromPixels(120));
			ColumnDefinition col3;
			col3.Width(GridLengthHelper::FromValueAndType(1, GridUnitType::Star));

			itemGrid.ColumnDefinitions().Append(col1);
			itemGrid.ColumnDefinitions().Append(col2);
			itemGrid.ColumnDefinitions().Append(col3);

			// Status icon
			FontIcon statusIcon;
			statusIcon.FontFamily(Microsoft::UI::Xaml::Media::FontFamily(L"Segoe MDL2 Assets"));
			statusIcon.FontSize(16);

			switch (result.Status)
			{
			case StatusLevel::Good:
				statusIcon.Glyph(L"\uE73E"); // Checkmark
				statusIcon.Foreground(Microsoft::UI::Xaml::Media::SolidColorBrush(Windows::UI::Colors::LimeGreen()));
				break;
			case StatusLevel::Warning:
				statusIcon.Glyph(L"\uE7BA"); // Warning
				statusIcon.Foreground(Microsoft::UI::Xaml::Media::SolidColorBrush(Windows::UI::Colors::Orange()));
				break;
			case StatusLevel::Bad:
				statusIcon.Glyph(L"\uE711"); // Cancel/X
				statusIcon.Foreground(Microsoft::UI::Xaml::Media::SolidColorBrush(Windows::UI::Colors::Red()));
				break;
			}
			Grid::SetColumn(statusIcon, 0);
			itemGrid.Children().Append(statusIcon);

			// Name
			TextBlock nameText;
			hstring localizedName{ result.Name };
			if (result.Name == L"Processor")
				localizedName = resourceLoader.GetString(L"HW_Processor");
			else if (result.Name == L"Graphics Card")
				localizedName = resourceLoader.GetString(L"HW_GraphicsCard");
			else if (result.Name == L"RAM")
				localizedName = resourceLoader.GetString(L"HW_RAM");
			else if (result.Name == L"Video Memory")
				localizedName = resourceLoader.GetString(L"HW_VideoMemory");
			else if (result.Name == L"Architecture")
				localizedName = resourceLoader.GetString(L"HW_Architecture");
			nameText.Text(localizedName);
			nameText.FontWeight(Windows::UI::Text::FontWeights::SemiBold());
			nameText.VerticalAlignment(VerticalAlignment::Center);
			Grid::SetColumn(nameText, 1);
			itemGrid.Children().Append(nameText);

			// Value and reason
			StackPanel valuePanel;
			valuePanel.VerticalAlignment(VerticalAlignment::Center);

			TextBlock valueText;
			valueText.Text(result.Value);
			valueText.TextTrimming(TextTrimming::CharacterEllipsis);
			valueText.MaxWidth(300);
			valuePanel.Children().Append(valueText);

			TextBlock reasonText;
			// Translate reason key to localized string
			hstring localizedReason = resourceLoader.GetString(hstring{ result.ReasonKey });
			reasonText.Text(localizedReason);
			reasonText.FontSize(11);
			reasonText.Opacity(0.7);
			reasonText.TextTrimming(TextTrimming::CharacterEllipsis);
			reasonText.MaxWidth(300);
			valuePanel.Children().Append(reasonText);

			Grid::SetColumn(valuePanel, 2);
			itemGrid.Children().Append(valuePanel);

			mainPanel.Children().Append(itemGrid);
		}

		// Separator
		Border separator;
		separator.Height(1);
		separator.Background(Microsoft::UI::Xaml::Media::SolidColorBrush(Windows::UI::Colors::Gray()));
		separator.Margin(ThicknessHelper::FromLengths(0, 15, 0, 15));
		separator.Opacity(0.3);
		mainPanel.Children().Append(separator);

		// Global Score
		StackPanel scorePanel;
		scorePanel.Orientation(Orientation::Horizontal);
		scorePanel.HorizontalAlignment(HorizontalAlignment::Center);

		TextBlock scoreLabel;
		scoreLabel.Text(resourceLoader.GetString(L"GlobalScoreLabel"));
		scoreLabel.FontSize(20);
		scoreLabel.Margin(ThicknessHelper::FromLengths(0, 0, 10, 0));
		scoreLabel.FontWeight(Windows::UI::Text::FontWeights::SemiBold());
		scorePanel.Children().Append(scoreLabel);

		TextBlock scoreValue;
		scoreValue.Text(std::to_wstring(score) + L"/100");
		scoreValue.FontSize(20);
		scoreValue.FontWeight(Windows::UI::Text::FontWeights::Bold());

		// Color based on score
		if (score >= 70)
			scoreValue.Foreground(Microsoft::UI::Xaml::Media::SolidColorBrush(Windows::UI::Colors::LimeGreen()));
		else if (score >= 40)
			scoreValue.Foreground(Microsoft::UI::Xaml::Media::SolidColorBrush(Windows::UI::Colors::Orange()));
		else
			scoreValue.Foreground(Microsoft::UI::Xaml::Media::SolidColorBrush(Windows::UI::Colors::Red()));

		scorePanel.Children().Append(scoreValue);
		mainPanel.Children().Append(scorePanel);

		// Score interpretation
		TextBlock interpretation;
		interpretation.HorizontalAlignment(HorizontalAlignment::Center);
		interpretation.Margin(ThicknessHelper::FromLengths(0, 5, 0, 0));
		interpretation.Opacity(0.7);

		if (score >= 85)
			interpretation.Text(resourceLoader.GetString(L"ScoreExcellent"));
		else if (score >= 70)
			interpretation.Text(resourceLoader.GetString(L"ScoreGood"));
		else if (score >= 55)
			interpretation.Text(resourceLoader.GetString(L"ScoreAcceptable"));
		else if (score >= 40)
			interpretation.Text(resourceLoader.GetString(L"ScoreBelowAverage"));
		else
			interpretation.Text(resourceLoader.GetString(L"ScoreSignificantConcerns"));

		mainPanel.Children().Append(interpretation);

		// Wrap in ScrollViewer for long content
		ScrollViewer scrollViewer;
		scrollViewer.Content(mainPanel);
		scrollViewer.MaxHeight(500);
		scrollViewer.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);

		dlg.Content(scrollViewer);
		dlg.CloseButtonText(resourceLoader.GetString(L"CloseButton"));
		dlg.ShowAsync();
	}
}
