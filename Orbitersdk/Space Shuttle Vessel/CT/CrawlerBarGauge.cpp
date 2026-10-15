/******* SSV File Modification Notice *******
Date         Developer
2020/06/14   GLS
2020/06/20   GLS
2020/08/24   GLS
2021/08/23   GLS
2021/08/24   GLS
2022/08/05   GLS
2022/09/29   GLS
2026/10/15   racerx2
********************************************/
#include "CrawlerBarGauge.h"
#include <DrawAPI.h>
#include <MathSSV.h>

namespace vc
{

CrawlerBarGauge::CrawlerBarGauge(Crawler* _v, const std::string& _ident, bool IsVertical)
: BasicVCComponent(_v, _ident), bVertical(IsVertical), fOldValue(-2.0), fScale(1.0)
{
}

CrawlerBarGauge::~CrawlerBarGauge()
{
}

void CrawlerBarGauge::OnPreStep(double simt, double simdt, double mjd)
{
	if(!Eq(fOldValue, input.GetVoltage(), 0.01/fScale)) {
		TriggerRedraw();
		fOldValue = input.GetVoltage();
	}
}

bool CrawlerBarGauge::OnVCRedrawEvent(int id, int _event, SURFHANDLE surf)
{
	int midPos;
	RECT litRect;
	if(!bVertical) {
		double barSize = (usWidth*input.GetVoltage()*fScale)/abs(max-min);
		int segmentCount = static_cast<int>(barSize)/usBarWidth; // get integer number of segments
		//midPos = static_cast<int>(usWidth*((1.0+max+min)/2.0));
		midPos = static_cast<int>(usWidth*(-min/(max-min)));

		litRect.top = redrawBase.y;
		litRect.bottom = redrawBase.y+usHeight;
		if(input.GetVoltage() < 0.0) {
			litRect.left = redrawBase.x + std::max<int>(midPos + segmentCount*usBarWidth, 0);// not upstream: windows.h max macro (the class has min/max members)
			litRect.right = redrawBase.x + midPos;
		}
		else {
			litRect.left = redrawBase.x + midPos;
			litRect.right = redrawBase.x + std::min<int>(midPos + segmentCount*usBarWidth, usWidth);// not upstream: windows.h min macro
		}
	}
	else {
		double barSize = (usHeight*input.GetVoltage()*fScale)/abs(max-min);
		int segmentCount = static_cast<int>(barSize)/usBarWidth; // get integer number of segments
		//midPos = static_cast<int>(usHeight*((1.0+max+min)/2.0));
		midPos = static_cast<int>(usHeight*(1.0 + min/(max-min)));

		litRect.left = redrawBase.x;
		litRect.right = redrawBase.x+usWidth;
		if(input.GetVoltage() < 0.0) {
			// TODO: make sure this works for -ve values (we don't need it yet)
			litRect.bottom = redrawBase.y + std::max<int>(midPos - segmentCount*usBarWidth, 0);// not upstream: windows.h max macro
			litRect.top = redrawBase.y + midPos;
		}
		else {
			litRect.bottom = redrawBase.y + midPos;
			litRect.top = redrawBase.y + std::min<int>(midPos - segmentCount*usBarWidth, usHeight);// not upstream: windows.h min macro
		}
	}
	RECT gaugeRect;
	gaugeRect.top = redrawBase.y;
	gaugeRect.bottom = redrawBase.y+usHeight;
	gaugeRect.left = redrawBase.x;
	gaugeRect.right = redrawBase.x+usWidth;
	// draw gauge
#ifdef _WIN32
	HDC hDC = oapiGetDC(surf);
	HBRUSH hUnlitBrush = CreateSolidBrush(RGB(70, 76, 88));
	FillRect(hDC, &gaugeRect, hUnlitBrush);
	HBRUSH hLitBrush = CreateSolidBrush(RGB(144, 77, 101));
	FillRect(hDC, &litRect, hLitBrush);

	// draw lines
	HBRUSH hLineBrush = CreateSolidBrush(RGB(0, 0, 0));
	HGDIOBJ oldBrush = SelectObject(hDC, hLineBrush);
	int maxPos;
	if(bVertical) maxPos = usHeight;
	else maxPos = usWidth;
	for(int pos=midPos;pos < maxPos;pos+=usBarWidth) {
		if(bVertical) {
			MoveToEx(hDC, redrawBase.x, redrawBase.y+pos, NULL);
			LineTo(hDC, redrawBase.x+usWidth, redrawBase.y+pos);
		}
		else {
			MoveToEx(hDC, redrawBase.x+pos, redrawBase.y, NULL);
			LineTo(hDC, redrawBase.x+pos, redrawBase.y+usHeight);
		}
	}
	for(int pos=midPos-usBarWidth;pos >= 0;pos-=usBarWidth) {
		if(bVertical) {
			MoveToEx(hDC, redrawBase.x, redrawBase.y+pos, NULL);
			LineTo(hDC, redrawBase.x+usWidth, redrawBase.y+pos);
		}
		else {
			MoveToEx(hDC, redrawBase.x+pos, redrawBase.y, NULL);
			LineTo(hDC, redrawBase.x+pos, redrawBase.y+usHeight);
		}
	}
	SelectObject(hDC, oldBrush);

	oapiReleaseDC(surf, hDC);
	DeleteObject(hUnlitBrush);
	DeleteObject(hLitBrush);
	DeleteObject(hLineBrush);
#else
	// not upstream: the same drawing with Sketchpad (no GDI on Linux)
	oapi::Sketchpad* skp = oapiGetSketchpad(surf);
	if (!skp) return false;
	// FillRect: the right and bottom edges excluded, nothing for an empty rectangle; Rectangle without a pen fills to r-1, b-1
	auto FillRect = [skp](const RECT& r, oapi::Brush* br) { if (r.right > r.left && r.bottom > r.top) { skp->SetBrush(br); skp->Rectangle(r.left, r.top, r.right + 1, r.bottom + 1); } };
	oapi::Brush* hUnlitBrush = oapiCreateBrush(RGB(70, 76, 88));
	oapi::Brush* hLitBrush = oapiCreateBrush(RGB(144, 77, 101));
	oapi::Pen* hLinePen = oapiCreatePen(1, 1, RGB(0, 0, 0));// the DC's default black pen draws the lines
	skp->SetPen(NULL);
	FillRect(gaugeRect, hUnlitBrush);
	FillRect(litRect, hLitBrush);
	skp->SetBrush(NULL);
	skp->SetPen(hLinePen);
	int maxPos;
	if(bVertical) maxPos = usHeight;
	else maxPos = usWidth;
	for(int pos=midPos;pos < maxPos;pos+=usBarWidth) {
		if(bVertical) skp->Line(redrawBase.x, redrawBase.y+pos, redrawBase.x+usWidth, redrawBase.y+pos);
		else skp->Line(redrawBase.x+pos, redrawBase.y, redrawBase.x+pos, redrawBase.y+usHeight);
	}
	for(int pos=midPos-usBarWidth;pos >= 0;pos-=usBarWidth) {
		if(bVertical) skp->Line(redrawBase.x, redrawBase.y+pos, redrawBase.x+usWidth, redrawBase.y+pos);
		else skp->Line(redrawBase.x+pos, redrawBase.y, redrawBase.x+pos, redrawBase.y+usHeight);
	}
	oapiReleaseSketchpad(skp);
	oapiReleaseBrush(hUnlitBrush);
	oapiReleaseBrush(hLitBrush);
	oapiReleasePen(hLinePen);
#endif

	return true;
}

void CrawlerBarGauge::SetScaleValue(float _fScale)
{
	fScale = _fScale;
}

void CrawlerBarGauge::SetRange(double _min, double _max)
{
	max = _max;
	min = _min;
}

void CrawlerBarGauge::SetDimensions(unsigned short _usWidth, unsigned short _usHeight, unsigned short _usBarCount)
{
	usWidth = _usWidth;
	usHeight = _usHeight;
	if(bVertical) usBarWidth = _usHeight/_usBarCount;
	else usBarWidth = _usWidth/_usBarCount;
}

void CrawlerBarGauge::SetBaseCoords(unsigned short usX, unsigned short usY)
{
	redrawBase.x = usX;
	redrawBase.y = usY;
}

void CrawlerBarGauge::ConnectPort(discsignals::DiscreteBundle *pBundle, unsigned short usLine)
{
	input.Connect(pBundle, usLine);
}

}
