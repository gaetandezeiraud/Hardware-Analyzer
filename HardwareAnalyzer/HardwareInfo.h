#pragma once
#include "pch.h"
#include <string>
#include <vector>
#include <regex>
#include <algorithm>

namespace HardwareAnalyzer
{
	enum class StatusLevel
	{
		Good,
		Warning,
		Bad
	};

	struct HardwareCheckResult
	{
		std::wstring Name;
		std::wstring Value;
		StatusLevel Status;
		std::wstring ReasonKey;  // Resource key for localization
	};

	struct HardwareInfo
	{
		std::wstring DeviceName;
		std::wstring Processor;
		std::wstring RAM;
		std::wstring GPU;
		std::wstring VRAM;
		std::wstring SystemType;

		double RamGB = 0;
		double VramGB = 0;
	};

	class HardwareAnalyzerService
	{
	public:
		static HardwareInfo ParseOcrText(const std::wstring& text)
		{
			HardwareInfo info;

			// Normalize textn replace common OCR mistakes
			std::wstring normalizedText = text;

			// Extract processor/CPU - multi-language support
			// English: Processor, French: Processeur, German: Prozessor, Spanish: Procesador
			std::wregex cpuRegex(LR"((?:Processor|Processeur|Prozessor|Procesador)\s*[:\-]?\s*(.+?)(?:\n|$))", std::regex::icase);
			std::wsmatch cpuMatch;
			if (std::regex_search(normalizedText, cpuMatch, cpuRegex))
			{
				info.Processor = cpuMatch[1].str();
				// Clean up trailing whitespace
				info.Processor.erase(info.Processor.find_last_not_of(L" \t\r\n") + 1);
			}

			// Also check for CPU in header cards format (like "AMD Ryzen 9 7900...")
			if (info.Processor.empty())
			{
				std::wregex cpuHeaderRegex(LR"((AMD|Intel|Qualcomm|Apple)[^\n]+(?:Core|Ryzen|Xeon|Snapdragon|M\d)[^\n]+)", std::regex::icase);
				if (std::regex_search(normalizedText, cpuMatch, cpuHeaderRegex))
				{
					info.Processor = cpuMatch[0].str();
				}
			}

			// Extract RAM - multi-language
			// English: Installed RAM, French: Mémoire RAM installée, German: Installierter RAM
			std::wregex ramRegex(LR"((?:Installed RAM|RAM install[eé]e?|M[eé]moire RAM install[eé]e?|Installierter RAM|Memoria RAM)\s*[:\-]?\s*(\d+[\.,]?\d*)\s*(GB|Go|GiB|TB|To))", std::regex::icase);
			std::wsmatch ramMatch;
			if (std::regex_search(normalizedText, ramMatch, ramRegex))
			{
				info.RAM = ramMatch[1].str() + L" " + ramMatch[2].str();
				std::wstring ramValue = ramMatch[1].str();
				// Replace comma with dot for parsing
				std::replace(ramValue.begin(), ramValue.end(), L',', L'.');
				try {
					info.RamGB = std::stod(ramValue);
					std::wstring unit = ramMatch[2].str();
					std::transform(unit.begin(), unit.end(), unit.begin(), ::towlower);
					if (unit == L"tb" || unit == L"to") {
						info.RamGB *= 1024;
					}
				}
				catch (...) {}
			}

			// Also try simpler RAM pattern from header cards
			if (info.RamGB == 0)
			{
				std::wregex ramSimpleRegex(LR"((\d+[\.,]?\d*)\s*(GB|Go|GiB))", std::regex::icase);
				std::wstring::const_iterator searchStart(normalizedText.cbegin());
				while (std::regex_search(searchStart, normalizedText.cend(), ramMatch, ramSimpleRegex))
				{
					std::wstring ramValue = ramMatch[1].str();
					std::replace(ramValue.begin(), ramValue.end(), L',', L'.');
					try {
						double val = std::stod(ramValue);
						// RAM is usually 8, 12, 16, 32, 64, 128 GB
						if (val >= 4 && val <= 256 && info.RamGB == 0) {
							info.RamGB = val;
							info.RAM = ramMatch[0].str();
						}
					}
					catch (...) {}
					searchStart = ramMatch.suffix().first;
				}
			}

			// Extract GPU - multi-language
			// English: Graphics card, French: Carte graphique, German: Grafikkarte
			std::wregex gpuRegex(LR"((?:Graphics card|Carte graphique|Grafikkarte|GPU|Tarjeta gr[aá]fica)\s*[:\-]?\s*(.+?)(?:\n|$))", std::regex::icase);
			std::wsmatch gpuMatch;
			if (std::regex_search(normalizedText, gpuMatch, gpuRegex))
			{
				info.GPU = gpuMatch[1].str();
				info.GPU.erase(info.GPU.find_last_not_of(L" \t\r\n") + 1);
			}

			// Check for GPU in text (NVIDIA, AMD, Intel patterns)
			if (info.GPU.empty())
			{
				std::wregex gpuPatternRegex(LR"((NVIDIA|GeForce|Radeon|Intel.*(?:UHD|Iris|Arc)|AMD.*Radeon|Qualcomm.*Adreno)[^\n]*)", std::regex::icase);
				if (std::regex_search(normalizedText, gpuMatch, gpuPatternRegex))
				{
					info.GPU = gpuMatch[0].str();
				}
			}

			// Extract VRAM if present
			std::wregex vramRegex(LR"((?:VRAM|Video RAM|GPU Memory|M[eé]moire vid[eé]o)\s*[:\-]?\s*(\d+[\.,]?\d*)\s*(GB|Go|GiB|MB|Mo))", std::regex::icase);
			std::wsmatch vramMatch;
			if (std::regex_search(normalizedText, vramMatch, vramRegex))
			{
				info.VRAM = vramMatch[1].str() + L" " + vramMatch[2].str();
				std::wstring vramValue = vramMatch[1].str();
				std::replace(vramValue.begin(), vramValue.end(), L',', L'.');
				try {
					info.VramGB = std::stod(vramValue);
					std::wstring unit = vramMatch[2].str();
					std::transform(unit.begin(), unit.end(), unit.begin(), ::towlower);
					if (unit == L"mb" || unit == L"mo") {
						info.VramGB /= 1024;
					}
				}
				catch (...) {}
			}

			// Look for VRAM in GPU section (e.g., "16 GB" near GPU info)
			if (info.VramGB == 0 && !info.GPU.empty())
			{
				// Check if GPU line contains memory info
				std::wregex gpuVramRegex(LR"((\d+)\s*(GB|Go))", std::regex::icase);
				if (std::regex_search(info.GPU, vramMatch, gpuVramRegex))
				{
					try {
						info.VramGB = std::stod(vramMatch[1].str());
						info.VRAM = vramMatch[0].str();
					}
					catch (...) {}
				}
			}

			// Extract device name
			std::wregex deviceRegex(LR"((?:Device name|Nom de l'appareil|Ger[aä]tename|Nombre del dispositivo)\s*[:\-]?\s*(.+?)(?:\n|$))", std::regex::icase);
			std::wsmatch deviceMatch;
			if (std::regex_search(normalizedText, deviceMatch, deviceRegex))
			{
				info.DeviceName = deviceMatch[1].str();
				info.DeviceName.erase(info.DeviceName.find_last_not_of(L" \t\r\n") + 1);
			}

			// Extract system type - look for architecture-specific patterns
			// French: "Système d'exploitation 64 bits, processeur x64"
			// English: "64-bit operating system, x64-based processor"
			std::wregex systemRegex(LR"((?:System type|Type du syst[eè]me|Systemtyp|Tipo de sistema)\s*[:\-]?\s*(.+?)(?:\n|$))", std::regex::icase);
			std::wsmatch systemMatch;
			if (std::regex_search(normalizedText, systemMatch, systemRegex))
			{
				info.SystemType = systemMatch[1].str();
				info.SystemType.erase(info.SystemType.find_last_not_of(L" \t\r\n") + 1);
			}

			// If SystemType doesn't contain architecture info, try to find it directly
			if (info.SystemType.empty() ||
				(info.SystemType.find(L"64") == std::wstring::npos &&
					info.SystemType.find(L"32") == std::wstring::npos &&
					info.SystemType.find(L"ARM") == std::wstring::npos &&
					info.SystemType.find(L"arm") == std::wstring::npos &&
					info.SystemType.find(L"x64") == std::wstring::npos &&
					info.SystemType.find(L"x86") == std::wstring::npos))
			{
				// Look for architecture patterns directly in text
				std::wregex archRegex(LR"((64[- ]?bit|32[- ]?bit|x64|x86|ARM64|ARM|aarch64)[^\n]*(?:processor|processeur|based|bas[eé]))", std::regex::icase);
				if (std::regex_search(normalizedText, systemMatch, archRegex))
				{
					info.SystemType = systemMatch[0].str();
				}
				// Also check for standalone architecture mentions
				else
				{
					std::wregex simpleArchRegex(LR"((?:processeur|processor)\s+(x64|x86|ARM64|ARM))", std::regex::icase);
					if (std::regex_search(normalizedText, systemMatch, simpleArchRegex))
					{
						info.SystemType = systemMatch[0].str();
					}
				}
			}

			return info;
		}

		static std::vector<HardwareCheckResult> AnalyzeHardware(const HardwareInfo& info)
		{
			std::vector<HardwareCheckResult> results;

			// Analyze CPU
			HardwareCheckResult cpuResult;
			cpuResult.Name = L"Processor";
			cpuResult.Value = info.Processor.empty() ? L"?" : info.Processor;
			cpuResult.Status = AnalyzeCPU(info.Processor, cpuResult.ReasonKey);
			results.push_back(cpuResult);

			// Analyze GPU
			HardwareCheckResult gpuResult;
			gpuResult.Name = L"Graphics Card";
			gpuResult.Value = info.GPU.empty() ? L"?" : info.GPU;
			gpuResult.Status = AnalyzeGPU(info.GPU, info.VramGB, gpuResult.ReasonKey);
			results.push_back(gpuResult);

			// Analyze RAM
			HardwareCheckResult ramResult;
			ramResult.Name = L"RAM";
			ramResult.Value = info.RAM.empty() ? L"?" : info.RAM;
			ramResult.Status = AnalyzeRAM(info.RamGB, ramResult.ReasonKey);
			results.push_back(ramResult);

			// Analyze VRAM (shared memory check)
			HardwareCheckResult vramResult;
			vramResult.Name = L"Video Memory";
			vramResult.Value = info.VRAM.empty() ? L"?" : info.VRAM;
			vramResult.Status = AnalyzeVRAM(info.VramGB, info.GPU, vramResult.ReasonKey);
			results.push_back(vramResult);

			// Analyze System Architecture
			HardwareCheckResult archResult;
			archResult.Name = L"Architecture";
			std::wstring detectedArch;
			archResult.Status = AnalyzeArchitecture(info.SystemType, info.Processor, archResult.ReasonKey, detectedArch);
			archResult.Value = detectedArch;
			results.push_back(archResult);

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

			// Check for unsupported architecture first - return 0 immediately
			for (const auto& result : results)
			{
				if (result.Value == L"?")
					continue;

				// If Architecture is Bad (ARM or x86), the system is not supported
				if (result.Name == L"Architecture" && result.Status == StatusLevel::Bad)
				{
					return 0;
				}
			}

			int score = 100;
			for (const auto& result : results)
			{
				if (result.Value == L"?")
					continue; // Don't penalize unknown values

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
		static StatusLevel AnalyzeCPU(const std::wstring& cpu, std::wstring& reasonKey)
		{
			if (cpu.empty())
			{
				reasonKey = L"Reason_CPUNotFound";
				return StatusLevel::Warning;
			}

			std::wstring cpuLower = cpu;
			std::transform(cpuLower.begin(), cpuLower.end(), cpuLower.begin(), ::towlower);

			// Check for Qualcomm ARM (bad)
			if (cpuLower.find(L"qualcomm") != std::wstring::npos ||
				cpuLower.find(L"snapdragon") != std::wstring::npos)
			{
				reasonKey = L"Reason_QualcommARM";
				return StatusLevel::Bad;
			}

			// Check CPU generation/age
			// Modern CPUs (2020+): Intel 10th gen+, AMD Ryzen 3000+

			// Intel Core patterns
			std::wregex intelGenRegex(LR"(i[3579]-(\d{2})(\d{2,3}))", std::regex::icase);
			std::wsmatch intelMatch;
			if (std::regex_search(cpuLower, intelMatch, intelGenRegex))
			{
				int gen = std::stoi(intelMatch[1].str());
				if (gen >= 10)
				{
					reasonKey = L"Reason_ModernIntel";
					return StatusLevel::Good;
				}
				else
				{
					reasonKey = L"Reason_OlderIntel";
					return StatusLevel::Warning;
				}
			}

			// AMD Ryzen patterns
			std::wregex ryzenRegex(LR"(ryzen\s*[3579]\s*(\d)(\d{3}))", std::regex::icase);
			std::wsmatch ryzenMatch;
			if (std::regex_search(cpuLower, ryzenMatch, ryzenRegex))
			{
				int series = std::stoi(ryzenMatch[1].str());
				if (series >= 3)
				{
					reasonKey = L"Reason_ModernRyzen";
					return StatusLevel::Good;
				}
				else
				{
					reasonKey = L"Reason_OlderRyzen";
					return StatusLevel::Warning;
				}
			}

			// Check for known old CPU families
			if (cpuLower.find(L"pentium") != std::wstring::npos ||
				cpuLower.find(L"celeron") != std::wstring::npos ||
				cpuLower.find(L"atom") != std::wstring::npos)
			{
				reasonKey = L"Reason_LowPerfCPU";
				return StatusLevel::Warning;
			}

			// Check for Intel Core without generation (older naming)
			if (cpuLower.find(L"core 2") != std::wstring::npos ||
				cpuLower.find(L"core2") != std::wstring::npos)
			{
				reasonKey = L"Reason_VeryOldCPU";
				return StatusLevel::Bad;
			}

			// Default for unrecognized but present CPU
			reasonKey = L"Reason_CPUDetected";
			return StatusLevel::Good;
		}

		static StatusLevel AnalyzeGPU(const std::wstring& gpu, double vramGB, std::wstring& reasonKey)
		{
			if (gpu.empty())
			{
				reasonKey = L"Reason_GPUNotFound";
				return StatusLevel::Warning;
			}

			std::wstring gpuLower = gpu;
			std::transform(gpuLower.begin(), gpuLower.end(), gpuLower.begin(), ::towlower);

			// Check for Qualcomm Adreno (bad - ARM)
			if (gpuLower.find(L"adreno") != std::wstring::npos ||
				gpuLower.find(L"qualcomm") != std::wstring::npos)
			{
				reasonKey = L"Reason_QualcommAdreno";
				return StatusLevel::Bad;
			}

			// Check for Intel integrated graphics
			if (gpuLower.find(L"intel") != std::wstring::npos)
			{
				// Intel Arc is dedicated - good
				if (gpuLower.find(L"arc") != std::wstring::npos)
				{
					reasonKey = L"Reason_IntelArc";
					return StatusLevel::Good;
				}
				// Intel Iris is better integrated
				if (gpuLower.find(L"iris") != std::wstring::npos)
				{
					reasonKey = L"Reason_IntelIris";
					return StatusLevel::Warning;
				}
				// Intel UHD
				if (gpuLower.find(L"uhd") != std::wstring::npos)
				{
					reasonKey = L"Reason_IntelUHD";
					return StatusLevel::Warning;
				}
				// Generic Intel
				reasonKey = L"Reason_IntelIntegrated";
				return StatusLevel::Warning;
			}

			// NVIDIA dedicated GPU - good
			if (gpuLower.find(L"nvidia") != std::wstring::npos ||
				gpuLower.find(L"geforce") != std::wstring::npos ||
				gpuLower.find(L"rtx") != std::wstring::npos ||
				gpuLower.find(L"gtx") != std::wstring::npos ||
				gpuLower.find(L"quadro") != std::wstring::npos)
			{
				reasonKey = L"Reason_NVIDIADedicated";
				return StatusLevel::Good;
			}

			// AMD dedicated GPU - good
			if (gpuLower.find(L"radeon") != std::wstring::npos)
			{
				// Check if it's integrated (Vega in APUs)
				if (gpuLower.find(L"vega") != std::wstring::npos &&
					(gpuLower.find(L"ryzen") != std::wstring::npos || vramGB < 2))
				{
					reasonKey = L"Reason_AMDVega";
					return StatusLevel::Warning;
				}
				reasonKey = L"Reason_AMDRadeon";
				return StatusLevel::Good;
			}

			// Default
			reasonKey = L"Reason_GPUDetected";
			return StatusLevel::Good;
		}

		static StatusLevel AnalyzeRAM(double ramGB, std::wstring& reasonKey)
		{
			if (ramGB <= 0)
			{
				reasonKey = L"Reason_RAMNotFound";
				return StatusLevel::Warning;
			}

			if (ramGB < 8)
			{
				reasonKey = L"Reason_VeryLowRAM";
				return StatusLevel::Bad;
			}
			else if (ramGB < 12)
			{
				reasonKey = L"Reason_LowRAM";
				return StatusLevel::Warning;
			}
			else if (ramGB < 16)
			{
				reasonKey = L"Reason_AcceptableRAM";
				return StatusLevel::Good;
			}
			else
			{
				reasonKey = L"Reason_GoodRAM";
				return StatusLevel::Good;
			}
		}

		static StatusLevel AnalyzeVRAM(double vramGB, const std::wstring& gpu, std::wstring& reasonKey)
		{
			if (vramGB <= 0)
			{
				// Check if GPU is integrated (shared memory)
				std::wstring gpuLower = gpu;
				std::transform(gpuLower.begin(), gpuLower.end(), gpuLower.begin(), ::towlower);

				if (gpuLower.find(L"intel") != std::wstring::npos &&
					gpuLower.find(L"arc") == std::wstring::npos)
				{
					reasonKey = L"Reason_SharedMemory";
					return StatusLevel::Warning;
				}

				reasonKey = L"Reason_VRAMNotFound";
				return StatusLevel::Warning;
			}

			if (vramGB < 2)
			{
				reasonKey = L"Reason_VeryLowVRAM";
				return StatusLevel::Bad;
			}
			else if (vramGB < 4)
			{
				reasonKey = L"Reason_LowVRAM";
				return StatusLevel::Warning;
			}
			else
			{
				reasonKey = L"Reason_GoodVRAM";
				return StatusLevel::Good;
			}
		}

		static StatusLevel AnalyzeArchitecture(const std::wstring& systemType, const std::wstring& cpu, std::wstring& reasonKey, std::wstring& detectedArch)
		{
			std::wstring combined = systemType + L" " + cpu;
			std::transform(combined.begin(), combined.end(), combined.begin(), ::towlower);

			// Check for ARM architecture (Qualcomm/Snapdragon - not supported)
			if (combined.find(L"arm") != std::wstring::npos ||
				combined.find(L"aarch64") != std::wstring::npos ||
				combined.find(L"qualcomm") != std::wstring::npos ||
				combined.find(L"snapdragon") != std::wstring::npos)
			{
				detectedArch = L"ARM64";
				reasonKey = L"Reason_ARM64";
				return StatusLevel::Bad;
			}

			// Check for 64-bit x86 - the only officially supported architecture
			if (combined.find(L"x64") != std::wstring::npos ||
				combined.find(L"64-bit") != std::wstring::npos ||
				combined.find(L"64 bits") != std::wstring::npos ||
				combined.find(L"amd64") != std::wstring::npos)
			{
				detectedArch = L"x64";
				reasonKey = L"Reason_x64";
				return StatusLevel::Good;
			}

			// 32-bit x86 is not supported
			if (combined.find(L"x86") != std::wstring::npos ||
				combined.find(L"32-bit") != std::wstring::npos ||
				combined.find(L"32 bits") != std::wstring::npos)
			{
				detectedArch = L"x86";
				reasonKey = L"Reason_x86";
				return StatusLevel::Bad;
			}

			detectedArch = L"?";
			reasonKey = L"Reason_ArchUnknown";
			return StatusLevel::Warning;
		}
	};
}
