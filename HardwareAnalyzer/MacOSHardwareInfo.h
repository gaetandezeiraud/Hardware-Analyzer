#pragma once
#include "pch.h"
#include "HardwareInfo.h"
#include <string>
#include <vector>
#include <regex>
#include <algorithm>

namespace HardwareAnalyzer
{
	enum class TargetPlatform
	{
		Windows,
		macOS
	};

	struct MacOSHardwareInfo
	{
		std::wstring DeviceName;      // e.g., "MacBook Pro"
		std::wstring DeviceYear;      // e.g., "2020", "2023"
		std::wstring Chip;            // e.g., "Apple M1", "Apple M2 Max"
		std::wstring Memory;          // e.g., "16 GB"
		std::wstring MacOSVersion;    // e.g., "Ventura 13.0", "Sonoma 15.0"

		double MemoryGB = 0;
		int ChipGeneration = 0;       // 1 for M1, 2 for M2, etc.
		int MacOSMajorVersion = 0;    // 15 for Sonoma, etc.
		bool IsAppleSilicon = false;
		bool IsIntelMac = false;
	};

	class MacOSHardwareAnalyzerService
	{
	public:
		static MacOSHardwareInfo ParseMacOSOcrText(const std::wstring& text)
		{
			MacOSHardwareInfo info;
			std::wstring normalizedText = text;

			// Normalize OCR errors: replace common misreads
			// OCR often reads "M1" as "Ml" (lowercase L) or "MI" (uppercase I)
			// Replace "Ml" and "MI" patterns that look like Apple chip names
			std::wregex ocrFixRegex(LR"(Apple\s*M[lI](\d*))", std::regex::icase);
			normalizedText = std::regex_replace(normalizedText, ocrFixRegex, L"Apple M1$1");

			// Also fix standalone Ml/MI followed by comma or space (like "13-inch, Ml, 2020")
			std::wregex ocrFixRegex2(LR"(\bM[lI]\b)", std::regex::icase);
			normalizedText = std::regex_replace(normalizedText, ocrFixRegex2, L"M1");

			// Extract device name (MacBook Pro, MacBook Air, iMac, Mac Mini, Mac Studio, Mac Pro)
			std::wregex deviceRegex(LR"((MacBook\s*Pro|MacBook\s*Air|iMac|Mac\s*Mini|Mac\s*Studio|Mac\s*Pro))", std::regex::icase);
			std::wsmatch deviceMatch;
			if (std::regex_search(normalizedText, deviceMatch, deviceRegex))
			{
				info.DeviceName = deviceMatch[1].str();
			}

			// Extract year from subtitle line like "13-inch, M1, 2020"
			std::wregex yearRegex(LR"((\d{4}))", std::regex::icase);
			std::wsmatch yearMatch;
			if (std::regex_search(normalizedText, yearMatch, yearRegex))
			{
				int year = 0;
				try { year = std::stoi(yearMatch[1].str()); } catch (...) {}
				if (year >= 2010 && year <= 2035)
				{
					info.DeviceYear = yearMatch[1].str();
				}
			}

			// Extract Chip - look for "Apple M" followed by digit(s) and optional Pro/Max/Ultra
			std::wregex appleChipRegex(LR"((Apple\s*M(\d+)(?:\s*(?:Pro|Max|Ultra))?))", std::regex::icase);
			std::wsmatch chipMatch;
			if (std::regex_search(normalizedText, chipMatch, appleChipRegex))
			{
				info.Chip = chipMatch[1].str();
				info.IsAppleSilicon = true;

				// Extract generation number from capture group 2
				try {
					info.ChipGeneration = std::stoi(chipMatch[2].str());
				}
				catch (...) {}
			}

			// Check for Intel processor if no Apple Silicon found
			if (!info.IsAppleSilicon)
			{
				std::wregex intelRegex(LR"((Intel[^\n]{0,50}))", std::regex::icase);
				if (std::regex_search(normalizedText, chipMatch, intelRegex))
				{
					info.Chip = chipMatch[1].str();
					info.IsIntelMac = true;
				}
			}

			// Extract Memory - look for common RAM sizes followed by GB/Go
			std::wregex memoryRegex(LR"(\b(8|16|24|32|48|64|96|128)\s*(GB|Go)\b)", std::regex::icase);
			std::wsmatch memMatch;
			if (std::regex_search(normalizedText, memMatch, memoryRegex))
			{
				info.Memory = memMatch[1].str() + L" " + memMatch[2].str();
				try {
					info.MemoryGB = std::stod(memMatch[1].str());
				}
				catch (...) {}
			}

			// Extract macOS version - FIRST look for known version names
			// This avoids matching "macOS Apple" incorrectly
			std::wregex osNameRegex(LR"((Sonoma|Sequoia|Ventura|Monterey|Big\s*Sur|Catalina|Mojave|High\s*Sierra|Sierra|Tahoe)\s*(\d+)(?:\.(\d+))?(?:\.(\d+))?)", std::regex::icase);
			std::wsmatch osMatch;
			if (std::regex_search(normalizedText, osMatch, osNameRegex))
			{
				std::wstring versionName = osMatch[1].str();
				std::wstring majorStr = osMatch[2].str();
				std::wstring minorStr = osMatch[3].matched ? osMatch[3].str() : L"0";
				std::wstring patchStr = osMatch[4].matched ? osMatch[4].str() : L"";

				info.MacOSVersion = versionName + L" " + majorStr + L"." + minorStr;
				if (!patchStr.empty())
				{
					info.MacOSVersion += L"." + patchStr;
				}

				try {
					info.MacOSMajorVersion = std::stoi(majorStr);
				}
				catch (...) {}
			}

			return info;
		}

		static std::vector<HardwareCheckResult> AnalyzeMacOSHardware(const MacOSHardwareInfo& info)
		{
			std::vector<HardwareCheckResult> results;

			// Analyze Chip (Apple Silicon vs Intel)
			HardwareCheckResult chipResult;
			chipResult.Name = L"Chip";
			chipResult.Value = info.Chip.empty() ? L"?" : info.Chip;
			chipResult.Status = AnalyzeChip(info, chipResult.ReasonKey);
			results.push_back(chipResult);

			// Analyze Memory (6 GB is warning threshold per user requirement)
			HardwareCheckResult memResult;
			memResult.Name = L"Memory";
			memResult.Value = info.Memory.empty() ? L"?" : info.Memory;
			memResult.Status = AnalyzeMacMemory(info.MemoryGB, memResult.ReasonKey);
			results.push_back(memResult);

			// Analyze macOS Version (minimum macOS 15 Sonoma)
			HardwareCheckResult osResult;
			osResult.Name = L"macOS Version";
			osResult.Value = info.MacOSVersion.empty() ? L"?" : info.MacOSVersion;
			osResult.Status = AnalyzeMacOSVersion(info.MacOSMajorVersion, osResult.ReasonKey);
			results.push_back(osResult);

			return results;
		}

		static int CalculateGlobalScore(const std::vector<HardwareCheckResult>& results)
		{
			// Count how many results have actual values (not "?")
			int validResults = 0;
			for (const auto& result : results)
			{
				if (result.Value != L"?")
					validResults++;
			}

			// If no data could be extracted, return -1 to indicate no result possible
			if (validResults == 0)
			{
				return -1;
			}

			// Check for unsupported OS or architecture first - return 0 immediately
			for (const auto& result : results)
			{
				if (result.Value == L"?")
					continue;

				// If Chip or macOS Version is Bad, the system is not supported
				if ((result.Name == L"Chip" || result.Name == L"macOS Version") && result.Status == StatusLevel::Bad)
				{
					return 0;
				}
			}

			int score = 100;
			for (const auto& result : results)
			{
				if (result.Value == L"?")
					continue;

				switch (result.Status)
				{
				case StatusLevel::Warning:
					score -= 15;
					break;
				case StatusLevel::Bad:
					score -= 30;
					break;
				default:
					break;
				}
			}
			return (std::max)(0, score);
		}

	private:
		static StatusLevel AnalyzeChip(const MacOSHardwareInfo& info, std::wstring& reasonKey)
		{
			if (info.Chip.empty())
			{
				reasonKey = L"Reason_ChipNotFound";
				return StatusLevel::Warning;
			}

			// Intel Mac is not supported
			if (info.IsIntelMac)
			{
				reasonKey = L"Reason_IntelMacNotSupported";
				return StatusLevel::Bad;
			}

			// Apple Silicon is supported (M1, M2, M3, M4, M5+... no upper limit)
			if (info.IsAppleSilicon && info.ChipGeneration >= 1)
			{
				reasonKey = L"Reason_AppleSiliconSupported";
				return StatusLevel::Good;
			}

			reasonKey = L"Reason_ChipUnknown";
			return StatusLevel::Warning;
		}

		static StatusLevel AnalyzeMacMemory(double memoryGB, std::wstring& reasonKey)
		{
			if (memoryGB <= 0)
			{
				reasonKey = L"Reason_MemoryNotFound";
				return StatusLevel::Warning;
			}

			// Per user requirement: 6 GB is warning threshold (same rule as Windows)
			if (memoryGB < 6)
			{
				reasonKey = L"Reason_MacVeryLowMemory";
				return StatusLevel::Bad;
			}
			else if (memoryGB < 8)
			{
				reasonKey = L"Reason_MacLowMemory";
				return StatusLevel::Warning;
			}
			else if (memoryGB < 16)
			{
				reasonKey = L"Reason_MacAcceptableMemory";
				return StatusLevel::Good;
			}
			else
			{
				reasonKey = L"Reason_MacGoodMemory";
				return StatusLevel::Good;
			}
		}

		static StatusLevel AnalyzeMacOSVersion(int majorVersion, std::wstring& reasonKey)
		{
			if (majorVersion <= 0)
			{
				reasonKey = L"Reason_MacOSVersionNotFound";
				return StatusLevel::Warning;
			}

			// Minimum supported: macOS 15 (Sonoma) and above
			// Support up to macOS 26 (Tahoe) and future versions (no upper limit)
			if (majorVersion < 15)
			{
				reasonKey = L"Reason_MacOSTooOld";
				return StatusLevel::Bad;
			}
			else
			{
				reasonKey = L"Reason_MacOSSupported";
				return StatusLevel::Good;
			}
		}
	};
}
