#include <pch.h>
#include <core/mdl/anim_qc.h>
#include <game/rtech/assets/animseq.h>
#include <game/rtech/assets/animseq_data.h>
#include <game/rtech/assets/animrig.h>
#include <game/rtech/cpakfile.h>
#include <game/rtech/utils/studio/studio.h>
#include <game/rtech/utils/utils.h>
#include <game/asset.h>
#include <core/mdl/stringtable.h>
#include <core/mdl/modeldata.h>
#include <core/mdl/animdata.h>
#include <core/math/matrix.h>

#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>

namespace animqc
{
	// Helper function to get name from path
	std::string GetNameFromPath(const std::string& animName)
	{
		std::string name = animName;
		std::replace(name.begin(), name.end(), '\\', '/');
		size_t slash_pos = name.find_last_of('/');
		name = (slash_pos != std::string::npos) ? name.substr(slash_pos + 1) : name;
		return name;
	}

	// AddSequence implementation - just adds the sequence to the list
	// GUID mapping is handled separately during export
	void QCModelData::AddSequence(const Sequence& seq)
	{
		sequences.push_back(seq);
	}

	// Write consolidated QC file
	bool WriteQCFile(const QCModelData& model, const std::filesystem::path& outputPath)
	{
		std::ofstream out(outputPath);
		if (!out)
		{
			Log("ANIMQC: Failed to open output file: %s\n", outputPath.string().c_str());
			return false;
		}

		out << std::fixed << std::setprecision(6);

		if (model.hasRrig)
		{
			out << "$modelname \"" << model.rigName << "\"\n\n";

			// illumposition
			out << "$illumposition "
				<< model.illumposition[0] << " " << model.illumposition[1] << " " << model.illumposition[2] << "\n";

			// cbox
			out << "$cbox "
				<< model.cboxmin[0] << " " << model.cboxmin[1] << " " << model.cboxmin[2] << " "
				<< model.cboxmax[0] << " " << model.cboxmax[1] << " " << model.cboxmax[2] << "\n";

			// bbox
			out << "$bbox "
				<< model.bboxmin[0] << " " << model.bboxmin[1] << " " << model.bboxmin[2] << " "
				<< model.bboxmax[0] << " " << model.bboxmax[1] << " " << model.bboxmax[2] << "\n\n";

			// attachments
			for (const auto& att : model.attachments)
			{
				out << "$attachment \"" << att.name << "\" \"" << att.bone << "\" "
					<< att.pos[0] << " " << att.pos[1] << " " << att.pos[2] << " rotate "
					<< att.rot[0] << " " << att.rot[1] << " " << att.rot[2] << "\n";
			}
			if (!model.attachments.empty()) out << "\n";

			// poseparam
			for (const auto& pp : model.poseparam)
			{
				out << "$poseparameter \"" << pp.name << "\" " << pp.flags << " "
					<< pp.start << " " << pp.end;
				if (pp.loop > 0.0f)
					out << " loop " << pp.loop;
				out << "\n";
			}
			if (!model.poseparam.empty()) out << "\n";

			out << "$unlockdefinebones" << "\n";
			for (const auto& b : model.bones)
			{
				out << "$definebone \"" << b.name << "\" \"" << b.parentName << "\" "
					<< std::setprecision(16)
					<< b.pos[0] << " " << b.pos[1] << " " << b.pos[2] << " "
					<< b.rot[0] << " " << b.rot[1] << " " << b.rot[2]
					<< " 0 0 0 0 0 0"
					<< std::setprecision(6) << "\n";
			}
			out << "\n";
			for (const auto& b : model.bones)
			{
				out << "$bonemerge \"" << b.name << "\"\n";
			}
			out << "\n";
		}

		out << "$sectionframes 63 120\n\n";

		// $animation
		for (const auto& anim : model.animations)
		{
			out << "$animation " << anim.name << " \"" << anim.smdFile << "\" {\n";
			out << "\tfps " << anim.fps << "\n";
			out << "}\n\n";
		}

		// Sequences
		for (const auto& seq : model.sequences)
		{
			out << "$sequence \"" << seq.name << "\" {\n";

			for (const auto& animName : seq.animName)
			{
				if (animName.empty()) continue;
				out << "\t\"" << animName << "\"\n";
			}

			out << "\n";

			if (seq.fadeIn >= 0.0f) out << "\tfadein " << seq.fadeIn << "\n";
			if (seq.fadeOut >= 0.0f) out << "\tfadeout " << seq.fadeOut << "\n";
			if (seq.loop) out << "\tloop\n";
			if (seq.delta) out << "\tdelta\n";

			if (!seq.activity.empty())
				out << "\tactivity " << seq.activity << " " << seq.activityWeight << "\n";

			for (const auto& mod : seq.activityModifiers)
				out << "\tactivitymodifier \"" << mod << "\"\n";

			if (seq.blendWidth > 1)
				out << "\tblendwidth " << seq.blendWidth << "\n";

			for (const auto& b : seq.blendkeys)
			{
				out << "\tblend " << b.name << " " << b.start << " " << b.end << "\n";
			}

			for (const auto& guid : seq.autolayerGuid)
			{
				std::string layerName = model.GetAutoLayerName(guid);
				out << "\taddlayer \"" << layerName << "\"" << "\n";
			}

			// Events
			for (const auto& e : seq.events)
				out << "\t" << e << "\n";

			// Nodes and transitions
			if (!seq.nodes.empty())
			{
				out << "\tnode ";
				for (const auto& n : seq.nodes) out << n << " ";
				out << "\n";
			}
			for (const auto& t : seq.transitions)
				out << "\ttransition " << t.first << " " << t.second << "\n";

			for (const auto& n : seq.exitnode)
				out << "\t//transition 0 " << n << "\n";

			for (const auto& n : seq.entrynode)
				out << "\t//transition " << n << " 0\n";

			out << "}\n\n";
		}

		out.close();
		Log("ANIMQC: Exported QC file to: %s\n", outputPath.string().c_str());
		return true;
	}

	// Read raw rrig file (simplified version)
	bool ReadRrigFile(const std::filesystem::path& rrigPath, QCModelData& modelOut, int version)
	{
		UNUSED(version);

		if (!std::filesystem::exists(rrigPath))
		{
			Log("ANIMQC: Rrig file does not exist: %s\n", rrigPath.string().c_str());
			return false;
		}

		size_t fileSize = std::filesystem::file_size(rrigPath);
		std::ifstream rrig_stream(rrigPath, std::ios::binary);

		if (!rrig_stream)
		{
			Log("ANIMQC: Failed to open rrig file: %s\n", rrigPath.string().c_str());
			return false;
		}

		std::vector<char> buffer(fileSize);
		rrig_stream.read(buffer.data(), fileSize);
		rrig_stream.close();

		// Check for valid studio header
		int id = *reinterpret_cast<int*>(buffer.data());
		if (id != 'TSDI' && id != 'IDST')
		{
			Log("ANIMQC: Invalid studio header in rrig file\n");
			return false;
		}

		modelOut.hasRrig = true;
		modelOut.rigName = rrigPath.stem().string();

		Log("ANIMQC: Loaded rrig file: %s (%zu bytes)\n", rrigPath.string().c_str(), fileSize);

		return true;
	}

	// Read raw rseq file
	bool ReadRseqFile(const std::filesystem::path& rseqPath, QCModelData& modelOut, int version)
	{
		UNUSED(version);

		if (!std::filesystem::exists(rseqPath))
		{
			Log("ANIMQC: Rseq file does not exist: %s\n", rseqPath.string().c_str());
			return false;
		}

		size_t fileSize = std::filesystem::file_size(rseqPath);
		std::ifstream rseq_stream(rseqPath, std::ios::binary);

		if (!rseq_stream)
		{
			Log("ANIMQC: Failed to open rseq file: %s\n", rseqPath.string().c_str());
			return false;
		}

		std::vector<char> buffer(fileSize);
		rseq_stream.read(buffer.data(), fileSize);
		rseq_stream.close();

		std::string seqName = rseqPath.stem().string();
		Sequence seq(seqName);

		modelOut.AddSequence(seq);

		Log("ANIMQC: Loaded rseq file: %s (%zu bytes)\n", rseqPath.string().c_str(), fileSize);

		return true;
	}

	// Read multiple rseq files
	bool ReadRseqFiles(const std::vector<std::filesystem::path>& rseqPaths, QCModelData& modelOut, int version)
	{
		UNUSED(version);

		for (const auto& path : rseqPaths)
		{
			if (!ReadRseqFile(path, modelOut, version))
				return false;
		}
		return true;
	}

	// Export animrig as QC (from loaded asset)
	bool ExportAnimRigQC(const AnimRigAsset* const animRigAsset, std::filesystem::path& exportPath)
	{
		if (!animRigAsset)
		{
			Log("ANIMQC: Null animrig asset\n");
			return false;
		}

		Log("ANIMQC: Exporting animrig as QC: %s\n", animRigAsset->name);

		const ModelParsedData_t* parsedData = &animRigAsset->parsedData;
		const studiohdr_generic_t* hdr = parsedData->pStudioHdr();

		// Get rig name stem for creating subdirectories
		const std::filesystem::path rigPath(animRigAsset->name);
		const std::string rigStem = rigPath.stem().string();

		// Create anims directory for SMD exports
		std::filesystem::path animsDir = exportPath.parent_path() / ("anims_" + rigStem);
		if (!std::filesystem::exists(animsDir))
		{
			std::filesystem::create_directories(animsDir);
		}

		QCModelData modelData;

		// Track exported sequences to prevent duplicates
		std::unordered_set<std::string> exportedSequences;
		modelData.hasRrig = true;
		modelData.rigName = animRigAsset->name;

		// Copy illumposition
		modelData.illumposition[0] = hdr->illumposition.x;
		modelData.illumposition[1] = hdr->illumposition.y;
		modelData.illumposition[2] = hdr->illumposition.z;

		// Copy hull_min and hull_max
		modelData.cboxmin[0] = hdr->hull_min.x;
		modelData.cboxmin[1] = hdr->hull_min.y;
		modelData.cboxmin[2] = hdr->hull_min.z;
		modelData.cboxmax[0] = hdr->hull_max.x;
		modelData.cboxmax[1] = hdr->hull_max.y;
		modelData.cboxmax[2] = hdr->hull_max.z;

		// Copy view_bbmin and view_bbmax
		modelData.bboxmin[0] = hdr->view_bbmin.x;
		modelData.bboxmin[1] = hdr->view_bbmin.y;
		modelData.bboxmin[2] = hdr->view_bbmin.z;
		modelData.bboxmax[0] = hdr->view_bbmax.x;
		modelData.bboxmax[1] = hdr->view_bbmax.y;
		modelData.bboxmax[2] = hdr->view_bbmax.z;

		// Export bones
		for (int i = 0; i < parsedData->BoneCount(); i++)
		{
			const ModelBone_t* bone = parsedData->pBone(i);
			QCModelData::bonedata_t boneData;
			boneData.name = bone->pszName();
			boneData.parentName = bone->parent >= 0 ? parsedData->pBone(bone->parent)->pszName() : "";
			boneData.pos[0] = bone->pos.x;
			boneData.pos[1] = bone->pos.y;
			boneData.pos[2] = bone->pos.z;
			boneData.rot[0] = bone->rot.x;
			boneData.rot[1] = bone->rot.y;
			boneData.rot[2] = bone->rot.z;
			modelData.AddBone(boneData);
		}

		// Export pose parameters
		for (int i = 0; i < hdr->localPoseParamCount; i++)
		{
			const ModelPoseParam_t* poseParam = parsedData->pPoseParam(i);
			QCModelData::poseparam_t pp;
			pp.name = poseParam->name;
			pp.flags = poseParam->flags;
			pp.start = poseParam->start;
			pp.end = poseParam->end;
			pp.loop = poseParam->loop;
			modelData.poseparam.push_back(pp);
		}

		// Export node names
		for (int i = 0; i < hdr->localNodeCount; i++)
		{
			const char* nodeName = parsedData->pszNodeName(i);
			if (nodeName)
				modelData.nodename.push_back(nodeName);
		}

		// Export attachments
		for (size_t i = 0; i < parsedData->attachments.size(); i++)
		{
			const ModelAttachment_t* att = parsedData->pAttachment(i);
			QCModelData::attachment_t attData;
			attData.name = att->name;
			attData.bone = parsedData->pBone(att->localbone)->pszName();

			// Extract position from matrix
			attData.pos[0] = att->localmatrix->m_flMatVal[0][3];
			attData.pos[1] = att->localmatrix->m_flMatVal[1][3];
			attData.pos[2] = att->localmatrix->m_flMatVal[2][3];

			// Extract rotation from matrix
			const matrix3x4_t& mat = *att->localmatrix;
			float yaw = asinf(-mat.m_flMatVal[0][2]);
			float pitch, roll;
			if (cosf(yaw) > 1e-6f)
			{
				pitch = atan2f(mat.m_flMatVal[1][2], mat.m_flMatVal[2][2]);
				roll = atan2f(mat.m_flMatVal[0][1], mat.m_flMatVal[0][0]);
			}
			else
			{
				pitch = atan2f(-mat.m_flMatVal[2][1], mat.m_flMatVal[1][1]);
				roll = 0;
			}
			attData.rot[0] = -pitch / (2 * M_PI) * 360.0f;
			attData.rot[1] = yaw / (2 * M_PI) * 360.0f;
			attData.rot[2] = -roll / (2 * M_PI) * 360.0f;

			modelData.attachments.push_back(attData);
		}
		// Export local sequences from the rig (if any)
		// Note: Only export truly local sequences, skip any that are also external animseqs
		// In practice, most rigs use external animseqs, so local sequences are rare
		for (int i = 0; i < parsedData->NumLocalSeq(); i++)
		{
			const ModelSeq_t* seq = parsedData->LocalSeq(i);

			// Check if this sequence exists as an external animseq
			bool isExternal = false;
			std::string fullseqname = std::string(seq->szlabel) + ".rseq";
			uint64_t seqGuid = RTech::StringToGuid(fullseqname.c_str());

			const AssetGuid_t* externalSeqGuids = reinterpret_cast<const AssetGuid_t*>(animRigAsset->animSeqs);
			for (int k = 0; k < animRigAsset->numAnimSeqs; k++)
			{
				if (externalSeqGuids[k].guid == seqGuid)
				{
					isExternal = true;
					break;
				}
			}

			// Skip if this is an external animseq (will be exported separately)
			if (isExternal)
				continue;

			Sequence seqData(seq->szlabel);

			// Add animations
			for (int j = 0; j < seq->AnimCount(); j++)
			{
				const ModelAnim_t* anim = seq->Anim(j);
				std::string animName = anim->pszName();
				seqData.AddAnimation(animName);

				// Create animation entry with SMD path
				std::string smdFile = "anims_" + rigStem + "/" + animName + ".smd";
				Animation animEntry(animName, smdFile, anim->fps, anim->numframes);
				animEntry.sectionFrames = anim->sectionframes;
				modelData.AddAnimation(animEntry);
			}

			// Sequence flags
			if (seq->flags & STUDIO_LOOPING) seqData.loop = true;
			if (seq->flags & STUDIO_DELTA) seqData.delta = true;
			seqData.fadeIn = seq->fadeintime;
			seqData.fadeOut = seq->fadeouttime;

			// Activity
			if (seq->szactivityname && strlen(seq->szactivityname) > 0)
			{
				seqData.activity = seq->szactivityname;
				seqData.activityWeight = seq->actweight;
			}

			// Blend keys
			if (seq->groupsize[0] > 1 || seq->groupsize[1] > 1)
			{
				seqData.blendWidth = std::max(seq->groupsize[0], seq->groupsize[1]);
				for (int j = 0; j < 2; j++)
				{
					if (seq->paramindex[j] >= 0)
					{
						Sequence::poseparamkey_t key;
						const ModelPoseParam_t* poseParam = parsedData->pPoseParam(seq->paramindex[j]);
						key.name = poseParam->name;
						key.start = seq->paramstart[j];
						key.end = seq->paramend[j];
						seqData.blendkeys.push_back(key);
					}
				}
			}

			// Events
			// Calculate total frame count as the maximum across all animations
			int totalframes = 0;
			for (int a = 0; a < seq->AnimCount(); a++)
			{
				const ModelAnim_t* anim = seq->Anim(a);
				if (anim->numframes > totalframes)
					totalframes = anim->numframes;
			}
			if (totalframes == 0) totalframes = 1;  // Safety check

			for (int j = 0; j < seq->numevents; j++)
			{
				const ModelEvent_t* event = seq->pEvent(j);
				int frame = static_cast<int>(event->cycle * totalframes);

				// Clamp frame to valid range to prevent out-of-range events
				if (frame < 0) frame = 0;
				if (frame >= totalframes) frame = totalframes - 1;

				std::stringstream ss;
				ss << "{ event " << event->name << " " << frame << " \"" << event->options << "\" }";
				seqData.AddEvent(ss.str());
			}

			// Nodes
			if (seq->localentrynode > 0 || seq->localexitnode > 0)
			{
				if (seq->localentrynode == seq->localexitnode && seq->localentrynode > 0)
				{
					const char* nodeName = parsedData->pszNodeName(seq->localentrynode - 1);
					if (nodeName) seqData.AddNode(nodeName);
				}
				else if (seq->localentrynode != seq->localexitnode)
				{
					const char* entryNode = seq->localentrynode > 0 ? parsedData->pszNodeName(seq->localentrynode - 1) : "";
					const char* exitNode = seq->localexitnode > 0 ? parsedData->pszNodeName(seq->localexitnode - 1) : "";
					if (entryNode && exitNode) seqData.AddTransition(entryNode, exitNode);
				}
			}

			// Auto layers - map GUIDs to sequence names
			for (int j = 0; j < seq->numautolayers; j++)
			{
				const ModelAutoLayer_t* autolayer = seq->pAutoLayer(j);
				seqData.autolayerGuid.push_back(autolayer->sequence.guid);

				// Find the sequence name for this GUID
				const uint64_t layerGuid = autolayer->sequence.guid;

				// Check local sequences first
				for (int k = 0; k < parsedData->NumLocalSeq(); k++)
				{
					const ModelSeq_t* localSeq = parsedData->LocalSeq(k);
					std::string localSeqName = std::string(localSeq->szlabel) + ".rseq";
					uint64_t localGuid = RTech::StringToGuid(localSeqName.c_str());
					if (localGuid == layerGuid)
					{
						modelData.autolayers[layerGuid] = localSeq->szlabel;
						break;
					}
				}

				// Also check external animseqs
				for (uint16_t k = 0; k < animRigAsset->numAnimSeqs; k++)
				{
					const AssetGuid_t* seqGuids = reinterpret_cast<const AssetGuid_t*>(animRigAsset->animSeqs);
					if (seqGuids[k].guid == layerGuid)
					{
						CPakAsset* const layerAsset = g_assetData.FindAssetByGUID<CPakAsset>(layerGuid);
						if (layerAsset)
						{
							AnimSeqAsset* const layerSeq = reinterpret_cast<AnimSeqAsset* const>(layerAsset->extraData());
							if (layerSeq)
							{
								modelData.autolayers[layerGuid] = layerSeq->seqdesc.szlabel;
							}
						}
						break;
					}
				}
			}

			// Add this sequence's own GUID to the autolayers map so other sequences can reference it
			std::string seqFullname = std::string(seq->szlabel) + ".rseq";
			uint64_t thisSeqGuid = RTech::StringToGuid(seqFullname.c_str());
			modelData.autolayers[thisSeqGuid] = seq->szlabel;

			// Check if already exported to prevent duplicates
			if (exportedSequences.find(seq->szlabel) == exportedSequences.end())
			{
				exportedSequences.insert(seq->szlabel);
				modelData.AddSequence(seqData);
			}
		}

		// Export external animseqs associated with this rig
		if (animRigAsset->numAnimSeqs > 0)
		{
			Log("ANIMQC: Exporting %d external animseqs\n", animRigAsset->numAnimSeqs);

			const AssetGuid_t* guids = reinterpret_cast<const AssetGuid_t*>(animRigAsset->animSeqs);

			for (uint16_t seqIdx = 0; seqIdx < animRigAsset->numAnimSeqs; seqIdx++)
			{
				const uint64_t guid = guids[seqIdx].guid;

				CPakAsset* const pakAsset = g_assetData.FindAssetByGUID<CPakAsset>(guid);

				if (!pakAsset)
				{
					Log("ANIMQC: Failed to find animseq asset by GUID: %llx\n", guid);
					continue;
				}

				AnimSeqAsset* const animSeqAsset = reinterpret_cast<AnimSeqAsset* const>(pakAsset->extraData());

				if (!animSeqAsset)
				{
					Log("ANIMQC: Animseq asset not loaded: %s\n", animSeqAsset ? animSeqAsset->name : "unknown");
					continue;
				}

				// Parse the sequence data if it hasn't been parsed yet
				if (!animSeqAsset->animationParsed)
				{
					Log("ANIMQC: Parsing unparsed animseq: %s\n", animSeqAsset->name);

					// Set parent rig if not set
					if (!animSeqAsset->parentRig)
						animSeqAsset->parentRig = const_cast<AnimRigAsset*>(animRigAsset);

					// Parse the sequence data
					switch (animSeqAsset->version)
					{
					case eSeqVersion::VERSION_7:
					{
						ParseSequence(&animSeqAsset->seqdesc, &parsedData->bones, AnimdataFuncType_t::ANIM_FUNC_NOSTALL);
						break;
					}
					case eSeqVersion::VERSION_7_1:
					case eSeqVersion::VERSION_8:
					case eSeqVersion::VERSION_10:
					case eSeqVersion::VERSION_11:
					{
						ParseSequence(&animSeqAsset->seqdesc, &parsedData->bones, AnimdataFuncType_t::ANIM_FUNC_STALL_BASEPTR);
						break;
					}
					case eSeqVersion::VERSION_12:
					{
						if (animSeqAsset->dataSize == 0)
							animSeqAsset->UpdateDataSize_V12(static_cast<int>(parsedData->BoneCount()));
						ParseSequence(&animSeqAsset->seqdesc, &parsedData->bones, AnimdataFuncType_t::ANIM_FUNC_STALL_BASEPTR);
						break;
					}
					case eSeqVersion::VERSION_12_1:
					{
						if (animSeqAsset->dataSize == 0)
							animSeqAsset->UpdateDataSize_V12_1(static_cast<int>(parsedData->BoneCount()));
						ParseAnimSeqDataForSeq(&animSeqAsset->seqdesc, parsedData->BoneCount());
						ParseSequence(&animSeqAsset->seqdesc, &parsedData->bones, AnimdataFuncType_t::ANIM_FUNC_STALL_ANIMDATA);
						break;
					}
					default:
					{
						Log("ANIMQC: Unsupported animseq version: %d\n", static_cast<int>(animSeqAsset->version));
						continue;
					}
					}

					animSeqAsset->animationParsed = true;
				}

				const ModelSeq_t* seq = &animSeqAsset->seqdesc;

				// Export this animseq as SMD
				std::filesystem::path smdPath = animsDir / std::filesystem::path(seq->szlabel).replace_extension(".smd");
				ExportSeqDesc(eAnimSeqExportSetting::ANIMSEQ_SMD, seq, smdPath, animRigAsset->name, &parsedData->bones, guid);

				// Add sequence data to QC
				Sequence seqData(seq->szlabel);

				// Add animations
				for (int j = 0; j < seq->AnimCount(); j++)
				{
					const ModelAnim_t* anim = seq->Anim(j);
					std::string animName = anim->pszName();
					seqData.AddAnimation(animName);

					// Create animation entry with SMD path
					std::string smdFile = "anims_" + rigStem + "/" + animName + ".smd";
					Animation animEntry(animName, smdFile, anim->fps, anim->numframes);
					animEntry.sectionFrames = anim->sectionframes;
					modelData.AddAnimation(animEntry);
				}

				// Sequence flags
				if (seq->flags & STUDIO_LOOPING) seqData.loop = true;
				if (seq->flags & STUDIO_DELTA) seqData.delta = true;
				seqData.fadeIn = seq->fadeintime;
				seqData.fadeOut = seq->fadeouttime;

				// Activity
				if (seq->szactivityname && strlen(seq->szactivityname) > 0)
				{
					seqData.activity = seq->szactivityname;
					seqData.activityWeight = seq->actweight;
				}

				// Blend keys
				if (seq->groupsize[0] > 1 || seq->groupsize[1] > 1)
				{
					seqData.blendWidth = std::max(seq->groupsize[0], seq->groupsize[1]);
					for (int j = 0; j < 2; j++)
					{
						if (seq->paramindex[j] >= 0)
						{
							Sequence::poseparamkey_t key;
							const ModelPoseParam_t* poseParam = parsedData->pPoseParam(seq->paramindex[j]);
							key.name = poseParam->name;
							key.start = seq->paramstart[j];
							key.end = seq->paramend[j];
							seqData.blendkeys.push_back(key);
						}
					}
				}

				// Events
				for (int j = 0; j < seq->numevents; j++)
				{
					const ModelEvent_t* event = seq->pEvent(j);
					int numframes = seq->Anim(0)->numframes;
					int frame = static_cast<int>(event->cycle * numframes);

					// Clamp frame to valid range to prevent out-of-range events
					if (frame < 0) frame = 0;
					if (frame >= numframes && numframes > 0) frame = numframes - 1;

					std::stringstream ss;
					ss << "{ event " << event->name << " " << frame << " \"" << event->options << "\" }";
					seqData.AddEvent(ss.str());
				}

				// Nodes
				if (seq->localentrynode > 0 || seq->localexitnode > 0)
				{
					if (seq->localentrynode == seq->localexitnode && seq->localentrynode > 0)
					{
						const char* nodeName = parsedData->pszNodeName(seq->localentrynode - 1);
						if (nodeName) seqData.AddNode(nodeName);
					}
					else if (seq->localentrynode != seq->localexitnode)
					{
						const char* entryNode = seq->localentrynode > 0 ? parsedData->pszNodeName(seq->localentrynode - 1) : "";
						const char* exitNode = seq->localexitnode > 0 ? parsedData->pszNodeName(seq->localexitnode - 1) : "";
						if (entryNode && exitNode) seqData.AddTransition(entryNode, exitNode);
					}
				}

				// Auto layers - map GUIDs to sequence names
				for (int j = 0; j < seq->numautolayers; j++)
				{
					const ModelAutoLayer_t* autolayer = seq->pAutoLayer(j);
					seqData.autolayerGuid.push_back(autolayer->sequence.guid);

					// Find the sequence name for this GUID and add to autolayers map
					const uint64_t layerGuid = autolayer->sequence.guid;

					// Check external animseqs
					bool found = false;
					for (uint16_t k = 0; k < animRigAsset->numAnimSeqs && !found; k++)
					{
						const AssetGuid_t* seqGuids = reinterpret_cast<const AssetGuid_t*>(animRigAsset->animSeqs);
						if (seqGuids[k].guid == layerGuid)
						{
							CPakAsset* const layerAsset = g_assetData.FindAssetByGUID<CPakAsset>(layerGuid);
							if (layerAsset)
							{
								AnimSeqAsset* const layerSeq = reinterpret_cast<AnimSeqAsset* const>(layerAsset->extraData());
								if (layerSeq)
								{
									modelData.autolayers[layerGuid] = layerSeq->seqdesc.szlabel;
									found = true;
								}
							}
						}
					}

					// Check local sequences if not found in external
					if (!found)
					{
						for (int k = 0; k < parsedData->NumLocalSeq(); k++)
						{
							const ModelSeq_t* localSeq = parsedData->LocalSeq(k);
							// Create GUID from local sequence name to match
							std::string fullseqname = std::string(localSeq->szlabel) + ".rseq";
							uint64_t localGuid = RTech::StringToGuid(fullseqname.c_str());
							if (localGuid == layerGuid)
							{
								modelData.autolayers[layerGuid] = localSeq->szlabel;
								found = true;
								break;
							}
						}
					}
				}

				// Add this sequence's own GUID to the autolayers map
				modelData.autolayers[guid] = seq->szlabel;

				// Check if already exported to prevent duplicates
				if (exportedSequences.find(seq->szlabel) == exportedSequences.end())
				{
					exportedSequences.insert(seq->szlabel);
					modelData.AddSequence(seqData);
				}
			}
		}

		// Change extension to .qc
		exportPath.replace_extension(".qc");

		return WriteQCFile(modelData, exportPath);
	}

	// Export animseqs as QC (from loaded assets)
	bool ExportAnimSeqsFromAsset(const std::vector<const AnimSeqAsset*>& animSeqAssets, const AnimRigAsset* const parentRig, std::filesystem::path& exportPath)
	{
		if (animSeqAssets.empty())
		{
			Log("ANIMQC: No animseq assets to export\n");
			return false;
		}

		Log("ANIMQC: Exporting %zu animseqs as QC\n", animSeqAssets.size());

		const ModelParsedData_t* parsedData = nullptr;
		if (parentRig)
		{
			parsedData = &parentRig->parsedData;
		}
		else if (!animSeqAssets.empty() && animSeqAssets[0]->parentModel)
		{
			parsedData = animSeqAssets[0]->parentModel->GetParsedData();
		}

		if (!parsedData)
		{
			Log("ANIMQC: No parsed data available for export\n");
			return false;
		}

		const studiohdr_generic_t* hdr = parsedData->pStudioHdr();

		QCModelData modelData;
		modelData.hasRrig = true;
		modelData.rigName = parentRig ? parentRig->name : "unknown";

		// Copy illumposition
		modelData.illumposition[0] = hdr->illumposition.x;
		modelData.illumposition[1] = hdr->illumposition.y;
		modelData.illumposition[2] = hdr->illumposition.z;

		// Copy hull_min and hull_max
		modelData.cboxmin[0] = hdr->hull_min.x;
		modelData.cboxmin[1] = hdr->hull_min.y;
		modelData.cboxmin[2] = hdr->hull_min.z;
		modelData.cboxmax[0] = hdr->hull_max.x;
		modelData.cboxmax[1] = hdr->hull_max.y;
		modelData.cboxmax[2] = hdr->hull_max.z;

		// Copy view_bbmin and view_bbmax
		modelData.bboxmin[0] = hdr->view_bbmin.x;
		modelData.bboxmin[1] = hdr->view_bbmin.y;
		modelData.bboxmin[2] = hdr->view_bbmin.z;
		modelData.bboxmax[0] = hdr->view_bbmax.x;
		modelData.bboxmax[1] = hdr->view_bbmax.y;
		modelData.bboxmax[2] = hdr->view_bbmax.z;

		// Export bones
		for (int i = 0; i < parsedData->BoneCount(); i++)
		{
			const ModelBone_t* bone = parsedData->pBone(i);
			QCModelData::bonedata_t boneData;
			boneData.name = bone->pszName();
			boneData.parentName = bone->parent >= 0 ? parsedData->pBone(bone->parent)->pszName() : "";
			boneData.pos[0] = bone->pos.x;
			boneData.pos[1] = bone->pos.y;
			boneData.pos[2] = bone->pos.z;
			boneData.rot[0] = bone->rot.x;
			boneData.rot[1] = bone->rot.y;
			boneData.rot[2] = bone->rot.z;
			modelData.AddBone(boneData);
		}

		// Export pose parameters
		for (int i = 0; i < hdr->localPoseParamCount; i++)
		{
			const ModelPoseParam_t* poseParam = parsedData->pPoseParam(i);
			QCModelData::poseparam_t pp;
			pp.name = poseParam->name;
			pp.flags = poseParam->flags;
			pp.start = poseParam->start;
			pp.end = poseParam->end;
			pp.loop = poseParam->loop;
			modelData.poseparam.push_back(pp);
		}

		// Export node names
		for (int i = 0; i < hdr->localNodeCount; i++)
		{
			const char* nodeName = parsedData->pszNodeName(i);
			if (nodeName)
				modelData.nodename.push_back(nodeName);
		}

		// Export sequences from animseq assets
		for (const AnimSeqAsset* seqAsset : animSeqAssets)
		{
			if (!seqAsset || !seqAsset->animationParsed)
				continue;

			const ModelSeq_t* seq = &seqAsset->seqdesc;

			Sequence seqData(seq->szlabel);

			// Add animations
			for (int j = 0; j < seq->AnimCount(); j++)
			{
				const ModelAnim_t* anim = seq->Anim(j);
				std::string animName = anim->pszName();
				seqData.AddAnimation(animName);

				// Create animation entry
				std::string smdFile = "anims/" + animName + ".smd";
				Animation animEntry(animName, smdFile, anim->fps, anim->numframes);
				animEntry.sectionFrames = anim->sectionframes;
				modelData.AddAnimation(animEntry);
			}

			// Sequence flags
			if (seq->flags & STUDIO_LOOPING) seqData.loop = true;
			if (seq->flags & STUDIO_DELTA) seqData.delta = true;
			seqData.fadeIn = seq->fadeintime;
			seqData.fadeOut = seq->fadeouttime;

			// Activity
			if (seq->szactivityname && strlen(seq->szactivityname) > 0)
			{
				seqData.activity = seq->szactivityname;
				seqData.activityWeight = seq->actweight;
			}

			// Blend keys
			if (seq->groupsize[0] > 1 || seq->groupsize[1] > 1)
			{
				seqData.blendWidth = std::max(seq->groupsize[0], seq->groupsize[1]);
				for (int j = 0; j < 2; j++)
				{
					if (seq->paramindex[j] >= 0)
					{
						Sequence::poseparamkey_t key;
						const ModelPoseParam_t* poseParam = parsedData->pPoseParam(seq->paramindex[j]);
						key.name = poseParam->name;
						key.start = seq->paramstart[j];
						key.end = seq->paramend[j];
						seqData.blendkeys.push_back(key);
					}
				}
			}

			// Events
			// Calculate total frame count as the maximum across all animations
			int totalframes = 0;
			for (int a = 0; a < seq->AnimCount(); a++)
			{
				const ModelAnim_t* anim = seq->Anim(a);
				if (anim->numframes > totalframes)
					totalframes = anim->numframes;
			}
			if (totalframes == 0) totalframes = 1;  // Safety check

			for (int j = 0; j < seq->numevents; j++)
			{
				const ModelEvent_t* event = seq->pEvent(j);
				int frame = static_cast<int>(event->cycle * totalframes);

				// Clamp frame to valid range to prevent out-of-range events
				if (frame < 0) frame = 0;
				if (frame >= totalframes) frame = totalframes - 1;

				std::stringstream ss;
				ss << "{ event " << event->name << " " << frame << " \"" << event->options << "\" }";
				seqData.AddEvent(ss.str());
			}

			// Nodes
			if (seq->localentrynode > 0 || seq->localexitnode > 0)
			{
				if (seq->localentrynode == seq->localexitnode && seq->localentrynode > 0)
				{
					const char* nodeName = parsedData->pszNodeName(seq->localentrynode - 1);
					if (nodeName) seqData.AddNode(nodeName);
				}
				else if (seq->localentrynode != seq->localexitnode)
				{
					const char* entryNode = seq->localentrynode > 0 ? parsedData->pszNodeName(seq->localentrynode - 1) : "";
					const char* exitNode = seq->localexitnode > 0 ? parsedData->pszNodeName(seq->localexitnode - 1) : "";
					if (entryNode && exitNode) seqData.AddTransition(entryNode, exitNode);
				}
			}

			// Auto layers
			for (int j = 0; j < seq->numautolayers; j++)
			{
				const ModelAutoLayer_t* autolayer = seq->pAutoLayer(j);
				seqData.autolayerGuid.push_back(autolayer->sequence.guid);
			}

			modelData.AddSequence(seqData);
		}

		// Change extension to .qc
		exportPath.replace_extension(".qc");

		return WriteQCFile(modelData, exportPath);
	}
}

// Implement the wrapper functions that are declared in modeldata.h
bool ExportAnimRigQC(const AnimRigAsset* const animRigAsset, std::filesystem::path& exportPath)
{
	return animqc::ExportAnimRigQC(animRigAsset, exportPath);
}

bool ExportAnimSeqsQC(const std::vector<std::filesystem::path>& rseqPaths, const std::filesystem::path& rrigPath, std::filesystem::path& exportPath)
{
	animqc::QCModelData modelData;

	// Read rrig if provided
	if (!rrigPath.empty())
	{
		if (!animqc::ReadRrigFile(rrigPath, modelData))
		{
			Log("ANIMQC: Failed to read rrig file\n");
			return false;
		}
	}

	// Read rseq files
	if (!animqc::ReadRseqFiles(rseqPaths, modelData))
	{
		Log("ANIMQC: Failed to read rseq files\n");
		return false;
	}

	return animqc::WriteQCFile(modelData, exportPath);
}
