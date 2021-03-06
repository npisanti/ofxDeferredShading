#include "HdrBloomPass.hpp"

using namespace ofxDeferred;

HdrBloomPass::HdrBloomPass(const glm::vec2& size) : RenderPass(size, "HdrBloomPass") {
	blurShader.load("shader/vfx/PassThru.vert", "shader/vfx/Blur.frag");
	blurFbo[0].allocate(size.x, size.y, GL_RGB);
	blurFbo[1].allocate(size.x, size.y, GL_RGB);

	vector<float> gaussian = createGaussianWeights(16, 0.2f);
	int center = gaussian.size() / 2;
	coefficients.push_back(gaussian[center]);
	for (int i = 1; i < gaussian.size(); i += 2) {
		float weightSum = (gaussian[i] + gaussian[i + 1]) / 2.;
		coefficients.push_back(weightSum);
	}
	offsets = createOffsets(gaussian);
}

vector<float> HdrBloomPass::createGaussianWeights(int radius, float variance) {
	int rowSize = 1 + 2 * radius;
	vector<float> row(rowSize, 0.f);
	float sum = 0.f;
	for (int i = 0; i < row.size(); i++) {
		float x = ofMap(i, 0, rowSize - 1, -1, 1);
		row[i] = Gaussian(x, 0, variance);
		sum += row[i];
	}

	for (int i = 0; i < row.size(); i++) {
		row[i] /= sum;
	}

	return row;
}
vector<float> HdrBloomPass::createOffsets(const vector<float>& gaussian) {
	vector<float> offsets;

	int center = gaussian.size() / 2;
	for (int i = 1; i < gaussian.size(); i += 2) {
		float li = i - center;
		float ri = li + 1;
		float l = gaussian[i];
		float r = gaussian[i + 1];
		float weightedAvrg = (l * li + r * ri) / (l + r);
		offsets.push_back(weightedAvrg);
	}
	return offsets;
}

float HdrBloomPass::Gaussian(float x, float mean, float variance) {
	x -= mean;
	return (1. / sqrt(TWO_PI * variance)) * exp(-(x * x) / (2. * variance));
}

void HdrBloomPass::render(ofFbo &readFbo, ofFbo &writeFbo, GBuffer &gbuffer) {

	// vertical blur
	blurFbo[0].begin();
	ofClear(0);
	blurShader.begin();
	blurShader.setUniform1fv("coefficients", coefficients.data(), coefficients.size());
	blurShader.setUniform1fv("offsets", offsets.data(), offsets.size());
	blurShader.setUniform1i("sampleSize", coefficients.size());
	blurShader.setUniform1i("horizontal", 0);
	gbuffer.getTexture(GBuffer::TYPE_HDR).draw(0, 0);
	blurShader.end();
	blurFbo[0].end();

	// horizontal blur
	blurFbo[1].begin();
	ofClear(0);
	blurShader.begin();
	blurShader.setUniform1fv("coefficients", coefficients.data(), coefficients.size());
	blurShader.setUniform1fv("offsets", offsets.data(), offsets.size());
	blurShader.setUniform1i("sampleSize", coefficients.size());
	blurShader.setUniform1i("horizontal", 1);
	blurFbo[0].draw(0, 0);
	blurShader.end();
	blurFbo[1].end();

	// write start
	writeFbo.begin();
	ofPushStyle();
	ofClear(0);
	ofEnableBlendMode(OF_BLENDMODE_ADD);

	blurFbo[1].draw(0, 0);
	readFbo.draw(0, 0);

	ofDisableBlendMode();
	ofPopStyle();
	writeFbo.end();

}
