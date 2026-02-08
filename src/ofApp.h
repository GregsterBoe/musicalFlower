#pragma once

#include "ofMain.h"
#include <essentia/essentia.h>
#include <essentia/algorithmfactory.h>
#include <mutex>
#include <deque>

class ofApp : public ofBaseApp{

	public:
		void setup() override;
		void update() override;
		void draw() override;
		void exit() override;

		void audioIn(ofSoundBuffer& input) override;

		void keyPressed(int key) override;
		void keyReleased(int key) override;
		void mouseMoved(int x, int y ) override;
		void mouseDragged(int x, int y, int button) override;
		void mousePressed(int x, int y, int button) override;
		void mouseReleased(int x, int y, int button) override;
		void mouseScrolled(int x, int y, float scrollX, float scrollY) override;
		void mouseEntered(int x, int y) override;
		void mouseExited(int x, int y) override;
		void windowResized(int w, int h) override;
		void dragEvent(ofDragInfo dragInfo) override;
		void gotMessage(ofMessage msg) override;

	private:
		std::string pitchToNoteName(float freqHz);
		void autoRouteAudio();

		// Audio input
		ofSoundStream soundStream;

		// Thread-safe audio buffering
		std::mutex audioMutex;
		std::vector<float> audioInputBuffer;
		std::vector<float> processingBuffer;
		float rmsVolume = 0.0f;

		// Essentia algorithms
		essentia::standard::Algorithm* windowing = nullptr;
		essentia::standard::Algorithm* spectrum = nullptr;
		essentia::standard::Algorithm* pitchYinFFT = nullptr;

		// Essentia I/O buffers
		std::vector<essentia::Real> frame;
		std::vector<essentia::Real> windowedFrame;
		std::vector<essentia::Real> spectrumValues;
		essentia::Real currentPitch = 0.0f;
		essentia::Real currentPitchConfidence = 0.0f;

		// Visualization state
		std::vector<float> displaySpectrum;
		float smoothedPitch = 0.0f;
		float smoothedConfidence = 0.0f;
		std::deque<std::pair<float, float>> melodyHistory; // pitch, confidence

		// Constants
		static const int kFrameSize = 2048;
		static const int kSampleRate = 44100;
};
