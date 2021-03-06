//========= Copyright � 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

// Triangle rendering, if any

#include "hud.h"
#include "cl_util.h"

// Triangle rendering apis are in gEngfuncs.pTriAPI

#include "const.h"
#include "entity_state.h"
#include "cl_entity.h"
#include "triangleapi.h"
#include "Exports.h"

#include "particleman.h"
#include "tri.h"
extern IParticleMan *g_pParticleMan;

/*
	CRASH FORT:
*/
#include <string>
#include "HLCam Client\Client.hpp"
#include "pm_defs.h"

namespace
{
	HSPRITE WhiteSprite = 0;
	model_s* WhiteSpriteModel;
}

namespace Tri
{
	namespace Commands
	{
		cvar_t* RenderCameraText;
		cvar_t* RenderPlayerPing;
		cvar_t* RenderPlayerPingWhenHidden;

		cvar_t* RenderEnemyPingOnAim;
	}

	void Init()
	{
		auto regvar = gEngfuncs.pfnRegisterVariable;
		Commands::RenderCameraText = regvar("hlcam_render_cameratext", "0", FCVAR_ARCHIVE);
		Commands::RenderPlayerPing = regvar("hlcam_render_playerping", "0", FCVAR_ARCHIVE);
		Commands::RenderPlayerPingWhenHidden = regvar("hlcam_render_playerping_hidden", "0", FCVAR_ARCHIVE);

		Commands::RenderEnemyPingOnAim = regvar("hlcam_render_enemyping", "0", FCVAR_ARCHIVE);
	}

	void VidInit()
	{
		WhiteSprite = gEngfuncs.pfnSPR_Load("sprites\\white.spr");
		WhiteSpriteModel = const_cast<model_s*>(gEngfuncs.GetSpritePointer(WhiteSprite));
	}
}

namespace
{
	/*
		    3---7
		   /|  /|
		  / | / |
		 2---6  |
		 |  1|--5
		 | / | /
		 |/  |/
		 0---4

		 From Source Engine 2007
	*/
	void PointsFromBox(const Vector& mins, const Vector& maxs, Vector* points)
	{
		points[0][0] = mins[0];
		points[0][1] = mins[1];
		points[0][2] = mins[2];

		points[1][0] = mins[0];
		points[1][1] = mins[1];
		points[1][2] = maxs[2];

		points[2][0] = mins[0];
		points[2][1] = maxs[1];
		points[2][2] = mins[2];

		points[3][0] = mins[0];
		points[3][1] = maxs[1];
		points[3][2] = maxs[2];

		points[4][0] = maxs[0];
		points[4][1] = mins[1];
		points[4][2] = mins[2];

		points[5][0] = maxs[0];
		points[5][1] = mins[1];
		points[5][2] = maxs[2];

		points[6][0] = maxs[0];
		points[6][1] = maxs[1];
		points[6][2] = mins[2];

		points[7][0] = maxs[0];
		points[7][1] = maxs[1];
		points[7][2] = maxs[2];
	}

	void DrawWireframeBox(const Vector& posmax, const Vector& posmin)
	{
		Vector points[8];
		PointsFromBox(posmin, posmax, points);

		auto vertexfunc = gEngfuncs.pTriAPI->Vertex3fv;

		gEngfuncs.pTriAPI->Begin(TRI_LINES);

		/*
			Bottom
		*/
		vertexfunc(points[0]);
		vertexfunc(points[1]);

		vertexfunc(points[1]);
		vertexfunc(points[5]);

		vertexfunc(points[5]);
		vertexfunc(points[4]);

		vertexfunc(points[4]);
		vertexfunc(points[0]);

		/*
			Top
		*/
		vertexfunc(points[2]);
		vertexfunc(points[3]);

		vertexfunc(points[3]);
		vertexfunc(points[7]);

		vertexfunc(points[7]);
		vertexfunc(points[6]);

		vertexfunc(points[6]);
		vertexfunc(points[2]);

		/*
			Bars between
		*/
		vertexfunc(points[0]);
		vertexfunc(points[2]);

		vertexfunc(points[1]);
		vertexfunc(points[3]);

		vertexfunc(points[5]);
		vertexfunc(points[7]);

		vertexfunc(points[4]);
		vertexfunc(points[6]);

		gEngfuncs.pTriAPI->End();
	}
	
	void DrawBox(const Vector& posmax, const Vector& posmin)
	{
		Vector points[8];
		PointsFromBox(posmin, posmax, points);

		auto vertexfunc = gEngfuncs.pTriAPI->Vertex3fv;

		gEngfuncs.pTriAPI->Begin(TRI_QUADS);

		/*
			Bottom
		*/
		vertexfunc(points[0]);
		vertexfunc(points[4]);
		vertexfunc(points[5]);
		vertexfunc(points[1]);

		/*
			Top
		*/
		vertexfunc(points[2]);
		vertexfunc(points[6]);
		vertexfunc(points[7]);
		vertexfunc(points[3]);

		/*
			Left
		*/
		vertexfunc(points[0]);
		vertexfunc(points[1]);
		vertexfunc(points[3]);
		vertexfunc(points[2]);

		/*
			Right
		*/
		vertexfunc(points[4]);
		vertexfunc(points[5]);
		vertexfunc(points[7]);
		vertexfunc(points[6]);

		/*
			Front
		*/
		vertexfunc(points[0]);
		vertexfunc(points[4]);
		vertexfunc(points[6]);
		vertexfunc(points[2]);

		/*
			Back
		*/
		vertexfunc(points[1]);
		vertexfunc(points[5]);
		vertexfunc(points[7]);
		vertexfunc(points[3]);

		gEngfuncs.pTriAPI->End();
	}

	void DrawLine(const Vector& pos1, const Vector& pos2)
	{
		gEngfuncs.pTriAPI->Begin(TRI_LINES);
		gEngfuncs.pTriAPI->Vertex3f(pos1.x, pos1.y, pos1.z);
		gEngfuncs.pTriAPI->Vertex3f(pos2.x, pos2.y, pos2.z);
		gEngfuncs.pTriAPI->End();
	}

	inline Vector VectorFromArray(const float* ptr)
	{
		return {ptr[0], ptr[1], ptr[2]};
	}

	inline void VectorShift(Vector& vec, float amount)
	{
		vec.x += amount;
		vec.y += amount;
		vec.z += amount;
	}
}

/*
=================
HUD_DrawNormalTriangles

Non-transparent triangles-- add them here
=================
*/
void CL_DLLEXPORT HUD_DrawNormalTriangles( void )
{
//	RecClDrawNormalTriangles();

	gHUD.m_Spectator.DrawOverview();
}

#if defined( _TFC )
void RunEventList( void );
#endif

/*
=================
HUD_DrawTransparentTriangles

Render any triangles with transparent rendermode needs here
=================
*/
void CL_DLLEXPORT HUD_DrawTransparentTriangles( void )
{
//	RecClDrawTransparentTriangles();

#if defined( _TFC )
	RunEventList();
#endif

	if ( g_pParticleMan )
		 g_pParticleMan->Update();

	if (Cam::InEditMode())
	{
		const auto& triggers = Cam::GetAllTriggers();
		const auto& cameras = Cam::GetAllCameras();

		gEngfuncs.pTriAPI->SpriteTexture(WhiteSpriteModel, 0);

		for (const auto& trigitr : triggers)
		{
			const auto& trig = trigitr.second;

			auto corner1 = VectorFromArray(trig.Corner1);
			auto corner2 = VectorFromArray(trig.Corner2);

			gEngfuncs.pTriAPI->CullFace(TRICULLSTYLE::TRI_NONE);

			gEngfuncs.pTriAPI->RenderMode(kRenderTransAlpha);

			bool defaultcolor = true;

			if (trig.Highlighted)
			{
				gEngfuncs.pTriAPI->Color4f(1, 1, 0, 0.6);
				defaultcolor = false;
			}

			if (trig.Selected)
			{
				gEngfuncs.pTriAPI->Color4f(1, 0, 0, 0.6);
				defaultcolor = false;
			}

			if (defaultcolor)
			{
				gEngfuncs.pTriAPI->Color4f(0, 1, 0, 0.2);
			}
			
			DrawBox(corner1, corner2);

			gEngfuncs.pTriAPI->RenderMode(kRenderNormal);

			if (trig.Selected && trig.Highlighted)
			{
				gEngfuncs.pTriAPI->Color4f(1, 1, 0, 1);
			}

			else if (trig.Selected)
			{
				gEngfuncs.pTriAPI->Color4f(1, 0, 0, 1);
			}

			else
			{
				gEngfuncs.pTriAPI->Color4f(0, 1, 0, 1);
			}
			
			DrawWireframeBox(corner1, corner2);
		}

		gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
		gEngfuncs.pTriAPI->Color4f(1, 0, 0, 1);

		for (const auto& camitr : cameras)
		{
			const auto& cam = camitr.second;

			/*
				Don't render cams that are in the primary view.
			*/
			if (cam.InPreview)
			{
				continue;
			}

			auto pos = VectorFromArray(cam.Position);		
			auto angles = VectorFromArray(cam.Angle);

			Vector minpos = pos;
			Vector maxpos = pos;

			const auto camboxsize = 4;

			VectorShift(minpos, -camboxsize);
			VectorShift(maxpos, camboxsize);

			/*
				Selection box
			*/
			if (cam.Selected)
			{
				auto minsel = minpos;
				auto maxsel = maxpos;

				for (size_t i = 0; i < 2; i++)
				{
					VectorShift(minsel, -camboxsize);
					VectorShift(maxsel, camboxsize);

					DrawWireframeBox(maxsel, minsel);
				}

				if (cam.Adjusting)
				{
					VectorShift(minsel, -camboxsize);
					VectorShift(maxsel, camboxsize);

					gEngfuncs.pTriAPI->RenderMode(kRenderTransAlpha);
					gEngfuncs.pTriAPI->Color4f(0, 0, 1, 0.6);

					DrawBox(maxsel, minsel);

					gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
					gEngfuncs.pTriAPI->Color4f(1, 0, 0, 1);
				}
			}

			/*
				Starting box
			*/
			DrawBox(maxpos, minpos);
			
			Vector forward;
			gEngfuncs.pfnAngleVectors(angles, forward, nullptr, nullptr);
			VectorScale(forward, 128, forward);
			VectorAdd(forward, pos, forward);
			
			/*
				Guide angle line
			*/
			DrawLine(pos, forward);

			minpos = forward;
			maxpos = forward;

			VectorShift(minpos, -camboxsize);
			VectorShift(maxpos, camboxsize);

			/*
				End point wire box
			*/
			DrawWireframeBox(maxpos, minpos);
		}
	}
}

/*
=================
HUD_DrawOrthoTriangles
Orthogonal Triangles -- (relative to resolution,
smackdab on the screen) add them here
=================
*/
void HUD_DrawOrthoTriangles()
{
	auto drawping = [](const Vector& position, const Vector& color)
	{
		constexpr auto triwidth = 24;
		constexpr auto triheight = 12;

		auto point1 = position;
		auto point2 = position;
		auto point3 = position;
		point1.x -= triwidth;
		point2.y += triheight;
		point3.x += triwidth;

		gEngfuncs.pTriAPI->SpriteTexture(WhiteSpriteModel, 0);
		gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
		gEngfuncs.pTriAPI->Color4ub(color.x, color.y, color.z, 0);

		gEngfuncs.pTriAPI->Begin(TRI_TRIANGLES);

		auto vertexfunc = gEngfuncs.pTriAPI->Vertex3fv;
		vertexfunc(point1);
		vertexfunc(point2);
		vertexfunc(point3);

		gEngfuncs.pTriAPI->End();
	};

	if (Cam::InEditMode() && Tri::Commands::RenderCameraText->value > 0)
	{
		const auto& cameras = Cam::GetAllCameras();

		for (auto& camitr : cameras)
		{
			auto& cam = camitr.second;

			if (cam.InPreview)
			{
				continue;
			}

			auto pos = VectorFromArray(cam.Position);
			Vector screen;

			if (!gEngfuncs.pTriAPI->WorldToScreen(pos, screen))
			{
				screen.x = XPROJECT(screen[0]);
				screen.y = YPROJECT(screen[1]);
				screen.z = 0.0f;

				std::string endstr = "Camera_";
				endstr += std::to_string(cam.ID);

				if (cam.IsNamed)
				{
					endstr += " (";
					endstr += cam.Name;
					endstr += ")";
				}

				gEngfuncs.pfnDrawString(screen.x, screen.y, endstr.c_str(), 255, 255, 255);
			}
		}
	}

	else if (!Cam::InEditMode() && Tri::Commands::RenderPlayerPing->value > 0)
	{
		auto player = gEngfuncs.GetLocalPlayer();
		auto renderpos = player->origin;
		renderpos.z += 56;

		bool shoulddraw = true;

		if (Tri::Commands::RenderPlayerPingWhenHidden->value > 0)
		{
			Vector campos;
			Cam::GetActiveCameraPosition(campos);

			/*
				28 is what the server uses for client "eye" view offset.
			*/
			auto testpos = renderpos;
			testpos.z -= 28;

			auto trace = gEngfuncs.PM_TraceLine(campos, testpos, PM_TRACELINE_PHYSENTSONLY, 2, -1);

			shoulddraw = trace->fraction != 1;
		}

		if (shoulddraw)
		{
			Vector screen;

			if (!gEngfuncs.pTriAPI->WorldToScreen(renderpos, screen))
			{
				screen.x = XPROJECT(screen[0]);
				screen.y = YPROJECT(screen[1]);
				screen.z = 0.0f;

				int r;
				int g;
				int b;
				UnpackRGB(r, g, b, RGB_YELLOWISH);

				drawping(screen, {r, g, b});
			}
		}
	}

	if (!Cam::InEditMode() && Tri::Commands::RenderEnemyPingOnAim->value > 0)
	{
		const auto& pingdata = Cam::GetEnemyPingData();

		if (pingdata.Active)
		{
			auto endent = pingdata.EntityPtr;

			if (endent)
			{
				auto renderpos = endent->origin;
				renderpos.z = pingdata.ZOffset;

				Vector screen;

				if (!gEngfuncs.pTriAPI->WorldToScreen(renderpos, screen))
				{
					screen.x = XPROJECT(screen[0]);
					screen.y = YPROJECT(screen[1]);
					screen.z = 0.0f;

					int r;
					int g;
					int b;
					UnpackRGB(r, g, b, RGB_REDISH);

					drawping(screen, {r, g, b});
				}
			}
		}
	}
}
