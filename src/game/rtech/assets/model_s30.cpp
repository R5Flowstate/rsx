// S30/S31 mdl_ (pak version 20/21) support: rebuilds the studiohdr into the
// v17 layout the v16/v17 parsers read, and turns a stored vertex slice into
// the raw v17 vertex buffer. Ported from rmdlconv (CafeFPS, GPL v3).
//
// The S30 studiohdr is the v17 228-byte header with a 16-byte insert at file
// offset 180. Per-bone data is 16 bytes (pose lives in the 8-index linear
// arrays) and expands to the 128-byte v16 inline form; seqdesc 116->112,
// animdesc 48->40 with a synthesized reference anim block; bonestate/bonetable
// truncate u16->u8; linear repacks 8->6 arrays (dropped for single-bone
// models); skin fam-0 names are dropped; flags bit 0x100 is cleared.

#include <pch.h>
#include <game/rtech/assets/model_s30.h>
#include <thirdparty/oodle/oodle2.h>

namespace Oodle
{
	static bool Decompress(const uint8_t* src, size_t srcSize, uint8_t* dst, size_t dstSize)
	{
		return OodleLZ_Decompress(src, static_cast<OO_SINTa>(srcSize), dst, static_cast<OO_SINTa>(dstSize),
			OodleLZ_FuzzSafe_Yes, OodleLZ_CheckCRC_No, OodleLZ_Verbosity_None) == static_cast<OO_SINTa>(dstSize);
	}
}

namespace {

// S30 header: v17 228 B + 16 B insert at file offset 180.
constexpr size_t kS30Hdr = 244;
constexpr size_t kV17Hdr = 228;
constexpr size_t kInsAt = 180;
constexpr size_t kInsSize = 16;
constexpr size_t kSeqS30 = 116;
constexpr size_t kSeqV17 = 112;
constexpr size_t kAnimS30 = 48;
constexpr size_t kAnimV17 = 40;
constexpr size_t kBoneS30 = 16;
constexpr size_t kBoneV17 = 128;

// Header-relative u16 offset fields (v17 file offsets).
const int kHdrOff[] = { 8, 52, 118, 120, 124, 132, 136, 138, 142, 146, 152, 156,
	160, 164, 166, 168, 174, 198, 200, 202, 206, 208, 212, 214, 220, 222 };
// Field-relative u16 offset fields.
const int kHdrFieldOff[] = { 176, 180, 184 };
// Count fields copied verbatim.
const int kCount16[] = { 116, 122, 134, 140, 144, 148, 150, 154, 158, 162, 170,
	172, 178, 182, 186, 196, 204, 210, 218 };

// seqdesc sub-field offsets shared by both layouts.
constexpr size_t kSqLabel = 0;
constexpr size_t kSqActivity = 2;
constexpr size_t kSqEvents = 12;
constexpr size_t kSqEventIdx = 14;
constexpr size_t kSqAii = 42;
constexpr size_t kSqAutolayers = 78;
constexpr size_t kSqAutolayerIdx = 80;
constexpr size_t kSqWeightlist = 82;
constexpr size_t kSqIklocks = 88;
constexpr size_t kSqIklockIdx = 90;
constexpr size_t kSqActmodIdx = 96;
constexpr size_t kSqActmods = 98;
constexpr size_t kSqWeightFixup = 108;
constexpr size_t kSqWeightFixupCount = 110;

constexpr int kAnimExternalFlag = 0x200000;
constexpr int kStaleFlag = 0x100;
// S30 studiohdr flag: vertex bone index pairs are packed 10 bits each (see
// RepackVGBoneIndices19S30). Cleared on emit; S21 vertices are plain bytes.
constexpr int kPackedBoneIndexFlag = 0x4000;

// Engine u16 encoding: even = direct, odd = (o & ~1) >> 4 (x16 scale).
inline size_t DecOff(uint16_t o)
{
	return static_cast<size_t>(o & 0xFFFE) << (4 * (o & 1));
}

inline bool EncOff(size_t v, uint16_t& o)
{
	if (v < 0x10000 && (v & 1) == 0)
	{
		o = static_cast<uint16_t>(v);
		return true;
	}
	// Decode is (o & 0xFFFE) << 4, so the shifted form needs 32-byte alignment.
	if ((v & 0x1F) == 0 && (v >> 4) <= 0xFFFE)
	{
		o = static_cast<uint16_t>((v >> 4) | 1);
		return true;
	}
	return false;
}

inline size_t Align4(size_t n) { return (n + 3) & ~static_cast<size_t>(3); }
inline size_t Align16(size_t n) { return (n + 15) & ~static_cast<size_t>(15); }

inline float F32(float v) { return v; }

struct Quat
{
	float x, y, z, w;
};

// Reference anim block: bone-flag nibbles + one Quaternion64 (byte-exact vs
// authored S21 models).
inline size_t RefFlagSize(int nbones)
{
	return static_cast<size_t>((((4 * nbones + 7) / 8 + 1) & ~1));
}

inline void WriteRefBlock(char* pOut, int nbones, const Quat& bone0)
{
	const size_t fs = RefFlagSize(nbones);
	memset(pOut, 0, fs + 10);
	if (nbones <= 0)
		return;
	pOut[0] = 2;
	const float bx = 0.5f, by = 0.5f, bz = 0.5f, bw = 0.5f;
	const float rx = bw * bone0.x + bx * bone0.w + by * bone0.z - bz * bone0.y;
	const float ry = bw * bone0.y + by * bone0.w + bz * bone0.x - bx * bone0.z;
	const float rz = bw * bone0.z + bz * bone0.w + bx * bone0.y - by * bone0.x;
	const float rw = bw * bone0.w - bx * bone0.x - by * bone0.y - bz * bone0.z;
	auto enc = [](float v) -> int
	{
		int e = static_cast<int>(v * 1048576.0f) + 1048576;
		return e < 0 ? 0 : (e > 2097151 ? 2097151 : e);
	};
	const uint64_t u = static_cast<uint64_t>(enc(rx))
		| (static_cast<uint64_t>(enc(ry)) << 21)
		| (static_cast<uint64_t>(enc(rz)) << 42)
		| ((rw < 0 ? 1ull : 0ull) << 63);
	*reinterpret_cast<uint16_t*>(pOut + fs) = 10;
	memcpy(pOut + fs + 2, &u, sizeof(u));
}

struct Piece
{
	size_t s = 0;
	size_t e = 0;
	bool hasEnd = false;
	std::string kind;
	int a = 0; // payload (seq idx / nan / cell)
	int b = 0;
};

struct StructRef
{
	size_t ns = 0;   // new struct pos
	size_t fo = 0;   // field offset within struct
	int sz = 0;      // 2 or 4
	int base = 0;    // 0 = header-relative, 1 = struct-relative
	size_t basePos = 0;
	size_t tgt = 0;  // old target
};

struct AnimKey
{
	int seq = 0;
	int cell = 0;
	bool operator<(const AnimKey& o) const
	{
		return seq < o.seq || (seq == o.seq && cell < o.cell);
	}
};

class S30ToV17
{
public:
	S30ToV17(const char* pIn, size_t size) : m_in(pIn), m_size(size) {}

	bool Run(std::vector<char>& out, std::vector<std::string>& warnings, std::string& err)
	{
		try
		{
			Parse();
			Convert(out);
		}
		catch (const std::exception& e)
		{
			err = e.what();
			return false;
		}
		warnings = m_warn;
		return true;
	}

private:
	const char* m_in = nullptr;
	size_t m_size = 0;
	std::vector<std::string> m_warn;
	std::vector<Piece> m_pieces;

	struct SeqInfo
	{
		size_t base = 0;
		size_t ai = 0;
		int nan = 0;
		std::vector<size_t> anims;
	};
	std::vector<SeqInfo> m_seqs;
	int m_nbone = 0;

	std::map<size_t, long long> m_new; // old start -> new start (-1 = dropped)
	std::map<size_t, long long> m_gap; // old gap start -> new start
	std::map<AnimKey, size_t> m_animNew;

	// ---- checked reads ----

	void Need(size_t off, size_t n) const
	{
		if (n > m_size || off > m_size - n)
		{
			char msg[128];
			sprintf_s(msg, "S30 model truncated off=%zu n=%zu size=%zu", off, n, m_size);
			throw std::runtime_error(msg);
		}
	}

	uint16_t RU16(size_t off) const
	{
		Need(off, 2);
		uint16_t v;
		memcpy(&v, m_in + off, 2);
		return v;
	}

	int32_t RI32(size_t off) const
	{
		Need(off, 4);
		int32_t v;
		memcpy(&v, m_in + off, 4);
		return v;
	}

	uint8_t RU8(size_t off) const
	{
		Need(off, 1);
		return static_cast<uint8_t>(m_in[off]);
	}

	static size_t SOff(int voff) { return static_cast<size_t>(voff) + (voff >= 180 ? kInsSize : 0); }
	uint16_t HRaw(int voff) const { return RU16(SOff(static_cast<size_t>(voff))); }
	size_t HDec(int voff) const { return DecOff(HRaw(voff)); }

	// Field-relative; the table can sit past alignment padding after the group headers.
	size_t BoneStateStart() const
	{
		if (HRaw(176))
			return 176 + HDec(176);
		return 196 + HDec(180) + static_cast<size_t>(HRaw(182)) * 16;
	}

	void Warn(const std::string& w) { m_warn.push_back(w); }

	// ---- parse ----

	size_t SpanAt(size_t s, const std::string& kind, int pay)
	{
		UNUSED(pay);
		const int nb = m_nbone;
		if (kind == "bonehdr") return s + static_cast<size_t>(nb) * 12;
		if (kind == "bonedata") return s + static_cast<size_t>(nb) * kBoneS30;
		if (kind == "attachments") return s + static_cast<size_t>(RU8(SOff(131))) * 56;
		if (kind == "hitboxset") return s + 6;
		if (kind == "bonetable") return s + static_cast<size_t>(nb) * 2;
		if (kind == "seqdesc") return s + kSeqS30;
		if (kind == "textures") return s + static_cast<size_t>(HRaw(144)) * 8;
		if (kind == "skins") return s + static_cast<size_t>(HRaw(148)) * HRaw(150) * 2;
		if (kind == "skinnames") return s + static_cast<size_t>(HRaw(150) - 1) * 2;
		if (kind == "skinfam0") return s + 2;
		if (kind == "bodyparts") return s + static_cast<size_t>(HRaw(154)) * 16;
		if (kind == "poseparams") return s + static_cast<size_t>(HRaw(162)) * 16;
		if (kind == "ikchains") return s + static_cast<size_t>(HRaw(140)) * 12;
		if (kind == "nodenames" || kind == "nodedata") return s + static_cast<size_t>(HRaw(134)) * 2;
		if (kind == "linear")
		{
			uint16_t idx[8];
			for (int i = 0; i < 8; i++)
				idx[i] = RU16(s + 2 + static_cast<size_t>(2 * i));
			const size_t sz[8] = { 4ull * nb, 2ull * nb, 12ull * nb, 16ull * nb,
				12ull * nb, 48ull * nb, 16ull * nb, 12ull * nb };
			size_t n = 20;
			for (int i = 0; i < 8; i++)
				n = std::max(n, static_cast<size_t>(idx[i]) + sz[i]);
			return s + n;
		}
		if (kind == "lods") return s + static_cast<size_t>(HRaw(186)) * 4;
		if (kind == "groups") return s + static_cast<size_t>(HRaw(182)) * 16;
		if (kind == "bonestate") return s + static_cast<size_t>(HRaw(178)) * 2;
		if (kind == "followers") return s + static_cast<size_t>(HRaw(210)) * 2;
		if (kind == "srcbones") return s + static_cast<size_t>(HRaw(196)) * 100; // name index + pre/post matrix3x4
		if (kind == "procmap") return s + static_cast<size_t>(nb);
		if (kind == "uipanels") return s + static_cast<size_t>(HRaw(158)) * 16;
		if (kind == "aii")
		{
			for (const SeqInfo& sq : m_seqs)
				if (static_cast<int>(sq.ai) == static_cast<int>(s))
					return s + static_cast<size_t>(sq.nan) * 2;
			throw std::runtime_error("aii without seq");
		}
		if (kind == "animdesc") return s + kAnimS30;
		throw std::runtime_error("span for " + kind);
	}

	void Parse()
	{
		m_nbone = HRaw(116);
		const int nb = m_nbone;
		std::vector<Piece> raw;
		auto add = [&](size_t s, const std::string& kind, int pay = 0)
		{
			Piece p;
			p.s = s;
			p.kind = kind;
			p.a = pay;
			raw.push_back(p);
		};
		if (nb)
		{
			add(HDec(118), "bonehdr");
			add(HDec(120), "bonedata");
		}
		if (RU8(SOff(131)))
			add(HDec(132), "attachments");
		if (RU8(SOff(54)))
		{
			const size_t hs = HDec(52);
			for (int i = 0; i < RU8(SOff(54)); i++)
			{
				Piece p;
				p.s = hs + static_cast<size_t>(i) * 6;
				p.kind = "hitboxset";
				p.a = i;
				raw.push_back(p);
			}
		}
		if (nb)
			add(HDec(174), "bonetable");
		if (HRaw(122))
		{
			const size_t sq = HDec(124);
			for (int i = 0; i < HRaw(122); i++)
			{
				const size_t base = sq + static_cast<size_t>(i) * kSeqS30;
				const uint16_t ai = RU16(base + kSqAii);
				const int gs0 = RU8(base + 84), gs1 = RU8(base + 85);
				const int nan = (gs0 * gs1 > 0) ? gs0 * gs1 : 1;
				SeqInfo info;
				info.base = base;
				info.ai = base + DecOff(ai);
				info.nan = nan;
				for (int j = 0; j < nan; j++)
				{
					const uint16_t rel = RU16(base + DecOff(ai) + static_cast<size_t>(2 * j));
					info.anims.push_back(base + rel);
				}
				m_seqs.push_back(info);
				add(base, "seqdesc", i);
				add(base + DecOff(ai), "aii", i);
				for (int j = 0; j < nan; j++)
				{
					Piece p;
					p.s = info.anims[static_cast<size_t>(j)];
					p.kind = "animdesc";
					p.a = i;
					p.b = j;
					raw.push_back(p);
				}
			}
		}
		if (HRaw(144))
			add(HDec(146), "textures");
		if (HRaw(148) && HRaw(150))
		{
			add(HDec(152), "skins");
			if (HRaw(150) > 1)
				add(HDec(152) + static_cast<size_t>(HRaw(148)) * HRaw(150) * 2, "skinnames");
			else if (HRaw(148) == 1)
			{
				const size_t so = HDec(152) + 2;
				if (RU16(so) != 0)
					add(so, "skinfam0");
			}
		}
		if (HRaw(154))
			add(HDec(156), "bodyparts");
		if (HRaw(162))
			add(HDec(164), "poseparams");
		if (HRaw(140))
			add(HDec(142), "ikchains");
		if (HRaw(134))
		{
			add(HDec(136), "nodenames");
			add(HDec(138), "nodedata");
		}
		if (HDec(202))
			add(HDec(202), "linear");
		if (HRaw(186))
			add(200 + HDec(184), "lods");
		if (HRaw(182))
			add(196 + HDec(180), "groups");
		if (HRaw(178))
			add(BoneStateStart(), "bonestate");
		if (HRaw(210))
			add(HDec(212), "followers");
		if (HRaw(196))
			add(HDec(198), "srcbones");
		if (HRaw(204))
		{
			Piece p;
			p.s = HDec(206);
			p.kind = "procbones"; // span resolved to next piece below
			raw.push_back(p);
			add(HDec(208), "procmap");
		}
		if (HRaw(158))
			add(HDec(160), "uipanels");
		if (HDec(214))
		{
			Piece p;
			p.s = HDec(214);
			p.kind = "bvh"; // runs to next piece/EOF
			raw.push_back(p);
		}

		for (Piece& p : raw)
		{
			if (p.kind == "procbones" || p.kind == "bvh")
				continue;
			p.e = SpanAt(p.s, p.kind, p.a);
			p.hasEnd = true;
		}

		// hitbox arrays + ik links + bodypart chain sub-pieces.
		std::vector<Piece> extra;
		for (const Piece& p : raw)
		{
			if (!p.hasEnd)
				continue;
			if (p.kind == "hitboxset")
			{
				const int nbx = RU16(p.s + 2);
				const size_t hbx = DecOff(RU16(p.s + 4));
				if (nbx)
					extra.push_back({ p.s + hbx, p.s + hbx + static_cast<size_t>(nbx) * 32, true, "hitboxes", 0, 0 });
			}
			else if (p.kind == "ikchains")
			{
				for (int i = 0; i < HRaw(140); i++)
				{
					const int nl = RU16(p.s + static_cast<size_t>(i) * 12 + 4);
					const size_t li = DecOff(RU16(p.s + static_cast<size_t>(i) * 12 + 6));
					if (nl)
						extra.push_back({ p.s + li, p.s + li + static_cast<size_t>(nl) * 16, true, "iklinks", 0, 0 });
				}
			}
			else if (p.kind == "bodyparts")
			{
				size_t pos = p.s;
				for (int i = 0; i < HRaw(154); i++)
				{
					Need(pos, 12);
					uint16_t sz, mi;
					int32_t base, nm, mo;
					memcpy(&sz, m_in + pos, 2);
					memcpy(&mi, m_in + pos + 2, 2);
					memcpy(&base, m_in + pos + 4, 4);
					memcpy(&nm, m_in + pos + 8, 4);
					memcpy(&mo, m_in + pos + 12, 4);
					(void)sz; (void)base; (void)mo;
					const size_t m = pos + DecOff(mi);
					for (int j = 0; j < nm; j++)
					{
						const size_t mj = m + static_cast<size_t>(j) * 10;
						extra.push_back({ mj, mj + 10, true, "model", 0, 0 });
						Need(mj, 10);
						uint16_t unks, mt, mb, mbl, moff;
						memcpy(&unks, m_in + mj, 2);
						memcpy(&mt, m_in + mj + 2, 2);
						memcpy(&mb, m_in + mj + 4, 2);
						memcpy(&mbl, m_in + mj + 6, 2);
						memcpy(&moff, m_in + mj + 8, 2);
						(void)unks; (void)mb; (void)mbl;
						const size_t ms = mj + DecOff(moff);
						extra.push_back({ ms, ms + static_cast<size_t>(mt) * 20, true, "meshes", 0, 0 });
					}
					pos += 16;
				}
			}
		}
		for (const Piece& p : extra)
			raw.push_back(p);

		// Dedupe identical spans.
		std::vector<Piece> uniq;
		for (const Piece& p : raw)
		{
			bool seen = false;
			for (const Piece& q : uniq)
			{
				if (q.s == p.s && q.hasEnd == p.hasEnd && (!p.hasEnd || q.e == p.e) && q.kind == p.kind)
				{
					seen = true;
					break;
				}
			}
			if (!seen)
				uniq.push_back(p);
		}
		raw.swap(uniq);

		// Skin fam-0 orphan string: S21 stores no fam-0 name, drop it.
		std::vector<Piece> drops;
		for (const Piece& p : raw)
		{
			if (p.kind != "skinfam0" || !p.hasEnd)
				continue;
			const uint16_t v = RU16(p.s);
			if (v >= m_size)
			{
				Warn("skinfam0 bad target");
				continue;
			}
			size_t nul = v;
			while (nul < m_size && m_in[nul] != 0)
				nul++;
			if (nul >= m_size || nul == v || nul - v >= 128)
			{
				Warn("skinfam0 bad target");
				continue;
			}
			bool shared = false;
			for (const Piece& q : raw)
			{
				if (!q.hasEnd)
					continue;
				if (q.s < nul + 1 && v < q.e)
				{
					shared = true;
					break;
				}
			}
			if (shared)
			{
				Warn("skinfam0 string shared, kept");
			}
			else
			{
				char name[128] = {};
				memcpy(name, m_in + v, std::min(nul - v, sizeof(name) - 1));
				Warn(std::string("drop skin fam-0 name ") + name);
				drops.push_back({ v, nul + 1, true, "dropstr", 0, 0 });
			}
		}
		for (const Piece& p : drops)
			raw.push_back(p);

		std::sort(raw.begin(), raw.end(), [](const Piece& x, const Piece& y)
		{
			if (x.s != y.s) return x.s < y.s;
			size_t xe = x.hasEnd ? x.e : SIZE_MAX;
			size_t ye = y.hasEnd ? y.e : SIZE_MAX;
			if (xe != ye) return xe < ye;
			return x.kind < y.kind;
		});

		// Open spans (procbones/bvh) run to the next piece or EOF.
		for (size_t i = 0; i < raw.size(); i++)
		{
			if (raw[i].hasEnd)
				continue;
			raw[i].e = (i + 1 < raw.size()) ? raw[i + 1].s : m_size;
			raw[i].hasEnd = true;
		}
		m_pieces.swap(raw);
	}

	// ---- emit helpers ----

	static void WrU16(std::vector<char>& out, size_t off, uint16_t v)
	{
		memcpy(&out[off], &v, 2);
	}

	static void WrI32(std::vector<char>& out, size_t off, int32_t v)
	{
		memcpy(&out[off], &v, 4);
	}

	static uint16_t OutU16(const std::vector<char>& out, size_t off)
	{
		uint16_t v;
		memcpy(&v, &out[off], 2);
		return v;
	}

	void PadTo(std::vector<char>& out, size_t align)
	{
		while (out.size() % align)
			out.push_back(0);
	}

	// Keeps a piece that a u16 header offset (relative to `base`) points at encodable past 64 KiB.
	void PadForHdrRef(std::vector<char>& out, size_t base)
	{
		if (out.size() < base || out.size() - base < 0x10000)
			return;
		while ((out.size() - base) & 0x1F)
			out.push_back(0);
	}

	long long TgtNew(size_t t) const
	{
		for (const Piece& p : m_pieces)
		{
			if (p.s <= t && t < p.e)
			{
				auto it = m_new.find(p.s);
				if (it != m_new.end() && it->second >= 0)
					return it->second + static_cast<long long>(t - p.s);
			}
		}
		size_t pos = kS30Hdr;
		for (const Piece& p : m_pieces)
		{
			if (pos <= t && t < p.s)
			{
				auto it = m_gap.find(pos);
				if (it != m_gap.end())
					return it->second + static_cast<long long>(t - pos);
			}
			if (p.e > pos)
				pos = p.e;
		}
		auto it = m_gap.find(pos);
		if (it != m_gap.end() && t >= pos)
			return it->second + static_cast<long long>(t - pos);
		return -1;
	}

	// Count backing a header offset field, or -1 when the field has none.
	// A count-0 offset is never dereferenced, so its stale value carries
	// without a warning (S30 ships these on every model).
	int FieldCount(int voff) const
	{
		switch (voff)
		{
		case 198: return HRaw(196);
		case 160: return HRaw(158);
		case 142: return HRaw(140);
		case 136:
		case 138: return HRaw(134);
		case 164: return HRaw(162);
		case 212: return HRaw(210);
		case 206:
		case 208: return HRaw(204);
		case 132: return RU8(131);
		case 156: return HRaw(154);
		case 152: return (HRaw(148) && HRaw(150)) ? 1 : 0;
		case 146: return HRaw(144);
		case 124: return HRaw(122);
		case 52: return RU8(SOff(54));
		case 118:
		case 120:
		case 174: return HRaw(116);
		case 220:
		case 222: return HRaw(218);
		default: return -1;
		}
	}

	void BoneRec(std::vector<char>& out, int i, bool hasLin, const uint16_t linIdx[8], size_t lb,
		std::vector<StructRef>& structRefs)
	{
		char rec[kBoneV17] = {};
		if (hasLin)
		{
			Need(lb + linIdx[5] + static_cast<size_t>(i) * 48, 48);
			Need(lb + linIdx[6] + static_cast<size_t>(i) * 16, 16);
			Need(lb + linIdx[2] + static_cast<size_t>(i) * 12, 12);
			Need(lb + linIdx[3] + static_cast<size_t>(i) * 16, 16);
			Need(lb + linIdx[4] + static_cast<size_t>(i) * 12, 12);
			Need(lb + linIdx[7] + static_cast<size_t>(i) * 12, 12);
			memcpy(rec + 0, m_in + lb + linIdx[5] + static_cast<size_t>(i) * 48, 48);
			memcpy(rec + 48, m_in + lb + linIdx[6] + static_cast<size_t>(i) * 16, 16);
			memcpy(rec + 64, m_in + lb + linIdx[2] + static_cast<size_t>(i) * 12, 12);
			memcpy(rec + 76, m_in + lb + linIdx[3] + static_cast<size_t>(i) * 16, 16);
			memcpy(rec + 92, m_in + lb + linIdx[4] + static_cast<size_t>(i) * 12, 12);
			memcpy(rec + 104, m_in + lb + linIdx[7] + static_cast<size_t>(i) * 12, 12);
		}
		else
		{
			const float one = 1.0f;
			memcpy(rec + 0, &one, 4);
			memcpy(rec + 20, &one, 4);
			memcpy(rec + 88, &one, 4);
			memcpy(rec + 104, &one, 4);
			memcpy(rec + 108, &one, 4);
			memcpy(rec + 112, &one, 4);
		}
		const size_t bs = HDec(120) + static_cast<size_t>(i) * kBoneS30;
		Need(bs, 12);
		int16_t parent;
		uint16_t unk76, procindex;
		int32_t flags;
		memcpy(&parent, m_in + bs, 2);
		memcpy(&unk76, m_in + bs + 2, 2);
		memcpy(&flags, m_in + bs + 4, 4);
		const uint8_t ci = static_cast<uint8_t>(m_in[bs + 8]);
		const uint8_t pt = static_cast<uint8_t>(m_in[bs + 9]);
		memcpy(&procindex, m_in + bs + 10, 2);
		memcpy(rec + 116, &parent, 2);
		memcpy(rec + 118, &unk76, 2);
		memcpy(rec + 120, &flags, 4);
		rec[124] = static_cast<char>(ci);
		rec[125] = static_cast<char>(pt);
		memcpy(rec + 126, &procindex, 2);
		const size_t recPos = out.size();
		out.insert(out.end(), rec, rec + kBoneV17);
		// procindex is relative to the 16-byte S30 bone record; v17 wants it relative to the 128-byte one.
		if (pt && procindex)
			structRefs.push_back({ recPos, 126, 2, 1, recPos, bs + DecOff(procindex) });
	}

	// Rebuild the seq region grouped (all descs, then per-seq blocks).
	// Returns the old end consumed.
	size_t EmitSeqRegion(std::vector<char>& out, std::vector<StructRef>& structRefs)
	{
		const int nb = m_nbone;
		std::vector<size_t> newbases;
		for (const SeqInfo& sq : m_seqs)
		{
			Need(sq.base, kSeqV17);
			const size_t nb2 = out.size();
			m_new[sq.base] = static_cast<long long>(nb2);
			newbases.push_back(nb2);
			out.insert(out.end(), m_in + sq.base, m_in + sq.base + kSeqV17);
		}
		for (size_t si = 0; si < m_seqs.size(); si++)
		{
			const SeqInfo& sq = m_seqs[si];
			const size_t newbase = newbases[si];
			PadTo(out, 4);
			const size_t wpos = out.size();
			const int nw = nb ? nb : 1;
			for (int w = 0; w < nw; w++)
			{
				const float one = 1.0f;
				out.insert(out.end(), reinterpret_cast<const char*>(&one),
					reinterpret_cast<const char*>(&one) + 4);
			}
			PadTo(out, 4);
			const size_t apos = out.size();
			std::vector<uint16_t> aii(static_cast<size_t>(sq.nan), 0);
			out.insert(out.end(), reinterpret_cast<const char*>(aii.data()),
				reinterpret_cast<const char*>(aii.data()) + aii.size() * 2);
			PadTo(out, 4);
			for (int j = 0; j < sq.nan; j++)
			{
				const size_t ab = sq.anims[static_cast<size_t>(j)];
				Need(ab, kAnimS30);
				const size_t anew = out.size();
				m_animNew[{static_cast<int>(si), j}] = anew;
				char a[kAnimV17] = {};
				memcpy(a, m_in + ab, 12);
				int32_t flags;
				memcpy(&flags, m_in + ab + 4, 4);
				flags &= ~kAnimExternalFlag;
				memcpy(a + 4, &flags, 4);
				uint16_t fm;
				memcpy(&fm, m_in + ab + 14, 2);
				memcpy(a + 14, &fm, 2);
				uint16_t nrule, irule;
				memcpy(&nrule, m_in + ab + 16, 2);
				memcpy(&irule, m_in + ab + 22, 2);
				if (nrule || irule)
					Warn("anim ikrules seq" + std::to_string(si) + " cell" + std::to_string(j));
				uint16_t namerel;
				memcpy(&namerel, m_in + ab + 12, 2);
				const size_t t = ab + DecOff(namerel);
				structRefs.push_back({ anew, 12, 2, 1, anew, t });
				out.insert(out.end(), a, a + kAnimV17);
				aii[static_cast<size_t>(j)] = static_cast<uint16_t>(anew - newbase);
			}
			memcpy(&out[apos], aii.data(), aii.size() * 2);
			auto wrel = [&](size_t v) -> uint16_t { return static_cast<uint16_t>(v - newbase); };
			WrU16(out, newbase + kSqWeightlist, wrel(wpos));
			WrU16(out, newbase + kSqEventIdx, wrel(wpos));
			WrU16(out, newbase + kSqAutolayerIdx, wrel(wpos));
			WrU16(out, newbase + kSqIklockIdx, wrel(apos));
			WrU16(out, newbase + kSqActmodIdx, wrel(apos));
			WrU16(out, newbase + kSqAii, wrel(apos));
			PadTo(out, 4);
			WrU16(out, newbase + kSqWeightFixup, wrel(out.size()));
			for (size_t fo : {kSqLabel, kSqActivity})
			{
				const uint16_t oldRel = RU16(sq.base + fo);
				if (oldRel == 0)
					continue;
				structRefs.push_back({ newbase, fo, 2, 1, newbase, sq.base + DecOff(oldRel) });
			}
			const uint16_t ev = RU16(sq.base + kSqEvents);
			const uint16_t al = RU16(sq.base + kSqAutolayers);
			const uint16_t il = RU16(sq.base + kSqIklocks);
			const uint16_t am = RU16(sq.base + kSqActmods);
			const uint16_t wf = RU16(sq.base + kSqWeightFixupCount);
			if (ev) Warn("seq" + std::to_string(si) + " events count>0 unhandled");
			if (al) Warn("seq" + std::to_string(si) + " autolayers count>0 unhandled");
			if (il) Warn("seq" + std::to_string(si) + " iklocks count>0 unhandled");
			if (am) Warn("seq" + std::to_string(si) + " actmods count>0 unhandled");
			if (wf) Warn("seq" + std::to_string(si) + " weightfixup count>0 unhandled");
		}
		size_t end = 0;
		for (const SeqInfo& sq : m_seqs)
		{
			if (sq.ai + static_cast<size_t>(sq.nan) * 2 > end)
				end = sq.ai + static_cast<size_t>(sq.nan) * 2;
			for (size_t ab : sq.anims)
			{
				if (ab + kAnimS30 > end)
					end = ab + kAnimS30;
			}
		}
		return end;
	}

	void RegRefs(const std::string& kind, size_t s, size_t e, size_t ns,
		std::vector<StructRef>& structRefs)
	{
		const size_t n = e - s;
		if (kind == "bonehdr")
		{
			const size_t surf = HDec(166);
			for (size_t i = 0; i < n / 12; i++)
			{
				const size_t t = s + i * 12 + DecOff(RU16(s + i * 12 + 10));
				structRefs.push_back({ ns + i * 12, 10, 2, 1, ns + i * 12, t });
				const size_t t6 = s + i * 12 + DecOff(RU16(s + i * 12 + 6));
				if (t6 == surf)
					structRefs.push_back({ ns + i * 12, 6, 2, 1, ns + i * 12, t6 });
			}
		}
		else if (kind == "attachments")
		{
			for (size_t i = 0; i < n / 56; i++)
			{
				const size_t t = s + i * 56 + DecOff(RU16(s + i * 56));
				structRefs.push_back({ ns + i * 56, 0, 2, 1, ns + i * 56, t });
			}
		}
		else if (kind == "hitboxset")
		{
			structRefs.push_back({ ns, 0, 2, 1, ns, s + DecOff(RU16(s)) });
			const int nbx = RU16(s + 2);
			if (nbx)
				structRefs.push_back({ ns, 4, 2, 1, ns, s + DecOff(RU16(s + 4)) });
		}
		else if (kind == "hitboxes")
		{
			for (size_t j = 0; j < n / 32; j++)
			{
				const size_t st = s + j * 32 + DecOff(RU16(s + j * 32 + 28));
				structRefs.push_back({ ns + j * 32, 28, 2, 1, ns + j * 32, st });
			}
		}
		else if (kind == "bodyparts")
		{
			for (size_t i = 0; i < n / 16; i++)
			{
				structRefs.push_back({ ns + i * 16, 0, 2, 1, ns + i * 16,
					s + i * 16 + DecOff(RU16(s + i * 16)) });
				const size_t mi = DecOff(RU16(s + i * 16 + 2));
				structRefs.push_back({ ns + i * 16, 2, 2, 1, ns + i * 16, s + i * 16 + mi });
			}
		}
		else if (kind == "model")
		{
			structRefs.push_back({ ns, 0, 2, 1, ns, s + DecOff(RU16(s)) });
			structRefs.push_back({ ns, 8, 2, 1, ns, s + DecOff(RU16(s + 8)) });
		}
		else if (kind == "poseparams")
		{
			for (size_t i = 0; i < n / 16; i++)
			{
				const size_t t = s + i * 16 + DecOff(RU16(s + i * 16));
				structRefs.push_back({ ns + i * 16, 0, 2, 1, ns + i * 16, t });
			}
		}
		else if (kind == "ikchains")
		{
			for (size_t i = 0; i < n / 12; i++)
			{
				const size_t t = s + i * 12 + DecOff(RU16(s + i * 12));
				structRefs.push_back({ ns + i * 12, 0, 2, 1, ns + i * 12, t });
				const int nl = RU16(s + i * 12 + 4);
				if (nl)
					structRefs.push_back({ ns + i * 12, 6, 2, 1, ns + i * 12,
						s + i * 12 + DecOff(RU16(s + i * 12 + 6)) });
			}
		}
		else if (kind == "skinnames")
		{
			for (size_t i = 0; i < n / 2; i++)
				structRefs.push_back({ ns, 2 * i, 2, 0, 0, RU16(s + 2 * i) });
		}
		else if (kind == "uipanels")
		{
			for (size_t i = 0; i < n / 16; i++)
			{
				for (size_t fo : {4, 12})
				{
					Need(s + i * 16 + fo, 4);
					uint32_t v;
					memcpy(&v, m_in + s + i * 16 + fo, 4);
					structRefs.push_back({ ns + i * 16, fo, 4, 1, ns + i * 16,
						s + i * 16 + v });
				}
			}
		}
	}

	void PatchHeader(std::vector<char>& out)
	{
		for (int voff : kHdrOff)
		{
			const size_t soff = SOff(static_cast<size_t>(voff));
			Need(soff, 2);
			uint16_t v;
			memcpy(&v, m_in + soff, 2);
			if (v == 0)
			{
				WrU16(out, static_cast<size_t>(voff), 0);
				continue;
			}
			if (voff == 202 && m_nbone <= 1)
			{
				WrU16(out, static_cast<size_t>(voff), 0);
				continue;
			}
			const size_t tgt = DecOff(v);
			const long long nt = TgtNew(tgt);
			if (nt < 0 || static_cast<size_t>(nt) > out.size())
			{
				if (FieldCount(voff) != 0)
					Warn("hdr field@" + std::to_string(voff) + " target unmapped");
				continue;
			}
			uint16_t enc = 0;
			if (!EncOff(static_cast<size_t>(nt), enc) && !RelocateForRef(out, tgt, 0, enc))
			{
				Warn("hdr field@" + std::to_string(voff) + " target unencodable, raw kept");
				WrU16(out, static_cast<size_t>(voff), v);
				continue;
			}
			WrU16(out, static_cast<size_t>(voff), enc);
		}
		for (int voff : kHdrFieldOff)
		{
			const size_t soff = SOff(static_cast<size_t>(voff));
			Need(soff, 2);
			uint16_t v;
			memcpy(&v, m_in + soff, 2);
			if (v == 0 && voff != 176)
			{
				WrU16(out, static_cast<size_t>(voff), 0);
				continue;
			}
			if (voff == 176)
			{
				const size_t go = BoneStateStart();
				auto it = m_new.find(go);
				if (it != m_new.end() && it->second >= 0)
				{
					uint16_t enc = 0;
					if (EncOff(static_cast<size_t>(it->second) - 176, enc))
						WrU16(out, 176, enc);
					else
					{
						Warn("bonestate offset unencodable");
						WrU16(out, 176, 0);
					}
				}
				else
				{
					WrU16(out, 176, 0);
				}
				continue;
			}
			const size_t tgt = soff + DecOff(v);
			const long long nt = TgtNew(tgt);
			if (nt < 0)
			{
				Warn("hdr fieldfield@" + std::to_string(voff) + " unmapped");
				continue;
			}
			uint16_t enc = 0;
			if (!EncOff(static_cast<size_t>(nt) - static_cast<size_t>(voff), enc))
			{
				Warn("hdr fieldfield@" + std::to_string(voff) + " unencodable, raw kept");
				WrU16(out, static_cast<size_t>(voff), v);
				continue;
			}
			WrU16(out, static_cast<size_t>(voff), enc);
		}
		for (int voff : kCount16)
		{
			const size_t soff = SOff(static_cast<size_t>(voff));
			Need(soff, 2);
			memcpy(&out[static_cast<size_t>(voff)], m_in + soff, 2);
		}
		int32_t flags;
		memcpy(&flags, &out[0], 4);
		flags &= ~(kStaleFlag | kPackedBoneIndexFlag);
		memcpy(&out[0], &flags, 4);
	}

	// A u16 offset reaches past 64K only as a 32-byte aligned scaled value, so a
	// far string the layout left unaligned gets a copy at an aligned tail slot.
	// Strings are the only targets emitted verbatim from the source.
	bool RelocateForRef(std::vector<char>& out, size_t tgt, size_t base, uint16_t& enc)
	{
		size_t n = 0;
		while (tgt + n < m_size && m_in[tgt + n] != 0 && n < 0x10000)
		{
			const unsigned char c = static_cast<unsigned char>(m_in[tgt + n]);
			if ((c < 32 && c != '\t' && c != '\n' && c != '\r') || c >= 127)
				return false;
			n++;
		}
		if (tgt + n >= m_size || m_in[tgt + n] != 0 || out.size() < base)
			return false;
		while ((out.size() - base) & 0x1F)
			out.push_back(0);
		const size_t at = out.size();
		out.insert(out.end(), m_in + tgt, m_in + tgt + n + 1);
		return EncOff(at - base, enc);
	}

	void PatchStructs(std::vector<char>& out, const std::vector<StructRef>& refs)
	{
		for (const StructRef& r : refs)
		{
			const long long nt = TgtNew(r.tgt);
			if (nt < 0)
			{
				Warn("struct ref target unmapped");
				continue;
			}
			if (r.base == 0)
			{
				uint16_t enc = 0;
				if (!EncOff(static_cast<size_t>(nt), enc) && !RelocateForRef(out, r.tgt, 0, enc))
				{
					Warn("struct hdr-ref unencodable tgt=" + std::to_string(r.tgt) + " nt=" + std::to_string(nt));
					continue;
				}
				WrU16(out, r.ns + r.fo, enc);
			}
			else
			{
				const long long v = nt - static_cast<long long>(r.basePos);
				if (v < 0)
				{
					Warn("struct ref target bad, kept");
					continue;
				}
				if (r.sz == 2)
				{
					uint16_t enc = 0;
					if (!EncOff(static_cast<size_t>(v), enc) && !RelocateForRef(out, r.tgt, r.basePos, enc))
					{
						Warn("struct ref target bad, kept tgt=" + std::to_string(r.tgt) + " v=" + std::to_string(v));
						continue;
					}
					WrU16(out, r.ns + r.fo, enc);
				}
				else
				{
					WrI32(out, r.ns + r.fo, static_cast<int32_t>(v));
				}
			}
		}
	}

	void Convert(std::vector<char>& out)
	{
		const int nb = m_nbone;
		Need(kS30Hdr, 1);

		std::vector<StructRef> structRefs;

		// Header: v17 shape, 16 B insert dropped. Flags cleared at the end.
		out.resize(kV17Hdr);
		memcpy(&out[0], m_in, kInsAt);
		memcpy(&out[kInsAt], m_in + kInsAt + kInsSize, kV17Hdr - kInsAt);

		// Linear pose source.
		const size_t lb = HDec(202);
		bool hasLin = false;
		uint16_t linIdx[8] = {};
		if (lb)
		{
			Need(lb, 18);
			for (int i = 0; i < 8; i++)
				memcpy(&linIdx[i], m_in + lb + 2 + static_cast<size_t>(2 * i), 2);
			hasLin = true;
		}
		Quat bone0{ 0.0f, 0.0f, 0.0f, 1.0f };
		if (hasLin && nb)
		{
			Need(lb + linIdx[3], 16);
			memcpy(&bone0, m_in + lb + linIdx[3], 16);
		}

		size_t pos = kS30Hdr;
		bool seqsRebuilt = false;

		for (const Piece& p : m_pieces)
		{
			const size_t s = p.s, e = p.e;
			const std::string& k = p.kind;
			if (s < pos && e <= pos)
				continue;
			if (k == "seqdesc" && !seqsRebuilt)
			{
				if (s > pos)
				{
					Need(pos, s - pos);
					bool any = false;
					for (size_t i = pos; i < s; i++)
					{
						if (m_in[i] != 0)
						{
							any = true;
							break;
						}
					}
					if (any)
					{
						Warn("seq pre-gap has data");
						m_gap[pos] = static_cast<long long>(out.size());
						out.insert(out.end(), m_in + pos, m_in + s);
					}
				}
				PadTo(out, 4);
				EmitSeqRegion(out, structRefs);
				seqsRebuilt = true;
				if (e > pos)
					pos = e;
				continue;
			}
			if ((k == "seqdesc" || k == "aii" || k == "animdesc") && seqsRebuilt)
			{
				if (e > pos)
					pos = e;
				continue;
			}
			if (k == "lods" || k == "bvh")
			{
				if (s > pos)
				{
					Need(pos, s - pos);
					bool any = false;
					for (size_t i = pos; i < s; i++)
					{
						if (m_in[i] != 0)
						{
							any = true;
							break;
						}
					}
					if (any)
					{
						Warn((k == "lods" ? "lods" : "bvh") + std::string(" pre-gap has data"));
						m_gap[pos] = static_cast<long long>(out.size());
						out.insert(out.end(), m_in + pos, m_in + s);
					}
					pos = s;
				}
				PadTo(out, 16);
				if (k == "bvh")
				{
					Need(s, e - s);
					PadForHdrRef(out, 0);
					m_new[s] = static_cast<long long>(out.size());
					out.insert(out.end(), m_in + s, m_in + e);
					pos = e;
					continue;
				}
			}
			if (s > pos)
			{
				Need(pos, s - pos);
				if (k == "bonedata")
				{
					size_t head = s;
					while (head > pos && m_in[head - 1] == 0)
						head--;
					const size_t zeros = (s - pos) - (head - pos);
					(void)zeros;
					if (head > pos)
					{
						bool strlike = true;
						for (size_t i = pos; i < head; i++)
						{
							const char c = m_in[i];
							if (c != 0 && (c < 32 || c >= 127))
							{
								strlike = false;
								break;
							}
						}
						if (!strlike)
							Warn("pre-bonedata nonstring with zero slack");
						m_gap[pos] = static_cast<long long>(out.size());
						out.insert(out.end(), m_in + pos, m_in + head);
					}
				}
				else
				{
					m_gap[pos] = static_cast<long long>(out.size());
					out.insert(out.end(), m_in + pos, m_in + s);
				}
			}
			else if (s < pos)
			{
				Warn("overlap " + k);
			}
			if (k == "skinfam0" || k == "dropstr")
			{
				if (e > pos)
					pos = e;
				PadTo(out, 4);
				continue;
			}
			if (k == "bonedata")
			{
				PadTo(out, 64);
				m_new[s] = static_cast<long long>(out.size());
				for (int i = 0; i < nb; i++)
					BoneRec(out, i, hasLin, linIdx, lb, structRefs);
			}
			else if (k == "linear")
			{
				if (nb > 1)
				{
					PadForHdrRef(out, 0);
					m_new[s] = static_cast<long long>(out.size());
					const size_t sizes[6] = { 4ull * nb, 2ull * nb, 12ull * nb,
						16ull * nb, 12ull * nb, 48ull * nb };
					size_t newidx[6] = { 14, 0, 0, 0, 0, 0 };
					size_t cur = 14 + sizes[0];
					for (int i = 1; i < 6; i++)
					{
						cur = Align4(cur);
						newidx[i] = cur;
						cur += sizes[i];
					}
					const size_t base = out.size();
					const uint16_t h0 = static_cast<uint16_t>(nb);
					out.insert(out.end(), reinterpret_cast<const char*>(&h0),
						reinterpret_cast<const char*>(&h0) + 2);
					for (int i = 0; i < 6; i++)
					{
						const uint16_t v = static_cast<uint16_t>(newidx[i]);
						out.insert(out.end(), reinterpret_cast<const char*>(&v),
							reinterpret_cast<const char*>(&v) + 2);
					}
					const int order[6] = { 0, 1, 2, 3, 4, 5 };
					for (int li = 0; li < 6; li++)
					{
						const int oi = order[li];
						while (out.size() - base < newidx[li])
							out.push_back(0);
						Need(lb + linIdx[oi], sizes[li]);
						m_new[lb + linIdx[oi]] = static_cast<long long>(out.size());
						out.insert(out.end(), m_in + lb + linIdx[oi],
							m_in + lb + linIdx[oi] + sizes[li]);
					}
				}
				else
				{
					m_new[s] = -1;
				}
			}
			else if (k == "bonestate")
			{
				PadTo(out, 16);
				PadForHdrRef(out, 176);
				m_new[s] = static_cast<long long>(out.size());
				const int cnt = HRaw(178);
				Need(s, static_cast<size_t>(cnt) * 2);
				for (int i = 0; i < cnt; i++)
				{
					const uint16_t v = RU16(s + static_cast<size_t>(2 * i));
					if (v > 255)
						Warn("bonestate >255");
					out.push_back(static_cast<char>(v & 0xFF));
				}
			}
			else if (k == "bonetable")
			{
				m_new[s] = static_cast<long long>(out.size());
				const size_t n = (e - s) / 2;
				Need(s, n * 2);
				for (size_t i = 0; i < n; i++)
				{
					const uint16_t v = RU16(s + 2 * i);
					if (v > 255)
						Warn("bonetable >255");
					out.push_back(static_cast<char>(v & 0xFF));
				}
				if (n % 2)
					out.push_back(0);
			}
			else if (k == "bonehdr" || k == "attachments" || k == "hitboxset" ||
				k == "hitboxes" || k == "textures" || k == "skins" || k == "skinnames" ||
				k == "bodyparts" || k == "model" || k == "meshes" || k == "poseparams" ||
				k == "ikchains" || k == "iklinks" || k == "nodenames" || k == "nodedata" ||
				k == "lods" || k == "groups" || k == "followers" || k == "srcbones" ||
				k == "procbones" || k == "procmap" || k == "uipanels")
			{
				Need(s, e - s);
				if (k == "procbones" || k == "procmap" || k == "uipanels" || k == "followers" || k == "srcbones")
					PadForHdrRef(out, 0);
				else if (k == "lods")
					PadForHdrRef(out, 184);
				else if (k == "groups")
					PadForHdrRef(out, 180);
				m_new[s] = static_cast<long long>(out.size());
				out.insert(out.end(), m_in + s, m_in + e);
				RegRefs(k, s, e, m_new[s], structRefs);
			}
			else
			{
				Warn("unhandled " + k);
				Need(s, e - s);
				m_new[s] = static_cast<long long>(out.size());
				out.insert(out.end(), m_in + s, m_in + e);
			}
			if (e > pos)
				pos = e;
		}
		if (pos < m_size)
		{
			m_gap[pos] = static_cast<long long>(out.size());
			out.insert(out.end(), m_in + pos, m_in + m_size);
		}

		// Reference anim blocks at the tail, one per animdesc.
		for (auto& kv : m_animNew)
		{
			const size_t apos = kv.second;
			PadTo(out, 16);
			const size_t rpos = out.size();
			const int rnb = nb ? nb : 1;
			const size_t fs = RefFlagSize(rnb);
			const size_t need = fs + 10;
			const size_t at = out.size();
			out.resize(at + need, 0);
			WriteRefBlock(&out[at], rnb, bone0);
			WrI32(out, apos + 16, static_cast<int32_t>(rpos) - static_cast<int32_t>(apos));
		}

		PatchHeader(out);
		PatchStructs(out, structRefs);
	}
};

} // anonymous namespace

// S30 rmdl -> S21 v17 bytes.
bool ConvertRMDL19S30To17(const char* pIn, size_t inSize, std::vector<char>& outBytes,
	std::vector<std::string>& warnings, std::string& err)
{
	if (!pIn || inSize < kS30Hdr)
	{
		err = "input too small for S30 header";
		return false;
	}
	S30ToV17 cvt(pIn, inSize);
	return cvt.Run(outBytes, warnings, err);
}

// A model whose studiohdr carries kPackedBoneIndexFlag packs idx[1] as 10
// bits over the second and third index bytes (idx[1] = b5 >> 2 | b6 << 6,
// idx[0] = b4 | (b5 & 3) << 8); S21 reads idx[1] straight from b5. Models
// without the flag keep plain inline indices. Rewrites one rev4 group in place.
static bool RepackVGBoneIndices19S30(char* pGroup, size_t groupSize, std::string& err)
{
	auto rd8 = [&](size_t o) -> uint8_t { return static_cast<uint8_t>(pGroup[o]); };
	auto rd32 = [&](size_t o) -> uint32_t { uint32_t v; memcpy(&v, pGroup + o, 4); return v; };
	if (groupSize < 8)
	{
		err = "group payload too small";
		return false;
	}
	const uint8_t lodCount = rd8(1);
	const size_t lodBase = 4 + rd32(4);
	for (uint8_t l = 0; l < lodCount; l++)
	{
		const size_t le = lodBase + static_cast<size_t>(l) * 8;
		if (le + 8 > groupSize)
		{
			err = "lod entry out of range";
			return false;
		}
		const uint8_t meshCount = rd8(le);
		const size_t meshBase = le + 4 + rd32(le + 4);
		for (uint8_t m = 0; m < meshCount; m++)
		{
			const size_t me = meshBase + static_cast<size_t>(m) * 48;
			if (me + 48 > groupSize)
			{
				err = "mesh entry out of range";
				return false;
			}
			const uint32_t flags = rd32(me);
			const uint32_t vertCount = rd32(me + 8);
			uint16_t vertSize;
			memcpy(&vertSize, pGroup + me + 12, 2);
			if (vertCount == 0 || !(flags & 0x5000))
				continue;
			static const size_t kPosSize[4] = { 0, 12, 8, 6 };
			const size_t blendAt = kPosSize[flags & 3];
			const size_t vertBase = me + 24 + rd32(me + 24);
			if (vertBase + static_cast<size_t>(vertCount) * vertSize > groupSize || blendAt + 8 > vertSize)
			{
				err = "vertex buffer out of range";
				return false;
			}
			for (uint32_t v = 0; v < vertCount; v++)
			{
				char* pIdx = pGroup + vertBase + static_cast<size_t>(v) * vertSize + blendAt + 4;
				const uint8_t b5 = static_cast<uint8_t>(pIdx[1]);
				const uint8_t b6 = static_cast<uint8_t>(pIdx[2]);
				const unsigned idx1 = (b5 >> 2) | (static_cast<unsigned>(b6) << 6);
				if ((b5 & 3) || idx1 > 0xFF)
				{
					err = "vertex bone index exceeds 255";
					return false;
				}
				pIdx[1] = static_cast<char>(idx1);
				pIdx[2] = 0;
			}
		}
	}
	return true;
}

// Stored starpak VG slice -> S21 raw VG: slice/decompress/concat per group.
bool ConvertVGStored19S30(const char* pRmdl, size_t rmdlSize, const char* pStored,
	size_t storedSize, std::vector<char>& outRaw, std::string& err)
{
	if (!pRmdl || rmdlSize < kS30Hdr)
	{
		err = "rmdl too small for S30 header";
		return false;
	}
	auto rdU16 = [&](size_t off, uint16_t& v) -> bool
	{
		if (off + 2 > rmdlSize)
			return false;
		memcpy(&v, pRmdl + off, 2);
		return true;
	};
	int32_t hdrFlags = 0;
	memcpy(&hdrFlags, pRmdl, 4);
	const bool packedBoneIndices = (hdrFlags & kPackedBoneIndexFlag) != 0;
	uint16_t grpRaw = 0, grpCnt = 0;
	if (!rdU16(196, grpRaw) || !rdU16(198, grpCnt))
	{
		err = "rmdl too small for group header";
		return false;
	}
	if (grpCnt > 64)
	{
		err = "implausible group count";
		return false;
	}
	const size_t base = 196 + DecOff(grpRaw);
	outRaw.clear();
	for (int i = 0; i < grpCnt; i++)
	{
		if (base + static_cast<size_t>(i) * 16 + 16 > rmdlSize)
		{
			err = "group record out of range";
			return false;
		}
		int32_t doff, cs, ds;
		memcpy(&doff, pRmdl + base + static_cast<size_t>(i) * 16, 4);
		memcpy(&cs, pRmdl + base + static_cast<size_t>(i) * 16 + 4, 4);
		memcpy(&ds, pRmdl + base + static_cast<size_t>(i) * 16 + 8, 4);
		const uint8_t comp = static_cast<uint8_t>(*(pRmdl + base + static_cast<size_t>(i) * 16 + 12));
		if (comp != 0 && comp != 3)
		{
			err = "group has unknown compression";
			return false;
		}
		if (doff < 0 || cs <= 0 || ds <= 0 || static_cast<size_t>(doff) + static_cast<size_t>(cs) > storedSize)
		{
			err = "group slice out of range";
			return false;
		}
		const size_t at = outRaw.size();
		if (comp == 0)
		{
			if (static_cast<size_t>(doff) + static_cast<size_t>(ds) > storedSize)
			{
				err = "raw group slice out of range";
				return false;
			}
			outRaw.insert(outRaw.end(), pStored + doff, pStored + doff + ds);
		}
		else
		{
			outRaw.resize(at + static_cast<size_t>(ds));
			if (!Oodle::Decompress(reinterpret_cast<const uint8_t*>(pStored + doff), static_cast<size_t>(cs), reinterpret_cast<uint8_t*>(&outRaw[at]), static_cast<size_t>(ds)))
			{
				outRaw.resize(at);
				err = "oodle decompression failed";
				return false;
			}
		}
		if (packedBoneIndices && !RepackVGBoneIndices19S30(&outRaw[at], static_cast<size_t>(ds), err))
			return false;
	}
	return true;
}
