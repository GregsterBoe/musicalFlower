#pragma once
#include "ofMain.h"
#include <deque>

// --- Petal shape parameters ---

struct PetalParams {
	int count = 5;
	float length = 60.0f;        // pixels from center to tip
	float width = 0.35f;         // max half-width as fraction of length (0.0-1.0)
	float tipPointiness = 0.5f;  // 0 = fully rounded tip, 1 = sharp point
	float bulgePosition = 0.5f;  // where widest point is along petal (0=base, 1=tip)
	float edgeCurvature = 0.2f;  // >0 convex, <0 concave, 0 straight edges
};

// Build a single petal shape into the given path (shared by Inflorescence and FallingPetalSystem)
void buildPetalPath(ofPath& path, const PetalParams& params, ofColor color);

// --- Head type enum and type-specific params ---

enum class HeadType {
	RADIAL,
	PHYLLOTAXIS,
	ROSE_CURVE,
	SUPERFORMULA,
	LAYERED_WHORLS
};

enum class CenterType {
    SIMPLE_DISC,    // The original circle
    STAMENS,        // Thin lines with pollen tips
    POLLEN_GRID,    // Dense clusters of small dots
    GEOMETRIC_STAR  // A small star-like shape
};

struct PhyllotaxisParams {
	float spiralSpacing = 4.0f;     // 'c' in r = c*sqrt(n)
};

struct RoseCurveParams {
	float k = 3.0f;                 // lobe parameter
	float baseScale = 0.3f;         // min petal length fraction at curve minima
};

struct SuperformulaParams {
	float m = 5.0f;
	float n1 = 1.0f;
	float n2 = 1.0f;
	float n3 = 1.0f;
	float a = 1.0f;
	float b = 1.0f;
};

struct LayeredWhorlsParams {
	int layerCount = 3;
	int petalsPerLayer = 6;
	float lengthFalloff = 0.7f;     // inner layers shorter
	float widthGrowth = 1.4f;       // inner layers wider
	float phaseShift = 0.5f;        // fraction of angleStep offset per alternate layer
};

struct NoiseModParams {
	bool enabled = false;
	float seed = 0.0f;
	float lengthAmount = 0.15f;     // +/- fraction of length
	float angleAmount = 8.0f;       // +/- degrees
	float scaleAmount = 0.1f;       // +/- uniform scale
	float timeSpeed = 0.3f;         // noise animation speed
};

// --- Petal position helper (for falling petal spawn) ---

struct PetalPosition {
	float angleDeg;
	float radiusFromCenter;
};

PetalPosition computePetalPosition(HeadType type, int petalIdx, int totalPetals,
                                    const struct InflorescenceParams& params);

// --- Inflorescence (flower head) ---

struct InflorescenceParams {
	HeadType headType = HeadType::RADIAL;
	PetalParams petal;
	float centerRadius = 8.0f;
	float rotation = 0.0f;       // degrees
	ofColor petalColor{220, 80, 120};
	ofColor centerColor{255, 220, 50};

	PhyllotaxisParams phyllotaxis;
	RoseCurveParams roseCurve;
	SuperformulaParams superformula;
	LayeredWhorlsParams whorls;
	NoiseModParams noise;

	CenterType centerType = CenterType::SIMPLE_DISC;
    float centerDetail = 1.0f; // Controls density/points
};

class Inflorescence {
public:
	void setup(const InflorescenceParams& params);
	void draw();
	void setParams(const InflorescenceParams& params);
	InflorescenceParams& getParams();

private:
	void rebuild();

	void drawRadial();
	void drawPhyllotaxis();
	void drawRoseCurve();
	void drawSuperformula();
	void drawLayeredWhorls();
	void drawCenter();

	struct NoiseResult { float lengthScale; float angleDeg; float scaleVal; };
	NoiseResult computeNoise(int petalIdx) const;

	InflorescenceParams params;
	ofPath petalPath;
	std::vector<ofPath> whorlPaths;
	bool dirty = true;
};

// --- Stem ---

struct StemParams {
	float height = 120.0f;
	float thickness = 3.0f;
	float taperRatio = 0.3f; // tip thickness as fraction of base (0.1-1.0)
	float curvature = 0.0f;  // -1 to 1, bend left/right
	ofColor color{60, 140, 50};
	int segments = 1;         // 1 = smooth taper, 2+ = visible node joints
	float nodeWidth = 1.3f;   // thickness multiplier at node joints
};

struct TendrilDef {
	float stemT;              // 0-1 position along stem
	float length;             // fraction of stem height
	float curlAmount;         // curl rotation (1.0 = half-circle, 2.0 = full)
	float direction;          // 1=right, -1=left
	float startAngle;         // degrees tilt from perpendicular
	float thickness;          // line width
};

class Stem {
public:
	void setup(const StemParams& params);
	void draw();
	void setParams(const StemParams& params);
	StemParams& getParams();
	glm::vec2 getTopPosition() const;
	void setTendrils(const std::vector<TendrilDef>& tendrils);

private:
	void rebuild();
	void drawTendrils();
	glm::vec2 stemPointAt(float t) const;
	glm::vec2 stemTangentAt(float t) const;

	StemParams params;
	std::vector<TendrilDef> tendrils;
	ofPath stemPath;
	bool dirty = true;
};

// --- Complete Flower ---

class Flower {
public:
	void setup(const InflorescenceParams& inflorescence, const StemParams& stem);
	void draw(float x, float y);  // (x, y) = ground position (stem base)

	Inflorescence& getInflorescence();
	Stem& getStem();

private:
	Inflorescence inflorescence;
	Stem stem;
};

// --- A single flower instance in the field with random personality ---

struct FlowerInstance {
	Flower flower;
	glm::vec2 normPos;          // 0-1 normalized screen position (ground point)

	// Head type
	HeadType baseHeadType = HeadType::RADIAL;
	PhyllotaxisParams basePhyllotaxis;
	RoseCurveParams baseRoseCurve;
	SuperformulaParams baseSuperformula;
	LayeredWhorlsParams baseWhorls;
	NoiseModParams baseNoise;

	//center type

	CenterType baseCenterType = CenterType::SIMPLE_DISC;
	float baseCenterDetail = 1.0f;

	// Random base properties (set once at creation)
	int basePetalCount;
	float baseLength;
	float baseWidth;
	float basePointiness;
	float baseBulge;
	float baseEdgeCurvature;
	float baseCenterRadius;
	float baseStemHeight;
	float baseStemCurvature;
	float baseTaperRatio;
	int baseSegments;
	float baseNodeWidth;
	std::vector<TendrilDef> baseTendrils;
	ofColor basePetalColor;
	ofColor baseCenterColor;
	ofColor baseStemColor;

	// Per-flower music reactivity
	float pitchDirection;       // +1 or -1: how pitch modulates pointiness
	float depthScale;           // computed from y position (perspective)

	// Rotation (beat-driven)
	float rotationAccum = 0.0f;   // accumulated rotation degrees
	float rotationSpeed = 0.0f;   // base speed deg/s (0 = no rotation)
	float rotationDir = 1.0f;     // +1 or -1, flipped on beat

	// Lifecycle
	float lifePhase = 0.0f;    // 0-1 progress through bloom→decay cycle
	float lifeSpeedMult = 1.0f; // slight per-flower speed variation
	float currentAlpha = 1.0f;  // computed per frame for draw
	int lastVisiblePetals = -1; // tracks petal count for detecting drops

	// Fast death: dramatic rapid wilt triggered when flower count needs to shrink
	bool fastDeath = false;
	float fastDeathTimer = 0.0f;  // 0-1 progress of fast death animation
};

// --- Falling petal animation ---

struct FallingPetalConfig {
	float gravity = 50.0f;           // px/s² downward acceleration
	float waverAmplitude = 25.0f;    // px max horizontal oscillation
	float waverFrequency = 0.6f;     // oscillations per second
	float tumbleSpeed = 120.0f;      // degrees/s base tumble rate
	float fadeDelay = 1.5f;          // seconds before alpha fade begins
	float fadeSpeed = 0.6f;          // alpha reduction per second
	float initialUpPop = 10.0f;      // px/s initial upward velocity
	float maxLifetime = 4.0f;        // seconds before auto-remove
};

struct FallingPetal {
	glm::vec2 basePosition;          // center of oscillation (moves with velocity)
	glm::vec2 velocity;              // px/s
	float rotation = 0.0f;           // current orientation degrees
	float rotationSpeed = 0.0f;      // degrees/s tumble
	float alpha = 1.0f;
	float age = 0.0f;                // seconds since detach
	float waverPhase = 0.0f;
	float waverAmp = 0.0f;
	float waverFreq = 0.0f;

	PetalParams shape;                // visual shape at final pixel size
	ofColor color;

	bool alive = true;

	glm::vec2 drawPosition() const;
};

class FallingPetalSystem {
public:
	void setConfig(const FallingPetalConfig& cfg);
	FallingPetalConfig& getConfig();

	void spawn(glm::vec2 headPos, float detachAngleDeg,
	           const PetalParams& shape, ofColor color);
	void update(float dt);
	void draw();
	void clear();
	int activeCount() const;

private:
	FallingPetalConfig config;
	std::vector<FallingPetal> petals;
};

// --- Field of flowers driven by audio ---

class FlowerField {
public:
	void setup(int count);
	void update(float volume, float pitch, float confidence, float fullness);
	void draw();
	void setReactiveMode(bool enabled);
	bool isReactiveMode() const;

private:
	void respawnFlower(FlowerInstance& fi);

	std::vector<FlowerInstance> instances;
	float smoothedVolume = 0.0f;
	float smoothedPitch = 0.0f;
	float smoothedFullness = 0.0f;

	// Beat/onset detection
	float slowVolume = 0.0f;      // slow EMA for baseline comparison
	float beatCooldown = 0.0f;    // seconds until next beat can trigger

	// Reactive mode: dynamic flower count driven by musical activity
	bool reactiveMode = false;
	int baseCount = 300;              // normal-mode count (from setup)
	float activityLevel = 0.0f;       // smoothed 0-1 composite activity score
	std::deque<float> beatHistory;    // timestamps of recent beats (for density)
	float elapsedTime = 0.0f;         // running clock

	FallingPetalSystem fallingPetals;
};
