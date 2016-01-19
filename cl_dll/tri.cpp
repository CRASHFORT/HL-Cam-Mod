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
#include "HLCam Client\Client.hpp"

namespace
{
	HSPRITE WhiteSprite = 0;
	model_s* WhiteSpriteModel;
}

namespace Tri
{
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

		for (const auto& trig : triggers)
		{
			Vector corner1;
			corner1.x = trig.Corner1[0];
			corner1.y = trig.Corner1[1];
			corner1.z = trig.Corner1[2];

			Vector corner2;
			corner2.x = trig.Corner2[0];
			corner2.y = trig.Corner2[1];
			corner2.z = trig.Corner2[2];

			gEngfuncs.pTriAPI->CullFace(TRICULLSTYLE::TRI_NONE);

			gEngfuncs.pTriAPI->RenderMode(kRenderTransAlpha);

			if (trig.Highlighted)
			{
				gEngfuncs.pTriAPI->Color4f(1, 1, 0, 0.6);
			}

			else
			{
				gEngfuncs.pTriAPI->Color4f(0, 1, 0, 0.2);
			}
			
			DrawBox(corner1, corner2);

			gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
			gEngfuncs.pTriAPI->Color4f(0, 1, 0, 1);
			
			DrawWireframeBox(corner1, corner2);
		}

		gEngfuncs.pTriAPI->RenderMode(kRenderNormal);
		gEngfuncs.pTriAPI->Color4f(1, 0, 0, 1);

		for (const auto& cam : cameras)
		{
			Vector pos;
			pos.x = cam.Position[0];
			pos.y = cam.Position[1];
			pos.z = cam.Position[2];
		
			Vector angles;
			angles.x = cam.Angle[0];
			angles.y = cam.Angle[1];
			angles.z = cam.Angle[2];

			Vector minpos = pos;
			Vector maxpos = pos;

			const auto camboxsize = 4;

			minpos.x -= camboxsize;
			minpos.y -= camboxsize;
			minpos.z -= camboxsize;

			maxpos.x += camboxsize;
			maxpos.y += camboxsize;
			maxpos.z += camboxsize;

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

			minpos.x -= camboxsize;
			minpos.y -= camboxsize;
			minpos.z -= camboxsize;

			maxpos.x += camboxsize;
			maxpos.y += camboxsize;
			maxpos.z += camboxsize;

			/*
				End point wire box
			*/
			DrawWireframeBox(maxpos, minpos);
		}
	}
}
