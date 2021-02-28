/*
 * @file lldrawpoolmirror.h
 * @brief LLDrawPoolMirror class implementation, draws mirrors
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * Copyright (C) 2014, Zi Ree @ SecondLife
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "lldrawpoolmirror.h"

#include "llagent.h"
#include "llagentcamera.h"		// for gAgentCamera
#include "llrendertarget.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewershadermgr.h"	// for gOneTextureNoColorProgram
#include "llviewerwindow.h"
#include "llvoavatarself.h"		// for gAgentAvatarp
#include "llvolume.h"
#include "pipeline.h"

LLDrawPoolMirror::LLDrawPoolMirror() :
	LLRenderPass(POOL_MIRROR),
	mIsInMirror(false)
{
	// set up offscreen render target for mirrored world rendering at configured resolution
	S32 mirror_resolution=gSavedSettings.getS32("MirrorResolution");
	if(!mMirrorTarget.allocate(mirror_resolution,mirror_resolution,GL_RGB8,true,false,LLTexUnit::TT_TEXTURE,true))
	{
		LL_WARNS("Mirrors") << "+++***+++ Could not allocate mirror render target." << LL_ENDL;
	}
}

// not sure yet what this does
U32 LLDrawPoolMirror::getVertexDataMask()
{
	return
		LLVertexBuffer::MAP_VERTEX |
		LLVertexBuffer::MAP_NORMAL |
		LLVertexBuffer::MAP_TEXCOORD0 |
		LLVertexBuffer::MAP_COLOR;
}

S32 LLDrawPoolMirror::getNumDeferredPasses()
{
	return 1;
}

void LLDrawPoolMirror::beginDeferredPass(S32 pass)
{
}

void LLDrawPoolMirror::endDeferredPass(S32 pass)
{
}

void LLDrawPoolMirror::renderDeferred(S32 pass)
{
	render();
}

void LLDrawPoolMirror::beginRenderPass(S32 pass)
{
}

void LLDrawPoolMirror::endRenderPass(S32 pass)
{
}

S32	 LLDrawPoolMirror::getNumPasses()
{
	return 1;
}

void LLDrawPoolMirror::render(S32 pass)
{
	// bail out if mirror rendering is disabled
	static LLCachedControl<bool> render_mirrors(gSavedSettings,"RenderMirrors");
	if(!render_mirrors)
	{
		return;
	}

	if(LLPipeline::sReflectionRender)
	{
		LL_DEBUGS("Mirrors") << "+++***+++ Do not render mirrors in water reflections." << LL_ENDL;
		return;
	}

	// bail out if no mirrors are in the scene
	if(!mMirrorFaces.size())
	{
		LL_DEBUGS("Mirrors") << "+++***+++ No mirrors to render." << LL_ENDL;
		return;
	}

	// bail out if our offscreen render target is not working
	if(!mMirrorTarget.isComplete())
	{
		LL_DEBUGS("Mirrors") << "+++***+++ Can't render mirrors into incomplete render target." << LL_ENDL;
		return;
	}

	if(mIsInMirror)
	{
		LL_DEBUGS("Mirrors") << "+++***+++ Not drawing mirrors inside of mirrors." << LL_ENDL;
		return;
	}

	// "cheap man's" depth sorting to prevent mirrors showing in front of other mirrors wrongly
	// also discarding any mirror that should not be drawn at all
	std::map<F32,LLFace*> sorted_mirrors;

	for (std::list<LLFace*>::iterator iter=mMirrorFaces.begin();
		iter!= mMirrorFaces.end();
		iter++)
	{
		LLFace* face=*iter;

		// mirrors that are not visible can be ignored
		if(!face->getDrawable()->isVisible())
		{
			LL_DEBUGS("Mirrors") << "+++***+++ mirror " << face << " is not visible" << LL_ENDL;
			continue;
		}

		// do not render attached mirrors
		if(face->getViewerObject()->isAttachment())
		{
			static LLCachedControl<bool> render_attached_mirrors(gSavedSettings,"RenderAttachedMirrors");
			if(!render_attached_mirrors)
			{
				LL_DEBUGS("Mirrors") << "+++***+++ not rendering attached mirror " << face << LL_ENDL;
				continue;
			}
		}

		// do not render mirrors that are far away
		static LLCachedControl<F32> mirror_max_distance(gSavedSettings,"MirrorMaxDistance");

		LLVector3 face_pos=face->getPositionAgent();
		F32 distance=(face_pos-LLViewerCamera::instance().getOrigin()).length();
		if(distance>mirror_max_distance)
		{
			LL_DEBUGS("Mirrors") << "+++***+++ Skipping far away mirror " << face << " at distance " << distance << LL_ENDL;
			continue;
		}

		// Still looking for a solution to only consider mirrors that are not occluded by other objects.
		// Tried various things like face->getDrawable()->getSpacialroup()->isOcclusionState() and
		// gPipeline.beginOcclusionGroups() loop etc. but none gave me a good solution. So for now we
		// must render all mirrors in frustum that are not excluded by other criteria, even if they are
		// technically not visible on screen because they are behind another object. The following code
		// works most of the time but sometimes it fails to show the mirror again once occlusion is over.

		// culled mirrors can be ignored
		const U32 occluded=
			LLOcclusionCullingGroup::OCCLUDED |
			LLOcclusionCullingGroup::ACTIVE_OCCLUSION |
			LLOcclusionCullingGroup::EARLY_FAIL;

		U32 state=face->getDrawable()->getSpatialGroup()->getOcclusionState();
		if(state & occluded)
		{
			LL_DEBUGS("Mirrors") << "+++***+++ mirror " << face << " occlusion state " << state << LL_ENDL;
			if(gFrameCount % 10==0)
			{
				LL_DEBUGS("Mirrors") << "+++***+++ trying to update occlusion of mirror " << face << LL_ENDL;
				face->getDrawable()->updateMove();

				// this crashes
				// face->getDrawable()->getSpatialGroup()->doOcclusion(&LLViewerCamera::instance());
				// This doesn't seem to do anything
				// face->getDrawable()->getSpatialGroup()->clearOcclusionState(LLOcclusionCullingGroup::OCCLUDED);
			}
			continue;
		}
		// just out of curiosity
		else if(state)
		{
			if(state & LLOcclusionCullingGroup::DISCARD_QUERY)
			{
				LL_DEBUGS("Mirrors") << "+++***+++ DISCARD_QUERY" << LL_ENDL;
			}

			if(state & LLOcclusionCullingGroup::QUERY_PENDING)
			{
				LL_DEBUGS("Mirrors") << "+++***+++ QUERY_PENDING" << LL_ENDL;
			}
		}

		// It's very unlikely but not impossible to have two mirrors at exactly the same distance
		// so we try to find a close approximation to not overwrite the existing mirror
		while(sorted_mirrors.find(distance)!=sorted_mirrors.end())
		{
			LL_DEBUGS("Mirrors") << "+++***+++ mirror " << face << " had the same distance as another mirror" << LL_ENDL;
			distance+=0.01f;
		}
		sorted_mirrors[distance]=*iter;
	}

	// only render as many mirrors as set up in preferences
	static LLCachedControl<S32> max_num_of_mirrors(gSavedSettings,"RenderMaxMirrorCount");

	if(max_num_of_mirrors>0)
	{
		while(sorted_mirrors.size()>max_num_of_mirrors)
		{
			LL_DEBUGS("Mirrors") << "+++***+++ truncating sorted mirror list" << LL_ENDL;
			std::map<F32,LLFace*>::iterator iter=sorted_mirrors.end();
			--iter;
			sorted_mirrors.erase(iter);
		}
		LL_DEBUGS("Mirrors") << "+++***+++ to " << sorted_mirrors.size() << LL_ENDL;
	}

	// remember we're in mirror drawing mode now
	mIsInMirror=true;
	LL_DEBUGS("Mirrors") << "+++***+++ We are now rendering mirrors." << LL_ENDL;

	// good for debugging GL states but not really necessary, depends on gDebugGL
	LLGLState::checkStates();
	LLGLState::checkTextureChannels();	// is ifdef'd out in the source anyway
	LLGLState::checkClientArrays();

	// now render the mirrors from back to front
	for (std::map<F32,LLFace*>::reverse_iterator iter=sorted_mirrors.rbegin();
		iter!=sorted_mirrors.rend();
		iter++)
	{
		LLFace* face=iter->second;
		LLVector3 face_pos=face->getPositionAgent();

		// LL_WARNS("Mirrors") << "+++***+++ Rendering " << iter->second << " mirror " << index << " - " << face << " at distance " << distance.length() << LL_ENDL;
		renderMirror(face,face_pos);
	}

	// restore culling to make things outside the mirrors cull just fine
	// why static?
	static LLCullResult result;
	gPipeline.updateCull(LLViewerCamera::instance(),result);
	gPipeline.stateSort(LLViewerCamera::instance(),result);

	// good for debugging GL states but not really necessary, depends on gDebugGL
	LLGLState::checkStates();

	// we're done with mirror rendering
	mIsInMirror=false;
	LL_DEBUGS("Mirrors") << "+++***+++ We have finished rendering mirrors." << LL_ENDL;
}

// stencil render method - disadvantage is that overlapping stencils do weird things, even with depth sorting,
// mirror-in-mirror is not possible
void LLDrawPoolMirror::renderMirror(LLFace* face,const LLVector3& face_pos)
{
	LL_DEBUGS("Mirrors") << "+++***+++ Stencil render start for mirror " << face << LL_ENDL;
	// get vertex and vertex index array pointers
	LLVector4a* posp=face->getViewerObject()->getVolume()->getVolumeFace(face->getTEOffset()).mPositions;
	U16* indp=face->getViewerObject()->getVolume()->getVolumeFace(face->getTEOffset()).mIndices;

	// get first three vertices (first triangle)
	LLVector4a v0=posp[indp[0]];
	LLVector4a v1=posp[indp[1]];
	LLVector4a v2=posp[indp[2]];

	// calculate triangle's face normal (v1-v0) x (v2-v0)
	v1.sub(v0);
	v2.sub(v0);

	LLVector3 face_normal=LLVector3(v1[0],v1[1],v1[2]) % LLVector3(v2[0],v2[1],v2[2]);

	// make unit normal and rotate it according to face rotation
	face_normal.normalize();
	face_normal*=face->getXform()->getWorldRotation();

	// check if the camera is behind the mirror's surface
	LLVector3 cam_pos=LLViewerCamera::instance().getOrigin();
	if(face_normal*(cam_pos-face_pos)<0.0f)
	{
		LL_DEBUGS("Mirrors") << "+++***+++ camera is behind mirror " << face << " - ignoring." << LL_ENDL;
		return;
	}

	bool skip_avatar_update=false;
	if (!isAgentAvatarValid() || gAgentCamera.getCameraAnimating() || gAgentCamera.getCameraMode()!=CAMERA_MODE_MOUSELOOK || !LLVOAvatar::sVisibleInFirstPerson)
	{
		skip_avatar_update=true;
	}

	if(!skip_avatar_update)
	{
		// render the 3rd person view of the avatar in a mirror even in mouselook
		gAgentAvatarp->updateAttachmentVisibility(CAMERA_MODE_THIRD_PERSON);
	}

	// disable occlusion culling for mirrors for now
	S32 occlusion=LLPipeline::sUseOcclusion;
	LLPipeline::sUseOcclusion=0;

	// not sure if this really makes a difference, but LL does that with water reflections
	// LLViewerCamera::sCurCameraID=LLViewerCamera::CAMERA_WATER0;

	// render into our offscreen render target
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
	gGL.getTexUnit(0)->bind(&mMirrorTarget);
	glClearColor(0,0,0,0);

	mMirrorTarget.bindTarget();

	// clear out the render target
	gGL.setColorMask(true,true);
	mMirrorTarget.clear();
	gGL.setColorMask(true,false);

	// get our target's viewport
	mMirrorTarget.getViewport(gGLViewport);

	// find the world flip and translation according to the mirror's normal
	LLVector3 flip(-1.0f,1.0f,1.0f);
	LLVector3 translation=LLVector3::zero;

	LLVector3 mirror_default_normal;
	translation.mV[VX]=face_pos.mV[VX]*2.0f;
	mirror_default_normal=LLVector3::x_axis;

//	LL_WARNS("Mirrors") << "+++***+++ flip " << flip << " face_pos*2 " << face_pos*2.0f << " flipped face_pos (translation) " << translation << LL_ENDL;

	// transform world
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.pushMatrix();

	// get our current model view matrix, also used to restore it later
	glh::matrix4f current_modelview=get_current_modelview();

	// start with a fresh identity matrix
	glh::matrix4f mat;

	// set up plane to clip everything behind the mirror
	glh::matrix4f current_projection=get_current_projection();
	LLPlane plane;
	// face normal defines the plane's orientation, face_pos*face_normal is the distance to origin along normal
	//	plane.setVec(face_normal,-face_pos*face_normal);	// the line below seems to do it the same way
	plane.setVec(face_pos,face_normal);

	LLQuaternion mirror_rot;

	mirror_rot.shortestArc(-face_normal,mirror_default_normal);
	LLVector3 axis;
	F32 angle;
	mirror_rot.getAngleAxis(&angle,axis);
	mirror_rot.setAngleAxis(angle*2.0f,axis);

	glh::matrix4f mirror_rot_mat;
	glh::matrix4f mirror_delta_mat;

	mirror_rot_mat.set_value((glh::ns_float::real*) (mirror_rot.getMatrix4().mMatrix));

	// new position of the mirror in the rotated and flipped scene
	LLVector3 mirror_pos=(face_pos.scaledVec(flip)+translation)*mirror_rot;
	// difference between the new and the original position
	LLVector3 mirror_delta=face_pos-mirror_pos;

//	LL_WARNS("Mirrors") << "pos " << mirror_pos << " delta " << mirror_delta << LL_ENDL;

	// flip the world around the needed axis and move it back to the mirror position
	mat.set_translate(translation.mV);
	mat.set_scale(flip.mV);

	// rotate the flipped scene and translate it back to the mirror position
	mirror_delta_mat.set_translate(mirror_delta.mV);
	mat=mat*mirror_delta_mat;
	mat=mat*mirror_rot_mat;

	// apply all to the current modelview
	mat=current_modelview*mat;

	set_current_modelview(mat);
	// end world transform

	// make a copy of the main camera for mirror culling
	LLCamera mirror_camera=LLViewerCamera::instance();

	// update render frustum to the mirror camera's view
	LLViewerCamera::updateFrustumPlanes(mirror_camera,FALSE,TRUE);

	// get the inverse of our combined matrix and apply it to the mirror camera origin
	// so we can get the correct culling from the mirror's point of view
	glh::matrix4f inv_mat=mat.inverse();

	glh::vec3f origin(0.0f,0.0f,0.0f);
	inv_mat.mult_matrix_vec(origin);

	mirror_camera.setOrigin(origin.v);

	// flip face direction so objects don't appear inside-out
	glCullFace(GL_FRONT);

	// clipping / culling from the mirror's point of view
	// why does this have to be static?
	static LLCullResult ref_result;
	{
		LLGLUserClipPlane clip_plane(plane,mat,current_projection);
		LLGLDisable cull(GL_CULL_FACE);
		gPipeline.updateCull(mirror_camera,ref_result,0,&plane);
		gPipeline.stateSort(mirror_camera,ref_result);
	}
	// end clipping/culling

	// render scene, scope for clipping/culling
	{
		// get the previously collected culling data
		gPipeline.grabReferences(ref_result);

		// enable clip plane (will autodestruct after leaving the scope)
		LLGLUserClipPlane clip_plane(plane,mat,current_projection);

		gPipeline.renderGeom(mirror_camera,true);
	}
	// end render scene

	// set face direction back to default
	glCullFace(GL_BACK);

	// tell the offscreen render target to finish drawing
	mMirrorTarget.flush();

	// restore previous modelview
	set_current_modelview(current_modelview);

	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.popMatrix();

	// bind rendering back to the screen
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
	gGL.getTexUnit(0)->bind(&gPipeline.mScreen);
	gPipeline.mScreen.bindTarget();

	if(!skip_avatar_update)
	{
		// restore previous camera mode
		gAgentAvatarp->updateAttachmentVisibility(gAgentCamera.getCameraMode());
	}

	// back to world camera
	// LLViewerCamera::sCurCameraID=LLViewerCamera::CAMERA_WORLD;

	// Stencil out the mirror face
	GLint buffers=GL_NONE;
	glGetIntegerv(GL_DRAW_BUFFER,&buffers);
	glClearStencil(0x0);
	glClear(GL_STENCIL_BUFFER_BIT);
	glStencilFunc(GL_ALWAYS,0x1,0x1);
	glStencilOp(GL_REPLACE,GL_REPLACE,GL_REPLACE);
	glDrawBuffer(GL_NONE);

	// start using the stencil buffer
	{
		LLGLEnable stencil(GL_STENCIL_TEST);

		if(LLGLSLShader::sNoFixedFunction)
		{
			// Use a simple shader (not sure if I should use this UI shader)
			gUIProgram.bind();
		}

		face->renderSelected(LLViewerTexture::sNullImagep,LLColor4(1.0f,1.0f,1.0f,1.0f));

		if(LLGLSLShader::sNoFixedFunction)
		{
			gUIProgram.unbind();
		}

		glDrawBuffer((GLenum) buffers);
		glStencilFunc(GL_EQUAL,1,1);
		glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP );

		// Tutorials tell me to clear the depth buffer (some say color, too) but
		// it doesn't seem to do me much good
		// glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

		// disable everything we don't need for 2D overlay render
		{
			// add things to disable here in case I forgot any
			LLGLDepthTest depth(GL_FALSE);
			LLGLDisable cull_face(GL_CULL_FACE);
			LLGLDisable lighting(GL_LIGHTING);

			// save matrices and load defaults
			gGL.matrixMode(LLRender::MM_PROJECTION);
			gGL.pushMatrix();
			gGL.loadIdentity();
			gGL.matrixMode(LLRender::MM_MODELVIEW);
			gGL.pushMatrix();
			gGL.loadIdentity();

			// set up a flat perspective (orthogonal)
			glOrtho(0.0f,(F32) gViewerWindow->getWorldViewRectRaw().getWidth(),0.0f,(F32) gViewerWindow->getWorldViewRectRaw().getHeight(),-1.0f,1.0f);

			// set up viewport (not sure if I need this)
			// gGLViewport[0]=gViewerWindow->getWorldViewRectRaw().mLeft;
			// gGLViewport[1]=gViewerWindow->getWorldViewRectRaw().mBottom;
			// gGLViewport[2]=gViewerWindow->getWorldViewRectRaw().getWidth();
			// gGLViewport[3]=gViewerWindow->getWorldViewRectRaw().getHeight();
			// glViewport(gGLViewport[0],gGLViewport[1],gGLViewport[2],gGLViewport[3]);
			// gGL.flush();

			// bind the previously rendered world texture to the screen overlay to draw it on the quad
			mMirrorTarget.bindTexture(0,0);

			if(LLGLSLShader::sNoFixedFunction)
			{
				gUIProgram.unbind();
				// switch to simple texture shader
				gOneTextureNoColorProgram.bind();
			}

			// texure drawing mode (not sure if I need this)
			// gGL.getTexUnit(0)->setTextureColorBlend(LLTexUnit::TBO_REPLACE, LLTexUnit::TBS_TEX_COLOR);

			// render the stenciled mirror image overlay
			gGL.begin(LLRender::TRIANGLES);
			{
				gGL.texCoord2f(0.0f, 0.0f);
				gGL.vertex2f(-1.0f, -1.0f);
				gGL.texCoord2f(1.0f, 0.0f);
				gGL.vertex2f(1.0f, -1.0f);
				gGL.texCoord2f(1.0f, 1.0f);
				gGL.vertex2f(1.0f, 1.0f);

				gGL.texCoord2f(0.0f, 0.0f);
				gGL.vertex2f(-1.0f, -1.0f);
				gGL.texCoord2f(1.0f, 1.0f);
				gGL.vertex2f(1.0f, 1.0f);
				gGL.texCoord2f(0.0f, 1.0f);
				gGL.vertex2f(-1.0f, 1.0f);
			}
			gGL.end();

			// tell the screen to finish drawing
			gPipeline.mScreen.flush();

			// stop drawing with our mirror texture
			gGL.getTexUnit(0)->unbind(mMirrorTarget.getUsage());

			if(LLGLSLShader::sNoFixedFunction)
			{
				gOneTextureNoColorProgram.unbind();
			}

			// restore matrices
			gGL.matrixMode(LLRender::MM_PROJECTION);
			gGL.popMatrix();
			gGL.matrixMode(LLRender::MM_MODELVIEW);
			gGL.popMatrix();
		}
		// restore attributes (leaving scope)
	}
	// end usage  of stencil buffer

/*
	// draw the mirror surface again, but this time only into the depth buffer
	// doesn't seem to make a difference, makes it worse if anything. So disabling now

	// draw no colors, only depth
	glDrawBuffer(GL_NONE);
	if(LLGLSLShader::sNoFixedFunction)
	{
		// switch back to simple shader (not sure if I should use this UI shader)LL_WARNS
		gUIProgram.bind();
	}
	face->renderSelected(LLViewerTexture::sNullImagep,LLColor4(1.0f,1.0f,1.0f,1.0f));
	if (LLGLSLShader::sNoFixedFunction)
	{
		gUIProgram.unbind();
	}
	glDrawBuffer( (GLenum) buffers );
*/

	// restore occlusion setting
	LLPipeline::sUseOcclusion=occlusion;

	LL_DEBUGS("Mirrors") << "+++***+++ Stencil render finished for mirror " << face << LL_ENDL;
}

void LLDrawPoolMirror::prerender()
{
}

void LLDrawPoolMirror::addMirror(LLFace* face)
{
	LL_DEBUGS("Mirrors") << "+++***+++ adding mirror to face " << face << LL_ENDL;
	mMirrorFaces.push_back(face);
}

void LLDrawPoolMirror::remMirror(LLFace* face)
{
	LL_DEBUGS("Mirrors") << "+++***+++ removing mirror from face " << face << LL_ENDL;
	mMirrorFaces.remove(face);
}

bool LLDrawPoolMirror::getMirrorRender()
{
	return mIsInMirror;
}
