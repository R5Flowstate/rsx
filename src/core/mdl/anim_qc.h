#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <map>
#include <cstdint>

// Forward declarations - avoid circular dependencies
class AnimRigAsset;
class AnimSeqAsset;

namespace animqc
{
	// Animation data structure
	struct Animation
	{
		struct ikrule_t
		{
			int index;
		};

		struct movement_t
		{
			float v_start;
			float v_end;
		};

		std::string name;
		std::string smdFile;
		size_t numframes = 1;
		std::vector<ikrule_t> ikrules;
		std::vector<movement_t> movement;
		int sectionFrames = 0;
		float fps = 30.0f;

		Animation(const std::string& name, const std::string& smdFile, const float fps, const size_t numframes)
			: name(name), smdFile(smdFile), fps(fps), numframes(numframes) {}
	};

	// Sequence data structure
	struct Sequence
	{
		struct poseparamkey_t
		{
			std::string name;
			float start = 0.0f;
			float end = 1.0f;
		};

		std::string name;
		std::vector<std::string> animName;
		std::vector<uint64_t> autolayerGuid;
		bool loop = false;
		bool delta = false;
		float fadeIn = 0.2f;
		float fadeOut = 0.2f;

		std::string activity;
		int activityWeight = 0;
		std::vector<std::string> activityModifiers;
		std::vector<std::string> events;
		std::vector<std::string> nodes;
		std::vector<std::pair<std::string, std::string>> transitions;
		std::vector<std::string> exitnode;
		std::vector<std::string> entrynode;
		std::vector<poseparamkey_t> blendkeys;
		int blendWidth = 0;

		Sequence(const std::string& name) : name(name) {}

		void AddEvent(const std::string& e) { events.push_back(e); }
		void AddNode(const std::string& nodeName) { nodes.push_back(nodeName); }
		void AddAnimation(const std::string& raw_name) { animName.push_back(raw_name); }
		void AddTransition(const std::string& from, const std::string& to) { transitions.emplace_back(from, to); }
		void AddExitNode(const std::string& n) { exitnode.emplace_back(n); }
		void AddEntryNode(const std::string& n) { entrynode.emplace_back(n); }
		void AddActivityModifier(const std::string& mod) { activityModifiers.push_back(mod); }
	};

	// Consolidated QC model data
	struct QCModelData
	{
		struct bonedata_t
		{
			std::string name;
			std::string parentName;
			float pos[3] = { 0.0f, 0.0f, 0.0f };
			float rot[3] = { 0.0f, 0.0f, 0.0f };
		};

		struct attachment_t
		{
			std::string name;
			std::string bone;
			float pos[3] = { 0.0f, 0.0f, 0.0f };
			float rot[3] = { 0.0f, 0.0f, 0.0f };
		};

		struct poseparam_t
		{
			std::string name;
			int flags;
			float start;
			float end;
			float loop;
		};

		bool hasRrig = false;

		std::string rigName;
		float illumposition[3] = { 0.0f, 0.0f, 0.0f };
		float cboxmin[3] = { 0.0f, 0.0f, 0.0f };
		float cboxmax[3] = { 0.0f, 0.0f, 0.0f };
		float bboxmin[3] = { 0.0f, 0.0f, 0.0f };
		float bboxmax[3] = { 0.0f, 0.0f, 0.0f };

		std::vector<attachment_t> attachments;
		std::vector<bonedata_t> bones;
		std::vector<poseparam_t> poseparam;
		std::vector<std::string> nodename;
		std::vector<std::string> animnames;
		std::vector<Animation> animations;
		std::map<uint64_t, std::string> autolayers;
		std::vector<Sequence> sequences;

		void AddBone(const bonedata_t& b) { bones.push_back(b); }
		void AddAnimation(const Animation& a)
		{
			for (const auto& anim : animations)
			{
				if (anim.name == a.name) return;
			}
			animations.push_back(a);
		}
		void AddSequence(const Sequence& seq);
		std::string GetAutoLayerName(const uint64_t& guid) const
		{
			auto it = autolayers.find(guid);
			if (it != autolayers.end()) return it->second;
			return "Error : Couldn't find an autolayer string.";
		}
	};

	// Read raw rrig file
	bool ReadRrigFile(const std::filesystem::path& rrigPath, QCModelData& modelOut, int version = 19);

	// Read raw rseq file
	bool ReadRseqFile(const std::filesystem::path& rseqPath, QCModelData& modelOut, int version = 12);

	// Read multiple rseq files
	bool ReadRseqFiles(const std::vector<std::filesystem::path>& rseqPaths, QCModelData& modelOut, int version = 12);

	// Write consolidated QC file
	bool WriteQCFile(const QCModelData& model, const std::filesystem::path& outputPath);

	// Export animseqs as QC (from loaded asset)
	bool ExportAnimSeqsFromAsset(const std::vector<const AnimSeqAsset*>& animSeqAssets, const AnimRigAsset* const parentRig, std::filesystem::path& exportPath);
}


bool ExportAnimRigQC(const AnimRigAsset* const animRigAsset, std::filesystem::path& exportPath);
bool ExportAnimSeqsQC(const std::vector<std::filesystem::path>& rseqPaths, const std::filesystem::path& rrigPath, std::filesystem::path& exportPath);
