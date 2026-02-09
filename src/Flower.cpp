#include "Flower.h"

// ============================================================
// Petal path utility
// ============================================================

void buildPetalPath(ofPath& path, const PetalParams& p, ofColor color) {
	float halfWidth = p.length * p.width;
	float bulgeY = p.length * ofClamp(p.bulgePosition, 0.05f, 0.95f);
	float tipWidth = halfWidth * (1.0f - ofClamp(p.tipPointiness, 0.0f, 1.0f));
	float curveShift = p.edgeCurvature * halfWidth * 0.5f;

	path.clear();
	path.setFilled(true);
	path.setFillColor(color);

	path.moveTo(0, 0);
	path.bezierTo(
		-(halfWidth + curveShift), -bulgeY,
		-tipWidth, -(p.length - p.length * 0.08f),
		0, -p.length
	);
	path.bezierTo(
		tipWidth, -(p.length - p.length * 0.08f),
		(halfWidth + curveShift), -bulgeY,
		0, 0
	);
	path.close();
}

// ============================================================
// Inflorescence
// ============================================================

void Inflorescence::setup(const InflorescenceParams& p) {
	params = p;
	dirty = true;
}

void Inflorescence::setParams(const InflorescenceParams& p) {
	params = p;
	dirty = true;
}

InflorescenceParams& Inflorescence::getParams() {
	return params;
}

void Inflorescence::rebuild() {
	buildPetalPath(petalPath, params.petal, params.petalColor);
	dirty = false;
}

void Inflorescence::draw() {
	if (dirty) rebuild();

	ofPushMatrix();
	ofRotateDeg(params.rotation);

	float angleStep = 360.0f / params.petal.count;
	for (int i = 0; i < params.petal.count; i++) {
		ofPushMatrix();
		ofRotateDeg(i * angleStep);
		petalPath.draw();
		ofPopMatrix();
	}

	// Center disc
	ofFill();
	ofSetColor(params.centerColor);
	ofDrawCircle(0, 0, params.centerRadius);

	ofPopMatrix();
}

// ============================================================
// Stem
// ============================================================

void Stem::setup(const StemParams& p) {
	params = p;
	dirty = true;
}

void Stem::setParams(const StemParams& p) {
	params = p;
	dirty = true;
}

StemParams& Stem::getParams() {
	return params;
}

glm::vec2 Stem::getTopPosition() const {
	// The top of the stem is offset horizontally by curvature
	float xOffset = params.curvature * params.height * 0.3f;
	return glm::vec2(xOffset, -params.height);
}

void Stem::rebuild() {
	float h = params.height;
	float t = params.thickness * 0.5f;
	float xOffset = params.curvature * h * 0.3f;

	// Control point for the bend (at mid-height)
	float cpX = xOffset * 0.6f;
	float cpY = -h * 0.5f;

	stemPath.clear();
	stemPath.setFilled(true);
	stemPath.setFillColor(params.color);

	// Draw stem as a filled shape with thickness
	// Left edge going up
	stemPath.moveTo(-t, 0);
	stemPath.bezierTo(
		cpX - t, cpY,
		xOffset - t * 0.7f, -h + h * 0.1f,
		xOffset, -h
	);
	// Right edge coming back down
	stemPath.bezierTo(
		xOffset + t * 0.7f, -h + h * 0.1f,
		cpX + t, cpY,
		t, 0
	);
	stemPath.close();

	dirty = false;
}

void Stem::draw() {
	if (dirty) rebuild();
	stemPath.draw();
}

// ============================================================
// Flower
// ============================================================

void Flower::setup(const InflorescenceParams& ip, const StemParams& sp) {
	inflorescence.setup(ip);
	stem.setup(sp);
}

void Flower::draw(float x, float y) {
	ofPushMatrix();
	ofTranslate(x, y);  // ground position (stem base)

	// Draw stem from base upward
	stem.draw();

	// Move to top of stem and draw flower head
	glm::vec2 top = stem.getTopPosition();
	ofTranslate(top.x, top.y);
	inflorescence.draw();

	ofPopMatrix();
}

Inflorescence& Flower::getInflorescence() {
	return inflorescence;
}

Stem& Flower::getStem() {
	return stem;
}

// ============================================================
// Falling Petal System
// ============================================================

glm::vec2 FallingPetal::drawPosition() const {
	return glm::vec2(
		basePosition.x + std::sin(age * waverFreq * TWO_PI + waverPhase) * waverAmp,
		basePosition.y
	);
}

void FallingPetalSystem::setConfig(const FallingPetalConfig& cfg) {
	config = cfg;
}

FallingPetalConfig& FallingPetalSystem::getConfig() {
	return config;
}

void FallingPetalSystem::spawn(glm::vec2 headPos, float detachAngleDeg,
                                const PetalParams& shape, ofColor color) {
	FallingPetal fp;

	float rad = ofDegToRad(detachAngleDeg);
	float midDist = shape.length * 0.4f;
	fp.basePosition = glm::vec2(
		headPos.x + midDist * std::sin(rad),
		headPos.y - midDist * std::cos(rad)
	);

	float outward = config.initialUpPop * 0.4f;
	fp.velocity = glm::vec2(
		std::sin(rad) * outward,
		-config.initialUpPop
	);

	fp.rotation = detachAngleDeg;
	fp.rotationSpeed = config.tumbleSpeed * ofRandom(0.6f, 1.4f)
	                    * (ofRandom(1.0f) > 0.5f ? 1.0f : -1.0f);
	fp.alpha = 1.0f;
	fp.age = 0.0f;
	fp.waverPhase = ofRandom(0.0f, TWO_PI);
	fp.waverAmp = config.waverAmplitude * ofRandom(0.7f, 1.3f);
	fp.waverFreq = config.waverFrequency * ofRandom(0.7f, 1.3f);

	fp.shape = shape;
	fp.color = color;
	fp.alive = true;

	petals.push_back(fp);
}

void FallingPetalSystem::update(float dt) {
	for (auto& fp : petals) {
		if (!fp.alive) continue;

		fp.age += dt;

		if (fp.age > config.maxLifetime || fp.basePosition.y > ofGetHeight() + 50.0f) {
			fp.alive = false;
			continue;
		}

		// Gravity
		fp.velocity.y += config.gravity * dt;

		// Move base position
		fp.basePosition += fp.velocity * dt;

		// Tumble
		fp.rotation += fp.rotationSpeed * dt;

		// Fade after delay
		if (fp.age > config.fadeDelay) {
			fp.alpha -= config.fadeSpeed * dt;
			if (fp.alpha < 0.0f) fp.alpha = 0.0f;
		}

		if (fp.alpha <= 0.0f) fp.alive = false;
	}

	// Remove dead petals
	petals.erase(
		std::remove_if(petals.begin(), petals.end(),
			[](const FallingPetal& fp) { return !fp.alive; }),
		petals.end()
	);
}

void FallingPetalSystem::draw() {
	ofPath path;
	for (const auto& fp : petals) {
		if (!fp.alive || fp.alpha <= 0.01f) continue;

		ofColor c = fp.color;
		c.a = (unsigned char)(fp.alpha * 255.0f);
		buildPetalPath(path, fp.shape, c);

		glm::vec2 pos = fp.drawPosition();
		ofPushMatrix();
		ofTranslate(pos.x, pos.y);
		ofRotateDeg(fp.rotation);
		path.draw();
		ofPopMatrix();
	}
}

void FallingPetalSystem::clear() {
	petals.clear();
}

int FallingPetalSystem::activeCount() const {
	return (int)petals.size();
}

// ============================================================
// FlowerField
// ============================================================

void FlowerField::respawnFlower(FlowerInstance& fi) {
	// Random position: full screen coverage
	fi.normPos.x = ofRandom(0.02f, 0.98f);
	fi.normPos.y = ofRandom(0.05f, 0.98f);

	// Depth scale: flowers near bottom (y~0.98) are close/large, near top (y~0.05) are far/small
	float depthT = (fi.normPos.y - 0.05f) / 0.93f;
	fi.depthScale = ofLerp(0.3f, 1.2f, depthT);

	// Random base petal properties
	fi.basePetalCount = (int)ofRandom(4, 9);
	fi.baseLength = ofRandom(35.0f, 75.0f);
	fi.baseWidth = ofRandom(0.2f, 0.55f);
	fi.basePointiness = ofRandom(0.2f, 0.8f);
	fi.baseBulge = ofRandom(0.3f, 0.7f);
	fi.baseEdgeCurvature = ofRandom(-0.15f, 0.4f);
	fi.baseCenterRadius = ofRandom(4.0f, 12.0f);

	// Stem
	fi.baseStemHeight = ofRandom(60.0f, 140.0f);
	fi.baseStemCurvature = ofRandom(-0.4f, 0.4f);

	// Colors: warm petal hues
	float hue = ofRandom(0, 40);
	if (ofRandom(1.0f) > 0.6f) hue = ofRandom(200, 260);
	fi.basePetalColor.setHsb(
		(int)hue % 256,
		(int)ofRandom(120, 230),
		(int)ofRandom(160, 250)
	);
	fi.baseCenterColor.setHsb(
		(int)ofRandom(25, 50),
		(int)ofRandom(180, 240),
		(int)ofRandom(200, 255)
	);
	fi.baseStemColor.setHsb(
		(int)ofRandom(75, 110),
		(int)ofRandom(100, 200),
		(int)ofRandom(60, 160)
	);

	// Music reactivity personality
	fi.pitchDirection = (ofRandom(1.0f) > 0.5f) ? 1.0f : -1.0f;
	fi.lifeSpeedMult = ofRandom(0.7f, 1.3f);

	// Initialize flower with base params (small — will grow)
	InflorescenceParams ip;
	ip.petal.count = fi.basePetalCount;
	ip.petal.length = 0.1f;
	ip.petal.width = fi.baseWidth;
	ip.petal.tipPointiness = fi.basePointiness;
	ip.petal.bulgePosition = fi.baseBulge;
	ip.petal.edgeCurvature = fi.baseEdgeCurvature;
	ip.centerRadius = 0.1f;
	ip.petalColor = fi.basePetalColor;
	ip.centerColor = fi.baseCenterColor;

	StemParams sp;
	sp.height = 0.1f;
	sp.thickness = ofLerp(1.5f, 4.0f, fi.depthScale);
	sp.curvature = fi.baseStemCurvature;
	sp.color = fi.baseStemColor;

	fi.flower.setup(ip, sp);
	fi.lastVisiblePetals = -1;
}

void FlowerField::setup(int count) {
	instances.resize(count);

	for (auto& fi : instances) {
		respawnFlower(fi);
		// Stagger starting phases so they don't all bloom at once
		fi.lifePhase = ofRandom(0.0f, 1.0f);
	}

	// Sort back to front (lower y = farther = drawn first)
	std::sort(instances.begin(), instances.end(),
		[](const FlowerInstance& a, const FlowerInstance& b) {
			return a.normPos.y < b.normPos.y;
		});
}

void FlowerField::update(float volume, float pitch, float confidence, float fullness) {
	// Smooth inputs
	float volAlpha = 0.18f;  // smooth pulse
	float fullAlpha = 0.15f;
	float pitchAlpha = 0.3f;

	smoothedVolume = smoothedVolume * (1.0f - volAlpha) + ofClamp(volume * 5.0f, 0.0f, 1.0f) * volAlpha;
	smoothedFullness = smoothedFullness * (1.0f - fullAlpha) + fullness * fullAlpha;
	if (confidence > 0.1f && pitch > 50.0f) {
		smoothedPitch = smoothedPitch * (1.0f - pitchAlpha) + pitch * pitchAlpha;
	}

	// Normalize pitch to [-1, 1] centered at ~middle C (261 Hz)
	float pitchNorm = 0.0f;
	if (smoothedPitch > 50.0f) {
		float logP = std::log2(smoothedPitch);
		float logCenter = std::log2(261.0f);
		float logRange = std::log2(2500.0f) - std::log2(50.0f);
		pitchNorm = ofClamp((logP - logCenter) / (logRange * 0.5f), -1.0f, 1.0f);
	}

	// Lifecycle speed: fullness controls how fast the cycle runs
	// ~18s full cycle at fullness=1, slower when quiet, never fully stopped
	float dt = ofClamp(ofGetLastFrameTime(), 0.001f, 0.1f);
	float baseSpeed = 1.0f / 18.0f;
	float speed = baseSpeed * (0.05f + smoothedFullness * 0.95f);

	bool needsSort = false;

	for (auto& fi : instances) {
		// Advance lifecycle
		fi.lifePhase += speed * fi.lifeSpeedMult * dt;

		if (fi.lifePhase >= 1.0f) {
			respawnFlower(fi);
			fi.lifePhase = 0.0f;
			needsSort = true;
		}

		float phase = fi.lifePhase;

		// Lifecycle phase outputs
		float scale = 1.0f;         // flower head scale
		float stemScale = 1.0f;     // stem height scale
		float stemCurveMod = 0.0f;  // additional bend during wilt
		float alpha = 1.0f;
		float volumePulse = 1.0f;
		float currentPointiness = fi.basePointiness;
		int visiblePetals = fi.basePetalCount;

		// --- Phase: Growing (0.00 - 0.15) ---
		if (phase < 0.15f) {
			float t = phase / 0.15f;
			scale = t * t;       // ease-in growth
			stemScale = t;
			alpha = t;
		}
		// --- Phase: Blooming (0.15 - 0.60) ---
		else if (phase < 0.60f) {
			// Full music reactivity
			volumePulse = 1.0f + smoothedVolume * 0.9f;
			float pointinessMod = fi.pitchDirection * pitchNorm * 0.35f;
			currentPointiness = ofClamp(fi.basePointiness + pointinessMod, 0.0f, 1.0f);
		}
		// --- Phase: Losing petals (0.60 - 0.80) ---
		else if (phase < 0.80f) {
			float t = (phase - 0.60f) / 0.20f;
			visiblePetals = std::max(0, (int)std::round(fi.basePetalCount * (1.0f - t)));
			scale = 1.0f - t * 0.3f;
			// Fading music reactivity
			float reactivity = 1.0f - t;
			volumePulse = 1.0f + smoothedVolume * 0.9f * reactivity;
			float pointinessMod = fi.pitchDirection * pitchNorm * 0.35f * reactivity;
			currentPointiness = ofClamp(fi.basePointiness + pointinessMod, 0.0f, 1.0f);
		}
		// --- Phase: Wilting (0.80 - 0.95) ---
		else if (phase < 0.95f) {
			float t = (phase - 0.80f) / 0.15f;
			visiblePetals = 0;
			scale = (1.0f - t) * 0.7f;
			stemScale = 1.0f - t * 0.6f;
			stemCurveMod = t * 1.5f;
			alpha = 1.0f - t * 0.6f;
		}
		// --- Phase: Dead / fade out (0.95 - 1.0) ---
		else {
			float t = (phase - 0.95f) / 0.05f;
			visiblePetals = 0;
			scale = 0.01f;
			stemScale = 0.4f * (1.0f - t);
			stemCurveMod = 1.5f;
			alpha = (1.0f - t) * 0.4f;
		}

		fi.currentAlpha = ofClamp(alpha, 0.0f, 1.0f);

		// Detect petal drops and spawn falling petals
		if (fi.lastVisiblePetals >= 0 && visiblePetals < fi.lastVisiblePetals) {
			int dropped = fi.lastVisiblePetals - visiblePetals;
			float screenX = fi.normPos.x * ofGetWidth();
			float screenY = fi.normPos.y * ofGetHeight();
			glm::vec2 stemTop = fi.flower.getStem().getTopPosition();
			glm::vec2 headPos(screenX + stemTop.x, screenY + stemTop.y);

			float angleStep = 360.0f / std::max(fi.lastVisiblePetals, 1);
			for (int d = 0; d < dropped; d++) {
				int petalIdx = fi.lastVisiblePetals - 1 - d;
				float angle = petalIdx * angleStep;

				PetalParams detachedShape;
				detachedShape.length = fi.baseLength * fi.depthScale * scale * volumePulse;
				detachedShape.width = fi.baseWidth;
				detachedShape.tipPointiness = currentPointiness;
				detachedShape.bulgePosition = fi.baseBulge;
				detachedShape.edgeCurvature = fi.baseEdgeCurvature;

				fallingPetals.spawn(headPos, angle, detachedShape, fi.basePetalColor);
			}
		}
		fi.lastVisiblePetals = visiblePetals;

		// Update inflorescence params
		InflorescenceParams ip;
		ip.petal.count = visiblePetals;
		ip.petal.length = fi.baseLength * fi.depthScale * scale * volumePulse;
		ip.petal.width = fi.baseWidth;
		ip.petal.tipPointiness = currentPointiness;
		ip.petal.bulgePosition = fi.baseBulge;
		ip.petal.edgeCurvature = fi.baseEdgeCurvature;
		ip.centerRadius = fi.baseCenterRadius * fi.depthScale * std::max(scale, 0.1f);
		ip.petalColor = fi.basePetalColor;
		ip.centerColor = fi.baseCenterColor;
		fi.flower.getInflorescence().setParams(ip);

		// Update stem params
		StemParams sp;
		sp.height = fi.baseStemHeight * fi.depthScale * stemScale;
		sp.thickness = ofLerp(1.5f, 4.0f, fi.depthScale);
		sp.curvature = ofClamp(fi.baseStemCurvature + stemCurveMod, -2.0f, 2.0f);
		sp.color = fi.baseStemColor;
		fi.flower.getStem().setParams(sp);
	}

	// Re-sort if any flowers respawned to new y positions
	if (needsSort) {
		std::sort(instances.begin(), instances.end(),
			[](const FlowerInstance& a, const FlowerInstance& b) {
				return a.normPos.y < b.normPos.y;
			});
	}

	// Update falling petals
	fallingPetals.update(dt);
}

void FlowerField::draw() {
	float w = ofGetWidth();
	float h = ofGetHeight();

	for (auto& fi : instances) {
		if (fi.currentAlpha <= 0.01f) continue;

		float screenX = fi.normPos.x * w;
		float screenY = fi.normPos.y * h;
		unsigned char a = (unsigned char)(fi.currentAlpha * 255.0f);

		// Apply lifecycle alpha to colors
		auto& ip = fi.flower.getInflorescence().getParams();
		ofColor pc = fi.basePetalColor;  pc.a = a;
		ofColor cc = fi.baseCenterColor; cc.a = a;
		ip.petalColor = pc;
		ip.centerColor = cc;
		fi.flower.getInflorescence().setParams(ip);

		auto& sp = fi.flower.getStem().getParams();
		ofColor sc = fi.baseStemColor; sc.a = a;
		sp.color = sc;
		fi.flower.getStem().setParams(sp);

		fi.flower.draw(screenX, screenY);
	}

	// Draw falling petals on top of flowers
	fallingPetals.draw();
}
