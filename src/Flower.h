#pragma once
#include "ofMain.h"

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

// --- Inflorescence (flower head) ---

struct InflorescenceParams {
	PetalParams petal;
	float centerRadius = 8.0f;
	float rotation = 0.0f;       // degrees
	ofColor petalColor{220, 80, 120};
	ofColor centerColor{255, 220, 50};
};

class Inflorescence {
public:
	void setup(const InflorescenceParams& params);
	void draw();
	void setParams(const InflorescenceParams& params);
	InflorescenceParams& getParams();

private:
	void rebuild();
	InflorescenceParams params;
	ofPath petalPath;
	bool dirty = true;
};

// --- Stem ---

struct StemParams {
	float height = 120.0f;
	float thickness = 3.0f;
	float curvature = 0.0f;  // -1 to 1, bend left/right
	ofColor color{60, 140, 50};
};

class Stem {
public:
	void setup(const StemParams& params);
	void draw();
	void setParams(const StemParams& params);
	StemParams& getParams();
	glm::vec2 getTopPosition() const;

private:
	void rebuild();
	StemParams params;
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
	ofColor basePetalColor;
	ofColor baseCenterColor;
	ofColor baseStemColor;

	// Per-flower music reactivity
	float pitchDirection;       // +1 or -1: how pitch modulates pointiness
	float depthScale;           // computed from y position (perspective)

	// Lifecycle
	float lifePhase = 0.0f;    // 0-1 progress through bloom→decay cycle
	float lifeSpeedMult = 1.0f; // slight per-flower speed variation
	float currentAlpha = 1.0f;  // computed per frame for draw
	int lastVisiblePetals = -1; // tracks petal count for detecting drops
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

private:
	void respawnFlower(FlowerInstance& fi);

	std::vector<FlowerInstance> instances;
	float smoothedVolume = 0.0f;
	float smoothedPitch = 0.0f;
	float smoothedFullness = 0.0f;
	FallingPetalSystem fallingPetals;
};
