/*
	Panels with text in the headset (hotd2-vr). See xr_panel.h.

	The text is drawn on the CPU with stb_truetype (the copy that comes with Dear ImGui) in
	Flycast's own Roboto, into a picture that goes on a quad.

	Copyright 2026 mikermak. This file is part of Flycast and is distributed under the GNU GPL v2 or later.
*/
#include "xr_panel.h"
#include <stb_image.h>
#include "rend/gles/gles.h"
#include "rend/gles/glcache.h"
#include "oslib/resources.h"
#include "log/Log.h"

#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "imgui/imstb_truetype.h"
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <memory>

namespace vr::xr
{
namespace
{

// The lines panel's picture, and where it is
constexpr int Width = 1024, Height = 448;
constexpr float WidthMetres = 1.3f, Distance = 1.6f;
// The rows panel's picture: 1024 px across 0.9 m (about as sharp as a Quest 3 shows it at 1.2 m),
// a band for the title, the rows, a band for the footer.
constexpr int RowsWidth = 1024, TitleBand = 100, RowHeight = 72, FooterBand = 76;
constexpr float RowsWidthMetres = 0.9f;

// A single raster layout serves painting, ray targeting and presentation focus.
// Six slots cover the largest main/advanced page without moving the panel.
constexpr int SettingsWidth = 1200, SettingsHeight = 880, SettingsMaxRows = 6;
constexpr float SettingsWidthMetres = 1.1f;
constexpr float SettingsRowTop = 312.f, SettingsRowPitch = 80.f, SettingsRowHeight = 70.f;
constexpr float SettingsRight = 1140.f, SettingsChoiceGap = 7.f;

std::unique_ptr<u8[]> fontData;
stbtt_fontinfo font;
bool fontTried, fontLoaded;

GLuint program;
GLuint quadVao, quadVbo;
GLint uMvp = -1, uTint = -1;
// A panel's picture on the GPU, and what it says now (made again only when that changes)
struct Picture
{
	GLuint texture = 0;
	std::string shown;
	int width = 0, height = 0;	// its storage's size (0: none yet)
};
Picture linesPicture, rowsPicture, settingsPicture;
// one line's pictures (textTexture), by px, colour and text; and a white texel for lines and dots
struct TextPicture
{
	GLuint texture = 0;
	int width = 0, height = 0;
};
std::map<std::string, TextPicture> textPictures;
GLuint whiteTexture, contactTexture;
// the picture being painted
int canvasWidth, canvasHeight;
std::vector<u8> pixels;

void logGlErrors()
{
	// (hotd2-vr: the first few seconds only, at most 10 lines: checked after every draw it
	// costs a GPU round trip and filled the log at headset rate)
	static int checks, logged;
	if (checks >= 1000 || logged >= 10)
		return;
	checks++;
	for (GLenum error; (error = glGetError()) != GL_NO_ERROR && logged < 10; logged++)
		ERROR_LOG(RENDERER, "VR panel OpenGL error 0x%04x", error);
}

bool loadFont()
{
	if (fontTried)
		return fontLoaded;
	fontTried = true;
	size_t size = 0;
	fontData = resource::load("fonts/Roboto-Bold.ttf", size);
	fontLoaded = fontData != nullptr && stbtt_InitFont(&font, fontData.get(), stbtt_GetFontOffsetForIndex(fontData.get(), 0)) != 0;
	if (!fontLoaded)
		WARN_LOG(RENDERER, "XR: no font for the panel");
	return fontLoaded;
}

void blend(int x, int y, const glm::vec3& colour, float alpha)
{
	if (x < 0 || y < 0 || x >= canvasWidth || y >= canvasHeight || alpha <= 0.f)
		return;
	u8 *p = &pixels[((size_t)y * canvasWidth + x) * 4];
	for (int c = 0; c < 3; c++)
		p[c] = (u8)std::lround(p[c] * (1.f - alpha) + colour[c] * 255.f * alpha);
}

void fill(int x0, int y0, int x1, int y1, const glm::vec3& colour, float alpha = 1.f)
{
	for (int y = std::max(y0, 0); y < std::min(y1, canvasHeight); y++)
		for (int x = std::max(x0, 0); x < std::min(x1, canvasWidth); x++)
			blend(x, y, colour, alpha);
}

// A new picture in the shared purple VR4ALL theme.
void background(int width, int height, const glm::vec3& back = theme::Background,
		const glm::vec3& frame = theme::Accent)
{
	canvasWidth = width;
	canvasHeight = height;
	pixels.assign((size_t)width * height * 4, 0);
	for (size_t i = 0; i < pixels.size(); i += 4)
	{
		pixels[i] = (u8)(back.r * 255.f + 0.5f);
		pixels[i + 1] = (u8)(back.g * 255.f + 0.5f);
		pixels[i + 2] = (u8)(back.b * 255.f + 0.5f);
		pixels[i + 3] = 235;
	}
	fill(0, 0, width, 5, frame);
	fill(0, height - 5, width, height, frame);
	fill(0, 0, 5, height, frame);
	fill(width - 5, 0, width, height, frame);
}

// How wide a line is at px, in pixels
float textWidth(const std::string& s, float px)
{
	const float scale = stbtt_ScaleForPixelHeight(&font, px);
	float width = 0.f;
	for (size_t i = 0; i < s.size(); i++)
	{
		int advance, bearing;
		stbtt_GetCodepointHMetrics(&font, (u8)s[i], &advance, &bearing);
		width += advance * scale;
		if (i + 1 < s.size())
			width += scale * stbtt_GetCodepointKernAdvance(&font, (u8)s[i], (u8)s[i + 1]);
	}
	return width;
}

// One line, its baseline at y: its left end at x (align 0), its middle (0.5) or its right end (1).
void text(const std::string& s, float px, float x, int y, const glm::vec3& colour, float align = 0.5f)
{
	const float scale = stbtt_ScaleForPixelHeight(&font, px);
	const float width = textWidth(s, px);
	// (centred: worked out as the lines panel always did, to the bit)
	x = align == 0.5f ? (x * 2.f - width) * 0.5f : x - width * align;
	std::vector<u8> glyph;
	for (size_t i = 0; i < s.size(); i++)
	{
		const int c = (u8)s[i];
		int advance, bearing, x0, y0, x1, y1;
		stbtt_GetCodepointHMetrics(&font, c, &advance, &bearing);
		stbtt_GetCodepointBitmapBoxSubpixel(&font, c, scale, scale, x - std::floor(x), 0.f, &x0, &y0, &x1, &y1);
		const int w = x1 - x0, h = y1 - y0;
		if (w > 0 && h > 0)
		{
			glyph.assign((size_t)w * h, 0);
			stbtt_MakeCodepointBitmapSubpixel(&font, glyph.data(), w, h, w, scale, scale, x - std::floor(x), 0.f, c);
			for (int gy = 0; gy < h; gy++)
				for (int gx = 0; gx < w; gx++)
					blend((int)std::floor(x) + x0 + gx, y + y0 + gy, colour, glyph[(size_t)gy * w + gx] / 255.f);
		}
		x += advance * scale;
		if (i + 1 < s.size())
			x += scale * stbtt_GetCodepointKernAdvance(&font, c, (u8)s[i + 1]);
	}
}

void paint(const Panel& panel)
{
	if (panel.brand)
	{
		size_t size = 0;
		auto image = resource::load("picture/vr4all-splash.png", size);
		int w = 0, h = 0, channels = 0;
		stbi_set_flip_vertically_on_load_thread(0);
		u8 *decoded = image ? stbi_load_from_memory(image.get(), (int)size, &w, &h, &channels, 4) : nullptr;
		if (decoded != nullptr && w == Width && h == Height)
		{
			canvasWidth = w;
			canvasHeight = h;
			pixels.assign(decoded, decoded + (size_t)w * h * 4);
			stbi_image_free(decoded);
			return;
		}
		stbi_image_free(decoded);
		WARN_LOG(RENDERER, "XR: splash image unavailable, using VR4ALL text");
		background(Width, Height);
		text("VR4ALL", 104.f, Width * 0.5f, 245, theme::Text);
		text("STANDALONE VR / PC VR", 30.f, Width * 0.5f, 310, theme::Accent);
		return;
	}
	background(Width, Height, panel.back, panel.frame);
	float total = 0.f;
	for (const PanelLine& line : panel.lines)
		total += line.size * Height * 1.45f;
	const int bar = panel.progress >= 0.f ? 46 : 0;
	float y = (Height - total - bar) * 0.5f;
	for (const PanelLine& line : panel.lines)
	{
		const float px = line.size * Height;
		y += px * 1.45f;
		if (!line.text.empty())
			text(line.text, px, Width * 0.5f, (int)(y - px * 0.38f), line.colour);
	}
	if (panel.progress >= 0.f)
	{
		const int x0 = Width * 18 / 100, x1 = Width - x0, top = (int)y + 24, bottom = top + 14;
		fill(x0, top, x1, bottom, panel.barTrack);
		fill(x0, top, x0 + (int)((x1 - x0) * std::clamp(panel.progress, 0.f, 1.f)), bottom, panel.barFill);
	}
	if (!panel.footer.empty())
		text(panel.footer, Height * 0.042f, Width * 0.5f, Height - 28, panel.footerColour);
}

// A label's picture (textTexture): the line on a dark plate with round, soft-edged corners and a
// margin around it.
void paintLabel(const std::string& s, float px, const glm::vec3& colour)
{
	const int margin = (int)std::lround(px * 0.4f);
	canvasWidth = (int)std::ceil(textWidth(s, px)) + margin * 2;
	canvasHeight = (int)std::lround(px * 1.45f);
	pixels.assign((size_t)canvasWidth * canvasHeight * 4, 0);
	const float radius = canvasHeight * 0.3f;
	for (int y = 0; y < canvasHeight; y++)
		for (int x = 0; x < canvasWidth; x++)
		{
			const float cx = std::clamp(x + 0.5f, radius, canvasWidth - radius);
			const float cy = std::clamp(y + 0.5f, radius, canvasHeight - radius);
			const float inside = std::clamp(radius + 0.5f - std::hypot(x + 0.5f - cx, y + 0.5f - cy), 0.f, 1.f);
			u8 *p = &pixels[((size_t)y * canvasWidth + x) * 4];
			p[0] = (u8)(theme::Background.r * 255.f);
			p[1] = (u8)(theme::Background.g * 255.f);
			p[2] = (u8)(theme::Background.b * 255.f);
			p[3] = (u8)std::lround(225.f * inside);
		}
	// (capitals in the middle, as the rows panel puts them)
	text(s, px, (float)margin, (int)std::lround(canvasHeight * 0.5f + px * 0.3f), colour, 0.f);
}

int rowsHeight(const RowsPanel& panel)
{
	return TitleBand + RowHeight * (int)panel.rows.size() + FooterBand;
}

void paintRows(const RowsPanel& panel)
{
	const int w = RowsWidth, h = rowsHeight(panel);
	background(w, h);
	const glm::vec3 &accent = theme::Accent, &white = theme::Text, &grey = theme::Muted, &dim = theme::Dim, &rule = theme::Line;
	// the baseline that puts capitals this size in the middle of a band
	auto baseline = [](float middle, float px) { return (int)std::lround(middle + px * 0.3f); };
	text(panel.title, 50.f, w * 0.5f, baseline(TitleBand * 0.5f + 2.f, 50.f), accent);
	fill(40, TitleBand - 3, w - 40, TitleBand - 1, rule);
	for (size_t i = 0; i < panel.rows.size(); i++)
	{
		const PanelRow& row = panel.rows[i];
		const int top = TitleBand + RowHeight * (int)i;
		const float middle = top + RowHeight * 0.5f;
		if (row.selected)
		{
			fill(12, top + 4, w - 12, top + RowHeight - 4, theme::Selection, 0.85f);
			fill(12, top + 4, 20, top + RowHeight - 4, accent);
		}
		const glm::vec3 labelColour = row.dim ? dim : white;
		const glm::vec3 valueColour = row.dim ? dim : row.selected ? white : grey;
		if (row.stepper)
		{
			const glm::vec3 sign = row.dim ? dim : row.selected ? accent : grey;
			text("-", 48.f, 44.f, baseline(middle, 48.f), sign);
			text("+", 48.f, w - 44.f, baseline(middle, 48.f), sign);
		}
		text(row.label, 38.f, 76.f, baseline(middle, 38.f), labelColour, 0.f);
		if (!row.value.empty())
			text(row.value, 38.f, w - 76.f, baseline(middle, 38.f), valueColour, 1.f);
	}
	fill(40, h - FooterBand + 1, w - 40, h - FooterBand + 3, rule);
	if (!panel.footer.empty())
		text(panel.footer, 26.f, w * 0.5f, baseline(h - FooterBand * 0.5f - 1.f, 26.f), grey);
}

SettingsRect normalized(float x, float y, float w, float h)
{
	return {x / SettingsWidth, y / SettingsHeight, w / SettingsWidth, h / SettingsHeight};
}

bool contains(SettingsRect rect, glm::vec2 uv)
{
	return rect.w > 0.f && rect.h > 0.f && uv.x >= rect.x && uv.y >= rect.y &&
		uv.x < rect.x + rect.w && uv.y < rect.y + rect.h;
}

std::vector<SettingsRect> choiceRects(const SettingsPanel& panel, int index)
{
	const SettingsRect row = settingsRowRect(panel, index);
	if (row.w <= 0.f) return {};
	const auto& choices = panel.rows[index].choices;
	std::vector<float> widths;
	float total = 0.f;
	for (const auto& choice : choices)
	{
		int letters = 0;
		letters = (int)choice.size();
		const float width = std::max(78.f, letters * 15.f + 28.f);
		widths.push_back(width);
		total += width;
	}
	if (widths.empty()) return {};
	const float gaps = SettingsChoiceGap * (widths.size() - 1);
	const float scale = std::min(1.f, (670.f - gaps) / total);
	float x = SettingsRight - total * scale - gaps;
	std::vector<SettingsRect> rects;
	for (float width : widths)
	{
		rects.push_back(normalized(x, row.y * SettingsHeight + (row.h * SettingsHeight - 48.f) * .5f,
			width * scale, 48.f));
		x += width * scale + SettingsChoiceGap;
	}
	return rects;
}

SettingsRect stepRect(const SettingsPanel& panel, int row, int direction)
{
	const auto rect = settingsRowRect(panel, row);
	if (rect.w <= 0.f) return {};
	return normalized(direction < 0 ? 878.f : 1082.f,
		rect.y * SettingsHeight + (rect.h * SettingsHeight - 52.f) * .5f, 58.f, 52.f);
}

void rounded(float x, float y, float w, float h, float radius, const glm::vec3& colour)
{
	for (int py = std::max(0, (int)std::floor(y)); py < std::min(canvasHeight, (int)std::ceil(y + h)); ++py)
		for (int px = std::max(0, (int)std::floor(x)); px < std::min(canvasWidth, (int)std::ceil(x + w)); ++px)
		{
			const float cx = std::clamp(px + .5f, x + radius, x + w - radius);
			const float cy = std::clamp(py + .5f, y + radius, y + h - radius);
			blend(px, py, colour, std::clamp(radius + .5f - std::hypot(px + .5f - cx, py + .5f - cy), 0.f, 1.f));
		}
}

void card(SettingsRect r, const glm::vec3& back, const glm::vec3& edge, float radius = 13.f)
{
	const float x = r.x * SettingsWidth, y = r.y * SettingsHeight;
	const float w = r.w * SettingsWidth, h = r.h * SettingsHeight;
	rounded(x, y, w, h, radius, edge);
	rounded(x + 1.5f, y + 1.5f, w - 3.f, h - 3.f, radius - 1.5f, back);
}

int baseline(float middle, float px) { return (int)std::lround(middle + px * .3f); }

void fitText(const std::string& value, float px, float x, int y, float width,
		const glm::vec3& colour, float align = 0.f)
{
	const float natural = textWidth(value, px);
	if (natural > width && natural > 0.f) px *= width / natural;
	text(value, px, x, y, colour, align);
}

void wrappedText(const std::string& value, float px, float x, int y, float width,
		const glm::vec3& colour, int maxLines = 2)
{
	size_t at = 0;
	for (int line = 0; at < value.size() && line < maxLines; ++line)
	{
		size_t end = at, accepted = at;
		while (end < value.size())
		{
			const auto space = value.find(' ', end);
			end = space == std::string::npos ? value.size() : space + 1;
			if (textWidth(value.substr(at, end - at), px) > width && accepted > at) break;
			accepted = end;
			if (space == std::string::npos) break;
		}
		if (line == maxLines - 1) accepted = value.size();
		fitText(value.substr(at, accepted - at), px, x, y + line * (int)(px * 1.35f), width, colour);
		at = accepted;
	}
}

void paintSettings(const SettingsPanel& panel)
{
	background(SettingsWidth, SettingsHeight);
	const glm::vec3 surface(29/255.f,34/255.f,48/255.f), inset(17/255.f,21/255.f,31/255.f);
	const glm::vec3 edge(83/255.f,75/255.f,103/255.f);
	// Outer corner alpha is applied last, so all drawing uses the same CPU blend helper.
	rounded(0,0,SettingsWidth,SettingsHeight,26.f,edge);
	rounded(2,2,SettingsWidth-4,SettingsHeight-4,24.f,theme::Background);
	const auto headerColour = (panel.dialog||panel.locked) ? theme::Dim : theme::Text;
	text("VR4ALL", 27.f, 38.f, 43, (panel.dialog||panel.locked) ? theme::Dim : theme::Accent, 0.f);
	fitText(panel.game + " / Settings", 42.f, 38.f, 91, 940.f, headerColour);
	card(settingsCloseRect(), surface, panel.closeSelected && !(panel.dialog||panel.locked) ? theme::Accent : theme::Line);
	text("Close", 28.f, 1098.f, baseline(63.f,28.f), headerColour);
	fill(2,119,SettingsWidth-2,121,theme::Line);
	fill(2,121,SettingsWidth-2,204,inset);
	const char* tabs[] = {"Gun","World","System","In-game"};
	for (int i=0;i<4;++i)
	{
		const auto rect = settingsTabRect(i);
		const bool active = panel.activeTab == i, selected = panel.selectedTab == i && !panel.dialog && !panel.locked;
		card(rect, active ? theme::Selection : inset, selected ? theme::Accent : active ? edge : inset, 12.f);
		if (active) fill((int)(rect.x*SettingsWidth)+12,186,(int)((rect.x+rect.w)*SettingsWidth)-12,189,
			(panel.dialog||panel.locked) ? theme::Dim : theme::Accent);
		text(tabs[i],32.f,(rect.x+rect.w*.5f)*SettingsWidth,baseline(160.f,32.f),
			(panel.dialog||panel.locked) ? theme::Dim : active || selected ? theme::Text : theme::Muted);
	}
	fill(2,203,SettingsWidth-2,205,theme::Line);
	if (panel.dialog)
	{
		fitText(panel.title,44.f,84.f,307,1032.f,theme::Text);
		wrappedText(panel.subtitle,29.f,84.f,363,1032.f,theme::Muted,3);
	}
	else
	{
		const float left = panel.back ? 112.f : 38.f;
		if (panel.back)
		{
			card(settingsBackRect(),surface,panel.backSelected ? theme::Accent : theme::Line);
			text("<",37.f,65.f,baseline(257.f,37.f),theme::Accent);
		}
		fitText(panel.title,36.f,left,258,SettingsWidth-left-38.f,theme::Text);
		fitText(panel.subtitle,24.f,left,291,SettingsWidth-left-38.f,theme::Muted);
	}
	for (int i=0;i<(int)panel.rows.size() && i<(panel.dialog?2:SettingsMaxRows);++i)
	{
		const SettingsRow& row = panel.rows[i];
		const auto rect = settingsRowRect(panel,i);
		const float x=rect.x*SettingsWidth,y=rect.y*SettingsHeight,w=rect.w*SettingsWidth,h=rect.h*SettingsHeight;
		const float middle=y+h*.5f;
		const bool selected=row.selected && !row.dim;
		card(rect,selected?glm::mix(surface,theme::Selection,.65f):surface,
			selected?theme::Accent:theme::Line,14.f);
		const auto foreground = row.dim ? theme::Dim : row.action ? theme::Accent : theme::Text;
		const auto controls = choiceRects(panel,i);
		const float controlLeft = !controls.empty() ? controls.front().x*SettingsWidth : row.stepper ? 858.f : x+w-270.f;
		const float labelWidth = row.action ? w-100.f : std::max(180.f,controlLeft-x-42.f);
		fitText(row.label,34.f,x+22.f,baseline(middle-(row.note.empty()?0.f:11.f),34.f),labelWidth,foreground);
		if (!row.note.empty())
			fitText(row.note,22.f,x+22.f,baseline(middle+20.f,22.f),labelWidth,row.dim?theme::Dim:theme::Muted);
		if (!controls.empty())
		{
			for (int c=0;c<(int)controls.size();++c)
			{
				const bool chosen = c==row.choice, dim = row.dim || !row.canChoose(c);
				card(controls[c],chosen ? dim ? glm::mix(inset,theme::Selection,.45f) : theme::Selection : inset,
					chosen&&!dim?edge:theme::Line,10.f);
				fitText(row.choices[c],27.f,(controls[c].x+controls[c].w*.5f)*SettingsWidth,
					baseline(middle,27.f),controls[c].w*SettingsWidth-18.f,
					dim?theme::Dim:chosen?theme::Text:theme::Muted,.5f);
			}
		}
		else if (row.stepper)
		{
			for (int direction : {-1,1})
			{
				const auto step = stepRect(panel,i,direction);
				card(step,row.dim?surface:theme::Selection,row.dim?theme::Line:edge,10.f);
				text(direction<0?"-":"+",38.f,(step.x+step.w*.5f)*SettingsWidth,baseline(middle,38.f),
					row.dim?theme::Dim:theme::Accent);
			}
			fitText(row.value,33.f,1009.f,baseline(middle,33.f),132.f,row.dim?theme::Dim:theme::Text,.5f);
		}
		else if (row.action)
			text(">",36.f,x+w-34.f,baseline(middle,36.f),foreground);
		else if (!row.value.empty())
			fitText(row.value,30.f,x+w-24.f,baseline(middle,30.f),260.f,row.dim?theme::Dim:theme::Muted,1.f);
	}
	fill(2,810,SettingsWidth-2,SettingsHeight-2,inset);
	fill(2,809,SettingsWidth-2,811,theme::Line);
	fitText(panel.footer.empty()?"Aim + trigger to select  |  Stick to adjust":panel.footer,
		23.f,38.f,852,884.f,theme::Muted);
	text(panel.locked?"Saving...":"Menu to close",23.f,1162.f,852,panel.locked?theme::Muted:theme::Accent,1.f);
	for(int y=0;y<SettingsHeight;++y)
		for(int x=0;x<SettingsWidth;++x)
		{
			const float cx=std::clamp(x+.5f,26.f,SettingsWidth-26.f);
			const float cy=std::clamp(y+.5f,26.f,SettingsHeight-26.f);
			pixels[((size_t)y*SettingsWidth+x)*4+3]=(u8)std::lround(245.f*
				std::clamp(26.5f-std::hypot(x+.5f-cx,y+.5f-cy),0.f,1.f));
		}
}

bool initGl()
{
	if (program != 0)
		return true;
	OpenGlSource vertex;
	vertex.addSource(VertexCompatShader).addSource(R"(
in highp vec4 in_pos;
in highp vec2 in_uv;
uniform highp mat4 mvp;
out highp vec2 vtx_uv;
void main()
{
	vtx_uv = in_uv;
	gl_Position = mvp * vec4(in_pos.xyz, 1.0);
}
)");
	OpenGlSource fragment;
	fragment.addSource(PixelCompatShader).addSource(R"(
uniform sampler2D tex;
uniform lowp vec4 tint;
in highp vec2 vtx_uv;
void main()
{
	gl_FragColor = texture(tex, vtx_uv) * tint;
}
)");
	program = gl_CompileAndLink(vertex.generate().c_str(), fragment.generate().c_str());
	if (program == 0)
		return false;
	uMvp = glGetUniformLocation(program, "mvp");
	uTint = glGetUniformLocation(program, "tint");
	glcache.UseProgram(program);
	glUniform1i(glGetUniformLocation(program, "tex"), 0);
	quadVao = createVertexArray();
	bindVertexArray(quadVao);
	glGenBuffers(1, &quadVbo);
	glBindBuffer(GL_ARRAY_BUFFER, quadVbo);
	glBufferData(GL_ARRAY_BUFFER, 20 * sizeof(float), nullptr, GL_STREAM_DRAW);
	glVertexAttribPointer(VERTEX_POS_ARRAY, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (const void *)0);
	glVertexAttribPointer(VERTEX_UV_ARRAY, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (const void *)(3 * sizeof(float)));
	glEnableVertexAttribArray(VERTEX_POS_ARRAY);
	glEnableVertexAttribArray(VERTEX_UV_ARRAY);
	bindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	return true;
}

// Binds the picture's texture, painted again (paintIt) when what it says changed.
template<typename Paint>
void bindPicture(Picture& picture, const std::string& says, Paint paintIt)
{
	glActiveTexture(GL_TEXTURE0);
	const bool made = picture.texture != 0;
	if (!made)
		glGenTextures(1, &picture.texture);
	glcache.BindTexture(GL_TEXTURE_2D, picture.texture);
	if (!made)
	{
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	if (!made || says != picture.shown)
	{
		picture.shown = says;
		paintIt();
		// the storage once (again only at another size), after that only new pixels into it
		if (picture.width == canvasWidth && picture.height == canvasHeight)
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, canvasWidth, canvasHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
		else
		{
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, canvasWidth, canvasHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
			picture.width = canvasWidth;
			picture.height = canvasHeight;
		}
	}
}

// The bound picture on a quad hx by hy metres each way from the middle of `model`, facing +z,
// its colour times `tint`.
void drawQuad(const glm::mat4& viewProj, const glm::mat4& model, float hx, float hy, const glm::vec4& tint = glm::vec4(1.f))
{
	const float quad[] {
		-hx,  hy, 0.f,  0.f, 0.f,
		 hx,  hy, 0.f,  1.f, 0.f,
		-hx, -hy, 0.f,  0.f, 1.f,
		 hx, -hy, 0.f,  1.f, 1.f,
	};
	glBindBuffer(GL_ARRAY_BUFFER, quadVbo);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(quad), quad);
	glcache.UseProgram(program);
	const glm::mat4 mvp = viewProj * model;
	glUniformMatrix4fv(uMvp, 1, GL_FALSE, &mvp[0][0]);
	glUniform4f(uTint, tint.r, tint.g, tint.b, tint.a);
	glcache.Disable(GL_SCISSOR_TEST);
	glcache.Disable(GL_STENCIL_TEST);
	glcache.Disable(GL_DEPTH_TEST);
	glcache.Disable(GL_CULL_FACE);
	glcache.Enable(GL_BLEND);
	glcache.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);	// (through the cache: the game's next draw relies on it)
	bindVertexArray(quadVao);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	bindVertexArray(0);
	glcache.Disable(GL_BLEND);
	glcache.BindTexture(GL_TEXTURE_2D, 0);
	logGlErrors();
}

}	// namespace

void drawPanel(const glm::mat4& viewProj, const Panel& panel)
{
	if (!loadFont() || !initGl())
		return;
	// what it says, the bar in steps of half a percent
	std::string says = panel.brand ? "VR4ALL SPLASH\n" : "";
	for (const PanelLine& line : panel.lines)
		says += line.text + '\n';
	says += panel.footer + '\n' + std::to_string(panel.progress < 0.f ? -1 : (int)(panel.progress * 200.f))
			+ '\n' + std::to_string((int)(panel.back.r * 255.f)) + ' ' + std::to_string((int)(panel.barFill.g * 255.f));
	bindPicture(linesPicture, says, [&] { paint(panel); });
	const float hx = WidthMetres * 0.5f, hy = hx * Height / Width;
	drawQuad(viewProj, glm::translate(glm::mat4(1.f), glm::vec3(0.f, 0.f, -Distance)), hx, hy);
}

RowsLayout rowsLayout(const RowsPanel& panel)
{
	const float metres = RowsWidthMetres / RowsWidth;
	const int height = rowsHeight(panel);
	return { RowsWidthMetres, height * metres, RowHeight * metres, (height * 0.5f - (TitleBand + RowHeight * 0.5f)) * metres };
}

void drawRowsPanel(const glm::mat4& viewProj, const glm::mat4& model, const RowsPanel& panel)
{
	if (!loadFont() || !initGl())
		return;
	std::string says = panel.title + '\n';
	for (const PanelRow& row : panel.rows)
		says += row.label + '\t' + row.value + '\t' + (char)('0' + row.selected + row.dim * 2 + row.stepper * 4) + '\n';
	says += panel.footer;
	bindPicture(rowsPicture, says, [&] { paintRows(panel); });
	const RowsLayout layout = rowsLayout(panel);
	drawQuad(viewProj, model, layout.width * 0.5f, layout.height * 0.5f);
}

bool rowsPanelHit(const glm::mat4& model, const RowsPanel& panel, const glm::vec3& origin, const glm::vec3& dir,
		glm::vec3 *point, glm::vec2 *uv)
{
	const glm::mat4 toPanel = glm::inverse(model);
	const glm::vec3 o = glm::vec3(toPanel * glm::vec4(origin, 1.f));
	const glm::vec3 d = glm::vec3(toPanel * glm::vec4(dir, 0.f));
	if (d.z > -1e-6f)
		return false;	// along it, or from behind
	const float t = -o.z / d.z;
	if (t <= 0.f)
		return false;
	const glm::vec3 hit = o + d * t;
	const RowsLayout layout = rowsLayout(panel);
	const glm::vec2 at(hit.x / layout.width + 0.5f, 0.5f - hit.y / layout.height);
	if (at.x < 0.f || at.x > 1.f || at.y < 0.f || at.y > 1.f)
		return false;
	if (point != nullptr)
		*point = origin + dir * t;
	if (uv != nullptr)
		*uv = at;
	return true;
}

int rowAt(const glm::mat4& model, const RowsPanel& panel, const glm::vec3& origin, const glm::vec3& dir, float *u)
{
	glm::vec2 uv;
	if (!rowsPanelHit(model, panel, origin, dir, nullptr, &uv))
		return -1;
	if (u != nullptr)
		*u = uv.x;
	const float y = uv.y * rowsHeight(panel) - TitleBand;
	if (y < 0.f)
		return -1;
	const int row = (int)(y / RowHeight);
	return row < (int)panel.rows.size() ? row : -1;
}

RowsLayout settingsLayout()
{
	const float metre = SettingsWidthMetres / SettingsWidth;
	return {SettingsWidthMetres, SettingsHeight * metre, SettingsRowPitch * metre,
		(SettingsHeight * .5f - SettingsRowTop - SettingsRowHeight * .5f) * metre};
}

SettingsRect settingsRowRect(const SettingsPanel& panel, int row)
{
	if (row < 0 || row >= (int)panel.rows.size() || row >= (panel.dialog ? 2 : SettingsMaxRows)) return {};
	if (panel.dialog) return normalized(216.f, 492.f + row * 96.f, 768.f, 82.f);
	return normalized(36.f, SettingsRowTop + row * SettingsRowPitch, 1128.f, SettingsRowHeight);
}

SettingsRect settingsTabRect(int tab)
{
	return tab >= 0 && tab < 4 ? normalized(28.f + tab * 289.f, 134.f, 277.f, 56.f) : SettingsRect{};
}

SettingsRect settingsCloseRect() { return normalized(1036.f,32.f,124.f,62.f); }

SettingsRect settingsBackRect() { return normalized(36.f,228.f,58.f,58.f); }

SettingsHit settingsAt(const SettingsPanel& panel, glm::vec2 uv)
{
	if(panel.locked)return {};
	if (!std::isfinite(uv.x) || !std::isfinite(uv.y) || uv.x < 0.f || uv.x > 1.f || uv.y < 0.f || uv.y > 1.f) return {};
	if (!panel.dialog)
	{
		if (contains(settingsCloseRect(),uv)) return {SettingsHit::Kind::Close};
		if (panel.back && contains(settingsBackRect(),uv)) return {SettingsHit::Kind::Back};
		for (int tab=0;tab<4;++tab)
			if (contains(settingsTabRect(tab),uv)) return {SettingsHit::Kind::Tab,tab};
	}
	for (int row=0;row<(int)panel.rows.size() && row<(panel.dialog?2:SettingsMaxRows);++row)
	{
		if (panel.rows[row].dim || !contains(settingsRowRect(panel,row),uv)) continue;
		SettingsHit hit{SettingsHit::Kind::Row,row};
		const auto choices=choiceRects(panel,row);
		for(int choice=0;choice<(int)choices.size();++choice)
			if(contains(choices[choice],uv)) { if (!panel.rows[row].canChoose(choice)) return {}; hit.choice=choice;return hit; }
		if (choices.empty() && panel.rows[row].stepper)
			for (int direction : {-1,1})
				if (contains(stepRect(panel,row,direction),uv)) { hit.direction=direction;return hit; }
		return hit;
	}
	return {};
}

bool settingsPanelHit(const glm::mat4& model, const SettingsPanel&, const glm::vec3& origin,
		const glm::vec3& dir, glm::vec3* point, glm::vec2* uv)
{
	const auto inverse=glm::inverse(model);
	const glm::vec3 o=inverse*glm::vec4(origin,1.f), d=inverse*glm::vec4(dir,0.f);
	if (!std::isfinite(d.z) || d.z > -1e-6f) return false;
	const float t=-o.z/d.z;
	if (!std::isfinite(t) || t <= 0.f) return false;
	const glm::vec3 hit=o+d*t;
	const auto layout=settingsLayout();
	const glm::vec2 at(hit.x/layout.width+.5f,.5f-hit.y/layout.height);
	if (!std::isfinite(at.x) || !std::isfinite(at.y) || at.x<0.f || at.x>1.f || at.y<0.f || at.y>1.f) return false;
	if (point) *point=origin+dir*t;
	if (uv) *uv=at;
	return true;
}

SettingsHit settingsHit(const glm::mat4& model, const SettingsPanel& panel,
		const glm::vec3& origin, const glm::vec3& dir)
{
	glm::vec2 uv;
	return settingsPanelHit(model,panel,origin,dir,nullptr,&uv) ? settingsAt(panel,uv) : SettingsHit{};
}

void drawSettingsPanel(const glm::mat4& viewProj, const glm::mat4& model, const SettingsPanel& panel)
{
	if (!loadFont() || !initGl()) return;
	std::string says;
	auto append=[&](const std::string& value) { says+=std::to_string(value.size())+':'+value; };
	append(panel.game);append(panel.title);append(panel.subtitle);append(panel.footer);
	append(std::to_string(panel.activeTab));append(std::to_string(panel.selectedTab));
	says+=char('0'+panel.back+panel.backSelected*2+panel.closeSelected*4+panel.dialog*8+panel.locked*16);
	for(const auto& row:panel.rows)
	{
		append(row.label);append(row.value);append(row.note);append(std::to_string(row.choice));
		says+=char('0'+row.selected+row.dim*2+row.stepper*4+row.action*8);
		append(std::to_string(row.choices.size()));
		for(const auto& choice:row.choices) append(choice);
        append(std::to_string(row.choiceEnabled.size()));
        for (bool enabled : row.choiceEnabled) says += enabled ? '1' : '0';
	}
	bindPicture(settingsPicture,says,[&] { paintSettings(panel); });
	const auto layout=settingsLayout();
	drawQuad(viewProj,model,layout.width*.5f,layout.height*.5f);
}

unsigned textTexture(const std::string& text, float px, const glm::vec3& colour, int& width, int& height)
{
	width = height = 0;
	if (text.empty() || !loadFont())
		return 0;
	char key[48];
	std::snprintf(key, sizeof(key), "%d %d %d %d\n", (int)std::lround(px * 100.f), (int)std::lround(colour.r * 255.f),
			(int)std::lround(colour.g * 255.f), (int)std::lround(colour.b * 255.f));
	TextPicture& picture = textPictures[key + text];
	if (picture.texture == 0)
	{
		paintLabel(text, px, colour);
		glActiveTexture(GL_TEXTURE0);
		glGenTextures(1, &picture.texture);
		glcache.BindTexture(GL_TEXTURE_2D, picture.texture);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, canvasWidth, canvasHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
		glcache.BindTexture(GL_TEXTURE_2D, 0);
		picture.width = canvasWidth;
		picture.height = canvasHeight;
	}
	width = picture.width;
	height = picture.height;
	return picture.texture;
}

void drawTexture(const glm::mat4& viewProj, const glm::mat4& model, unsigned texture, float hx, float hy, const glm::vec4& tint)
{
	if (!initGl())
		return;
	glActiveTexture(GL_TEXTURE0);
	if (texture == 0)
	{
		if (whiteTexture == 0)
		{
			const u8 white[4] { 255, 255, 255, 255 };
			glGenTextures(1, &whiteTexture);
			glcache.BindTexture(GL_TEXTURE_2D, whiteTexture);
			glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
		}
		texture = whiteTexture;
	}
	glcache.BindTexture(GL_TEXTURE_2D, texture);
	drawQuad(viewProj, model, hx, hy, tint);
}


void drawPanelContact(const glm::mat4& viewProj, const glm::mat4& model)
{
 if (!initGl()) return;
 if (contactTexture == 0)
 {
  constexpr int size = 64;
  std::vector<u8> dot(size * size * 4);
  for (int y = 0; y < size; ++y) for (int x = 0; x < size; ++x)
  {
   const float radius = std::hypot((x + .5f) / size * 2.f - 1.f, (y + .5f) / size * 2.f - 1.f);
   const float core = 1.f - std::clamp((radius - .13f) / .12f, 0.f, 1.f);
   const glm::vec3 colour = glm::mix(theme::Accent, glm::vec3(1.f), core);
   const int at = (y * size + x) * 4;
   for (int c = 0; c < 3; ++c) dot[at + c] = (u8)std::lround(colour[c] * 255.f);
   dot[at + 3] = (u8)std::lround(255.f * std::max(core, .7f * std::pow(std::max(0.f, 1.f - radius), 2.f)));
  }
  glActiveTexture(GL_TEXTURE0);
  glGenTextures(1, &contactTexture);
  glcache.BindTexture(GL_TEXTURE_2D, contactTexture);
  glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, dot.data());
 }
 drawTexture(viewProj, model, contactTexture, .012f, .012f);
}

void termPanel()
{
	if (program != 0)
		glcache.DeleteProgram(program);
	for (Picture *picture : { &linesPicture, &rowsPicture, &settingsPicture })
	{
		if (picture->texture != 0)
			glcache.DeleteTextures(1, &picture->texture);
		*picture = Picture();
	}
	for (auto& [key, picture] : textPictures)
		if (picture.texture != 0)
			glcache.DeleteTextures(1, &picture.texture);
	textPictures.clear();
	if (whiteTexture != 0)
		glcache.DeleteTextures(1, &whiteTexture);
	whiteTexture = 0;
	if (contactTexture != 0) glcache.DeleteTextures(1, &contactTexture);
	contactTexture = 0;
	if (quadVbo != 0) glDeleteBuffers(1, &quadVbo);
	deleteVertexArray(quadVao);
	program = 0;
	quadVao = quadVbo = 0;
	uMvp = uTint = -1;
}

}
