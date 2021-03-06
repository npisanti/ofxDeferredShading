#include "ShadowLightPass.hpp"

using namespace ofxDeferred;

ShadowLightPass::ShadowLightPass(const glm::vec2& size) : RenderPass(size, "ShadowLightPass") {

	// create shadow map fbo
	ofFbo::Settings s;
	s.width = 1024;
	s.height = 1024;
	s.minFilter = GL_LINEAR;
	s.maxFilter = GL_LINEAR;
	s.wrapModeVertical = GL_CLAMP_TO_EDGE;
	s.wrapModeHorizontal = GL_CLAMP_TO_EDGE;
	s.internalformat = GL_R16F;
	s.useDepth = true;
	s.useStencil = true;
	s.depthStencilAsTexture = true;

	shadowMap.allocate(s);

	// TODO: blur fbo
	// setup camera for shadow map

	nearClip = 1.f;
	farClip = 5000.f;
	
	linearDepthScalar = 1.f / (farClip - nearClip);

	viewPortSize = 1024.f;

	// load shader
	shader.load("shader/vfx/PassThru.vert", "shader/vfx/ShadowLight.frag");
	linearDepthShader.load("shader/gbuffer.vert", "shader/vfx/LinearDetph.frag");
}

void ShadowLightPass::beginShadowMap(bool bUseOwnShader) {

	useShader = bUseOwnShader;

	// update view matrix of depth camera
	projection = ortho(-viewPortSize, viewPortSize, viewPortSize, -viewPortSize, nearClip, farClip);
	modelView = inverse(translate(getGlobalPosition()) * toMat4(getGlobalOrientation()));
	depthMVP = projection * modelView;
	shadowMap.begin();

	ofClear(0);
	ofBackground(255, 0, 0);
	ofEnableDepthTest();

	ofPushView();
	ofSetMatrixMode(OF_MATRIX_PROJECTION);
	ofLoadMatrix(projection);
	ofSetMatrixMode(OF_MATRIX_MODELVIEW);
	ofLoadMatrix(modelView);
	
	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);

	if (!useShader) {
		linearDepthShader.begin();
		linearDepthShader.setUniform1f("lds", linearDepthScalar);
	}

}

void ShadowLightPass::endShadowMap() {

	if (!useShader) linearDepthShader.end();

	glDisable(GL_CULL_FACE);
	
	ofDisableDepthTest();
	ofPopView();

	shadowMap.end();

	// TODO: blur shadow map

}

void ShadowLightPass::debugDraw() {
	shadowMap.getTexture().draw(0, 0, 256, 256);
}

void ShadowLightPass::update(const ofCamera &cam) {

	linearDepthScalar = 1.f / (farClip - nearClip);
	shadowTransMat = biasMat * depthMVP * inverse(cam.getModelViewMatrix());
	directionInView = (inverse(transpose(cam.getModelViewMatrix())) * vec4(getLookAtDir(), 0.f));

}

void ShadowLightPass::render(ofFbo &readFbo, ofFbo &writeFbo, GBuffer &gbuffer) {

	writeFbo.begin();

	ofClear(0);

	shader.begin();
	shader.setUniformTexture("lightDepthTex", shadowMap.getTexture(), 1);
	shader.setUniformTexture("colorTex", gbuffer.getTexture(GBuffer::TYPE_ALBEDO), 2);
	shader.setUniformTexture("positionTex", gbuffer.getTexture(GBuffer::TYPE_POSITION), 3);
	shader.setUniformTexture("normalAndDepthTex", gbuffer.getTexture(GBuffer::TYPE_DEPTH_NORMAL), 4);
	shader.setUniformMatrix4f("shadowTransMat", shadowTransMat);

	shader.setUniform3f("lightDir", directionInView);
	shader.setUniform1f("darkness", darkness);
	shader.setUniform1f("linearDepthScalar", linearDepthScalar);

	shader.setUniform4f("ambient", ambientColor);
	shader.setUniform4f("diffuse", diffuseColor);
	readFbo.draw(0, 0);

	shader.end();

	writeFbo.end();
}
