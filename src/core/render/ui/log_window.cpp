// This file is probably in the wrong place; I'll figure it out later.

#include <pch.h>
#include <imgui.h>
#include <game/asset.h>
#include "core/render/uistate.h"
#include <game/rtech/cpakfile.h>
#include <game/rtech/assets/settings.h>
#include <core/render/dx.h>
#include <game/rtech/assets/localization.h>

extern CDXParentHandler* g_dxHandler;

enum eLogMessageColumnID
{
	LMC_LOG_TIME,
	LMC_LOG_LEVEL,
	LMC_LOG_SOURCE,
	LMC_LOG_MSG,

	_LMC_COUNT,
};

void LogWindow_Draw(CUIState* uiState)
{
    ImGui::SetNextWindowSize(ImVec2(0.f, 0.f), ImGuiCond_Always);
    if (ImGui::Begin("Logs", &uiState->logWindowVisible, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize))
    {
		constexpr ImGuiTableFlags tableFlags =
			ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable
			| /*ImGuiTableFlags_Sortable |*/ ImGuiTableFlags_SortMulti
			| ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_NoBordersInBody
			| ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;

		const ImVec2 outerSize = ImVec2(0.f, 0.f);

		if (ImGui::BeginTable("Logs", eLogMessageColumnID::_LMC_COUNT, tableFlags, outerSize))
		{
			ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, 0.0f, eLogMessageColumnID::LMC_LOG_LEVEL);
			ImGui::TableSetupColumn("Level", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, 0.f, eLogMessageColumnID::LMC_LOG_LEVEL);
			ImGui::TableSetupColumn("File", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, 0.f, eLogMessageColumnID::LMC_LOG_SOURCE);
			ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_NoResize | ImGuiTableColumnFlags_WidthFixed, 500.0f, eLogMessageColumnID::LMC_LOG_MSG);

			ImGui::TableSetupScrollFreeze(1, 1);

			ImGui::TableHeadersRow();

			for (size_t i = 0; i < g_assetData.GetNumLogMessages(); ++i)
			{
				const ContainerMessage_t* msg = &g_assetData.GetLogMessages()[i];

				ImGui::PushID(static_cast<int>(i));

				ImGui::TableNextRow(ImGuiTableRowFlags_None, 0.f);

				if (ImGui::TableSetColumnIndex(eLogMessageColumnID::LMC_LOG_TIME))
					ImGui::TextUnformatted(msg->timestampStr);

#define HEX_TO_IMVEC4(hex) ImVec4(((hex & 0xFF000000) >> 24) / 255.f, ((hex & 0x00FF0000) >> 16) / 255.f, ((hex & 0x0000FF00) >> 8) / 255.f, (hex & 0xFF) / 255.f)
				ImGui::PushStyleColor(ImGuiCol_Text, HEX_TO_IMVEC4(s_LogLevelColours[static_cast<uint8_t>(msg->type)]));
				if (ImGui::TableSetColumnIndex(eLogMessageColumnID::LMC_LOG_LEVEL))
					ImGui::TextUnformatted(s_LogLevelNames[static_cast<uint8_t>(msg->type)]);
				ImGui::PopStyleColor();
#undef HEX_TO_IMVEC4

				if (ImGui::TableSetColumnIndex(eLogMessageColumnID::LMC_LOG_SOURCE))
					ImGui::TextUnformatted(msg->sourceName);

				if (ImGui::TableSetColumnIndex(eLogMessageColumnID::LMC_LOG_MSG))
					ImGui::TextWrapped(msg->message);

				ImGui::PopID();
			}

			ImGui::EndTable();
		}


        ImGui::End();
    }
}
