/*
 * @file lldrawpoolmirror.h
 * @brief LLDrawPoolMirror class definition, draws mirrors
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

#ifndef LL_LLDRAWPOOLMIRROR_H
#define LL_LLDRAWPOOLMIRROR_H

#include "lldrawpool.h"
#include "llrendertarget.h"

class LLFace;

class LLDrawPoolMirror : public LLRenderPass
{
public:
	LLDrawPoolMirror();

	virtual U32 getVertexDataMask();

	/*virtual*/ S32 getNumDeferredPasses();
	/*virtual*/ void beginDeferredPass(S32 pass);
	/*virtual*/ void endDeferredPass(S32 pass);
	/*virtual*/ void renderDeferred(S32 pass);

	/*virtual*/ void beginRenderPass(S32 pass);
	/*virtual*/ void endRenderPass(S32 pass);

	/*virtual*/ S32	 getNumPasses();
	/*virtual*/ void render(S32 pass=0);
	/*virtual*/ void prerender();

	void addMirror(LLFace* face);
	void remMirror(LLFace* face);

	// returns true if we are currently rendering a mirror
	bool getMirrorRender();

private:
	void renderMirror(LLFace* face,const LLVector3& face_pos);

	std::list<LLFace*> mMirrorFaces;

	bool mIsInMirror;

	LLRenderTarget mMirrorTarget;
};

#endif // LL_LLDRAWPOOLMIRROR_H
