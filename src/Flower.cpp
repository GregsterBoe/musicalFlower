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
	if (params.headType == HeadType::LAYERED_WHORLS) {
		const auto& w = params.whorls;
		whorlPaths.resize(w.layerCount);
		for (int layer = 0; layer < w.layerCount; layer++) {
			float t = (float)layer / std::max(w.layerCount - 1, 1);
			PetalParams lp = params.petal;
			lp.length *= (1.0f - t * (1.0f - w.lengthFalloff));
			lp.width = std::min(lp.width * (1.0f + t * (w.widthGrowth - 1.0f)), 0.8f);
			buildPetalPath(whorlPaths[layer], lp, params.petalColor);
		}
	} else {
		buildPetalPath(petalPath, params.petal, params.petalColor);
	}
	dirty = false;
}

Inflorescence::NoiseResult Inflorescence::computeNoise(int petalIdx) const {
	NoiseResult nr = {1.0f, 0.0f, 0.0f};
	if (!params.noise.enabled) return nr;

	float seed = params.noise.seed;
	float time = ofGetElapsedTimef() * params.noise.timeSpeed;
	float px = seed + petalIdx * 7.3f;

	nr.lengthScale = 1.0f + ofSignedNoise(px, time) * params.noise.lengthAmount;
	nr.angleDeg = ofSignedNoise(px + 100.0f, time) * params.noise.angleAmount;
	nr.scaleVal = ofSignedNoise(px + 200.0f, time) * params.noise.scaleAmount;

	return nr;
}

void Inflorescence::draw() {
	if (dirty) rebuild();

	ofPushMatrix();
	ofRotateDeg(params.rotation);

	switch (params.headType) {
		case HeadType::RADIAL:        drawRadial(); break;
		case HeadType::PHYLLOTAXIS:   drawPhyllotaxis(); break;
		case HeadType::ROSE_CURVE:    drawRoseCurve(); break;
		case HeadType::SUPERFORMULA:  drawSuperformula(); break;
		case HeadType::LAYERED_WHORLS: drawLayeredWhorls(); break;
	}

	// Center
	ofPushStyle();
	ofFill();
	ofSetColor(params.centerColor);
	drawCenter();
	ofPopStyle();

	ofPopMatrix();
}

void Inflorescence::drawCenter() {
    float r = params.centerRadius;
    
    switch (params.centerType) {
        case CenterType::SIMPLE_DISC:
            ofDrawCircle(0, 0, r);
            break;

        case CenterType::STAMENS: {
            int count = 8 * params.centerDetail;
            for (int i = 0; i < count; i++) {
                ofRotateDeg(360.0f / count);
                ofSetLineWidth(1.5);
                ofDrawLine(0, 0, r, 0); // The filament
                ofDrawCircle(r, 0, r * 0.2f); // The anther (tip)
            }
            break;
        }

        case CenterType::POLLEN_GRID: {
            // A mini phyllotaxis pattern for the center itself
            for (int i = 0; i < 20 * params.centerDetail; i++) {
                float angle = i * 137.508f;
                float dist = (r * 0.8f) * sqrt(i / (20.0f * params.centerDetail));
                ofDrawCircle(dist * cos(ofDegToRad(angle)), dist * sin(ofDegToRad(angle)), r * 0.15f);
            }
            break;
        }

        case CenterType::GEOMETRIC_STAR: {
            int points = 5 * params.centerDetail;
            ofBeginShape();
            for (int i = 0; i < points * 2; i++) {
                float angle = i * PI / points;
                float dist = (i % 2 == 0) ? r : r * 0.5f;
                ofVertex(cos(angle) * dist, sin(angle) * dist);
            }
            ofEndShape(true);
            break;
        }
    }
}

void Inflorescence::drawRadial() {
	int count = params.petal.count;
	if (count <= 0) return;
	float angleStep = 360.0f / count;
	for (int i = 0; i < count; i++) {
		NoiseResult nr = computeNoise(i);
		ofPushMatrix();
		ofRotateDeg(i * angleStep + nr.angleDeg);
		ofScale(1.0f + nr.scaleVal, nr.lengthScale);
		petalPath.draw();
		ofPopMatrix();
	}
}

void Inflorescence::drawPhyllotaxis() {
	int count = params.petal.count;
	if (count <= 0) return;
	float c = params.phyllotaxis.spiralSpacing;
	float goldenAngle = 137.508f;

	for (int i = 0; i < count; i++) {
		float angle = i * goldenAngle;
		float r = c * std::sqrt((float)i);

		NoiseResult nr = computeNoise(i);

		ofPushMatrix();
		float rad = ofDegToRad(angle);
		ofTranslate(r * std::cos(rad), -r * std::sin(rad));
		ofRotateDeg(-angle + 90.0f + nr.angleDeg);
		ofScale(1.0f + nr.scaleVal, nr.lengthScale);
		petalPath.draw();
		ofPopMatrix();
	}
}

void Inflorescence::drawRoseCurve() {
	int count = params.petal.count;
	if (count <= 0) return;
	float k = params.roseCurve.k;
	float baseScale = params.roseCurve.baseScale;
	float angleStep = 360.0f / count;

	for (int i = 0; i < count; i++) {
		float angle = i * angleStep;
		float theta = ofDegToRad(angle);
		float roseVal = std::abs(std::cos(k * theta));
		float lengthMod = baseScale + roseVal * (1.0f - baseScale);

		NoiseResult nr = computeNoise(i);

		ofPushMatrix();
		ofRotateDeg(angle + nr.angleDeg);
		ofScale(1.0f + nr.scaleVal, lengthMod * nr.lengthScale);
		petalPath.draw();
		ofPopMatrix();
	}
}

void Inflorescence::drawSuperformula() {
	int count = params.petal.count;
	if (count <= 0) return;
	const auto& sf = params.superformula;
	float angleStep = 360.0f / count;

	for (int i = 0; i < count; i++) {
		float angle = i * angleStep;
		float theta = ofDegToRad(angle);

		float ct = std::cos(sf.m * theta / 4.0f) / sf.a;
		float st = std::sin(sf.m * theta / 4.0f) / sf.b;
		float term = std::pow(std::abs(ct), sf.n2) + std::pow(std::abs(st), sf.n3);
		float r = (term > 1e-6f) ? std::pow(term, -1.0f / sf.n1) : 1.0f;
		r = ofClamp(r, 0.2f, 1.5f);

		NoiseResult nr = computeNoise(i);

		ofPushMatrix();
		ofRotateDeg(angle + nr.angleDeg);
		ofScale(1.0f + nr.scaleVal, r * nr.lengthScale);
		petalPath.draw();
		ofPopMatrix();
	}
}

void Inflorescence::drawLayeredWhorls() {
	const auto& w = params.whorls;
	int totalVisible = params.petal.count;
	if (totalVisible <= 0) return;

	int layers = w.layerCount;
	int perLayer = w.petalsPerLayer;

	// Draw outer-to-inner (outer = highest indices, drawn first = behind)
	for (int layer = layers - 1; layer >= 0; layer--) {
		int layerStart = layer * perLayer;
		float angleStep = 360.0f / perLayer;
		float phaseOffset = (layer % 2 == 1) ? angleStep * w.phaseShift : 0.0f;

		int pathIdx = layer;
		if (pathIdx >= (int)whorlPaths.size()) continue;

		for (int p = 0; p < perLayer; p++) {
			int globalIdx = layerStart + p;
			if (globalIdx >= totalVisible) break;

			float angle = p * angleStep + phaseOffset;
			NoiseResult nr = computeNoise(globalIdx);

			ofPushMatrix();
			ofRotateDeg(angle + nr.angleDeg);
			ofScale(1.0f + nr.scaleVal, nr.lengthScale);
			whorlPaths[pathIdx].draw();
			ofPopMatrix();
		}
	}
}

// ============================================================
// Stem
// ============================================================

void Stem::setup(const StemParams& p) {
	params = p;
	tendrils.clear();
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
	float xOff = params.curvature * h * 0.3f;

	// Center bezier: P0=base, P3=top
	glm::vec2 p0(0.0f, 0.0f);
	glm::vec2 p1(xOff * 0.6f, -h * 0.5f);
	glm::vec2 p2(xOff, -h + h * 0.1f);
	glm::vec2 p3(xOff, -h);

	int numSamples = std::max(params.segments * 8, 20);

	std::vector<glm::vec2> leftEdge, rightEdge;

	for (int i = 0; i <= numSamples; i++) {
		float t = (float)i / numSamples;

		// Evaluate cubic bezier
		float u = 1.0f - t;
		glm::vec2 pos = u*u*u*p0 + 3.0f*u*u*t*p1 + 3.0f*u*t*t*p2 + t*t*t*p3;
		glm::vec2 tang = 3.0f*u*u*(p1 - p0) + 6.0f*u*t*(p2 - p1) + 3.0f*t*t*(p3 - p2);

		// Normal (perpendicular to tangent)
		float tanLen = glm::length(tang);
		glm::vec2 normal = (tanLen > 0.001f)
			? glm::vec2(-tang.y, tang.x) / tanLen
			: glm::vec2(1.0f, 0.0f);

		// Thickness tapers from base to tip
		float localHalfThick = params.thickness * 0.5f * ofLerp(1.0f, params.taperRatio, t);

		// Node bumps at segment boundaries
		if (params.segments > 1) {
			for (int s = 1; s < params.segments; s++) {
				float nodeT = (float)s / params.segments;
				float dist = std::abs(t - nodeT);
				float bumpRadius = 0.06f;
				if (dist < bumpRadius) {
					float bump = 1.0f + (params.nodeWidth - 1.0f)
						* 0.5f * (1.0f + std::cos(PI * dist / bumpRadius));
					localHalfThick *= bump;
				}
			}
		}

		leftEdge.push_back(pos - normal * localHalfThick);
		rightEdge.push_back(pos + normal * localHalfThick);
	}

	// Build closed path: left edge forward, right edge backward
	stemPath.clear();
	stemPath.setFilled(true);
	stemPath.setFillColor(params.color);

	stemPath.moveTo(leftEdge[0]);
	for (size_t i = 1; i < leftEdge.size(); i++) {
		stemPath.lineTo(leftEdge[i]);
	}
	for (int i = (int)rightEdge.size() - 1; i >= 0; i--) {
		stemPath.lineTo(rightEdge[i]);
	}
	stemPath.close();

	dirty = false;
}

void Stem::draw() {
	if (dirty) rebuild();
	stemPath.draw();
	drawTendrils();
}

void Stem::setTendrils(const std::vector<TendrilDef>& t) {
	tendrils = t;
}

glm::vec2 Stem::stemPointAt(float t) const {
	float h = params.height;
	float xOff = params.curvature * h * 0.3f;
	glm::vec2 p0(0.0f, 0.0f);
	glm::vec2 p1(xOff * 0.6f, -h * 0.5f);
	glm::vec2 p2(xOff, -h + h * 0.1f);
	glm::vec2 p3(xOff, -h);

	float u = 1.0f - t;
	return u*u*u*p0 + 3.0f*u*u*t*p1 + 3.0f*u*t*t*p2 + t*t*t*p3;
}

glm::vec2 Stem::stemTangentAt(float t) const {
	float h = params.height;
	float xOff = params.curvature * h * 0.3f;
	glm::vec2 p0(0.0f, 0.0f);
	glm::vec2 p1(xOff * 0.6f, -h * 0.5f);
	glm::vec2 p2(xOff, -h + h * 0.1f);
	glm::vec2 p3(xOff, -h);

	float u = 1.0f - t;
	return 3.0f*u*u*(p1 - p0) + 6.0f*u*t*(p2 - p1) + 3.0f*t*t*(p3 - p2);
}

void Stem::drawTendrils() {
	if (tendrils.empty()) return;

	for (const auto& td : tendrils) {
		glm::vec2 stemPos = stemPointAt(td.stemT);
		glm::vec2 stemTang = stemTangentAt(td.stemT);

		// Normal pointing right (when stem is vertical)
		glm::vec2 rightNormal = glm::normalize(glm::vec2(-stemTang.y, stemTang.x));
		glm::vec2 upTang = glm::normalize(stemTang);

		// Base direction: mix of outward (perpendicular) + upward (along stem)
		glm::vec2 baseDir = glm::normalize(
			rightNormal * td.direction * std::cos(ofDegToRad(td.startAngle))
			+ upTang * std::sin(ofDegToRad(td.startAngle))
		);
		float angle = std::atan2(baseDir.y, baseDir.x);

		float actualLength = td.length * params.height;
		int steps = 15;
		float segLen = actualLength / steps;
		glm::vec2 pos = stemPos;

		ofSetColor(params.color);
		ofSetLineWidth(td.thickness);

		for (int i = 0; i < steps; i++) {
			float frac = (float)i / steps;
			angle += td.curlAmount * PI / steps * td.direction;
			float curSegLen = segLen * (1.0f - frac * 0.4f);
			glm::vec2 next = pos + glm::vec2(std::cos(angle), std::sin(angle)) * curSegLen;
			ofDrawLine(pos.x, pos.y, next.x, next.y);
			pos = next;
		}
	}

	ofSetLineWidth(1.0f);
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
// Petal position helper
// ============================================================

PetalPosition computePetalPosition(HeadType type, int petalIdx, int totalPetals,
                                    const InflorescenceParams& params) {
	PetalPosition pp;
	switch (type) {
		case HeadType::PHYLLOTAXIS: {
			float goldenAngle = 137.508f;
			pp.angleDeg = std::fmod(petalIdx * goldenAngle, 360.0f);
			pp.radiusFromCenter = params.phyllotaxis.spiralSpacing
			                      * std::sqrt((float)petalIdx);
			break;
		}
		case HeadType::LAYERED_WHORLS: {
			const auto& w = params.whorls;
			int perLayer = w.petalsPerLayer;
			int layer = petalIdx / perLayer;
			int posInLayer = petalIdx % perLayer;
			float angleStep = 360.0f / perLayer;
			float phaseOffset = (layer % 2 == 1) ? angleStep * w.phaseShift : 0.0f;
			pp.angleDeg = std::fmod(posInLayer * angleStep + phaseOffset, 360.0f);
			pp.radiusFromCenter = 0.0f;
			break;
		}
		default: {
			float angleStep = 360.0f / std::max(totalPetals, 1);
			pp.angleDeg = std::fmod(petalIdx * angleStep, 360.0f);
			pp.radiusFromCenter = 0.0f;
			break;
		}
	}
	if (pp.angleDeg < 0.0f) pp.angleDeg += 360.0f;
	return pp;
}

// ============================================================
// Color Schemes
// ============================================================

struct ColorSchemeDef {
	const char* name;
	float hueMin, hueMax;      // petal hue range (0-255 oF HSB)
	float satMin, satMax;
	float briMin, briMax;
};

// 8 palettes spaced around the color wheel
// oF HSB hue: 0=red, 42=yellow, 85=green, 128=cyan, 170=blue, 213=purple, 255=red
static const ColorSchemeDef kColorSchemes[8] = {
	{"Sunset",    5.0f,  25.0f,  180.0f, 240.0f, 200.0f, 255.0f},   // warm orange-red
	{"Golden",   35.0f,  55.0f,  180.0f, 240.0f, 200.0f, 255.0f},   // amber-yellow
	{"Emerald",  75.0f, 105.0f,  140.0f, 220.0f, 160.0f, 230.0f},   // green
	{"Ocean",   115.0f, 145.0f,  130.0f, 210.0f, 170.0f, 240.0f},   // teal-cyan
	{"Arctic",  150.0f, 175.0f,  100.0f, 180.0f, 190.0f, 255.0f},   // ice-blue
	{"Twilight",190.0f, 215.0f,  140.0f, 220.0f, 160.0f, 240.0f},   // indigo-purple
	{"Orchid",  220.0f, 242.0f,  130.0f, 210.0f, 180.0f, 250.0f},   // violet-magenta
	{"Rose",    242.0f, 255.0f,  150.0f, 230.0f, 190.0f, 255.0f},   // pink-red
};
static const int kNumSchemes = 8;

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

	// Random base petal properties (defaults; head type may override some)
	fi.baseLength = ofRandom(35.0f, 75.0f);
	fi.baseWidth = ofRandom(0.2f, 0.55f);
	fi.basePointiness = ofRandom(0.2f, 0.8f);
	fi.baseBulge = ofRandom(0.3f, 0.7f);
	fi.baseEdgeCurvature = ofRandom(-0.15f, 0.4f);
	fi.baseCenterRadius = ofRandom(4.0f, 12.0f);

	// Assign head type with weighted distribution
	float typeRoll = ofRandom(1.0f);
	if (typeRoll < 0.25f) {
		fi.baseHeadType = HeadType::RADIAL;
		fi.basePetalCount = (int)ofRandom(4, 9);
	} else if (typeRoll < 0.45f) {
		fi.baseHeadType = HeadType::PHYLLOTAXIS;
		fi.basePetalCount = (int)ofRandom(25, 41);
		fi.basePhyllotaxis.spiralSpacing = ofRandom(3.0f, 6.0f);
		fi.baseLength = ofRandom(15.0f, 30.0f);
		fi.baseCenterRadius = ofRandom(2.0f, 5.0f);
	} else if (typeRoll < 0.65f) {
		fi.baseHeadType = HeadType::ROSE_CURVE;
		fi.basePetalCount = (int)ofRandom(10, 17);
		float kOptions[] = {2.0f, 2.5f, 3.0f, 3.5f, 4.0f, 5.0f};
		fi.baseRoseCurve.k = kOptions[(int)ofRandom(0, 6)];
		fi.baseRoseCurve.baseScale = ofRandom(0.2f, 0.45f);
	} else if (typeRoll < 0.80f) {
		fi.baseHeadType = HeadType::SUPERFORMULA;
		fi.basePetalCount = (int)ofRandom(12, 21);
		fi.baseSuperformula.m = ofRandom(3.0f, 8.0f);
		fi.baseSuperformula.n1 = ofRandom(0.3f, 2.0f);
		fi.baseSuperformula.n2 = ofRandom(0.5f, 2.0f);
		fi.baseSuperformula.n3 = ofRandom(0.5f, 2.0f);
		fi.baseSuperformula.a = ofRandom(0.8f, 1.2f);
		fi.baseSuperformula.b = ofRandom(0.8f, 1.2f);
	} else {
		fi.baseHeadType = HeadType::LAYERED_WHORLS;
		fi.baseWhorls.layerCount = (int)ofRandom(3, 5);
		fi.baseWhorls.petalsPerLayer = (int)ofRandom(5, 9);
		fi.basePetalCount = fi.baseWhorls.layerCount * fi.baseWhorls.petalsPerLayer;
		fi.baseWhorls.lengthFalloff = ofRandom(0.55f, 0.8f);
		fi.baseWhorls.widthGrowth = ofRandom(1.2f, 1.6f);
		fi.baseWhorls.phaseShift = ofRandom(0.4f, 0.6f);
	}

	// Noise modifier: 60% of flowers get gentle wobble
	fi.baseNoise.enabled = (ofRandom(1.0f) > 0.4f);
	fi.baseNoise.seed = ofRandom(0.0f, 10000.0f);
	fi.baseNoise.lengthAmount = ofRandom(0.03f, 0.10f);
	fi.baseNoise.angleAmount = ofRandom(1.0f, 5.0f);
	fi.baseNoise.scaleAmount = ofRandom(0.02f, 0.06f);
	fi.baseNoise.timeSpeed = ofRandom(0.05f, 0.2f);

	// Stem
	fi.baseStemHeight = ofRandom(60.0f, 140.0f);
	fi.baseStemCurvature = ofRandom(-0.4f, 0.4f);

	// Stem diversity: taper + segments
	fi.baseTaperRatio = ofRandom(0.15f, 0.5f);
	fi.baseSegments = (ofRandom(1.0f) > 0.4f) ? (int)ofRandom(2, 5) : 1;
	fi.baseNodeWidth = ofRandom(1.4f, 2.0f);

	// Tendrils (40% of flowers)
	fi.baseTendrils.clear();
	if (ofRandom(1.0f) > 0.6f) {
		int numTendrils = (int)ofRandom(1, 4);
		for (int i = 0; i < numTendrils; i++) {
			TendrilDef td;
			td.stemT = ofRandom(0.2f, 0.7f);
			td.length = ofRandom(0.15f, 0.35f);
			td.curlAmount = ofRandom(1.0f, 3.0f);
			td.direction = (ofRandom(1.0f) > 0.5f) ? 1.0f : -1.0f;
			td.startAngle = ofRandom(10.0f, 50.0f);
			td.thickness = ofRandom(1.0f, 2.5f);
			fi.baseTendrils.push_back(td);
		}
	}

	// Color scheme selection
	int schemeIdx;
	if (colorMode == 0) {
		schemeIdx = iterateIndex;
		iterateIndex = (iterateIndex + 1) % kNumSchemes;
	} else if (colorMode == 9) {
		schemeIdx = (int)ofRandom(0, kNumSchemes);
	} else {
		schemeIdx = ofClamp(colorMode - 1, 0, kNumSchemes - 1);
	}
	const auto& cs = kColorSchemes[schemeIdx];

	// 1. Pick Petal Color from scheme
	float hue = ofRandom(cs.hueMin, cs.hueMax);
	fi.basePetalColor.setHsb(hue, ofRandom(cs.satMin, cs.satMax),
	                         ofRandom(cs.briMin, cs.briMax));

	// 2. Complementary center color (hue + 128)
	float centerHue = fmod(hue + 128.0f, 256.0f);
	fi.baseCenterColor.setHsb(
		(int)centerHue,
		(int)ofRandom(200, 255),
		(int)ofRandom(200, 255)
	);

	// 3. Stem: natural green tinted slightly toward the scheme
	float schemeMidHue = (cs.hueMin + cs.hueMax) * 0.5f;
	float stemHue = fmod(ofLerp(85.0f, schemeMidHue, 0.2f), 256.0f);
	fi.baseStemColor.setHsb(stemHue, ofRandom(100, 170), ofRandom(80, 160));

	// 3. Assign a Center Type based on Head Type for "Best Fit"
	if (fi.baseHeadType == HeadType::PHYLLOTAXIS) {
		fi.baseCenterType = CenterType::POLLEN_GRID; // Fits the "sunflower" look
	} else if (fi.baseHeadType == HeadType::RADIAL) {
		fi.baseCenterType = CenterType::STAMENS;     // Fits the "lily/daisy" look
	} else {
		fi.baseCenterType = (ofRandom(1.0f) > 0.5f) ? CenterType::SIMPLE_DISC : CenterType::GEOMETRIC_STAR;
	}
	fi.baseCenterDetail = ofRandom(1.0f, 2.5f);

	// Music reactivity personality
	fi.pitchDirection = (ofRandom(1.0f) > 0.5f) ? 1.0f : -1.0f;
	fi.lifeSpeedMult = ofRandom(0.7f, 1.3f);

	// Reset fast death state
	fi.fastDeath = false;
	fi.fastDeathTimer = 0.0f;

	// Rotation: in reactive mode all flowers rotate faster based on activity
	fi.rotationAccum = 0.0f;
	if (reactiveMode) {
		fi.rotationSpeed = ofRandom(20.0f, 60.0f) * (0.5f + activityLevel);
		fi.rotationDir = (ofRandom(1.0f) > 0.5f) ? 1.0f : -1.0f;
	} else if (ofRandom(1.0f) > 0.4f) {
		fi.rotationSpeed = ofRandom(15.0f, 45.0f);
		fi.rotationDir = (ofRandom(1.0f) > 0.5f) ? 1.0f : -1.0f;
	} else {
		fi.rotationSpeed = 0.0f;
		fi.rotationDir = 1.0f;
	}

	// Initialize flower with base params (small — will grow)
	InflorescenceParams ip;
	ip.headType = fi.baseHeadType;
	ip.petal.count = fi.basePetalCount;
	ip.petal.length = 0.1f;
	ip.petal.width = fi.baseWidth;
	ip.petal.tipPointiness = fi.basePointiness;
	ip.petal.bulgePosition = fi.baseBulge;
	ip.petal.edgeCurvature = fi.baseEdgeCurvature;
	ip.centerRadius = 0.1f;
	ip.petalColor = fi.basePetalColor;
	ip.centerColor = fi.baseCenterColor;
	ip.centerType = fi.baseCenterType;
	ip.centerDetail = fi.baseCenterDetail;
	ip.phyllotaxis = fi.basePhyllotaxis;
	ip.roseCurve = fi.baseRoseCurve;
	ip.superformula = fi.baseSuperformula;
	ip.whorls = fi.baseWhorls;
	ip.noise = fi.baseNoise;

	StemParams sp;
	sp.height = 0.1f;
	sp.thickness = ofLerp(1.5f, 4.0f, fi.depthScale);
	sp.taperRatio = fi.baseTaperRatio;
	sp.curvature = fi.baseStemCurvature;
	sp.color = fi.baseStemColor;
	sp.segments = fi.baseSegments;
	sp.nodeWidth = fi.baseNodeWidth;

	fi.flower.setup(ip, sp);
	fi.flower.getStem().setTendrils(fi.baseTendrils);
	fi.lastVisiblePetals = -1;
}

void FlowerField::setReactiveMode(bool enabled) {
	reactiveMode = enabled;
}

bool FlowerField::isReactiveMode() const {
	return reactiveMode;
}

void FlowerField::setColorMode(int mode) {
	colorMode = ofClamp(mode, 0, 9);
	iterateIndex = 0;
}

int FlowerField::getColorMode() const {
	return colorMode;
}

std::string FlowerField::getColorSchemeName() const {
	if (colorMode == 0) return "Cycling";
	if (colorMode == 9) return "Random";
	int idx = ofClamp(colorMode - 1, 0, kNumSchemes - 1);
	return kColorSchemes[idx].name;
}

void FlowerField::setup(int count) {
	baseCount = count;
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
	// Smooth inputs (lower alpha = smoother, less jitter)
	float volAlpha = 0.08f;
	float fullAlpha = 0.10f;
	float pitchAlpha = 0.12f;

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

	// In reactive mode, boost lifecycle speed so flowers turn over faster
	if (reactiveMode) {
		speed *= (1.0f + activityLevel * 1.5f);
	}
	// When returning to normal mode with too many flowers, gently accelerate lifecycle
	else if ((int)instances.size() > baseCount) {
		float overshoot = (float)instances.size() / (float)baseCount;
		speed *= (1.0f + (overshoot - 1.0f) * 2.0f);
	}

	// Beat/onset detection: compare fast volume to slow baseline
	float slowAlpha = 0.02f;
	slowVolume = slowVolume * (1.0f - slowAlpha) + smoothedVolume * slowAlpha;
	beatCooldown -= dt;
	elapsedTime += dt;

	bool beatThisFrame = false;
	if (beatCooldown <= 0.0f && smoothedVolume > 0.05f) {
		float ratio = (slowVolume > 0.01f) ? smoothedVolume / slowVolume : 0.0f;
		if (ratio > 1.4f) {
			beatThisFrame = true;
			beatCooldown = 0.25f;  // 250ms cooldown between beats
			beatHistory.push_back(elapsedTime);
		}
	}

	// Purge beat history older than 5 seconds
	while (!beatHistory.empty() && (elapsedTime - beatHistory.front()) > 5.0f) {
		beatHistory.pop_front();
	}

	// Compute activity score (0-1): beat density + volume + fullness
	float beatDensity = ofClamp((float)beatHistory.size() / 20.0f, 0.0f, 1.0f);
	float rawActivity = 0.5f * beatDensity + 0.3f * smoothedVolume + 0.2f * smoothedFullness;
	float actAlpha = 0.03f;
	activityLevel = activityLevel * (1.0f - actAlpha) + rawActivity * actAlpha;

	// Dynamic flower count management
	bool needsSort = false;
	int targetCount = baseCount;
	if (reactiveMode) {
		targetCount = (int)ofLerp(30.0f, 1500.0f, activityLevel);
	}

	int currentCount = (int)instances.size();

	// Growing: spawn new flowers (batched to avoid frame spikes)
	if (currentCount < targetCount) {
		int toSpawn = std::min(targetCount - currentCount, 10);
		for (int i = 0; i < toSpawn; i++) {
			FlowerInstance fi;
			respawnFlower(fi);
			fi.lifePhase = 0.0f;
			instances.push_back(std::move(fi));
		}
		needsSort = true;
	}
	// Shrinking: randomly mark flowers for dramatic fast death across the field
	else if (currentCount > targetCount + 5) {
		int toMark = std::min(currentCount - targetCount, 5);
		for (int i = 0; i < toMark; i++) {
			// Try a few random picks to find an eligible flower
			for (int attempt = 0; attempt < 5; attempt++) {
				int idx = (int)ofRandom(0, instances.size());
				auto& candidate = instances[idx];
				// Only mark flowers that are alive, visible, and not already dying
				if (!candidate.fastDeath && candidate.lifePhase > 0.15f
				    && candidate.lifePhase < 0.80f) {
					candidate.fastDeath = true;
					candidate.fastDeathTimer = 0.0f;
					break;
				}
			}
		}
	}

	for (auto& fi : instances) {
		// Advance lifecycle
		fi.lifePhase += speed * fi.lifeSpeedMult * dt;

		if (fi.lifePhase >= 1.0f) {
			// If over target count, mark for removal instead of respawning
			if ((int)instances.size() > targetCount) {
				fi.lifePhase = 999.0f;  // sentinel for removal
				continue;
			}
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

		// Fast death override: all petals burst off, stem collapses rapidly
		if (fi.fastDeath) {
			fi.fastDeathTimer += dt * 1.5f;  // ~0.67s total animation
			if (fi.fastDeathTimer >= 1.0f) {
				fi.currentAlpha = 0.0f;
				fi.lifePhase = 999.0f;  // mark for removal
				continue;
			}
			float fd = fi.fastDeathTimer;
			visiblePetals = 0;                         // all petals pop off on first frame
			scale = std::max(0.01f, (1.0f - fd) * 0.7f);
			stemScale = 1.0f - fd * 0.7f;
			stemCurveMod = fd * 3.0f;                  // dramatic droop
			alpha = 1.0f - fd * fd;                    // ease-out fade
		}

		fi.currentAlpha = ofClamp(alpha, 0.0f, 1.0f);

		// Beat-driven rotation: flip direction on onset, speed scaled by volume
		if (fi.rotationSpeed > 0.0f) {
			if (beatThisFrame && ofRandom(1.0f) > 0.3f) {
				fi.rotationDir *= -1.0f;
			}
			fi.rotationAccum += fi.rotationSpeed * fi.rotationDir
			                    * (0.3f + smoothedVolume * 0.7f) * dt;
		}

		// Detect petal drops and spawn falling petals
		if (fi.lastVisiblePetals >= 0 && visiblePetals < fi.lastVisiblePetals) {
			int dropped = fi.lastVisiblePetals - visiblePetals;
			float screenX = fi.normPos.x * ofGetWidth();
			float screenY = fi.normPos.y * ofGetHeight();
			glm::vec2 stemTop = fi.flower.getStem().getTopPosition();
			glm::vec2 headPos(screenX + stemTop.x, screenY + stemTop.y);

			InflorescenceParams currentIp = fi.flower.getInflorescence().getParams();

			for (int d = 0; d < dropped; d++) {
				int petalIdx = fi.lastVisiblePetals - 1 - d;

				PetalPosition pp = computePetalPosition(
					fi.baseHeadType, petalIdx, fi.basePetalCount, currentIp);

				PetalParams detachedShape;
				detachedShape.length = fi.baseLength * fi.depthScale * scale * volumePulse;
				detachedShape.width = fi.baseWidth;
				detachedShape.tipPointiness = currentPointiness;
				detachedShape.bulgePosition = fi.baseBulge;
				detachedShape.edgeCurvature = fi.baseEdgeCurvature;

				// Offset spawn by radial distance (for phyllotaxis spiral)
				float rad = ofDegToRad(pp.angleDeg);
				float rScaled = pp.radiusFromCenter * fi.depthScale * scale;
				glm::vec2 spawnPos = headPos + glm::vec2(
					rScaled * std::sin(rad),
					-rScaled * std::cos(rad));

				fallingPetals.spawn(spawnPos, pp.angleDeg, detachedShape, fi.basePetalColor);
			}
		}
		fi.lastVisiblePetals = visiblePetals;

		// Update inflorescence params
		InflorescenceParams ip;
		ip.headType = fi.baseHeadType;
		ip.petal.count = visiblePetals;
		ip.petal.length = fi.baseLength * fi.depthScale * scale * volumePulse;
		ip.petal.width = fi.baseWidth;
		ip.petal.tipPointiness = currentPointiness;
		ip.petal.bulgePosition = fi.baseBulge;
		ip.petal.edgeCurvature = fi.baseEdgeCurvature;
		ip.centerRadius = fi.baseCenterRadius * fi.depthScale * std::max(scale, 0.1f);
		ip.rotation = fi.rotationAccum;
		ip.petalColor = fi.basePetalColor;
		ip.centerColor = fi.baseCenterColor;
		ip.centerType = fi.baseCenterType;
		ip.centerDetail = fi.baseCenterDetail;
		ip.phyllotaxis = fi.basePhyllotaxis;
		ip.roseCurve = fi.baseRoseCurve;
		ip.superformula = fi.baseSuperformula;
		ip.whorls = fi.baseWhorls;
		ip.noise = fi.baseNoise;
		fi.flower.getInflorescence().setParams(ip);

		// Update stem params
		StemParams sp;
		sp.height = fi.baseStemHeight * fi.depthScale * stemScale;
		sp.thickness = ofLerp(1.5f, 4.0f, fi.depthScale);
		sp.taperRatio = fi.baseTaperRatio;
		sp.curvature = ofClamp(fi.baseStemCurvature + stemCurveMod, -2.0f, 2.0f);
		sp.color = fi.baseStemColor;
		sp.segments = fi.baseSegments;
		sp.nodeWidth = fi.baseNodeWidth;
		fi.flower.getStem().setParams(sp);
	}

	// Remove flowers marked for death (sentinel lifePhase)
	instances.erase(
		std::remove_if(instances.begin(), instances.end(),
			[](const FlowerInstance& fi) { return fi.lifePhase >= 2.0f; }),
		instances.end());

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
