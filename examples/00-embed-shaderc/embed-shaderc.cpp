/*
 * Copyright 2011-2020 Branimir Karadzic. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx#license-bsd-2-clause
 */

#include "common.h"
#include "bgfx_utils.h"
#include "imgui/imgui.h"

#include "bx/bx.h"
#include "bx/hash.h"
#include "bx/string.h"
#include "bx/readerwriter.h"
#include "../tools/shaderc/shaderc.h"

#include <tinystl/vector.h>
#include <tinystl/string.h>
namespace stl = tinystl;

namespace
{

	struct PosColorVertex
	{
		float m_x;
		float m_y;
		float m_z;
		uint32_t m_abgr;

		static void init()
		{
			ms_layout
				.begin()
				.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float, false)
				.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
				.end();
		};

		static bgfx::VertexLayout ms_layout;
	};

	static PosColorVertex s_cubeVertices[] =
	{
		{-1.0f,  1.0f,  1.0f, 0xff000000 },
		{ 1.0f,  1.0f,  1.0f, 0xff0000ff },
		{-1.0f, -1.0f,  1.0f, 0xff00ff00 },
		{ 1.0f, -1.0f,  1.0f, 0xff00ffff },
		{-1.0f,  1.0f, -1.0f, 0xffff0000 },
		{ 1.0f,  1.0f, -1.0f, 0xffff00ff },
		{-1.0f, -1.0f, -1.0f, 0xffffff00 },
		{ 1.0f, -1.0f, -1.0f, 0xffffffff },
	};

	bgfx::VertexLayout PosColorVertex::ms_layout;


	static const uint16_t s_cubeTriList[] =
	{
		0, 1, 2, // 0
		1, 3, 2,
		4, 6, 5, // 2
		5, 6, 7,
		0, 2, 4, // 4
		4, 2, 6,
		1, 5, 3, // 6
		5, 7, 3,
		0, 4, 1, // 8
		4, 5, 1,
		2, 3, 6, // 10
		6, 3, 7,
	};

	static const uint16_t s_cubeTriStrip[] =
	{
		0, 1, 2,
		3,
		7,
		1,
		5,
		0,
		4,
		2,
		6,
		7,
		4,
		5,
	};

	static const uint16_t s_cubeLineList[] =
	{
		0, 1,
		0, 2,
		0, 4,
		1, 3,
		1, 5,
		2, 3,
		2, 6,
		3, 7,
		4, 5,
		4, 6,
		5, 7,
		6, 7,
	};

	static const uint16_t s_cubeLineStrip[] =
	{
		0, 2, 3, 1, 5, 7, 6, 4,
		0, 2, 6, 4, 5, 7, 3, 1,
		0,
	};

	static const uint16_t s_cubePoints[] =
	{
		0, 1, 2, 3, 4, 5, 6, 7
	};

	static const char* s_ptNames[]
	{
		"Triangle List",
		"Triangle Strip",
		"Lines",
		"Line Strip",
		"Points",
	};

	static const uint64_t s_ptState[]
	{
		UINT64_C(0),
		BGFX_STATE_PT_TRISTRIP,
		BGFX_STATE_PT_LINES,
		BGFX_STATE_PT_LINESTRIP,
		BGFX_STATE_PT_POINTS,
	};
	BX_STATIC_ASSERT(BX_COUNTOF(s_ptState) == BX_COUNTOF(s_ptNames));


namespace compilHelper
{
	static bx::FileReaderI* s_fileReader = NULL;

	static const bgfx::Memory* loadMem(bx::FileReaderI* _reader, const char* _filePath)
	{
		if (bx::open(_reader, _filePath))
		{
			uint32_t size = (uint32_t)bx::getSize(_reader);
			const bgfx::Memory* mem = bgfx::alloc(size + 1);
			bx::read(_reader, mem->data, size);
			bx::close(_reader);
			mem->data[mem->size - 1] = '\0';
			return mem;
		}
		return NULL;
	}

	bgfx::ShaderHandle loadShader(bx::FileReaderI* _reader, const char* _name)
	{
		bgfx::ShaderHandle handle = bgfx::createShader(loadMem(_reader, _name));
		bgfx::setName(handle, _name);

		return handle;
	}

	bgfx::ShaderHandle loadShader(const char* _name)
	{
		return loadShader(s_fileReader, _name);
	}

	bgfx::ProgramHandle loadProgram(bx::FileReaderI* _reader, const char* _vsName, const char* _fsName)
	{
		bgfx::ShaderHandle vsh = loadShader(_reader, _vsName);
		bgfx::ShaderHandle fsh = BGFX_INVALID_HANDLE;
		if (NULL != _fsName)
		{
			fsh = loadShader(_reader, _fsName);
		}

		return bgfx::createProgram(vsh, fsh, true);
	}

	bgfx::ProgramHandle loadProgram(const char* _vsName, const char* _fsName)
	{
		if (!s_fileReader)
			s_fileReader = new bx::FileReader();
		return loadProgram(s_fileReader, _vsName, _fsName);
	}

	// TODO : better interface using options
	//shaderc::Options options;
	//std::string code;
	//bx::MemoryBlock* mem = new bx::MemoryBlock(entry::getAllocator());
	//bx::MemoryWriter* lWriter = new bx::MemoryWriter(mem);
	//shaderc::compileShader(options, 1, code, lWriter);
	bool shaderc(const char* pFilename, const char* pOutput, bgfx::RendererType::Enum pType, const char* pDefines = nullptr)
	{

		bool bVertex = !bx::strFind(pFilename, ".vert.sc").isEmpty();
		bool bFragment = !bx::strFind(pFilename, ".frag.sc").isEmpty();
		if (bVertex || bFragment)
		{
			// PATH should be bgfx\examples\runtime
			const char* argv[] =
			{
				"shaderc.exe",
				"-f",
				pFilename,
				"-i",
				"../../src",
				"-o",
				pOutput,
				"--platform",
				pType == bgfx::RendererType::Direct3D11 ? "windows" : "linux",
				"--type",
				bVertex ? "vertex" : "fragment",
				"--profile",

				pType == bgfx::RendererType::Direct3D11
				? bVertex
					? "vs_5_0"
					: "ps_5_0"
				: "120",

				"-O",
				"3",
				"--define",
				pDefines ? pDefines : ""
			};
			int error_code = shaderc::compileShader(BX_COUNTOF(argv), argv);
			return error_code == 0;
		}

		return false;
	}
}

// Compile different version of the uberprogram
const char* shader_defines[] =
{
	"",
	"COLOR=1",
	"COLOR=2",
	"COLOR=2;USE_TEX0"
};


class ExampleEmbedShaderc : public entry::AppI
{
public:
	ExampleEmbedShaderc(const char* _name, const char* _description, const char* _url)
		: entry::AppI(_name, _description, _url)
		, m_pt(0)
		, m_r(true)
		, m_g(true)
		, m_b(true)
		, m_a(true)
	{
	}

	void init(int32_t _argc, const char* const* _argv, uint32_t _width, uint32_t _height) override
	{
		Args args(_argc, _argv);

		m_width  = _width;
		m_height = _height;
		m_debug  = BGFX_DEBUG_NONE;
		m_reset  = BGFX_RESET_VSYNC;

		bgfx::Init init;		
		//init.type     = args.m_type;
		//init.type = bgfx::RendererType::Vulkan;
		//init.type = bgfx::RendererType::OpenGL;
		init.type = bgfx::RendererType::Direct3D11;
		init.vendorId = args.m_pciId;
		init.resolution.width  = m_width;
		init.resolution.height = m_height;
		init.resolution.reset  = m_reset;
		bgfx::init(init);

		for (int i = 0; i < BX_COUNTOF(shader_defines); ++i)
		{
			// PATH should be bgfx\examples\runtime
			uint32_t hashCode = bx::hash<bx::HashMurmur2A>(shader_defines[i]);
			char vert_bin[256];
			char frag_bin[256];
			bx::snprintf(vert_bin, BX_COUNTOF(vert_bin), "shader_cache/uberprogram.%u.vert.bin", hashCode);
			bx::snprintf(frag_bin, BX_COUNTOF(frag_bin), "shader_cache/uberprogram.%u.frag.bin", hashCode);
			bool bVert = compilHelper::shaderc("../00-embed-shaderc/uberprogram.vert.sc", vert_bin, init.type, shader_defines[i]);
			bool bFrag = compilHelper::shaderc("../00-embed-shaderc/uberprogram.frag.sc", frag_bin, init.type, shader_defines[i]);
			if (bVert && bFrag)
			{
				m_program[i] = compilHelper::loadProgram(vert_bin, frag_bin);
			}
			else
			{
				m_program[i] = BGFX_INVALID_HANDLE;
				assert(!"Compilation failed\n");
			}
		}

		// Enable debug text.
		bgfx::setDebug(m_debug);

		// Set view 0 clear state.
		bgfx::setViewClear(0
			, BGFX_CLEAR_COLOR|BGFX_CLEAR_DEPTH
			, 0x303030ff
			, 1.0f
			, 0
			);

		// Create vertex stream declaration.
		PosColorVertex::init();

		// Create static vertex buffer.
		m_vbh = bgfx::createVertexBuffer(
			// Static data can be passed with bgfx::makeRef
			  bgfx::makeRef(s_cubeVertices, sizeof(s_cubeVertices) )
			, PosColorVertex::ms_layout
			);

		// Create static index buffer for triangle list rendering.
		m_ibh[0] = bgfx::createIndexBuffer(
			// Static data can be passed with bgfx::makeRef
			bgfx::makeRef(s_cubeTriList, sizeof(s_cubeTriList) )
			);

		// Create static index buffer for triangle strip rendering.
		m_ibh[1] = bgfx::createIndexBuffer(
			// Static data can be passed with bgfx::makeRef
			bgfx::makeRef(s_cubeTriStrip, sizeof(s_cubeTriStrip) )
			);

		// Create static index buffer for line list rendering.
		m_ibh[2] = bgfx::createIndexBuffer(
			// Static data can be passed with bgfx::makeRef
			bgfx::makeRef(s_cubeLineList, sizeof(s_cubeLineList) )
			);

		// Create static index buffer for line strip rendering.
		m_ibh[3] = bgfx::createIndexBuffer(
			// Static data can be passed with bgfx::makeRef
			bgfx::makeRef(s_cubeLineStrip, sizeof(s_cubeLineStrip) )
			);

		// Create static index buffer for point list rendering.
		m_ibh[4] = bgfx::createIndexBuffer(
			// Static data can be passed with bgfx::makeRef
			bgfx::makeRef(s_cubePoints, sizeof(s_cubePoints) )
			);



		m_timeOffset = bx::getHPCounter();

		imguiCreate();
	}

	virtual int shutdown() override
	{
		imguiDestroy();

		// Cleanup.
		for (uint32_t ii = 0; ii < BX_COUNTOF(m_ibh); ++ii)
		{
			bgfx::destroy(m_ibh[ii]);
		}

		bgfx::destroy(m_vbh);
		for(int i = 0; i < BX_COUNTOF(m_program); ++i)
			bgfx::destroy(m_program[i]);

		// Shutdown bgfx.
		bgfx::shutdown();

		return 0;
	}

	bool update() override
	{
		if (!entry::processEvents(m_width, m_height, m_debug, m_reset, &m_mouseState) )
		{
			imguiBeginFrame(m_mouseState.m_mx
				,  m_mouseState.m_my
				, (m_mouseState.m_buttons[entry::MouseButton::Left  ] ? IMGUI_MBUT_LEFT   : 0)
				| (m_mouseState.m_buttons[entry::MouseButton::Right ] ? IMGUI_MBUT_RIGHT  : 0)
				| (m_mouseState.m_buttons[entry::MouseButton::Middle] ? IMGUI_MBUT_MIDDLE : 0)
				,  m_mouseState.m_mz
				, uint16_t(m_width)
				, uint16_t(m_height)
				);

			showExampleDialog(this);

			ImGui::SetNextWindowPos(
				  ImVec2(m_width - m_width / 5.0f - 10.0f, 10.0f)
				, ImGuiCond_FirstUseEver
				);
			ImGui::SetNextWindowSize(
				  ImVec2(m_width / 5.0f, m_height / 3.5f)
				, ImGuiCond_FirstUseEver
				);
			ImGui::Begin("Settings"
				, NULL
				, 0
				);

			ImGui::Checkbox("Write R", &m_r);
			ImGui::Checkbox("Write G", &m_g);
			ImGui::Checkbox("Write B", &m_b);
			ImGui::Checkbox("Write A", &m_a);

			ImGui::Text("Primitive topology:");
			ImGui::Combo("", (int*)&m_pt, s_ptNames, BX_COUNTOF(s_ptNames) );

			ImGui::End();

			imguiEndFrame();

			float time = (float)( (bx::getHPCounter()-m_timeOffset)/double(bx::getHPFrequency() ) );

			const bx::Vec3 at  = { 0.0f, 0.0f,   0.0f };
			const bx::Vec3 eye = { 0.0f, 0.0f, -35.0f };

			// Set view and projection matrix for view 0.
			{
				float view[16];
				bx::mtxLookAt(view, eye, at);

				float proj[16];
				bx::mtxProj(proj, 60.0f, float(m_width)/float(m_height), 0.1f, 100.0f, bgfx::getCaps()->homogeneousDepth);
				bgfx::setViewTransform(0, view, proj);

				// Set view 0 default viewport.
				bgfx::setViewRect(0, 0, 0, uint16_t(m_width), uint16_t(m_height) );
			}

			// This dummy draw call is here to make sure that view 0 is cleared
			// if no other draw calls are submitted to view 0.
			bgfx::touch(0);

			bgfx::IndexBufferHandle ibh = m_ibh[m_pt];
			uint64_t state = 0
				| (m_r ? BGFX_STATE_WRITE_R : 0)
				| (m_g ? BGFX_STATE_WRITE_G : 0)
				| (m_b ? BGFX_STATE_WRITE_B : 0)
				| (m_a ? BGFX_STATE_WRITE_A : 0)
				| BGFX_STATE_WRITE_Z
				| BGFX_STATE_DEPTH_TEST_LESS
				| BGFX_STATE_CULL_CW
				| BGFX_STATE_MSAA
				| s_ptState[m_pt]
				;

			// Submit 11x11 cubes.
			uint32_t yy = 0;
			for (uint32_t xx = 0; xx < BX_COUNTOF(m_program); ++xx)
			{
				if (bgfx::isValid(m_program[xx]))
				{
					float mtx[16];
					bx::mtxRotateXY(mtx, time + xx * 0.21f, time + yy * 0.37f);
					mtx[12] = -15.0f + float(xx) * 3.0f;
					mtx[13] = -15.0f + float(yy) * 3.0f;
					mtx[14] = 0.0f;

					// Set model matrix for rendering.
					bgfx::setTransform(mtx);

					// Set vertex and index buffer.
					bgfx::setVertexBuffer(0, m_vbh);
					bgfx::setIndexBuffer(ibh);

					// Set render states.
					bgfx::setState(state);

					// Submit primitive for rendering to view 0.
					bgfx::submit(0, m_program[xx]);
				}
			}

			// Advance to next frame. Rendering thread will be kicked to
			// process submitted rendering primitives.
			bgfx::frame();

			return true;
		}

		return false;
	}

	entry::MouseState m_mouseState;

	uint32_t m_width;
	uint32_t m_height;
	uint32_t m_debug;
	uint32_t m_reset;
	bgfx::VertexBufferHandle m_vbh;
	bgfx::IndexBufferHandle m_ibh[BX_COUNTOF(s_ptState)];
	bgfx::ProgramHandle m_program[BX_COUNTOF(shader_defines)];
	int64_t m_timeOffset;
	int32_t m_pt;

	bool m_r;
	bool m_g;
	bool m_b;
	bool m_a;

};

} // namespace

ENTRY_IMPLEMENT_MAIN(
	  ExampleEmbedShaderc
	, "00-embed-shaderc"
	, "Runtime shader compilation."
	, ""
	);
