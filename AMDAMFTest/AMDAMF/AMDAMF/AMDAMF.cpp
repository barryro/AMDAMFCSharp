// 
// Notice Regarding Standards.  AMD does not provide a license or sublicense to
// any Intellectual Property Rights relating to any standards, including but not
// limited to any audio and/or video codec technologies such as MPEG-2, MPEG-4;
// AVC/H.264; HEVC/H.265; AAC decode/FFMPEG; AAC encode/FFMPEG; VC-1; and MP3
// (collectively, the "Media Technologies"). For clarity, you will pay any
// royalties due for such third party technologies, which may include the Media
// Technologies that are owed as a result of AMD providing the Software to you.
// 
// MIT license 
// 
//
// Copyright (c) 2016 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

// this sample encodes NV12 frames using AMF Encoder and writes them to H.264 elmentary stream 
//#include "stdafx.h"
/*#include <Psapi.h>
#pragma comment(lib, "Psapi.lib")*/

#include <stdio.h>
#include <tchar.h>
#include <d3d9.h>
#include <d3d11.h>
#include "../common/AMFFactory.h"
#include "../include/components/VideoEncoderVCE.h"
#include "../include/components/VideoEncoderHEVC.h"
#include "../common/Thread.h"
#include "../common/AMFSTL.h"
#include "../common/TraceAdapter.h"

#define AMF_FACILITY L"SimpleEncoder"


static wchar_t *pCodec = AMFVideoEncoderVCE_AVC;
//static wchar_t *pCodec = AMFVideoEncoder_HEVC;

//#define ENABLE_4K

static amf::AMF_MEMORY_TYPE memoryTypeIn = amf::AMF_MEMORY_DX9;
static amf::AMF_SURFACE_FORMAT formatIn = amf::AMF_SURFACE_NV12;

#if defined(ENABLE_4K)
static amf_int32 widthIn = 1920 * 2;
static amf_int32 heightIn = 1080 * 2;
#else
static amf_int32 widthIn = 1920;
static amf_int32 heightIn = 1080;
#endif
static amf_int32 frameRateIn = 30;
static amf_int64 bitRateIn = 5000000L; // in bits, 5MBit
static amf_int32 rectSize = 50;
static amf_int32 frameCount = 500;
static bool bMaximumSpeed = true;

#define START_TIME_PROPERTY L"StartTimeProperty" // custom property ID to store submission time in a frame - all custom properties are copied from input to output

static wchar_t *fileNameOut = L"./output.h264";

static amf_int32 xPos = 0;
static amf_int32 yPos = 0;
static void FillSurface(amf::AMFContext *context, amf::AMFSurface *surface);

#define MILLISEC_TIME     10000

class PollingThread : public amf::AMFThread
{
protected:
	amf::AMFContextPtr      m_pContext;
	amf::AMFComponentPtr    m_pEncoder;
	FILE                    *m_pFile;
public:
	PollingThread(amf::AMFContext *context, amf::AMFComponent *encoder, const wchar_t *pFileName);
	~PollingThread();
	virtual void Run();
};
int _tmain(int argc, _TCHAR* argv[])
{
	AMF_RESULT res = AMF_OK; // error checking can be added later
	res = g_AMFFactory.Init();
	if (res != AMF_OK)
	{
		wprintf(L"AMF Failed to initialize");
		return 1;
	}

	::amf_increase_timer_precision();

	amf::AMFTraceEnableWriter(AMF_TRACE_WRITER_CONSOLE, true);
	amf::AMFTraceEnableWriter(AMF_TRACE_WRITER_DEBUG_OUTPUT, true);


	// initialize AMF
	amf::AMFContextPtr context;
	amf::AMFComponentPtr encoder;
	amf::AMFSurfacePtr surfaceIn;

	// context
	res = g_AMFFactory.GetFactory()->CreateContext(&context);
	AMF_RETURN_IF_FAILED(res, L"CreateContext() failed");

	if (memoryTypeIn == amf::AMF_MEMORY_DX9)
	{
		res = context->InitDX9(NULL); // can be DX9 or DX9Ex device
		AMF_RETURN_IF_FAILED(res, L"InitDX9(NULL) failed");
	}
	if (memoryTypeIn == amf::AMF_MEMORY_DX11)
	{
		res = context->InitDX11(NULL); // can be DX11 device
		AMF_RETURN_IF_FAILED(res, L"InitDX11(NULL) failed");
	}
	// component: encoder
	res = g_AMFFactory.GetFactory()->CreateComponent(context, pCodec, &encoder);
	AMF_RETURN_IF_FAILED(res, L"CreateComponent(%s) failed", pCodec);

	if (amf_wstring(pCodec) == amf_wstring(AMFVideoEncoderVCE_AVC))
	{
		res = encoder->SetProperty(AMF_VIDEO_ENCODER_USAGE, AMF_VIDEO_ENCODER_USAGE_TRANSCONDING);
		AMF_RETURN_IF_FAILED(res, L"SetProperty(AMF_VIDEO_ENCODER_USAGE, AMF_VIDEO_ENCODER_USAGE_TRANSCONDING) failed");

		if (bMaximumSpeed)
		{
			res = encoder->SetProperty(AMF_VIDEO_ENCODER_B_PIC_PATTERN, 0);
			// do not check error for AMF_VIDEO_ENCODER_B_PIC_PATTERN - can be not supported - check Capability Manager sample
			res = encoder->SetProperty(AMF_VIDEO_ENCODER_QUALITY_PRESET, AMF_VIDEO_ENCODER_QUALITY_PRESET_SPEED);
			AMF_RETURN_IF_FAILED(res, L"SetProperty(AMF_VIDEO_ENCODER_QUALITY_PRESET, AMF_VIDEO_ENCODER_QUALITY_PRESET_SPEED) failed");
		}

		res = encoder->SetProperty(AMF_VIDEO_ENCODER_TARGET_BITRATE, bitRateIn);
		AMF_RETURN_IF_FAILED(res, L"SetProperty(AMF_VIDEO_ENCODER_TARGET_BITRATE, %" LPRId64 L") failed", bitRateIn);
		res = encoder->SetProperty(AMF_VIDEO_ENCODER_FRAMESIZE, ::AMFConstructSize(widthIn, heightIn));
		AMF_RETURN_IF_FAILED(res, L"SetProperty(AMF_VIDEO_ENCODER_FRAMESIZE, %dx%d) failed", widthIn, heightIn);
		res = encoder->SetProperty(AMF_VIDEO_ENCODER_FRAMERATE, ::AMFConstructRate(frameRateIn, 1));
		AMF_RETURN_IF_FAILED(res, L"SetProperty(AMF_VIDEO_ENCODER_FRAMERATE, %dx%d) failed", frameRateIn, 1);

#if defined(ENABLE_4K)
		res = encoder->SetProperty(AMF_VIDEO_ENCODER_PROFILE, AMF_VIDEO_ENCODER_PROFILE_HIGH);
		AMF_RETURN_IF_FAILED(res, L"SetProperty(AMF_VIDEO_ENCODER_PROFILE, AMF_VIDEO_ENCODER_PROFILE_HIGH) failed");

		res = encoder->SetProperty(AMF_VIDEO_ENCODER_PROFILE_LEVEL, 51);
		AMF_RETURN_IF_FAILED(res, L"SetProperty(AMF_VIDEO_ENCODER_PROFILE_LEVEL, 51)");
		res = encoder->SetProperty(AMF_VIDEO_ENCODER_B_PIC_PATTERN, 0);
		AMF_RETURN_IF_FAILED(res, L"SetProperty(AMF_VIDEO_ENCODER_B_PIC_PATTERN, 0)");
#endif
	}
	else
	{
		res = encoder->SetProperty(AMF_VIDEO_ENCODER_HEVC_USAGE, AMF_VIDEO_ENCODER_HEVC_USAGE_TRANSCONDING);
		AMF_RETURN_IF_FAILED(res, L"SetProperty(AMF_VIDEO_ENCODER_HEVC_USAGE, AMF_VIDEO_ENCODER_HEVC_USAGE_TRANSCONDING)");

		if (bMaximumSpeed)
		{
			res = encoder->SetProperty(AMF_VIDEO_ENCODER_HEVC_QUALITY_PRESET, AMF_VIDEO_ENCODER_HEVC_QUALITY_PRESET_SPEED);
			AMF_RETURN_IF_FAILED(res, L"SetProperty(AMF_VIDEO_ENCODER_HEVC_QUALITY_PRESET, AMF_VIDEO_ENCODER_HEVC_QUALITY_PRESET_SPEED)");
		}

		res = encoder->SetProperty(AMF_VIDEO_ENCODER_HEVC_TARGET_BITRATE, bitRateIn);
		AMF_RETURN_IF_FAILED(res, L"SetProperty(AMF_VIDEO_ENCODER_HEVC_TARGET_BITRATE, %" LPRId64 L") failed", bitRateIn);
		res = encoder->SetProperty(AMF_VIDEO_ENCODER_HEVC_FRAMESIZE, ::AMFConstructSize(widthIn, heightIn));
		AMF_RETURN_IF_FAILED(res, L"SetProperty(AMF_VIDEO_ENCODER_HEVC_FRAMESIZE, %dx%d) failed", widthIn, heightIn);
		res = encoder->SetProperty(AMF_VIDEO_ENCODER_HEVC_FRAMERATE, ::AMFConstructRate(frameRateIn, 1));
		AMF_RETURN_IF_FAILED(res, L"SetProperty(AMF_VIDEO_ENCODER_HEVC_FRAMERATE, %dx%d) failed", frameRateIn, 1);

#if defined(ENABLE_4K)
		res = encoder->SetProperty(AMF_VIDEO_ENCODER_HEVC_TIER, AMF_VIDEO_ENCODER_HEVC_TIER_HIGH);
		AMF_RETURN_IF_FAILED(res, L"SetProperty(AMF_VIDEO_ENCODER_HEVC_TIER, AMF_VIDEO_ENCODER_HEVC_TIER_HIGH) failed");
		res = encoder->SetProperty(AMF_VIDEO_ENCODER_HEVC_PROFILE_LEVEL, AMF_LEVEL_5_1);
		AMF_RETURN_IF_FAILED(res, L"SetProperty(AMF_VIDEO_ENCODER_HEVC_PROFILE_LEVEL, AMF_LEVEL_5_1) failed");
#endif
	}
	res = encoder->Init(formatIn, widthIn, heightIn);
	AMF_RETURN_IF_FAILED(res, L"encoder->Init() failed");


	PollingThread thread(context, encoder, fileNameOut);
	thread.Start();

	// encode some frames
	amf_int32 submitted = 0;
	while (submitted < frameCount)
	{
		if (surfaceIn == NULL)
		{
			surfaceIn = NULL;
			res = context->AllocSurface(memoryTypeIn, formatIn, widthIn, heightIn, &surfaceIn);
			AMF_RETURN_IF_FAILED(res, L"AllocSurface() failed");
			FillSurface(context, surfaceIn);
		}
		// encode
		amf_pts start_time = amf_high_precision_clock();
		surfaceIn->SetProperty(START_TIME_PROPERTY, start_time);

		res = encoder->SubmitInput(surfaceIn);
		if (res == AMF_INPUT_FULL) // handle full queue
		{
			amf_sleep(1); // input queue is full: wait, poll and submit again
		}
		else
		{
			AMF_RETURN_IF_FAILED(res, L"SubmitInput() failed");

			surfaceIn = NULL;
			submitted++;
		}
	}
	// drain encoder; input queue can be full
	while (true)
	{
		res = encoder->Drain();
		if (res != AMF_INPUT_FULL) // handle full queue
		{
			break;
		}
		amf_sleep(1); // input queue is full: wait and try again
	}
	thread.WaitForStop();

	// cleanup in this order
	surfaceIn = NULL;
	encoder->Terminate();
	encoder = NULL;
	context->Terminate();
	context = NULL; // context is the last

	g_AMFFactory.Terminate();
	return 0;
}
static void FillSurface(amf::AMFContext *context, amf::AMFSurface *surface)
{
	HRESULT hr = S_OK;
	// fill surface with something something useful. We fill with color and color rect
	D3DCOLOR color1 = D3DCOLOR_XYUV(128, 255, 128);
	D3DCOLOR color2 = D3DCOLOR_XYUV(128, 0, 128);
	// get native DX objects
	IDirect3DDevice9 *deviceDX9 = (IDirect3DDevice9 *)context->GetDX9Device(); // no reference counting - do not Release()
	IDirect3DSurface9* surfaceDX9 = (IDirect3DSurface9*)surface->GetPlaneAt(0)->GetNative(); // no reference counting - do not Release()
	hr = deviceDX9->ColorFill(surfaceDX9, NULL, color1);

	if (xPos + rectSize > widthIn)
	{
		xPos = 0;
	}
	if (yPos + rectSize > heightIn)
	{
		yPos = 0;
	}
	RECT rect = { xPos, yPos, xPos + rectSize, yPos + rectSize };
	hr = deviceDX9->ColorFill(surfaceDX9, &rect, color2);

	xPos += 2; //DX9 NV12 surfaces do not accept odd positions - do not use ++
	yPos += 2; //DX9 NV12 surfaces do not accept odd positions - do not use ++
}
PollingThread::PollingThread(amf::AMFContext *context, amf::AMFComponent *encoder, const wchar_t *pFileName) : m_pContext(context), m_pEncoder(encoder), m_pFile(NULL)
{
	m_pFile = _wfopen(pFileName, L"wb");
}
PollingThread::~PollingThread()
{
	if (m_pFile)
	{
		fclose(m_pFile);
	}
}
void PollingThread::Run()
{
	RequestStop();

	amf_pts latency_time = 0;
	amf_pts write_duration = 0;
	amf_pts encode_duration = 0;
	amf_pts last_poll_time = 0;

	AMF_RESULT res = AMF_OK; // error checking can be added later
	while (true)
	{
		amf::AMFDataPtr data;
		res = m_pEncoder->QueryOutput(&data);
		if (res == AMF_EOF)
		{
			break; // Drain complete
		}
		if (data != NULL)
		{
			amf_pts poll_time = amf_high_precision_clock();
			amf_pts start_time = 0;
			data->GetProperty(START_TIME_PROPERTY, &start_time);
			if (start_time < last_poll_time) // remove wait time if submission was faster then encode
			{
				start_time = last_poll_time;
			}
			last_poll_time = poll_time;

			encode_duration += poll_time - start_time;

			if (latency_time == 0)
			{
				latency_time = poll_time - start_time;
			}

			amf::AMFBufferPtr buffer(data); // query for buffer interface
			fwrite(buffer->GetNative(), 1, buffer->GetSize(), m_pFile);

			write_duration += amf_high_precision_clock() - poll_time;
		}
		else
		{
			amf_sleep(1);
		}
	}
	printf("latency           = %.4fms\nencode  per frame = %.4fms\nwrite per frame   = %.4fms\n",
		double(latency_time) / MILLISEC_TIME,
		double(encode_duration) / MILLISEC_TIME / frameCount,
		double(write_duration) / MILLISEC_TIME / frameCount);

	m_pEncoder = NULL;
	m_pContext = NULL;
}

// This is the main DLL file.

//#include "stdafx.h"

#include "AMDAMF.h"

namespace AMDAMF {
	////For C++ dll to C# testing
	//double Arithmetics::Add(double x, double y)
	//{
	//return x + y;
	//}
	//double Arithmetics::Subtract(double a, double b)
	//{
	//return a - b;
	//}
	//double Arithmetics::Multiply(double x, double y)
	//{
	//return x*y;
	//}
	//double Arithmetics::Divide(double a, double b)
	//{
	//if (b == 0)
	//{
	//return -1;
	//}
	//return a / b;
	//}
	void AmdEncoder::TestEncode()
	{
		_tmain(NULL, NULL);
	}
}